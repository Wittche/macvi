#include "PELoader.h"
#include "VFSEmulator.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <dlfcn.h>
#include "FEXCore/Core/FEXLibrary.h"
#include <FEXCore/Config/Config.h>
#include <FEXCore/Core/CodeCache.h>
#include <FEXCore/Core/Context.h>
#include <FEXCore/HLE/SourcecodeResolver.h>
#include <xxhash.h>

namespace MacOSEmulation {

int PELoader::OpenCodeMapFile() {
    if (ActiveCodeMapPath.empty()) {
        return -1;
    }
    // O_CREAT | O_TRUNC | O_WRONLY | O_APPEND
    return open(ActiveCodeMapPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0644);
}

#pragma pack(push, 1)
struct IMAGE_DOS_HEADER { uint16_t e_magic; uint16_t e_cblp; uint16_t e_cp; uint16_t e_crlc; uint16_t e_cparhdr; uint16_t e_minalloc; uint16_t e_maxalloc; uint16_t e_ss; uint16_t e_sp; uint16_t e_csum; uint16_t e_ip; uint16_t e_cs; uint16_t e_lfarlc; uint16_t e_ovno; uint16_t e_res[4]; uint16_t e_oemid; uint16_t e_oeminfo; uint16_t e_res2[10]; int32_t  e_lfanew; };
struct IMAGE_FILE_HEADER { uint16_t Machine; uint16_t NumberOfSections; uint32_t TimeDateStamp; uint32_t PointerToSymbolTable; uint32_t NumberOfSymbols; uint16_t SizeOfOptionalHeader; uint16_t Characteristics; };
struct IMAGE_DATA_DIRECTORY { uint32_t VirtualAddress; uint32_t Size; };
struct IMAGE_OPTIONAL_HEADER64 { uint16_t Magic; uint8_t MajorLinkerVersion; uint8_t MinorLinkerVersion; uint32_t SizeOfCode; uint32_t SizeOfInitializedData; uint32_t SizeOfUninitializedData; uint32_t AddressOfEntryPoint; uint32_t BaseOfCode; uint64_t ImageBase; uint32_t SectionAlignment; uint32_t FileAlignment; uint16_t MajorOperatingSystemVersion; uint16_t MinorOperatingSystemVersion; uint16_t MajorImageVersion; uint16_t MinorImageVersion; uint16_t MajorSubsystemVersion; uint16_t MinorSubsystemVersion; uint32_t Win32VersionValue; uint32_t SizeOfImage; uint32_t SizeOfHeaders; uint32_t CheckSum; uint16_t Subsystem; uint16_t DllCharacteristics; uint64_t SizeOfStackReserve; uint64_t SizeOfStackCommit; uint64_t SizeOfHeapReserve; uint64_t SizeOfHeapCommit; uint32_t LoaderFlags; uint32_t NumberOfRvaAndSizes; IMAGE_DATA_DIRECTORY DataDirectory[16]; };
struct IMAGE_OPTIONAL_HEADER32 { uint16_t Magic; uint8_t MajorLinkerVersion; uint8_t MinorLinkerVersion; uint32_t SizeOfCode; uint32_t SizeOfInitializedData; uint32_t SizeOfUninitializedData; uint32_t AddressOfEntryPoint; uint32_t BaseOfCode; uint32_t BaseOfData; uint32_t ImageBase; uint32_t SectionAlignment; uint32_t FileAlignment; uint16_t MajorOperatingSystemVersion; uint16_t MinorOperatingSystemVersion; uint16_t MajorImageVersion; uint16_t MinorImageVersion; uint16_t MajorSubsystemVersion; uint16_t MinorSubsystemVersion; uint32_t Win32VersionValue; uint32_t SizeOfImage; uint32_t SizeOfHeaders; uint32_t CheckSum; uint16_t Subsystem; uint16_t DllCharacteristics; uint32_t SizeOfStackReserve; uint32_t SizeOfStackCommit; uint32_t SizeOfHeapReserve; uint32_t SizeOfHeapCommit; uint32_t LoaderFlags; uint32_t NumberOfRvaAndSizes; IMAGE_DATA_DIRECTORY DataDirectory[16]; };
struct IMAGE_NT_HEADERS { uint32_t Signature; IMAGE_FILE_HEADER FileHeader; IMAGE_OPTIONAL_HEADER64 OptionalHeader; };
struct IMAGE_SECTION_HEADER { uint8_t Name[8]; uint32_t VirtualSize; uint32_t VirtualAddress; uint32_t SizeOfRawData; uint32_t PointerToRawData; uint32_t PointerToRelocations; uint32_t PointerToLinenumbers; uint16_t NumberOfRelocations; uint16_t NumberOfLinenumbers; uint32_t Characteristics; };
struct IMAGE_IMPORT_DESCRIPTOR { uint32_t OriginalFirstThunk; uint32_t TimeDateStamp; uint32_t ForwarderChain; uint32_t Name; uint32_t FirstThunk; };
struct IMAGE_THUNK_DATA64 { union { uint64_t ForwarderString; uint64_t Function; uint64_t Ordinal; uint64_t AddressOfData; } u1; };
struct IMAGE_IMPORT_BY_NAME { uint16_t Hint; char Name[1]; };
struct IMAGE_EXPORT_DIRECTORY { uint32_t Characteristics; uint32_t TimeDateStamp; uint16_t MajorVersion; uint16_t MinorVersion; uint32_t Name; uint32_t Base; uint32_t NumberOfFunctions; uint32_t NumberOfNames; uint32_t AddressOfFunctions; uint32_t AddressOfNames; uint32_t AddressOfNameOrdinals; };
struct IMAGE_BASE_RELOCATION { uint32_t VirtualAddress; uint32_t SizeOfBlock; };
#pragma pack(pop)

bool PELoader::Load(struct FEX_Context* Ctx, const char* Path) {
    std::ifstream file(Path, std::ios::binary);
    if (!file) return false;

    // Avoid double loading
    std::string name = Path;
    size_t last_slash = name.find_last_of("/\\");
    if (last_slash != std::string::npos) name = name.substr(last_slash + 1);
    for (auto& dll : LoadedDLLs) if (dll.Name == name) return true;

    IMAGE_DOS_HEADER dos_header;
    file.read(reinterpret_cast<char*>(&dos_header), sizeof(dos_header));
    if (dos_header.e_magic != 0x5A4D) return false;

    file.seekg(dos_header.e_lfanew);
    uint32_t signature = 0;
    file.read(reinterpret_cast<char*>(&signature), 4);
    if (signature != 0x00004550) return false;

    IMAGE_FILE_HEADER file_header;
    file.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));

    uint16_t opt_magic = 0;
    file.read(reinterpret_cast<char*>(&opt_magic), sizeof(opt_magic));
    file.seekg(dos_header.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER)); // Go back to optional header start

    uint32_t SizeOfImage = 0;
    uint64_t PreferredBase = 0;
    uint32_t AddressOfEntryPoint = 0;
    uint32_t RelocVA = 0;
    uint32_t RelocSize = 0;
    uint32_t ExportDirectoryVA = 0;
    uint32_t ImportDirectoryVA = 0;
    uint32_t ImportDirectorySize = 0;
    uint32_t SizeOfHeaders = 0;

    if (opt_magic == 0x10b) { // PE32 (32-bit)
        Is32Bit = true;
        IMAGE_OPTIONAL_HEADER32 opt_hdr;
        file.read(reinterpret_cast<char*>(&opt_hdr), sizeof(opt_hdr));
        SizeOfImage = opt_hdr.SizeOfImage;
        PreferredBase = opt_hdr.ImageBase;
        AddressOfEntryPoint = opt_hdr.AddressOfEntryPoint;
        ExportDirectoryVA = opt_hdr.DataDirectory[0].VirtualAddress;
        ImportDirectoryVA = opt_hdr.DataDirectory[1].VirtualAddress;
        ImportDirectorySize = opt_hdr.DataDirectory[1].Size;
        RelocVA = opt_hdr.DataDirectory[5].VirtualAddress;
        RelocSize = opt_hdr.DataDirectory[5].Size;
        SizeOfHeaders = opt_hdr.SizeOfHeaders;
        fprintf(stderr, "[PELoader] Detected 32-bit PE binary (PE32)\n");
    } else if (opt_magic == 0x20b) { // PE32+ (64-bit)
        Is32Bit = false;
        IMAGE_OPTIONAL_HEADER64 opt_hdr;
        file.read(reinterpret_cast<char*>(&opt_hdr), sizeof(opt_hdr));
        SizeOfImage = opt_hdr.SizeOfImage;
        PreferredBase = opt_hdr.ImageBase;
        AddressOfEntryPoint = opt_hdr.AddressOfEntryPoint;
        ExportDirectoryVA = opt_hdr.DataDirectory[0].VirtualAddress;
        ImportDirectoryVA = opt_hdr.DataDirectory[1].VirtualAddress;
        ImportDirectorySize = opt_hdr.DataDirectory[1].Size;
        RelocVA = opt_hdr.DataDirectory[5].VirtualAddress;
        RelocSize = opt_hdr.DataDirectory[5].Size;
        SizeOfHeaders = opt_hdr.SizeOfHeaders;
        fprintf(stderr, "[PELoader] Detected 64-bit PE binary (PE32+)\n");
    } else {
        fprintf(stderr, "[PELoader] ERROR: Unknown PE optional header magic: 0x%X\n", opt_magic);
        return false;
    }

    // macOS Apple Silicon requires 16KB alignment for ImageBase mapping
    uint64_t AlignedBase = PreferredBase & ~0x3FFFULL;
    uint64_t BaseShift = PreferredBase - AlignedBase;
    uint64_t AlignedSize = (SizeOfImage + BaseShift + 0x3FFF) & ~0x3FFFULL;

    uintptr_t mapped = FEX_MapMemory(Ctx, AlignedBase, AlignedSize, FEX_MEM_READ | FEX_MEM_WRITE);
    bool relocated = false;
    if (!mapped) {
        mapped = FEX_MapMemory(Ctx, 0, AlignedSize, FEX_MEM_READ | FEX_MEM_WRITE);
        if (!mapped) return false;
        relocated = true;
    }

    BaseAddress = mapped + BaseShift;
    EntryPoint = BaseAddress + AddressOfEntryPoint;
    fprintf(stderr, "[PELoader] Mapped image at 0x%llx (Preferred: 0x%llx), Entry: 0x%llx\n", 
            (unsigned long long)BaseAddress, (unsigned long long)PreferredBase, (unsigned long long)EntryPoint);

    LoadedDLLs.push_back({name, BaseAddress, AlignedSize, {}});
    auto& currentDLL = LoadedDLLs.back();

    // Read section headers - move to correct offset
    file.seekg(dos_header.e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) + file_header.SizeOfOptionalHeader);
    Sections.clear();
    for (int i = 0; i < file_header.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER sh;
        if (!file.read(reinterpret_cast<char*>(&sh), sizeof(sh))) {
            fprintf(stderr, "[PELoader] ERROR: Failed to read section header %d\n", i);
            return false;
        }
        Section section;
        char section_name[9];
        memcpy(section_name, sh.Name, 8);
        section_name[8] = 0;
        section.Name = section_name;
        section.VirtualAddress = sh.VirtualAddress;
        section.VirtualSize = sh.VirtualSize;
        section.RawDataOffset = sh.PointerToRawData;
        section.RawDataSize = sh.SizeOfRawData;
        section.Characteristics = sh.Characteristics;
        Sections.push_back(section);
    }

    // Write Headers to mapped memory
    std::vector<char> header_buffer(SizeOfHeaders);
    file.seekg(0);
    if (!file.read(header_buffer.data(), SizeOfHeaders)) {
        fprintf(stderr, "[PELoader] ERROR: Failed to read PE headers (size 0x%x)\n", SizeOfHeaders);
        return false;
    }
    FEX_WriteMemory(Ctx, BaseAddress, header_buffer.data(), SizeOfHeaders);

    // Load sections
    for (const auto& section : Sections) {
        if (section.RawDataSize > 0) {
            uint64_t target_addr = BaseAddress + section.VirtualAddress;
            uint32_t write_size = std::min(section.RawDataSize, section.VirtualSize);
            if (write_size > 0) {
                file.seekg(section.RawDataOffset);
                std::vector<char> buffer(write_size);
                if (!file.read(buffer.data(), write_size)) {
                    fprintf(stderr, "[PELoader] ERROR: Failed to read section %s raw data\n", section.Name.c_str());
                    return false;
                }
                FEX_WriteMemory(Ctx, target_addr, buffer.data(), write_size);
            }
        }
    }

    // Apply Relocations if needed
    if (relocated) {
        if (RelocVA && RelocSize) {
            fprintf(stderr, "[PELoader] Relocating %s from 0x%llx to 0x%llx\n", name.c_str(), PreferredBase, BaseAddress);
            uint64_t delta = BaseAddress - PreferredBase;
            auto reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(BaseAddress + RelocVA);
            while (reinterpret_cast<uintptr_t>(reloc) < BaseAddress + RelocVA + RelocSize && reloc->SizeOfBlock > 0) {
                uint32_t count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
                uint16_t* entries = reinterpret_cast<uint16_t*>(reloc + 1);
                for (uint32_t i = 0; i < count; i++) {
                    uint16_t type = entries[i] >> 12;
                    uint16_t offset = entries[i] & 0x0FFF;
                    if (type == 10) { // IMAGE_REL_BASED_DIR64
                        *reinterpret_cast<uint64_t*>(BaseAddress + reloc->VirtualAddress + offset) += delta;
                    } else if (type == 3) { // IMAGE_REL_BASED_HIGHLOW (32-bit reloc)
                        *reinterpret_cast<uint32_t*>(BaseAddress + reloc->VirtualAddress + offset) += (uint32_t)delta;
                    }
                }
                reloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<uintptr_t>(reloc) + reloc->SizeOfBlock);
            }
        }
    }

    if (ExportDirectoryVA) {
        ParseExports(currentDLL);
    }

    this->ImportDirectoryVA = ImportDirectoryVA;
    this->ImportDirectorySize = ImportDirectorySize;

    fprintf(stderr, "[PELoader] Import Directory: VA=0x%llx, Size=0x%llx\n", (unsigned long long)ImportDirectoryVA, (unsigned long long)ImportDirectorySize);

    if (ImportDirectoryVA) {
        SetupIAT(Ctx);
    }

    // Set protections AFTER everything (Relocations & IAT) is written
    for (const auto& section : Sections) {
        uint64_t target_addr = BaseAddress + section.VirtualAddress;
        
        int prot = 0;
        if (section.Characteristics & 0x40000000) prot |= PROT_READ;
        if (section.Characteristics & 0x80000000) prot |= PROT_WRITE;
        if (section.Characteristics & 0x20000000) prot |= PROT_EXEC;
        
        // Default to READ if nothing else is set
        if (prot == 0) prot = PROT_READ;

        // macOS Apple Silicon (and modern systems) doesn't allow W^X by default
        if (prot & PROT_EXEC) {
            prot &= ~PROT_WRITE;
        }        // We do not call mprotect here because macOS 16KB pages cause overlapping protections.
        // Guest memory does not need to be executable by the host, and keeping it RW avoids protection faults.
    }

    fprintf(stderr, "[PELoader] SUCCESS: Loaded %s at 0x%llx\n", name.c_str(), BaseAddress);

    // Initialize AOT Cache for this image
    if (FEXCore::Config::Get_ENABLECODECACHINGWIP()()) {
        std::string lower_name = name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        uint64_t file_id = XXH3_64bits(lower_name.c_str(), lower_name.length()) ^ (static_cast<uint64_t>(SizeOfImage) << 32 | file_header.TimeDateStamp);

        FEXCore::ExecutableFileInfo file_info;
        file_info.FileId = file_id;
        file_info.Filename = lower_name;

        FEXCore::ExecutableFileSectionInfo section_info {
            .FileInfo = file_info,
            .FileStartVA = BaseAddress,
            .BeginVA = BaseAddress,
            .EndVA = BaseAddress + SizeOfImage
        };

        fextl::string fex_id = FEXCore::CodeMap::GetBaseFilename(file_info, false);
        
        std::string home_dir = getenv("HOME");
        std::string fex_cache_dir = home_dir + "/.fex-emu/cache/";
        system((std::string("mkdir -p ") + fex_cache_dir + "codemap/new").c_str());

        std::string cache_file = fex_cache_dir + std::string(fex_id.c_str());
        int fd = open(cache_file.c_str(), O_RDONLY);
        if (fd >= 0) {
            struct stat sb;
            fstat(fd, &sb);
            void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapped != MAP_FAILED) {
                reinterpret_cast<FEXCore::Context::Context*>(Ctx)->GetCodeCache().LoadData(nullptr, static_cast<std::byte*>(mapped), section_info);
                fprintf(stderr, "[PELoader] SUCCESS: Loaded AOT cache for %s\n", name.c_str());
            } else {
                fprintf(stderr, "[PELoader] WARNING: mmap failed for AOT cache %s\n", cache_file.c_str());
            }
            close(fd);
        } else {
            // Setup writing
            ActiveCodeMapPath = fex_cache_dir + "codemap/new/" + std::string(fex_id.c_str()) + ".bin";
            auto Writer = fextl::make_unique<FEXCore::CodeMapWriter>(*this, false);
            Writer->AppendSetMainExecutable(file_info);
            reinterpret_cast<FEXCore::Context::Context*>(Ctx)->SetCodeMapWriter(std::move(Writer));
            fprintf(stderr, "[PELoader] Info: AOT cache generation enabled for %s\n", name.c_str());
        }
    }

    return true;
}

