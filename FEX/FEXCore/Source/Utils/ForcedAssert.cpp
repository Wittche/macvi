
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

namespace FEXCore::Assert {
__attribute__((noinline)) void ForcedAssert() {
  fprintf(stderr, "FEX ForcedAssert! Stack trace:\n");
  void* callstack[128];
  int frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);
  for (int i = 0; i < frames; ++i) {
      fprintf(stderr, "%s\n", strs[i]);
  }
  free(strs);
  abort();
}
}
