#include "HttpDownloader.h"

#include <Arduino.h>
#include <IPAddress.h>
#include <Logging.h>
#include <ResumableFetch.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <string>

#include "WifiPowerSaveGuard.h"

extern "C" void wolfSSL_Arduino_Serial_Print(const char* const msg) { LOG_DBG("WOLFSSL", "%s", msg); }

namespace {
// Per-socket-op timeout. Some OPDS download endpoints are slow to send headers
// (>15s) and chunked catalogs stall mid-body, so 15s killed them. 60s gives
// slow servers room.
constexpr int HTTP_TIMEOUT_MS = 60000;
// Only worker-safe overloads use this short TCP setup budget. Existing callers
// retain the historical 60-second connect behavior.
constexpr int WORKER_CONNECT_TIMEOUT_MS = 2000;

constexpr int MAX_REDIRECTS = 5;

// postForm response cap: token responses are small JSON; a misbehaving server
// must not balloon the heap.
constexpr size_t MAX_RESPONSE_BYTES = 4096;

using AbortCallback = freeink::SecureHttpClient::AbortCallback;

bool isIpv4LiteralUrl(const std::string& url) {
  const size_t schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) return false;
  const size_t hostStart = schemeEnd + 3;
  const size_t hostEnd = url.find_first_of("/?#", hostStart);
  std::string authority = url.substr(hostStart, hostEnd == std::string::npos ? std::string::npos
                                                                            : hostEnd - hostStart);
  if (authority.empty() || authority.find('@') != std::string::npos || authority.front() == '[') return false;
  const size_t portSep = authority.rfind(':');
  const std::string host = portSep == std::string::npos ? authority : authority.substr(0, portSep);
  IPAddress address;
  return address.fromString(host.c_str());
}

void applyCommonClientSetup(freeink::SecureHttpClient& http) {
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setInsecure();
  // setUserAgent replaces SecureHttpClient's built-in UA; addHeader would
  // append a second User-Agent header, which strict servers reject (aiohttp
  // answers 400 "Duplicate 'User-Agent' header found").
  http.setUserAgent("CrossPoint-ESP32-" CROSSPOINT_VERSION);
}

// Written after every request, and read by the UI right after a failure.
int g_lastStatus = 0;
// Whether the last answer came from another origin than the request started at.
bool g_lastRedirected = false;

// All HTTP(S) fetches go through wolfSSL (the firmware's only TLS stack: it
// speaks TLS 1.3 and reads large bodies reliably). Plain-http URLs still use a
// WiFiClient here, so this is safe for non-TLS targets too. A body cut short
// mid-transfer resumes with a Range request (see ResumableFetch.h).
HttpDownloader::DownloadError runGetSecure(const std::string& url, const std::string& username,
                                           const std::string& password,
                                           const std::vector<HttpDownloader::Header>& headers,
                                           const freeink::FetchSink& sink, const AbortCallback& shouldAbort = nullptr,
                                           size_t* bytesOut = nullptr, const bool downgradeRedirectsToHttp = false,
                                           const bool boundedWorkerConnect = false) {
  WifiPowerSaveGuard psGuard;
  freeink::FetchOptions options;
  options.redirectToHttp = downgradeRedirectsToHttp;
  options.followRedirects = !boundedWorkerConnect;
  bool answeredBySameOrigin = true;
  const freeink::FetchResult result = freeink::fetchResumable(
      url, options,
      [&](freeink::SecureHttpClient& http, const bool sameOrigin) {
        answeredBySameOrigin = sameOrigin;
        applyCommonClientSetup(http);
        if (boundedWorkerConnect) http.setConnectTimeout(WORKER_CONNECT_TIMEOUT_MS);
        // Credentials and caller headers stay with the starting origin; a
        // redirect elsewhere (or to plain http) gets neither.
        if (sameOrigin) {
          if (!username.empty() && !password.empty()) http.setBasicAuth(username, password);
          for (const auto& h : headers) http.addHeader(h.first, h.second);
        }
        LOG_DBG("HTTP", "wolfSSL GET: %s (heap %u, max block %u)", url.c_str(), (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMaxAllocHeap());
      },
      sink, shouldAbort);
  if (bytesOut) *bytesOut = result.bytes;
  // FetchResult::status is -1 when no response arrived (bad URL, DNS, connect, TLS).
  g_lastStatus = result.status < 0 ? 0 : result.status;
  g_lastRedirected = !answeredBySameOrigin;

  if (result.aborted) return HttpDownloader::ABORTED;
  if (result.stopped) return HttpDownloader::FILE_ERROR;
  if (result.status == 401 || result.status == 403) {
    LOG_ERR("HTTP", "wolfSSL request unauthorized: status %d: %s", result.status, url.c_str());
    return HttpDownloader::UNAUTHORIZED;
  }
  if (result.status < 200 || result.status >= 300) {
    LOG_ERR("HTTP", "wolfSSL request failed: status %d: %s", result.status, url.c_str());
    return HttpDownloader::HTTP_ERROR;
  }
  if (!result.complete) {
    LOG_ERR("HTTP", "wolfSSL incomplete: got %zu of %zu bytes", result.bytes, result.total);
    return HttpDownloader::HTTP_ERROR;
  }
  return HttpDownloader::OK;
}

}  // namespace

