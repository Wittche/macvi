#include "MachOLoader.h"
#include <iostream>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libkern/OSByteOrder.h>
#include "FEXCore/Utils/Allocator.h"
#include "FEXCore/Utils/TypeDefines.h"

namespace MacOSEmulation {

bool MachOLoader::Load(FEXCore::Context::Context* Ctx, const std::string& Path) {
    std::cout << "[MacOSEmulation] Loading Mach-O binary: " << Path << std::endl;
    
    int fd = open(Path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[MacOSEmulation] Failed to open file: " << Path << std::endl;
        return false;
    }

    struct stat statbuf;
    fstat(fd, &statbuf);

    void* header_map = mmap(nullptr, 0x4000, PROT_READ, MAP_PRIVATE, fd, 0);
    if (header_map == MAP_FAILED) {
        close(fd);
        return false;
    }

    uint32_t magic = *reinterpret_cast<uint32_t*>(header_map);
    uint32_t slice_offset = 0;
    uint32_t slice_size = statbuf.st_size;

    if (magic == FAT_CIGAM || magic == FAT_MAGIC) {
        fat_header* fh = reinterpret_cast<fat_header*>(header_map);
        uint32_t nfat_arch = OSSwapBigToHostInt32(fh->nfat_arch);
        fat_arch* archs = reinterpret_cast<fat_arch*>((char*)header_map + sizeof(fat_header));
        bool found = false;
        for (uint32_t i = 0; i < nfat_arch; ++i) {
            uint32_t cputype = OSSwapBigToHostInt32(archs[i].cputype);
            if (cputype == CPU_TYPE_X86_64) {
                slice_offset = OSSwapBigToHostInt32(archs[i].offset);
                slice_size = OSSwapBigToHostInt32(archs[i].size);
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "[MacOSEmulation] Binary does not contain x86_64 slice!" << std::endl;
            munmap(header_map, 0x4000);
            close(fd);
            return false;
        }
    }
    
    munmap(header_map, 0x4000);

    bool is_dyld = (Path.find("dyld") != std::string::npos);
    bool success = MapSlice(Ctx, fd, slice_offset, slice_size, is_dyld);
    close(fd);
    return success;
}

bool MachOLoader::MapSlice(FEXCore::Context::Context* Ctx, int fd, uint32_t slice_offset, uint32_t slice_size, bool is_dyld) {
    mach_header_64 header;
    pread(fd, &header, sizeof(header), slice_offset);
    
    if (header.cputype != CPU_TYPE_X86_64) {
        return false;
    }

    std::vector<uint8_t> load_cmds(header.sizeofcmds);
    pread(fd, load_cmds.data(), header.sizeofcmds, slice_offset + sizeof(mach_header_64));

    uint64_t MinVMAddr = ~0ULL;
    uint64_t MaxVMAddr = 0;
    std::string dylinker_path = "";
    uint64_t LocalEntryPoint = 0;

    uint8_t* cmd_ptr = load_cmds.data();
    for (uint32_t i = 0; i < header.ncmds; ++i) {
        auto cmd = reinterpret_cast<load_command*>(cmd_ptr);
        if (cmd->cmd == LC_SEGMENT_64) {
            auto seg = reinterpret_cast<segment_command_64*>(cmd);
            if (seg->vmsize > 0 && std::string(seg->segname) != "__PAGEZERO") {
                if (seg->vmaddr < MinVMAddr) MinVMAddr = seg->vmaddr;
                if (seg->vmaddr + seg->vmsize > MaxVMAddr) MaxVMAddr = seg->vmaddr + seg->vmsize;
            }
        } else if (cmd->cmd == LC_MAIN) {
            auto main_cmd = reinterpret_cast<entry_point_command*>(cmd);
            LocalEntryPoint = main_cmd->entryoff; 
        } else if (cmd->cmd == LC_UNIXTHREAD) {
            uint32_t* thread_cmd = reinterpret_cast<uint32_t*>(cmd);
            if (thread_cmd[2] == 4 && thread_cmd[3] == 42) {
                LocalEntryPoint = *reinterpret_cast<uint64_t*>(&thread_cmd[36]);
            }
        } else if (cmd->cmd == LC_LOAD_DYLINKER && !is_dyld) {
            auto dylinker_cmd = reinterpret_cast<dylinker_command*>(cmd);
            dylinker_path = reinterpret_cast<char*>(cmd) + dylinker_cmd->name.offset;
        }
        cmd_ptr += cmd->cmdsize;
    }

    uint64_t Slide = 0;
    if (MinVMAddr != ~0ULL) {
        size_t HostPageSize = getpagesize();
        uint64_t HostPageMask = ~(HostPageSize - 1);
        uint64_t AlignedMin = MinVMAddr & HostPageMask;
        uint64_t AlignedMax = (MaxVMAddr + HostPageSize - 1) & HostPageMask;
        uint64_t TotalSize = AlignedMax - AlignedMin;

        // Allocate entire range via standard mmap (avoiding FEX hooks for guest base)
        uint64_t GuestBase = 0x100000000;
        uint64_t base_hint = GuestBase + FEXCore::Utils::GlobalMemoryBase;
        void* allocated = ::mmap((void*)base_hint, TotalSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (allocated == MAP_FAILED) {
            fprintf(stderr, "[MacOSEmulation] Failed to allocate base memory range at 0x%llx, trying any address...\n", (unsigned long long)base_hint);
            allocated = ::mmap(nullptr, TotalSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
        
        if (allocated == MAP_FAILED) {
            fprintf(stderr, "[MacOSEmulation] CRITICAL: Failed to allocate base memory range of size 0x%llx! errno=%d\n", (unsigned long long)TotalSize, errno);
            return false;
        }

        uint64_t base = reinterpret_cast<uint64_t>(allocated);
        Slide = base - AlignedMin;
        this->BaseAddress = base;
        std::cout << "[MacOSEmulation] Mapped " << (is_dyld ? "dyld" : "binary") << " at 0x" << std::hex << base << ", Slide: 0x" << Slide << std::dec << std::endl;

        cmd_ptr = load_cmds.data();
        for (uint32_t i = 0; i < header.ncmds; ++i) {
            auto cmd = reinterpret_cast<load_command*>(cmd_ptr);
            if (cmd->cmd == LC_SEGMENT_64) {
                auto seg = reinterpret_cast<segment_command_64*>(cmd);
                if (seg->vmsize > 0 && std::string(seg->segname) != "__PAGEZERO") {
                    uint64_t TargetAddr = seg->vmaddr + Slide;
                    if (seg->filesize > 0) {
                        pread(fd, (void*)TargetAddr, seg->filesize, slice_offset + seg->fileoff);
                    }
                    if (seg->vmsize > seg->filesize) {
                        memset((void*)(TargetAddr + seg->filesize), 0, seg->vmsize - seg->filesize);
                    }
                    std::cout << "[MacOSEmulation]   Mapping " << seg->segname << " at 0x" << std::hex << TargetAddr << " size 0x" << seg->vmsize << std::dec << std::endl;
                    
                    // If we have an entry offset (LC_MAIN), it's relative to the first __TEXT segment
                    if (LocalEntryPoint > 0 && LocalEntryPoint < 0x100000000 && std::string(seg->segname) == "__TEXT") {
                        uint64_t AbsoluteEntryPoint = seg->vmaddr + LocalEntryPoint;
                        std::cout << "[MacOSEmulation]   Resolved LC_MAIN entry point: 0x" << std::hex << AbsoluteEntryPoint << " (Offset 0x" << LocalEntryPoint << " + VMAddr 0x" << seg->vmaddr << ")" << std::dec << std::endl;
                        LocalEntryPoint = AbsoluteEntryPoint;
                    }
                }
            }
            cmd_ptr += cmd->cmdsize;
        }

        LocalEntryPoint += Slide;
    }

    // Entry point resolution
    if (!dylinker_path.empty() && !is_dyld) {
        std::cout << "[MacOSEmulation] Binary requested dylinker: " << dylinker_path << std::endl;
        MacOSEmulation::MachOLoader dyldLoader;
        if (dyldLoader.Load(Ctx, dylinker_path)) {
            EntryPoint = dyldLoader.GetEntryPoint();
            std::cout << "[MacOSEmulation] EntryPoint overridden to dyld: 0x" << std::hex << EntryPoint << std::dec << std::endl;
        } else {
            std::cerr << "[MacOSEmulation] Failed to load dylinker: " << dylinker_path << std::endl;
            return false;
        }
    } else {
        EntryPoint = LocalEntryPoint;
        std::cout << "[MacOSEmulation] Final Entry Point: 0x" << std::hex << EntryPoint << std::dec << std::endl;
    }

    return true;
}

} // namespace MacOSEmulation
