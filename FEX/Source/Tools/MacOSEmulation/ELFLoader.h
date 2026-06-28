#include "MachOLoader.h"
#include <iostream>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "FEXCore/Core/FEXLibrary.h"

namespace MacOSEmulation {

struct ELF64_Header {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct ELF64_Phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

#define PT_LOAD 1
#define PT_INTERP 3
#define PF_X 1
#define PF_W 2
#define PF_R 4

class ELFLoader {
public:
    bool Load(FEX_Context* Ctx, const char* Path, uint64_t* EntryPoint, uint64_t* BaseAddr) {
        int fd = open(Path, O_RDONLY);
        if (fd < 0) return false;

        ELF64_Header header;
        read(fd, &header, sizeof(header));

        if (memcmp(header.e_ident, "\x7f\x45\x4c\x46", 4) != 0) {
            close(fd);
            return false;
        }

        std::vector<ELF64_Phdr> phidrs(header.e_phnum);
        lseek(fd, header.e_phoff, SEEK_SET);
        read(fd, phidrs.data(), header.e_phnum * sizeof(ELF64_Phdr));

        uint64_t min_addr = ~0ULL;
        uint64_t max_addr = 0;
        std::string interp_path = "";

        for (const auto& ph : phidrs) {
            if (ph.p_type == PT_LOAD) {
                if (ph.p_vaddr < min_addr) min_addr = ph.p_vaddr;
                if (ph.p_vaddr + ph.p_memsz > max_addr) max_addr = ph.p_vaddr + ph.p_memsz;
            } else if (ph.p_type == PT_INTERP) {
                char path[512];
                lseek(fd, ph.p_offset, SEEK_SET);
                read(fd, path, ph.p_filesz);
                path[ph.p_filesz] = 0;
                interp_path = path;
            }
        }

        // For PIE binaries, min_addr is often 0. Use a fixed high base for guest
        uint64_t preferred_start = (min_addr == 0) ? 0x400000000 : min_addr;
        uint64_t size = (max_addr - min_addr + 0x3FFF) & ~0x3FFFULL;

        uintptr_t mapped = FEX_MapMemory(Ctx, preferred_start, size, FEX_MEM_READ | FEX_MEM_WRITE);
        if (!mapped) mapped = FEX_MapMemory(Ctx, 0, size, FEX_MEM_READ | FEX_MEM_WRITE);
        if (!mapped) { close(fd); return false; }

        uint64_t slide = mapped - min_addr;

        for (const auto& ph : phidrs) {
            if (ph.p_type == PT_LOAD) {
                lseek(fd, ph.p_offset, SEEK_SET);
                read(fd, (void*)(ph.p_vaddr + slide), ph.p_filesz);
                if (ph.p_memsz > ph.p_filesz) {
                    memset((void*)(ph.p_vaddr + ph.p_filesz + slide), 0, ph.p_memsz - ph.p_filesz);
                }
                
                int prot = 0;
                if (ph.p_flags & PF_R) prot |= PROT_READ;
                if (ph.p_flags & PF_W) prot |= PROT_WRITE;
                if (ph.p_flags & PF_X) prot |= PROT_EXEC;
                size_t HostPageSize = getpagesize();
                uint64_t HostPageMask = ~(HostPageSize - 1);
                mprotect((void*)((ph.p_vaddr + slide) & HostPageMask), (ph.p_memsz + HostPageSize - 1) & HostPageMask, prot);
            }
        }

        *EntryPoint = header.e_entry + ((header.e_type == 3) ? slide : 0);
        *BaseAddr = mapped;

        if (!interp_path.empty()) {
            fprintf(stderr, "[ELFLoader] Binary requests interpreter: %s\n", interp_path.c_str());
            const char* env_rootfs = getenv("FEX_ROOTFS");
            std::string rootfs_base = env_rootfs ? env_rootfs : "/Volumes/FEXRootFS";
            std::string full_interp_path = rootfs_base + interp_path;
            ELFLoader interpLoader;
            uint64_t interpEntry = 0;
            uint64_t interpBase = 0;
            if (interpLoader.Load(Ctx, full_interp_path.c_str(), &interpEntry, &interpBase)) {
                *EntryPoint = interpEntry;
                fprintf(stderr, "[ELFLoader] Redirected entry point to interpreter: 0x%llx\n", (unsigned long long)*EntryPoint);
            } else {
                fprintf(stderr, "[ELFLoader] ERROR: Failed to load interpreter from RootFS: %s\n", full_interp_path.c_str());
            }
        }

        fprintf(stderr, "[ELFLoader] Loaded ELF at 0x%llx (EP: 0x%llx, Type: %d)\n", (unsigned long long)mapped, (unsigned long long)*EntryPoint, header.e_type);

        close(fd);
        return true;
    }
};

} // namespace MacOSEmulation
