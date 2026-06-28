// SPDX-License-Identifier: MIT
#include "Interface/Context/Context.h"
#include "Interface/Core/OpcodeDispatcher.h"
#include "Interface/Core/Dispatcher/Dispatcher.h"
#include "Interface/Core/X86Tables/X86Tables.h"

#include <FEXCore/Core/CoreState.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/Core/CPUID.h>
#include <FEXCore/Core/HostFeatures.h>
#include <FEXCore/Core/SignalDelegator.h>
#include <FEXCore/HLE/SyscallHandler.h>

#include <FEXCore/Core/Thunks.h>
#include <FEXCore/Utils/MathUtils.h>
#include "FEXCore/Debug/InternalThreadState.h"

#include <execinfo.h>
#include <unistd.h>

namespace FEXCore::Context {
fextl::unique_ptr<FEXCore::Context::Context> FEXCore::Context::Context::CreateNewContext(const FEXCore::HostFeatures& Features) {
  return fextl::make_unique<FEXCore::Context::ContextImpl>(Features);
}

void FEXCore::Context::ContextImpl::CompileRIP(FEXCore::Core::InternalThreadState* Thread, uint64_t GuestRIP) {
  CompileBlock(Thread->CurrentFrame, GuestRIP);
}

void FEXCore::Context::ContextImpl::CompileRIPCount(FEXCore::Core::InternalThreadState* Thread, uint64_t GuestRIP, uint64_t MaxInst) {
  CompileBlock(Thread->CurrentFrame, GuestRIP, MaxInst);
}

void FEXCore::Context::ContextImpl::SetSignalDelegator(FEXCore::SignalDelegator* _SignalDelegation) {
  SignalDelegation = _SignalDelegation;
}

void FEXCore::Context::ContextImpl::SetSyscallHandler(FEXCore::HLE::SyscallHandler* Handler) {
  SyscallHandler = Handler;
  SourcecodeResolver = Handler->GetSourcecodeResolver();
}

void FEXCore::Context::ContextImpl::SetThunkHandler(FEXCore::ThunkHandler* Handler) {
  ThunkHandler = Handler;
}

FEXCore::CPUID::FunctionResults FEXCore::Context::ContextImpl::RunCPUIDFunction(uint32_t Function, uint32_t Leaf) {
  return CPUID.RunFunction(Function, Leaf);
}

FEXCore::CPUID::XCRResults FEXCore::Context::ContextImpl::RunXCRFunction(uint32_t Function) {
  return CPUID.RunXCRFunction(Function);
}

FEXCore::CPUID::FunctionResults FEXCore::Context::ContextImpl::RunCPUIDFunctionName(uint32_t Function, uint32_t Leaf, uint32_t CPU) {
  return CPUID.RunFunctionName(Function, Leaf, CPU);
}

bool FEXCore::Context::ContextImpl::IsAddressInCodeBuffer(FEXCore::Core::InternalThreadState* Thread, uintptr_t Address) const {
  if (Thread->CPUBackend->IsAddressInCodeBuffer(Address)) {
    return true;
  }
  if (Dispatcher->IsAddressInDispatcher(Address)) {
    return true;
  }

  // Check global code buffer list (for multi-threaded cases)
  {
    std::scoped_lock lk {const_cast<ContextImpl*>(this)->CodeBufferListLock};
    for (auto& WeakBuffer : CodeBufferList) {
      if (auto Buffer = WeakBuffer.lock()) {
        if (Address >= reinterpret_cast<uintptr_t>(Buffer->Ptr) && Address < (reinterpret_cast<uintptr_t>(Buffer->Ptr) + Buffer->AllocatedSize)) {
          return true;
        }
      }
    }
  }

  fprintf(stderr, "IsAddressInCodeBuffer: PC 0x%llx NOT found. Dispatcher: [0x%llx, 0x%llx)\n", 
          (unsigned long long)Address, (unsigned long long)Dispatcher->GetBufferBase(), (unsigned long long)Dispatcher->GetBufferBase() + Dispatcher->GetBufferSize());
  
  void* array[64];
  size_t size = backtrace(array, 64);
  fprintf(stderr, "Backtrace for failed IsAddressInCodeBuffer:\n");
  backtrace_symbols_fd(array, size, STDERR_FILENO);

  {
    std::scoped_lock lk {const_cast<ContextImpl*>(this)->CodeBufferListLock};
    int i = 0;
    for (auto& WeakBuffer : CodeBufferList) {
      if (auto Buffer = WeakBuffer.lock()) {
        fprintf(stderr, "  Buffer[%d]: [0x%lx, 0x%lx)\n", i++, (uintptr_t)Buffer->Ptr, (uintptr_t)Buffer->Ptr + Buffer->AllocatedSize);
      }
    }
  }

  return false;
}

bool FEXCore::Context::ContextImpl::IsAddressInDispatcher(uintptr_t Address) const {
  return Dispatcher->IsAddressInDispatcher(Address);
}
} // namespace FEXCore::Context
