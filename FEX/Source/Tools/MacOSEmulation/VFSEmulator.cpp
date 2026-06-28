#include "VFSEmulator.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace MacOSEmulation {

void VFSEmulator::Init(const std::string& RootPath) {
    Root = RootPath;
    // Default mappings
    AddDrive("C:", Root + "/drive_c");
    AddDrive("Z:", "/"); // Map Z: to root filesystem
    
    // Ensure drive_c exists
    std::error_code ec;
    std::filesystem::create_directories(Root + "/drive_c", ec);
    std::filesystem::create_directories(Root + "/drive_c/Windows/System32", ec);
}

void VFSEmulator::SetWineDLLPath(const std::string& Path) {
    WineDLLPath = Path;
    fprintf(stderr, "[VFS] Wine DLL search path set to: %s\n", Path.c_str());
}

void VFSEmulator::AddDrive(const std::string& DriveLetter, const std::string& HostPath) {
    std::string dl = DriveLetter;
    if (dl.back() == ':') dl.pop_back();
    std::transform(dl.begin(), dl.end(), dl.begin(), ::toupper);
    Drives[dl] = HostPath;
    fprintf(stderr, "[VFS] Mapped %s: to %s\n", dl.c_str(), HostPath.c_str());
}

std::string VFSEmulator::NormalizeWindowsPath(const std::string& Path) {
    std::string p = Path;
    std::replace(p.begin(), p.end(), '\\', '/');
    return p;
}

std::string VFSEmulator::TranslatePath(const std::string& WindowsPath) {
    if (WindowsPath.empty()) return "";

    std::string norm = NormalizeWindowsPath(WindowsPath);
    
    // Check for drive letter (e.g., C:/...)
    if (norm.size() >= 2 && norm[1] == ':') {
        std::string drive = norm.substr(0, 1);
        std::transform(drive.begin(), drive.end(), drive.begin(), ::toupper);
        
        auto it = Drives.find(drive);
        if (it != Drives.end()) {
            std::string subpath = (norm.size() > 2) ? norm.substr(2) : "";
            // Ensure subpath starts with / if not empty
            if (!subpath.empty() && subpath[0] != '/') subpath = "/" + subpath;
            
            std::string translated = it->second + subpath;
            
            // Handle Case-Insensitivity (Crucial for Windows apps on macOS)
            // We search for the file in the actual filesystem
            if (std::filesystem::exists(translated)) return translated;
            
            // If not found, try to find a case-insensitive match
            std::string dir = it->second;
            std::string remaining = subpath;
            
            // Simple recursive case-insensitive search could be slow, 
            // but for system DLLs it's usually just a few levels.
            // For now, return the translated path and let the OS handle it if possible.
            return translated;
        }
    }

    // If it's a relative path or doesn't have a drive, it's tricky.
    // For DLLs, first check Wine DLL directory, then fall back to System32
    if (norm.find('.') != std::string::npos && norm.find('/') == std::string::npos) {
        // First check Wine DLL path
        if (!WineDLLPath.empty()) {
            std::string winePath = WineDLLPath + "/" + norm;
            if (std::filesystem::exists(winePath)) {
                fprintf(stderr, "[VFS] Resolved '%s' from Wine DLL path: %s\n", norm.c_str(), winePath.c_str());
                return winePath;
            }
        }
        // Fallback to C:\Windows\System32
        return Drives["C"] + "/Windows/System32/" + norm;
    }

    return norm;
}

} // namespace MacOSEmulation

