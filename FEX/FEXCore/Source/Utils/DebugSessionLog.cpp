// SPDX-License-Identifier: MIT
#include <FEXCore/Utils/DebugSessionLog.h>

#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/X86Enums.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace FEXCore::DebugSession {

namespace {
constexpr const char* SessionId = "b5ed63";

void AppendNDJSON(const char* location, const char* hypothesis_id, const char* message, uint64_t guest_rip, uint64_t gregs_r14,
                  uint64_t host_x19, uint64_t extra) {
  fprintf(stderr,
          R"({"sessionId":"%s","location":"%s","message":"%s","data":{"guest_rip":"0x%llx","gregs_r14":"0x%llx","host_x19":"0x%llx","extra":"0x%llx"}})"
          "\n",
          SessionId, location, message, static_cast<unsigned long long>(guest_rip),
          static_cast<unsigned long long>(gregs_r14), static_cast<unsigned long long>(host_x19), static_cast<unsigned long long>(extra));
}
} // namespace

void Log(const char* location, const char* hypothesis_id, const char* message, uint64_t guest_rip, uint64_t gregs_r14,
         uint64_t host_x19, uint64_t extra) {
  AppendNDJSON(location, hypothesis_id, message, guest_rip, gregs_r14, host_x19, extra);
}

extern "C" void LogGPR14Tag(FEXCore::Core::CpuStateFrame* Frame, uint32_t tag, uint64_t host_x19, uint64_t extra) {
  if (!Frame) {
    return;
  }

  const char* Location = "unknown";
  const char* Hypothesis = "E";
  const char* Message = "gpr14_tag";

  switch (static_cast<GPR14LogTag>(tag)) {
  case GPR14LogTag::PushR14Before:
    Location = "MemoryOps.cpp:Push";
    Hypothesis = "A";
    Message = "push_r14_before";
    break;
  case GPR14LogTag::PushR14After:
    Location = "MemoryOps.cpp:Push";
    Hypothesis = "A";
    Message = "push_r14_after";
    break;
  case GPR14LogTag::PopR14Before:
    Location = "MemoryOps.cpp:Pop";
    Hypothesis = "A";
    Message = "pop_r14_before";
    break;
  case GPR14LogTag::PopR14After:
    Location = "MemoryOps.cpp:Pop";
    Hypothesis = "E";
    Message = "pop_r14_after";
    break;
  default: break;
  }

  const uint64_t GuestRIP = Frame->State.rip;
  const uint64_t GregsR14 = Frame->State.gregs[FEXCore::X86State::REG_R14];
  AppendNDJSON(Location, Hypothesis, Message, GuestRIP, GregsR14, host_x19, extra);
}

} // namespace FEXCore::DebugSession
