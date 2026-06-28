#include "RegistryEmulator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace MacOSEmulation {

// Predefined Keys
#define HKEY_CLASSES_ROOT     0x80000000
#define HKEY_CURRENT_USER     0x80000001
#define HKEY_LOCAL_MACHINE    0x80000002
#define HKEY_USERS            0x80000003

static void SerializeKey(std::ostream& os, const std::string& currentPath, const RegistryKey& key) {
    for (const auto& [name, val] : key.Values) {
        os << currentPath << "|" << name << "|" << val.Type << "|";
        for (uint8_t b : val.Data) {
            char buf[3];
            snprintf(buf, sizeof(buf), "%02X", b);
            os << buf;
        }
        os << "\n";
    }
    for (const auto& [name, subkey] : key.SubKeys) {
        std::string subPath = currentPath.empty() ? name : (currentPath + "\\" + name);
        SerializeKey(os, subPath, subkey);
    }
}

void RegistryEmulator::SaveToFile(const std::string& Path) {
    std::ofstream os(Path);
    if (!os.is_open()) {
        fprintf(stderr, "[Registry] Failed to open %s for writing\n", Path.c_str());
        return;
    }
    for (const auto& [hiveName, key] : Hive) {
        SerializeKey(os, hiveName, key);
    }
    fprintf(stderr, "[Registry] Saved registry state to %s\n", Path.c_str());
}

void RegistryEmulator::LoadFromFile(const std::string& Path) {
    std::ifstream is(Path);
    if (!is.is_open()) {
        fprintf(stderr, "[Registry] No registry save file found at %s. Using default seeds.\n", Path.c_str());
        return;
    }
    std::string line;
    int count = 0;
    while (std::getline(is, line)) {
        if (line.empty()) continue;
        size_t p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        std::string keyPath = line.substr(0, p1);

        size_t p2 = line.find('|', p1 + 1);
        if (p2 == std::string::npos) continue;
        std::string valName = line.substr(p1 + 1, p2 - p1 - 1);

        size_t p3 = line.find('|', p2 + 1);
        if (p3 == std::string::npos) continue;
        std::string typeStr = line.substr(p2 + 1, p3 - p2 - 1);
        uint32_t type = 0;
        try {
            type = std::stoul(typeStr);
        } catch (...) {
            continue;
        }

        std::string hexData = line.substr(p3 + 1);
        std::vector<uint8_t> data;
        for (size_t i = 0; i < hexData.length(); i += 2) {
            if (i + 1 < hexData.length()) {
                std::string byteStr = hexData.substr(i, 2);
                try {
                    uint8_t b = (uint8_t)std::stoul(byteStr, nullptr, 16);
                    data.push_back(b);
                } catch (...) {
                    break;
                }
            }
        }

        // Traverse and find/create key
        std::string path = keyPath;
        std::replace(path.begin(), path.end(), '\\', '/');
        size_t pos = path.find('/');
        std::string hiveName = (pos == std::string::npos) ? path : path.substr(0, pos);
        if (hiveName.empty()) continue;

        RegistryKey* current = &Hive[hiveName];
        if (pos != std::string::npos) {
            path.erase(0, pos + 1);
            while ((pos = path.find('/')) != std::string::npos) {
                std::string part = path.substr(0, pos);
                if (!part.empty()) current = &current->SubKeys[part];
                path.erase(0, pos + 1);
            }
            if (!path.empty()) current = &current->SubKeys[path];
        }
        current->Values[valName] = { type, data };
        count++;
    }
    fprintf(stderr, "[Registry] Loaded %d values from %s\n", count, Path.c_str());
}

void RegistryEmulator::Init() {
    fprintf(stderr, "[Registry] Initializing Emulator...\n");
    
    // Seed some basic values that apps often check
    auto& hklm = Hive["HKLM"];
    
    // Windows Version Info
    auto& cv = hklm.SubKeys["SOFTWARE"].SubKeys["Microsoft"].SubKeys["Windows NT"].SubKeys["CurrentVersion"];
    cv.Values["ProductName"] = { 1, {'W', 'i', 'n', 'd', 'o', 'w', 's', ' ', '1', '0', ' ', 'P', 'r', 'o', 0} };
    cv.Values["CurrentBuild"] = { 1, {'1', '9', '0', '4', '5', 0} };
    
    // Processor Info
    auto& proc = hklm.SubKeys["HARDWARE"].SubKeys["DESCRIPTION"].SubKeys["System"].SubKeys["CentralProcessor"].SubKeys["0"];
    proc.Values["Identifier"] = { 1, {'A', 'p', 'p', 'l', 'e', ' ', 'S', 'i', 'l', 'i', 'c', 'o', 'n', 0} };
    proc.Values["~MHz"] = { 4, {0x00, 0x0C, 0x00, 0x00} }; // 3072 MHz

    // Seed HKCU settings
    auto& hkcu = Hive["HKCU"];
    auto& drives = hkcu.SubKeys["Software"].SubKeys["Wine"].SubKeys["Drives"];
    drives.Values["c:"] = { 1, {'/', 'U', 's', 'e', 'r', 's', '/', 'f', 'i', 'r', 'a', 't', 'a', 'k', 't', 'u', 'g', '/', 'D', 'e', 's', 'k', 't', 'o', 'p', '/', 'F', 'E', 'X', 0} };
    drives.Values["d:"] = { 1, {'/', 0} };

    auto& drivers = hkcu.SubKeys["Software"].SubKeys["Wine"].SubKeys["Drivers"];
    drivers.Values["Audio"] = { 1, {'c', 'o', 'r', 'e', 'a', 'u', 'd', 'i', 'o', 0} };

    auto& shell = hkcu.SubKeys["Software"].SubKeys["Microsoft"].SubKeys["Windows"].SubKeys["CurrentVersion"].SubKeys["Explorer"].SubKeys["Shell Folders"];
    shell.Values["Desktop"] = { 1, {'/', 'U', 's', 'e', 'r', 's', '/', 'f', 'i', 'r', 'a', 't', 'a', 'k', 't', 'u', 'g', '/', 'D', 'e', 's', 'k', 't', 'o', 'p', '/', 'F', 'E', 'X', '/', 'D', 'e', 's', 'k', 't', 'o', 'p', 0} };
    shell.Values["Personal"] = { 1, {'/', 'U', 's', 'e', 'r', 's', '/', 'f', 'i', 'r', 'a', 't', 'a', 'k', 't', 'u', 'g', '/', 'D', 'e', 's', 'k', 't', 'o', 'p', '/', 'F', 'E', 'X', '/', 'D', 'o', 'c', 'u', 'm', 'e', 'n', 't', 's', 0} };

    LoadFromFile("registry.txt");
}