int HttpDownloader::lastStatus() { return g_lastStatus; }

bool HttpDownloader::lastAnswerRedirected() { return g_lastRedirected; }

HttpDownloader::DownloadError HttpDownloader::downloadToFile(
    const std::string& url, const std::string& destPath, const std::atomic<bool>& cancelToken,
    ProgressCallback progress, const std::string& username, const std::string& password,
    const std::vector<Header>& headers) {
  if (!isIpv4LiteralUrl(url)) {
    LOG_ERR("HTTP", "worker download requires an IPv4-literal URL");
    g_lastStatus = 0;
    return HTTP_ERROR;
  }
  if (cancelToken.load(std::memory_order_acquire)) return ABORTED;
  LOG_DBG("HTTP", "Worker download: %s -> %s", url.c_str(), destPath.c_str());
  const std::string partPath = destPath + ".part";
  Storage.remove(partPath.c_str());
  HalFile file;
  if (!Storage.openFileForWrite("HTTP", partPath.c_str(), file)) return FILE_ERROR;

  freeink::FetchSink sink;
  sink.write = [&file](const uint8_t* data, size_t len) { return file.write(data, len) == len; };
  sink.rewind = [&file, &partPath] {
    file.close();
    return Storage.openFileForWrite("HTTP", partPath.c_str(), file);
  };
  sink.progress = progress;
  const AbortCallback shouldAbort = [&cancelToken] {
    return cancelToken.load(std::memory_order_acquire);
  };
  size_t downloaded = 0;
  const DownloadError result =
      runGetSecure(url, username, password, headers, sink, shouldAbort, &downloaded, false, true);
  if (file.isOpen()) file.close();
  if (result != OK) {
    Storage.remove(partPath.c_str());
    return result;
  }
  if (downloaded == 0) {
    Storage.remove(partPath.c_str());
    return HTTP_ERROR;
  }
  if (!Storage.replaceFile(partPath.c_str(), destPath.c_str())) {
    Storage.remove(partPath.c_str());
    return FILE_ERROR;
  }
  return OK;
}



bool HttpDownloader::fetchUrl(const std::string& url, Stream& outContent, const std::string& username,
                              const std::string& password, const std::vector<Header>& headers) {
  return fetchUrl(
      url, [&outContent](const uint8_t* data, size_t len) { return outContent.write(data, len) == len; }, username,
      password, headers);
}

bool HttpDownloader::fetchUrl(const std::string& url, const DataCallback& onData, const std::string& username,
                              const std::string& password, const std::vector<Header>& headers) {
  LOG_DBG("HTTP", "Fetching: %s", url.c_str());
  freeink::FetchSink sink;
  sink.write = onData;
  return runGetSecure(url, username, password, headers, sink) == OK;
}

bool HttpDownloader::fetchUrl(const std::string& url, const DataCallback& onData,
                              const std::atomic<bool>& cancelToken, const std::string& username,
                              const std::string& password, const std::vector<Header>& headers) {
  if (!isIpv4LiteralUrl(url)) {
    LOG_ERR("HTTP", "worker request requires an IPv4-literal URL");
    g_lastStatus = 0;
    g_lastRedirected = false;
    return false;
  }
  if (cancelToken.load(std::memory_order_acquire)) {
    g_lastStatus = 0;
    g_lastRedirected = false;
    return false;
  }
  LOG_DBG("HTTP", "Worker GET: %s", url.c_str());
  freeink::FetchSink sink;
  sink.write = onData;
  const AbortCallback shouldAbort = [&cancelToken] {
    return cancelToken.load(std::memory_order_acquire);
  };
  return runGetSecure(url, username, password, headers, sink, shouldAbort, nullptr, false, true) == OK;
}

