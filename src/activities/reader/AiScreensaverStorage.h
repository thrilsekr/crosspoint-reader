#pragma once

#include <string>

// AI screensaver images live outside the disposable EPUB layout cache. The
// existing cache directory name is the stable per-book identity in this build.
inline std::string aiScreensaverDirectoryForCachePath(const std::string& cachePath) {
  const size_t slash = cachePath.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= cachePath.size()) return {};
  const std::string cacheName = cachePath.substr(slash + 1);
  if (cacheName.rfind("epub_", 0) != 0) return {};
  return "/.crosspoint/ai-screensaver/" + cacheName;
}