uint32_t RegistryEmulator::OpenKey(uint64_t RootKey, const std::string& SubKey, uint64_t* OutHandle) {
    std::string rootStr;
    switch (RootKey) {
        case HKEY_CLASSES_ROOT: rootStr = "HKCR"; break;
        case HKEY_CURRENT_USER: rootStr = "HKCU"; break;
        case HKEY_LOCAL_MACHINE: rootStr = "HKLM"; break;
        case HKEY_USERS: rootStr = "HKU"; break;
        default: rootStr = "HKLM"; break;
    }

    fprintf(stderr, "[Registry] OpenKey: %s\\%s\n", rootStr.c_str(), SubKey.c_str());
    
    // Simple path traversal
    RegistryKey* current = &Hive[rootStr];
    std::string path = SubKey;
    std::replace(path.begin(), path.end(), '\\', '/');
    
    size_t pos = 0;
    while ((pos = path.find('/')) != std::string::npos) {
        std::string part = path.substr(0, pos);
        if (!part.empty()) current = &current->SubKeys[part];
        path.erase(0, pos + 1);
    }
    if (!path.empty()) current = &current->SubKeys[path];

    uint64_t h = NextHandle++;
    OpenHandles[h] = current;
    if (OutHandle) *OutHandle = h;
    
    return 0; // ERROR_SUCCESS
}

uint32_t RegistryEmulator::QueryValue(uint64_t KeyHandle, const std::string& ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize) {
    auto it = OpenHandles.find(KeyHandle);
    if (it == OpenHandles.end()) return 6; // ERROR_INVALID_HANDLE

    auto valIt = it->second->Values.find(ValueName);
    if (valIt == it->second->Values.end()) return 2; // ERROR_FILE_NOT_FOUND

    const auto& regVal = valIt->second;
    if (OutType) *OutType = regVal.Type;
    
    if (Data && DataSize) {
        uint32_t toCopy = std::min((uint32_t)regVal.Data.size(), *DataSize);
        memcpy(Data, regVal.Data.data(), toCopy);
        *DataSize = (uint32_t)regVal.Data.size();
    } else if (DataSize) {
        *DataSize = (uint32_t)regVal.Data.size();
    }

    return 0; // ERROR_SUCCESS
}

uint32_t RegistryEmulator::SetValue(uint64_t KeyHandle, const std::string& ValueName, uint32_t Type, const void* Data, uint32_t DataSize) {
    auto it = OpenHandles.find(KeyHandle);
    if (it == OpenHandles.end()) return 6; // ERROR_INVALID_HANDLE

    RegistryValue regVal;
    regVal.Type = Type;
    regVal.Data.assign((const uint8_t*)Data, (const uint8_t*)Data + DataSize);
    it->second->Values[ValueName] = regVal;

    fprintf(stderr, "[Registry] SetValue: handle=0x%llx, name='%s', type=%d, size=%d\n", 
            (unsigned long long)KeyHandle, ValueName.c_str(), Type, DataSize);

    SaveToFile("registry.txt");

    return 0; // ERROR_SUCCESS
}

uint32_t RegistryEmulator::CloseKey(uint64_t KeyHandle) {
    OpenHandles.erase(KeyHandle);
    return 0;
}

} // namespace MacOSEmulation

extern "C" {
void FEX_RegistryInit() {
    MacOSEmulation::RegistryEmulator::Get().Init();
}

uint32_t FEX_RegistryOpenKey(uint64_t RootKey, const char* SubKey, uint64_t* OutHandle) {
    return MacOSEmulation::RegistryEmulator::Get().OpenKey(RootKey, SubKey ? SubKey : "", OutHandle);
}

uint32_t FEX_RegistryQueryValue(uint64_t KeyHandle, const char* ValueName, uint32_t* OutType, void* Data, uint32_t* DataSize) {
    return MacOSEmulation::RegistryEmulator::Get().QueryValue(KeyHandle, ValueName ? ValueName : "", OutType, Data, DataSize);
}

uint32_t FEX_RegistrySetValue(uint64_t KeyHandle, const char* ValueName, uint32_t Type, const void* Data, uint32_t DataSize) {
    return MacOSEmulation::RegistryEmulator::Get().SetValue(KeyHandle, ValueName ? ValueName : "", Type, Data, DataSize);
}

uint32_t FEX_RegistryCloseKey(uint64_t KeyHandle) {
    return MacOSEmulation::RegistryEmulator::Get().CloseKey(KeyHandle);
}
}