void PELoader::ParseExports(LoadedDLL& DLL) {
    auto dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(DLL.Base);
    auto nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(DLL.Base + dos_header->e_lfanew);
    auto export_dir_va = nt_headers->OptionalHeader.DataDirectory[0].VirtualAddress;
    if (!export_dir_va) return;

    auto export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(DLL.Base + export_dir_va);
    auto names = reinterpret_cast<uint32_t*>(DLL.Base + export_dir->AddressOfNames);
    auto ordinals = reinterpret_cast<uint16_t*>(DLL.Base + export_dir->AddressOfNameOrdinals);
    auto functions = reinterpret_cast<uint32_t*>(DLL.Base + export_dir->AddressOfFunctions);

    // Track by name
    for (uint32_t i = 0; i < export_dir->NumberOfNames; i++) {
        const char* name = reinterpret_cast<const char*>(DLL.Base + names[i]);
        uint32_t addr = functions[ordinals[i]];
        DLL.Exports[name] = DLL.Base + addr;
    }
    // Track by ordinal (base-relative)
    for (uint32_t i = 0; i < export_dir->NumberOfFunctions; i++) {
        uint32_t addr = functions[i];
        if (addr != 0) {
            DLL.Exports["#" + std::to_string(export_dir->Base + i)] = DLL.Base + addr;
        }
    }
    fprintf(stderr, "[PELoader] Parsed exports for %s\n", DLL.Name.c_str());
}

