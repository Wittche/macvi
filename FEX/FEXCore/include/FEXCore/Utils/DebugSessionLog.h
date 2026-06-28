// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace FEXCore::Core {
struct CpuStateFrame;
}

namespace FEXCore::DebugSession {

void Log(const char* location, const char* hypothesis_id, const char* message, uint64_t guest_rip, uint64_t gregs_r14,
         uint64_t host_x19, uint64_t extra = 0);

enum class GPR14LogTag : uint32_t {
  PushR14Before = 1,
  PushR14After = 2,
  PopR14Before = 3,
  PopR14After = 4,
  ExitLinkEnter = 5,
  ExitLinkReturn = 6,
};

// Called from JIT-generated code; host_x19 is live in the register.
extern "C" void LogGPR14Tag(FEXCore::Core::CpuStateFrame* Frame, uint32_t tag, uint64_t host_x19, uint64_t extra);

} // namespace FEXCore::DebugSession
