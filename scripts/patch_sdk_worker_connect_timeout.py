#!/usr/bin/env python3
"""Apply the X4 worker connection-timeout patch to the exact pinned FreeInk SDK.

The SDK is a git submodule and remains pinned to the upstream commit. This
repository-owned patch is applied only in the build workspace after checkout.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / "freeink-sdk"
EXPECTED_SDK_SHA = "5e94fe7480f019c89fbf691aef87523eb1b46704"


def replace_once(path: Path, old: str, new: str) -> None:
    source = path.read_text(encoding="utf-8")
    if new in source:
        return  # already patched; keep the script idempotent
    count = source.count(old)
    if count != 1:
        raise RuntimeError(
            f"{path.relative_to(ROOT)}: expected exactly one patch anchor, found {count}"
        )
    path.write_text(source.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    actual_sha = subprocess.check_output(
        ["git", "-C", str(SDK), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual_sha != EXPECTED_SDK_SHA:
        raise RuntimeError(
            f"Refusing to patch unexpected SDK revision: {actual_sha}"
        )

    secure_client_h = SDK / "libs/network/SecureNet/include/SecureClient.h"
    secure_client_cpp = SDK / "libs/network/SecureNet/src/SecureClient.cpp"
    secure_http_h = SDK / "libs/network/SecureNet/include/SecureHttpClient.h"
    resumable_h = SDK / "libs/network/SecureNet/include/ResumableFetch.h"

    replace_once(
        secure_client_h,
        "  void setAbortCallback(AbortCallback cb) { _shouldAbort = std::move(cb); }",
        "  void setAbortCallback(AbortCallback cb) { _shouldAbort = std::move(cb); }\n"
        "  // Separate TCP connection budget; zero preserves the Stream timeout.\n"
        "  void setConnectionTimeout(uint32_t ms) { _connectionTimeoutMs = ms; }",
    )
    replace_once(
        secure_client_h,
        "  AbortCallback _shouldAbort;\n",
        "  AbortCallback _shouldAbort;\n"
        "  uint32_t _connectionTimeoutMs = 0;  // zero inherits getTimeout()\n",
    )
    replace_once(
        secure_client_cpp,
        "  const uint32_t timeoutMs = getTimeout();\n"
        "  if (abortRequested()) return 0;\n"
        "  _transport.setConnectionTimeout(timeoutMs);",
        "  const uint32_t timeoutMs = getTimeout();\n"
        "  if (abortRequested()) return 0;\n"
        "  const uint32_t connectTimeoutMs = _connectionTimeoutMs == 0 ? timeoutMs : _connectionTimeoutMs;\n"
        "  _transport.setConnectionTimeout(connectTimeoutMs);",
    )
    replace_once(
        secure_http_h,
        "  void setTimeout(uint32_t ms) { _timeoutMs = ms; }",
        "  void setTimeout(uint32_t ms) { _timeoutMs = ms; }\n"
        "  // Zero inherits setTimeout(); otherwise bounds TCP connect separately.\n"
        "  void setConnectTimeout(uint32_t ms) { _connectTimeoutMs = ms; }",
    )
    replace_once(
        secure_http_h,
        "      _secure.setTimeout(_timeoutMs);\n"
        "      _secure.setAbortCallback(shouldAbort);",
        "      _secure.setTimeout(_timeoutMs);\n"
        "      _secure.setConnectionTimeout(_connectTimeoutMs == 0 ? _timeoutMs : _connectTimeoutMs);\n"
        "      _secure.setAbortCallback(shouldAbort);",
    )
    replace_once(
        secure_http_h,
        "      _plain.setTimeout(_timeoutMs);\n"
        "      _plain.setConnectionTimeout(_timeoutMs);",
        "      _plain.setTimeout(_timeoutMs);\n"
        "      _plain.setConnectionTimeout(_connectTimeoutMs == 0 ? _timeoutMs : _connectTimeoutMs);",
    )
    replace_once(
        secure_http_h,
        "  uint32_t _timeoutMs = 15000;\n",
        "  uint32_t _timeoutMs = 15000;\n"
        "  uint32_t _connectTimeoutMs = 0;  // zero inherits _timeoutMs\n",
    )
    replace_once(
        resumable_h,
        "  bool redirectToHttp = false;\n",
        "  bool redirectToHttp = false;\n"
        "  // Worker requests disable redirects to keep the IPv4-only DNS-free contract.\n"
        "  bool followRedirects = true;\n",
    )
    replace_once(
        resumable_h,
        "    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {\n"
        "      const std::string location = http.getHeader(\"location\");",
        "    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {\n"
        "      if (!options.followRedirects) return result;\n"
        "      const std::string location = http.getHeader(\"location\");",
    )
    print("Applied X4 worker TCP connect-timeout patch to pinned FreeInk SDK.")


if __name__ == "__main__":
    main()
