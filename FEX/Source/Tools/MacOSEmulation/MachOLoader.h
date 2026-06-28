#include <FEXCore/Core/Context.h>
#pragma once

#include <string>
#include <cstdint>

struct FEX_Context;

namespace MacOSEmulation {

class MachOLoader {
public:
    MachOLoader() = default;
    ~MachOLoader() = default;

    bool Load(FEXCore::Context::Context* Ctx, const std::string& Path);
    uint64_t GetEntryPoint() const { return EntryPoint; }
    uint64_t GetBaseAddress() const { return BaseAddress; }
    uint64_t BaseAddress{0};

private:
    bool MapSlice(FEXCore::Context::Context* Ctx, int fd, uint32_t slice_offset, uint32_t slice_size, bool is_dyld);
    uint64_t EntryPoint{0};
};

} // namespace MacOSEmulation