bool HttpDownloader::fetchUrl(const std::string& url, const DataCallback& onData, const FetchOptions& options) {
  // OPDS fetches need what fetchResumable withholds: the final HTTP status
  // and a 401 body (auth documents are served as 401 bodies). Direct GET with
  // a manual redirect loop; catalog feeds are small enough that Range resume
  // isn't worth carrying here (books go through downloadToFile, which has it).
  WifiPowerSaveGuard psGuard;
  std::string current = url;
  const std::string startOrigin = freeink::fetchOrigin(url);
  int status = 0;
  bool sinkStopped = false;
  for (int hop = 0; hop <= MAX_REDIRECTS; ++hop) {
    freeink::SecureHttpClient http;
    if (!http.begin(current)) {
      LOG_ERR("HTTP", "bad URL: %s", current.c_str());
      status = 0;
      break;
    }
    applyCommonClientSetup(http);
    http.setTimeout(options.timeoutMs);
    if (options.cancelFlag && options.cancelFlag->load(std::memory_order_acquire)) {
      status = 0;
      break;
    }
    // Credentials belong to the starting origin only: a redirect to another
    // origin (a CDN) must not receive them.
    if (freeink::fetchOrigin(current) == startOrigin) {
      if (!options.bearer.empty()) {
        http.addHeader("Authorization", "Bearer " + options.bearer);
      } else if (!options.username.empty() && !options.password.empty()) {
        http.setBasicAuth(options.username, options.password);
      }
      for (const auto& h : options.headers) http.addHeader(h.first, h.second);
    }
    if (!options.accept.empty()) http.addHeader("Accept", options.accept);
    if (!options.acceptLanguage.empty()) http.addHeader("Accept-Language", options.acceptLanguage);

    LOG_DBG("HTTP", "wolfSSL GET: %s", current.c_str());
    status = http.GET(
        [&](const uint8_t* data, size_t len) {
          const int st = http.getStatus();
          const bool wanted = (st >= 200 && st < 300) || (options.captureErrorBody && st == 401);
          if (!wanted) return true;  // redirect or error body: drain
          if (!onData(data, len)) {
            sinkStopped = true;
            return false;
          }
          return true;
        },
        [&options] { return options.cancelFlag && options.cancelFlag->load(std::memory_order_acquire); });
    if (http.aborted()) {
      status = 0;
      break;
    }
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
      if (!options.followRedirects) break;
      const std::string location = http.getHeader("location");
      if (location.empty() || !freeink::SecureHttpClient::resolveUrl(current, location, current)) break;
      continue;
    }
    break;
  }
  g_lastStatus = status > 0 ? status : 0;
  g_lastRedirected = freeink::fetchOrigin(current) != startOrigin;
  if (options.statusOut) *options.statusOut = g_lastStatus;
  return status >= 200 && status < 300 && !sinkStopped;
}

bool HttpDownloader::postForm(const std::string& url, const std::string& formBody, std::string& outResponse,
                              int* statusOut) {
  WifiPowerSaveGuard psGuard;
  outResponse.clear();
  freeink::SecureHttpClient http;
  if (!http.begin(url)) {
    LOG_ERR("HTTP", "bad URL: %s", url.c_str());
    if (statusOut) *statusOut = 0;
    return false;
  }
  applyCommonClientSetup(http);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  LOG_DBG("HTTP", "wolfSSL POST: %s", url.c_str());
  const int status = http.sendRequest("POST", reinterpret_cast<const uint8_t*>(formBody.data()), formBody.size(),
                                      [&outResponse](const uint8_t* data, size_t len) {
                                        const size_t room =
                                            MAX_RESPONSE_BYTES - std::min(outResponse.size(), MAX_RESPONSE_BYTES);
                                        outResponse.append(reinterpret_cast<const char*>(data), std::min(len, room));
                                        return true;
                                      });
  if (statusOut) *statusOut = status > 0 ? status : 0;
  return status >= 200 && status < 300;
}

