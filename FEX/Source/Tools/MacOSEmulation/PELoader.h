#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <map>

#include <FEXCore/Core/CodeCache.h>

struct FEX_Context;

namespace MacOSEmulation {

class PELoader : public FEXCore::CodeMapOpener {
public:
    PELoader() = default;
    ~PELoader() = default;

    int OpenCodeMapFile() override;

    bool Load(struct FEX_Context* Ctx, const char* Path);
    void SetDLLPath(const std::string& Path) { DLLPath = Path; }

    uint64_t GetEntryPoint() const { return EntryPoint; }
    uint64_t GetBaseAddress() const { return BaseAddress; }
    bool GetIs32Bit() const { return Is32Bit; }

private:
    void SetupIAT(struct FEX_Context* Ctx);
    uint64_t LoadDLL(struct FEX_Context* Ctx, const std::string& Name);
    uint64_t CreateThunkStub(struct FEX_Context* Ctx, uint32_t Class, uint32_t Index, uint32_t ArgSize = 0);



    uint64_t EntryPoint{0};
    uint64_t BaseAddress{0};
    bool Is32Bit{false};
    std::string DLLPath;
    std::string ActiveCodeMapPath;

    struct LoadedDLL {
        std::string Name;
        uint64_t Base;
        uint64_t Size;
        std::map<std::string, uint64_t> Exports;
    };
    std::vector<LoadedDLL> LoadedDLLs;

    void ParseExports(LoadedDLL& DLL);
    uint64_t FindSymbol(const std::string& DLLName, const std::string& SymbolName);

    uint64_t ImportDirectoryVA{0};
    uint64_t ImportDirectorySize{0};

    struct Section {
        std::string Name;
        uint64_t VirtualAddress;
        uint64_t VirtualSize;
        uint64_t RawDataOffset;
        uint64_t RawDataSize;
        uint32_t Characteristics;
    };
    std::vector<Section> Sections;
};

} // namespace MacOSEmulation