uint64_t PELoader::FindSymbol(const std::string& DLLName, const std::string& SymbolName) {
    for (const auto& dll : LoadedDLLs) {
        std::string n = dll.Name;
        for (auto &c : n) c = tolower(c);
        if (n == DLLName) {
            auto it = dll.Exports.find(SymbolName);
            if (it != dll.Exports.end()) return it->second;
        }
    }
    return 0;
}

static uint32_t GetArgSize(const std::string& name);

void PELoader::SetupIAT(struct FEX_Context* Ctx) {
    auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(BaseAddress + ImportDirectoryVA);
    while (desc->Name) {
        std::string dllName = reinterpret_cast<char*>(BaseAddress + desc->Name);
        for (auto &c : dllName) c = tolower(c);
        
        fprintf(stderr, "[PELoader] Resolving imports for %s\n", dllName.c_str());
        uint64_t dllBase = 0;
        bool isHostDLL = (dllName == "user32.dll" || dllName == "kernel32.dll" || 
                          dllName == "gdi32.dll" || dllName == "advapi32.dll" || 
                          dllName == "shell32.dll" || dllName == "ucrtbase.dll" ||
                          dllName == "shlwapi.dll" || dllName == "comctl32.dll" ||
                          dllName == "comdlg32.dll" || dllName == "ntdll.dll" ||
                          dllName == "imm32.dll" || dllName == "version.dll" ||
                          dllName == "winspool.drv" || dllName == "ole32.dll" ||
                          dllName == "d3d11.dll");
        
        if (!isHostDLL) {
            dllBase = LoadDLL(Ctx, dllName);
        }
        
        auto thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(BaseAddress + desc->FirstThunk);
        auto origThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(BaseAddress + desc->OriginalFirstThunk);
        if (!desc->OriginalFirstThunk) origThunk = thunk;

        while (origThunk->u1.AddressOfData) {
            std::string funcName;
            if (origThunk->u1.Ordinal & 0x8000000000000000ULL) {
                funcName = "#" + std::to_string(origThunk->u1.Ordinal & 0xFFFF);
            } else {
                auto importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(BaseAddress + origThunk->u1.AddressOfData);
                funcName = importByName->Name;
            }

            fprintf(stderr, "[PELoader]   - %s!%s ", dllName.c_str(), funcName.c_str());
            fflush(stderr);

            uint64_t addr = 0;

            auto CreateStub = [&](uint32_t Class, uint32_t Index) {
                return CreateThunkStub(Ctx, Class, Index, GetArgSize(funcName));
            };

            // Check for Host Thunks (Native Bridge)
            if (isHostDLL) {
                if (funcName == "CreateWindowExA" || funcName == "CreateWindowExW") addr = CreateStub(0x3, 60);
                else if (funcName == "ShowWindow") addr = CreateStub(0x3, 61);
                else if (funcName == "UpdateWindow") addr = CreateStub(0x3, 62);
                else if (funcName == "GetMessageA" || funcName == "GetMessageW") addr = CreateStub(0x3, 64);
                else if (funcName == "TranslateMessage") addr = CreateStub(0x3, 65);
                else if (funcName == "DispatchMessageA" || funcName == "DispatchMessageW") addr = CreateStub(0x3, 66);
                else if (funcName == "DefWindowProcA" || funcName == "DefWindowProcW") addr = CreateStub(0x3, 67);
                else if (funcName == "PostQuitMessage") addr = CreateStub(0x3, 68);
                else if (funcName == "ExitProcess" || funcName == "TerminateProcess") addr = CreateStub(0x3, 0);
                else if (funcName == "GetModuleHandleA" || funcName == "GetModuleHandleW") addr = CreateStub(0x3, 10);
                else if (funcName == "GetProcAddress") addr = CreateStub(0x3, 11);
                
                else if (funcName == "GetCommandLineW") addr = CreateStub(0x3, 130);
                else if (funcName == "GetCommandLineA") addr = CreateStub(0x3, 131);
                else if (funcName == "HeapAlloc") addr = CreateStub(0x3, 132);
                else if (funcName == "HeapFree") addr = CreateStub(0x3, 133);
                else if (funcName == "LocalFree") addr = CreateStub(0x3, 134);
                else if (funcName == "RegCloseKey") addr = CreateStub(0x3, 135);
                else if (funcName == "IsTextUnicode" || funcName == "RtlIsTextUnicode") addr = CreateStub(0x3, 136);
                else if (funcName == "RegOpenKeyW" || funcName == "RegOpenKeyExW") addr = CreateStub(0x3, 80);
                else if (funcName == "RegCreateKeyExW") addr = CreateStub(0x3, 82);
                else if (funcName == "RegCreateKeyW") addr = CreateStub(0x3, 83);
                else if (funcName == "RegQueryValueExW") addr = CreateStub(0x3, 81);
                else if (funcName == "RegSetValueExW") addr = CreateStub(0x3, 84);
                else if (funcName == "RegisterClassExW") addr = CreateStub(0x3, 85);
                else if (funcName == "RegisterClassW") addr = CreateStub(0x3, 86);
                else if (funcName == "GetSystemMetrics") addr = CreateStub(0x3, 90);
                else if (funcName == "GetClientRect" || funcName == "GetWindowRect") addr = CreateStub(0x3, 91);
                else if (funcName == "GetDC") addr = CreateStub(0x3, 92);
                else if (funcName == "ReleaseDC") addr = CreateStub(0x3, 93);
                else if (funcName == "GetDeviceCaps") addr = CreateStub(0x3, 94);
                else if (funcName == "DeleteObject") addr = CreateStub(0x3, 95);
                else if (funcName == "SetWindowTextW") addr = CreateStub(0x3, 96);
                else if (funcName == "GetWindowTextW") addr = CreateStub(0x3, 97);
                else if (funcName == "SendMessageW") addr = CreateStub(0x3, 98);
                else if (funcName == "BeginPaint") addr = CreateStub(0x3, 101);
                else if (funcName == "EndPaint") addr = CreateStub(0x3, 102);
                else if (funcName == "ExtTextOutW") addr = CreateStub(0x3, 103);
                else if (funcName == "FillRect") addr = CreateStub(0x3, 104);
                else if (funcName == "CreateFontIndirectW") addr = CreateStub(0x3, 105);
                else if (funcName == "SelectObject") addr = CreateStub(0x3, 106);
                else if (funcName == "GetStockObject") addr = CreateStub(0x3, 107);
                else if (funcName == "CreateSolidBrush") addr = CreateStub(0x3, 108);
                else if (funcName == "CreatePen") addr = CreateStub(0x3, 109);
                else if (funcName == "SetBkMode") addr = CreateStub(0x3, 110);
                else if (funcName == "SetTextColor") addr = CreateStub(0x3, 111);
                else if (funcName == "SetBkColor") addr = CreateStub(0x3, 112);
                else if (funcName == "GetTextMetricsW") addr = CreateStub(0x3, 113);
                else if (funcName == "LoadImageW") addr = CreateStub(0x3, 114);
                else if (funcName == "LoadCursorW") addr = CreateStub(0x3, 115);
                else if (funcName == "LoadIconW") addr = CreateStub(0x3, 116);
                else if (funcName == "GetMonitorInfoW") addr = CreateStub(0x3, 117);
                else if (funcName == "GetMenu") addr = CreateStub(0x3, 118);
                else if (funcName == "CheckMenuItem") addr = CreateStub(0x3, 119);
                else if (funcName == "EnableMenuItem") addr = CreateStub(0x3, 120);
                else if (funcName == "RegisterWindowMessageW") addr = CreateStub(0x3, 121);
                else if (funcName == "SetFocus") addr = CreateStub(0x3, 122);
                else if (funcName == "GetFocus") addr = CreateStub(0x3, 123);
                else if (funcName == "SendDlgItemMessageW") addr = CreateStub(0x3, 99);
                else if (funcName == "MultiByteToWideChar") addr = CreateStub(0x3, 140);
                else if (funcName == "GetProcessHeap") addr = CreateStub(0x3, 150);
                else if (funcName == "CloseHandle") addr = CreateStub(0x3, 73);
                else if (funcName == "CreateFileW") addr = CreateStub(0x3, 70); 
                else if (funcName == "WriteFile") addr = CreateStub(0x3, 72);
                else if (funcName == "ReadFile") addr = CreateStub(0x3, 71);
                else if (funcName == "GetFileSize") addr = CreateStub(0x3, 74);
                else if (funcName == "GetFileSizeEx") addr = CreateStub(0x3, 75);
                
                else if (funcName == "PathFindFileNameW") addr = CreateStub(0x3, 160);
                else if (funcName == "InitCommonControls" || funcName == "InitCommonControlsEx") addr = CreateStub(0x3, 170);
                else if (funcName == "GetOpenFileNameW") addr = CreateStub(0x3, 180);
                else if (funcName == "PropertySheetW") addr = CreateStub(0x3, 500);

                
                else if (funcName == "memcpy") addr = CreateStub(0x3, 200);
                else if (funcName == "memset") addr = CreateStub(0x3, 201);
                else if (funcName == "strlen") addr = CreateStub(0x3, 202);
                else if (funcName == "sin") addr = CreateStub(0x3, 250);
                else if (funcName == "cos") addr = CreateStub(0x3, 251);
                else if (funcName == "sinf") addr = CreateStub(0x3, 252);
                else if (funcName == "cosf") addr = CreateStub(0x3, 253);
                
                // ucrtbase extensions
                else if (funcName == "__p___argc") addr = CreateStub(0x3, 211);
                else if (funcName == "__p___argv" || funcName == "__p___wargv") addr = CreateStub(0x3, 212);
                else if (funcName == "_set_app_type") addr = CreateStub(0x3, 213);
                else if (funcName == "_configure_narrow_argv" || funcName == "_configure_wide_argv") addr = CreateStub(0x3, 214);
                else if (funcName == "_initialize_narrow_environment" || funcName == "_initialize_wide_environment") addr = CreateStub(0x3, 214); // Map both to same init thunk
                else if (funcName == "_get_initial_narrow_environment" || funcName == "_get_initial_wide_environment") addr = CreateStub(0x3, 215);
                else if (funcName == "_initterm" || funcName == "_initterm_e") addr = CreateStub(0x3, 210);

                // ntdll extensions
                else if (funcName == "LdrInitializeThunk") addr = CreateStub(0x3, 300);
                else if (funcName == "NtTerminateProcess") addr = CreateStub(0x3, 301);
                else if (funcName == "NtQuerySystemInformation") addr = CreateStub(0x3, 302);
                else if (funcName == "NtQueryInformationProcess") addr = CreateStub(0x3, 999);
                else if (funcName == "NtSetInformationProcess") addr = CreateStub(0x3, 999);
                else if (funcName == "NtClose") addr = CreateStub(0x3, 73); // Map to CloseHandle thunk
                else if (funcName == "RtlAllocateHeap") addr = CreateStub(0x3, 132); // Map to HeapAlloc thunk
                else if (funcName == "RtlFreeHeap") addr = CreateStub(0x3, 133); // Map to HeapFree thunk

                // d3d11 extensions
                else if (funcName == "D3D11CreateDevice") addr = CreateStub(0x4, 100);
                else if (funcName == "D3D11CreateDeviceAndSwapChain") addr = CreateStub(0x4, 101);

                if (!addr) {
                    fprintf(stderr, "-> STUB (Missing: %s)\n", funcName.c_str());
                    addr = CreateStub(0x3, 999);
                } else {
                    fprintf(stderr, "-> OK\n");
                }
            }

            if (!addr && dllBase) {
                addr = FindSymbol(dllName, funcName);
            }

            if (addr) {
                thunk->u1.Function = addr;
                fprintf(stderr, "[PELoader] Resolved %s!%s -> 0x%llx\n", dllName.c_str(), funcName.c_str(), addr);
                FEX_RegisterStubAddress(addr, (dllName + "!" + funcName).c_str());
            } else {
                fprintf(stderr, "[PELoader] WARNING: Could not resolve %s!%s\n", dllName.c_str(), funcName.c_str());
                thunk->u1.Function = 0xDEADBEEF; 
            }
            thunk++;
            origThunk++;
        }
        fprintf(stderr, "[PELoader] Finished resolving imports for %s\n", dllName.c_str());
        fflush(stderr);
        desc++;
    }
    fprintf(stderr, "[PELoader] ALL imports resolved successfully.\n");
    fflush(stderr);
}

