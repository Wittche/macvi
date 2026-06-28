// SPDX-License-Identifier: MIT
/*
$info$
tags: glue|log-manager
$end_info$
*/

#include <FEXCore/Utils/CompilerDefs.h>
#include <FEXCore/Utils/LogManager.h>
#include <FEXCore/fextl/fmt.h>
#include <cstdio>

namespace LogMan {

namespace Throw {
  ThrowHandler Handler {};
  void InstallHandler(ThrowHandler _Handler) {
    Handler = _Handler;
  }
  void UnInstallHandler() {
    Handler = nullptr;
  }

  void MFmt(const char* fmt, const fmt::format_args& args) {
    auto msg = fextl::fmt::vformat(fmt, args);
    fprintf(stderr, "[FEX-ASSERT-DIRECT] %s\n", msg.c_str());
    fflush(stderr);
    if (Handler) {
      Handler(msg.c_str());
    }

    FEX_TRAP_EXECUTION;
  }
} // namespace Throw

namespace Msg {
  MsgHandler Handler {};
  void InstallHandler(MsgHandler _Handler) {
    Handler = _Handler;
  }
  void UnInstallHandler() {
    Handler = nullptr;
  }

  void MFmtImpl(DebugLevels level, const char* fmt, const fmt::format_args& args) {
    if (Handler) {
      const auto msg = fextl::fmt::vformat(fmt, args);
      Handler(level, msg.c_str());
    }
  }

} // namespace Msg
} // namespace LogMan
