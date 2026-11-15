#include "BookInfo.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <Memory.h>
#include <Xtc.h>

#include "util/BookProgress.h"

namespace wren::home {
namespace {
constexpr const char* CACHE_DIR = "/.crosspoint";
}

int loadBookProgress(const std::string& path) { return ::loadBookProgress(path); }

bool thumbPath(const std::string& path, int height, std::string& out) {
  // Constructors only derive cache paths; the objects exceed the stack budget.
  if (FsHelpers::hasReflowableBookExtension(path)) {
    auto epub = makeUniqueNoThrow<Epub>(path, CACHE_DIR);
    if (!epub) return false;
    out = epub->getThumbBmpPath(height);
    return true;
  }
  if (FsHelpers::hasXtcExtension(path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(path, CACHE_DIR);
    if (!xtc) return false;
    out = xtc->getThumbBmpPath(height);
    return true;
  }
  return false;
}

}  // namespace wren::home
