#pragma once

#include <string>
#include <map>
#include <vector>
#include <stdint.h>

namespace MacOSEmulation {

struct RegistryValue {
    uint32_t Type;
    std::vector<uint8_t> Data;
};

struct RegistryKey {
    std::map<std::string, RegistryKey> SubKeys;
    std::map<std::string, RegistryValue> Values;
};

class RegistryEmulator {
public:
    static RegistryEmulator& Get() {
        static RegistryEmulator instance;
        return instance;
    }

    void Init();
    void LoadFromFile(const std::string& Path);
    void SaveToFile(const std::string& Path);


    // Emulates RegOpenKeyExA
    uint32_t OpenKey(uint64_t RootKey, const std::string& SubKey, uint64_t* OutHandle);
    
    // Emulates RegQueryValueExA
    uint32_t QueryValue(uint64_t KeyHandle, const std::string& ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);

    // Emulates RegSetValueExA
    uint32_t SetValue(uint64_t KeyHandle, const std::string& ValueName, uint32_t Type, const void* Data, uint32_t DataSize);

    // Emulates RegCloseKey
    uint32_t CloseKey(uint64_t KeyHandle);

private:
    RegistryEmulator() = default;
    
    std::map<std::string, RegistryKey> Hive;
    std::map<uint64_t, RegistryKey*> OpenHandles;
    uint64_t NextHandle = 0x1000;

    RegistryKey* FindKey(const std::string& Path);
};

} // namespace MacOSEmulation

#ifdef __cplusplus
extern "C" {
#endif

void FEX_RegistryInit();
uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle);
uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize);
uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize);
uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle);

#ifdef __cplusplus
}
#endif

