#pragma once

#include <string>
#include <vector>
#include <map>

namespace MacOSEmulation {

class VFSEmulator {
public:
    static VFSEmulator& Get() {
        static VFSEmulator instance;
        return instance;
    }

    // Initializes the VFS with a root directory (e.g., "$HOME/FEXRoot")
    void Init(const std::string& RootPath);

    // Translates a Windows path (C:\Windows\System32\ntdll.dll) to a macOS path
    std::string TranslatePath(const std::string& WindowsPath);

    // Adds a drive mapping (e.g., "C:" -> "/Users/user/FEXRoot/drive_c")
    void AddDrive(const std::string& DriveLetter, const std::string& HostPath);

    // Sets the Wine DLL search path (e.g., "/path/to/wine11.9/lib/wine/x86_64-windows")
    void SetWineDLLPath(const std::string& Path);

private:
    VFSEmulator() = default;
    std::string Root;
    std::string WineDLLPath;
    std::map<std::string, std::string> Drives;

    // Helper to normalize Windows paths (backslashes to forward slashes, lowercase for comparison)
    std::string NormalizeWindowsPath(const std::string& Path);
};

} // namespace MacOSEmulation