static uint32_t GetArgSize(const std::string& name) {
    if (name == "CreateWindowExW" || name == "CreateWindowExA") return 48;
    if (name == "ShowWindow") return 8;
    if (name == "UpdateWindow") return 4;
    if (name == "GetMessageW" || name == "GetMessageA") return 16;
    if (name == "TranslateMessage") return 4;
    if (name == "DispatchMessageW" || name == "DispatchMessageA") return 4;
    if (name == "DefWindowProcW" || name == "DefWindowProcA") return 16;
    if (name == "PostQuitMessage") return 4;
    if (name == "ExitProcess" || name == "TerminateProcess") return 4;
    if (name == "GetModuleHandleW" || name == "GetModuleHandleA") return 4;
    if (name == "GetProcAddress") return 8;
    if (name == "GetCommandLineW") return 0;
    if (name == "HeapAlloc") return 12;
    if (name == "HeapFree") return 12;
    if (name == "LocalFree") return 4;
    if (name == "RegCloseKey") return 4;
    if (name == "RegOpenKeyExW" || name == "RegOpenKeyW") return 20;
    if (name == "RegCreateKeyExW") return 36;
    if (name == "RegCreateKeyW") return 12;
    if (name == "RegQueryValueExW") return 24;
    if (name == "RegSetValueExW") return 24;
    if (name == "RegisterClassExW") return 4;
    if (name == "RegisterClassW") return 4;
    if (name == "GetSystemMetrics") return 4;
    if (name == "GetClientRect" || name == "GetWindowRect") return 8;
    if (name == "GetDC") return 4;
    if (name == "ReleaseDC") return 8;
    if (name == "GetDeviceCaps") return 8;
    if (name == "DeleteObject") return 4;
    if (name == "SetWindowTextW") return 8;
    if (name == "GetWindowTextW") return 12;
    if (name == "SendMessageW") return 16;
    if (name == "SendDlgItemMessageW") return 20;
    if (name == "BeginPaint") return 8;
    if (name == "EndPaint") return 8;
    if (name == "ExtTextOutW") return 32;
    if (name == "FillRect") return 12;
    if (name == "CreateFontIndirectW") return 4;
    if (name == "SelectObject") return 8;
    if (name == "GetStockObject") return 4;
    if (name == "CreateSolidBrush") return 4;
    if (name == "CreatePen") return 12;
    if (name == "SetBkMode") return 8;
    if (name == "SetTextColor") return 8;
    if (name == "SetBkColor") return 8;
    if (name == "GetTextMetricsW") return 8;
    if (name == "LoadImageW") return 24;
    if (name == "LoadCursorW") return 8;
    if (name == "LoadIconW") return 8;
    if (name == "GetMonitorInfoW") return 8;
    if (name == "GetMenu") return 4;
    if (name == "CheckMenuItem") return 12;
    if (name == "EnableMenuItem") return 12;
    if (name == "RegisterWindowMessageW") return 4;
    if (name == "InitCommonControls" || name == "InitCommonControlsEx") return 0;
    if (name == "PropertySheetW") return 4;
    if (name == "D3D11CreateDevice") return 40;
    if (name == "D3D11CreateDeviceAndSwapChain") return 48;
    return 0; // Default
}

