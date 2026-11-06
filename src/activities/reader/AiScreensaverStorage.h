#pragma once

#include <string>

// AI screensaver images live outside the disposable EPUB layout cache. The
// existing cache directory name is the stable per-book identity in this build.
inline std::string aiScreensaverDirectoryForCachePath(const std::string& cachePath) {
  size_t end = cachePath.size();
  while (end > 0 && cachePath[end - 1] == '/') --end;
  if (end == 0) return {};
  const size_t slash = cachePath.rfind('/', end - 1);
  if (slash == std::string::npos || slash + 1 >= end) return {};
  const std::string cacheName = cachePath.substr(slash + 1, end - slash - 1);
  if (cacheName.rfind("epub_", 0) != 0) return {};
  return "/.crosspoint/ai-screensaver/" + cacheName;
}