bool HttpDownloader::postForm(const std::string& url, const std::string& formBody,
                              std::string& outResponse, const std::atomic<bool>& cancelToken, int* statusOut) {
  if (!isIpv4LiteralUrl(url)) {
    LOG_ERR("HTTP", "worker request requires an IPv4-literal URL");
    g_lastStatus = 0;
    if (statusOut) *statusOut = 0;
    return false;
  }
  if (cancelToken.load(std::memory_order_acquire)) {
    g_lastStatus = 0;
    if (statusOut) *statusOut = 0;
    return false;
  }
  WifiPowerSaveGuard psGuard;
  outResponse.clear();
  freeink::SecureHttpClient http;
  if (!http.begin(url)) {
    g_lastStatus = 0;
    if (statusOut) *statusOut = 0;
    return false;
  }
  applyCommonClientSetup(http);
  http.setConnectTimeout(WORKER_CONNECT_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  const AbortCallback shouldAbort = [&cancelToken] {
    return cancelToken.load(std::memory_order_acquire);
  };
  const int status = http.sendRequest("POST", reinterpret_cast<const uint8_t*>(formBody.data()), formBody.size(),
                                      [&outResponse](const uint8_t* data, size_t len) {
                                        const size_t room =
                                            MAX_RESPONSE_BYTES - std::min(outResponse.size(), MAX_RESPONSE_BYTES);
                                        outResponse.append(reinterpret_cast<const char*>(data), std::min(len, room));
                                        return true;
                                      }, shouldAbort);
  g_lastStatus = status > 0 ? status : 0;
  if (statusOut) *statusOut = g_lastStatus;
  return status >= 200 && status < 300;
}

HttpDownloader::DownloadError HttpDownloader::postToFile(const std::string& url, const std::string& body,
                                                         const std::string& contentType, const std::string& destPath,
                                                         ProgressCallback progress, const bool* cancelFlag) {
  LOG_DBG("HTTP", "POST download: %s -> %s", url.c_str(), destPath.c_str());
  WifiPowerSaveGuard psGuard;

  const std::string partPath = destPath + ".part";
  Storage.remove(partPath.c_str());
  HalFile file;
  if (!Storage.openFileForWrite("HTTP", partPath.c_str(), file)) {
    LOG_ERR("HTTP", "Failed to open file for writing");
    return FILE_ERROR;
  }

  freeink::SecureHttpClient http;
  if (!http.begin(url)) {
    LOG_ERR("HTTP", "bad URL: %s", url.c_str());
    return HTTP_ERROR;
  }
  applyCommonClientSetup(http);
  http.addHeader("Content-Type", contentType);

  size_t downloaded = 0;
  bool writeFailed = false;
  const int status = http.sendRequest(
      "POST", reinterpret_cast<const uint8_t*>(body.data()), body.size(),
      [&](const uint8_t* data, size_t len) {
        const int st = http.getStatus();
        if (st < 200 || st >= 300) return true;  // drain the error body
        if (file.write(data, len) != len) {
          writeFailed = true;
          return false;
        }
        downloaded += len;
        if (progress && http.hasContentLength()) progress(downloaded, http.getContentLength());
        return true;
      },
      [cancelFlag] { return cancelFlag && *cancelFlag; });
  // Close before any remove() on the same path; DESTRUCTOR_CLOSES_FILE would
  // otherwise close only after the remove.
  if (file.isOpen()) file.close();

  DownloadError result = OK;
  if (http.aborted()) {
    result = ABORTED;
  } else if (writeFailed) {
    result = FILE_ERROR;
  } else if (status < 200 || status >= 300) {
    LOG_ERR("HTTP", "POST failed: status %d: %s", status, url.c_str());
    result = HTTP_ERROR;
  } else if (downloaded == 0) {
    LOG_ERR("HTTP", "no data received");
    result = HTTP_ERROR;
  } else if (http.hasContentLength() && downloaded != http.getContentLength()) {
    LOG_ERR("HTTP", "POST response incomplete: %zu of %zu bytes", downloaded, (size_t)http.getContentLength());
    result = HTTP_ERROR;
  }
  if (result != OK) {
    Storage.remove(partPath.c_str());
    return result;
  }
  if (!Storage.replaceFile(partPath.c_str(), destPath.c_str())) {
    LOG_ERR("HTTP", "Failed to move download into place: %s", destPath.c_str());
    Storage.remove(partPath.c_str());
    return FILE_ERROR;
  }
  LOG_DBG("HTTP", "Downloaded %zu bytes", downloaded);
  return OK;
}

HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url, const std::string& destPath,
                                                             ProgressCallback progress, const bool* cancelFlag,
                                                             const std::string& username, const std::string& password,
                                                             const std::vector<Header>& headers,
                                                             bool downgradeRedirectsToHttp) {
  LOG_DBG("HTTP", "Downloading: %s -> %s", url.c_str(), destPath.c_str());

  // Stage in <dest>.part: a failed or cancelled download never replaces an
  // existing copy, and a partial file never sits under the real name.
  const std::string partPath = destPath + ".part";
  Storage.remove(partPath.c_str());
  HalFile file;
  if (!Storage.openFileForWrite("HTTP", partPath.c_str(), file)) {
    LOG_ERR("HTTP", "Failed to open file for writing");
    return FILE_ERROR;
  }

  freeink::FetchSink sink;
  sink.write = [&file](const uint8_t* data, size_t len) { return file.write(data, len) == len; };
  // Reopening for write truncates: the server restarted the body from byte 0.
  sink.rewind = [&file, &partPath] {
    file.close();
    return Storage.openFileForWrite("HTTP", partPath.c_str(), file);
  };
  sink.progress = progress;

  size_t downloaded = 0;
  const DownloadError result =
      runGetSecure(url, username, password, headers, sink, [cancelFlag] { return cancelFlag && *cancelFlag; },
                   &downloaded, downgradeRedirectsToHttp);
  // Close before any remove() on the same path; DESTRUCTOR_CLOSES_FILE would
  // otherwise close only after the remove. A failed rewind leaves no open handle.
  if (file.isOpen()) file.close();

  if (result != OK) {
    Storage.remove(partPath.c_str());
    return result;
  }
  if (downloaded == 0) {
    LOG_ERR("HTTP", "no data received");
    Storage.remove(partPath.c_str());
    return HTTP_ERROR;
  }
  if (!Storage.replaceFile(partPath.c_str(), destPath.c_str())) {
    LOG_ERR("HTTP", "Failed to move download into place: %s", destPath.c_str());
    Storage.remove(partPath.c_str());
    return FILE_ERROR;
  }
  LOG_DBG("HTTP", "Downloaded %zu bytes", downloaded);
  return OK;
}