uint64_t PELoader::CreateThunkStub(struct FEX_Context* Ctx, uint32_t Class, uint32_t Index, uint32_t ArgSize) {
    uint64_t stub = FEX_MapMemory(Ctx, 0, 0x1000, FEX_MEM_READ | FEX_MEM_WRITE);
    uint8_t* code = (uint8_t*)stub;
    // Use valid Linux syscall index (39 = getpid) so FEXCore's OS_LINUX64 ABI knows it returns a value and updates RAX
    uint32_t syscall_num = 8;

    if (Is32Bit) {
        // 32-bit: mov eax, syscall_num; syscall; ret ArgSize
        code[0] = 0xB8; *(uint32_t*)&code[1] = syscall_num;
        code[5] = 0x0F; code[6] = 0x05;
        if (ArgSize > 0) {
            code[7] = 0xC2;
            code[8] = (uint8_t)(ArgSize & 0xFF);
            code[9] = (uint8_t)((ArgSize >> 8) & 0xFF);
        } else {
            code[7] = 0xC3;
        }
    } else {
        // 64-bit: 
        // push rdi (57)
        // mov r10, rcx (49 89 CA)
        // mov rdi, syscall_num (48 BF ...)
        // mov rax, syscall_num (48 B8 ...)
        // syscall (0F 05)
        // pop rdi (5F)
        // ret (C3)
        code[0] = 0x57; // push rdi
        code[1] = 0x49; code[2] = 0x89; code[3] = 0xCA; // mov r10, rcx
        code[4] = 0x48; code[5] = 0xBF; *(uint64_t*)&code[6] = (uint64_t)syscall_num;
        code[14] = 0x48; code[15] = 0xB8; *(uint64_t*)&code[16] = (uint64_t)syscall_num;
        code[24] = 0x0F; code[25] = 0x05;
        code[26] = 0x5F; // pop rdi
        code[27] = 0xC3;
    }

    mprotect((void*)stub, 0x1000, PROT_READ | PROT_EXEC);
    return stub;
    }

uint64_t PELoader::LoadDLL(struct FEX_Context* Ctx, const std::string& Name) {
    for (const auto& dll : LoadedDLLs) if (dll.Name == Name) return dll.Base;

    std::string fullPath = VFSEmulator::Get().TranslatePath(Name);
    fprintf(stderr, "[PELoader] Resolving DLL %s -> %s\n", Name.c_str(), fullPath.c_str());
    
    PELoader subLoader;
    subLoader.LoadedDLLs = LoadedDLLs; // Pass current state
    subLoader.SetDLLPath(DLLPath);
    if (subLoader.Load(Ctx, fullPath.c_str())) {
        LoadedDLLs = subLoader.LoadedDLLs;
        return subLoader.GetBaseAddress();
    }
    return 0;
}



}
