#pragma once

#include <cstdint>
#include <FEXCore/HLE/SyscallHandler.h>

namespace MacOSEmulation {

class MacOSSyscalls : public FEXCore::HLE::SyscallHandler {
public:
    MacOSSyscalls() {
        this->OSABI = FEXCore::HLE::SyscallOSABI::OS_LINUX64;
    }
    ~MacOSSyscalls() override = default;

    uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* Frame, FEXCore::HLE::SyscallArguments* Args) override;

    bool Init();

    FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Address) override {
        return {0, ~0ULL, true}; // Stub
    }

    std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState* Thread, uint64_t GuestAddr) override {
        return std::nullopt; // Stub
    }
};

} // namespace MacOSEmulation
