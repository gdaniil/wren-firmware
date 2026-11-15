#pragma once

// Removes every book cache directory under /.crosspoint. Shared by the Wren setup flow and the
// upstream ClearCacheActivity so both behave identically.
struct ReadingCacheResult {
  bool opened = false;  // false: /.crosspoint could not be opened
  int cleared = 0;
  int failed = 0;
};

ReadingCacheResult clearReadingCache();