HttpDownloader::DownloadError HttpDownloader::postToFile(
    const std::string& url, const std::string& body, const std::string& contentType, const std::string& destPath,
    const std::atomic<bool>& cancelToken, ProgressCallback progress) {
  if (!isIpv4LiteralUrl(url)) {
    LOG_ERR("HTTP", "worker POST requires an IPv4-literal URL");
    g_lastStatus = 0;
    return HTTP_ERROR;
  }
  if (cancelToken.load(std::memory_order_acquire)) return ABORTED;
  WifiPowerSaveGuard psGuard;
  const std::string partPath = destPath + ".part";
  Storage.remove(partPath.c_str());
  HalFile file;
  if (!Storage.openFileForWrite("HTTP", partPath.c_str(), file)) return FILE_ERROR;

  freeink::SecureHttpClient http;
  if (!http.begin(url)) {
    if (file.isOpen()) file.close();
    Storage.remove(partPath.c_str());
    g_lastStatus = 0;
    return HTTP_ERROR;
  }
  applyCommonClientSetup(http);
  http.setConnectTimeout(WORKER_CONNECT_TIMEOUT_MS);
  http.addHeader("Content-Type", contentType);
  size_t downloaded = 0;
  bool writeFailed = false;
  const AbortCallback shouldAbort = [&cancelToken] {
    return cancelToken.load(std::memory_order_acquire);
  };
  const int status = http.sendRequest(
      "POST", reinterpret_cast<const uint8_t*>(body.data()), body.size(),
      [&](const uint8_t* data, size_t len) {
        if (http.getStatus() < 200 || http.getStatus() >= 300) return true;
        if (file.write(data, len) != len) {
          writeFailed = true;
          return false;
        }
        downloaded += len;
        if (progress && http.hasContentLength()) progress(downloaded, http.getContentLength());
        return true;
      }, shouldAbort);
  if (file.isOpen()) file.close();
  g_lastStatus = status > 0 ? status : 0;
  if (http.aborted()) {
    Storage.remove(partPath.c_str());
    return ABORTED;
  }
  if (writeFailed) {
    Storage.remove(partPath.c_str());
    return FILE_ERROR;
  }
  if (status < 200 || status >= 300 || downloaded == 0 ||
      (http.hasContentLength() && downloaded != http.getContentLength())) {
    Storage.remove(partPath.c_str());
    return HTTP_ERROR;
  }
  if (!Storage.replaceFile(partPath.c_str(), destPath.c_str())) {
    Storage.remove(partPath.c_str());
    return FILE_ERROR;
  }
  return OK;
}

