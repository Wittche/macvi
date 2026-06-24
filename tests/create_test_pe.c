/**
 * @file create_test_pe.c
 * @brief Creates a minimal valid 32-bit PE executable for testing MacWI's PE parser.
 *
 * This generates a tiny .exe that contains valid DOS and PE headers,
 * one .text section with a simple x86 "ret" instruction, and minimal
 * optional header fields.
 *
 * Usage: compile and run this to produce test_hello.exe
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* PE constants */
#define DOS_MAGIC       0x5A4D
#define PE_SIGNATURE    0x00004550
#define PE32_MAGIC      0x10B
#define MACHINE_I386    0x14C

#define SECTION_ALIGN   0x1000
#define FILE_ALIGN      0x200
#define IMAGE_BASE      0x00400000

/* Section characteristics */
#define SCN_CNT_CODE            0x00000020
#define SCN_MEM_EXECUTE         0x20000000
#define SCN_MEM_READ            0x40000000

#pragma pack(push, 1)

typedef struct {
    uint16_t e_magic;       /* 0x5A4D */
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t  e_lfanew;      /* Offset to PE header */
} MinDosHeader;

typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} MinFileHeader;

typedef struct {
    uint16_t Magic;                     /* 0x10B for PE32 */
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOSVersion;
    uint16_t MinorOSVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    uint32_t DataDirectory[16 * 2]; /* 16 entries of VirtualAddress and Size */
} MinOptionalHeader32;

typedef struct {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} MinSectionHeader;

#pragma pack(pop)

int main(void) {
    const char* output_path = "test_hello.exe";
    FILE* f = fopen(output_path, "wb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    /* Calculate offsets */
    uint32_t dos_size = sizeof(MinDosHeader);
    uint32_t pe_offset = 0x80; /* Standard offset for PE header */
    uint32_t pe_sig_size = 4;
    uint32_t file_header_size = sizeof(MinFileHeader);
    uint32_t opt_header_size = sizeof(MinOptionalHeader32);
    uint32_t section_header_size = sizeof(MinSectionHeader);
    uint32_t headers_end = pe_offset + pe_sig_size + file_header_size +
                           opt_header_size + section_header_size;

    /* Round up to file alignment */
    uint32_t headers_on_disk = ((headers_end + FILE_ALIGN - 1) / FILE_ALIGN) * FILE_ALIGN;

    /* .text section: just a "ret" instruction (0xC3) */
    uint8_t code[] = { 0xC3 }; /* x86 RET */
    uint32_t text_rva = SECTION_ALIGN; /* First section at 0x1000 */
    uint32_t text_raw_offset = headers_on_disk;
    uint32_t text_raw_size = FILE_ALIGN; /* Minimum one file-aligned block */
    uint32_t image_size = text_rva + SECTION_ALIGN; /* header + one section */

    /* --- DOS Header --- */
    MinDosHeader dos = {0};
    dos.e_magic = DOS_MAGIC;
    dos.e_cparhdr = 4;
    dos.e_lfanew = (int32_t)pe_offset;

    /* Write DOS header */
    fwrite(&dos, sizeof(dos), 1, f);

    /* Pad to PE offset */
    uint8_t zero = 0;
    long pos = ftell(f);
    while (pos < pe_offset) {
        fwrite(&zero, 1, 1, f);
        pos++;
    }

    /* --- PE Signature --- */
    uint32_t pe_sig = PE_SIGNATURE;
    fwrite(&pe_sig, 4, 1, f);

    /* --- COFF File Header --- */
    MinFileHeader fhdr = {0};
    fhdr.Machine = MACHINE_I386;
    fhdr.NumberOfSections = 1;
    fhdr.TimeDateStamp = 0x60000000; /* Arbitrary */
    fhdr.SizeOfOptionalHeader = (uint16_t)opt_header_size;
    fhdr.Characteristics = 0x0102; /* EXECUTABLE_IMAGE | 32BIT_MACHINE */
    fwrite(&fhdr, sizeof(fhdr), 1, f);

    /* --- Optional Header (PE32) --- */
    MinOptionalHeader32 opt = {0};
    opt.Magic = PE32_MAGIC;
    opt.MajorLinkerVersion = 1;
    opt.SizeOfCode = (uint32_t)sizeof(code);
    opt.AddressOfEntryPoint = text_rva; /* Entry = start of .text */
    opt.BaseOfCode = text_rva;
    opt.ImageBase = IMAGE_BASE;
    opt.SectionAlignment = SECTION_ALIGN;
    opt.FileAlignment = FILE_ALIGN;
    opt.MajorOSVersion = 4;
    opt.MajorSubsystemVersion = 4;
    opt.SizeOfImage = image_size;
    opt.SizeOfHeaders = headers_on_disk;
    opt.Subsystem = 3; /* IMAGE_SUBSYSTEM_WINDOWS_CUI */
    opt.SizeOfStackReserve = 0x100000;
    opt.SizeOfStackCommit = 0x1000;
    opt.SizeOfHeapReserve = 0x100000;
    opt.SizeOfHeapCommit = 0x1000;
    opt.NumberOfRvaAndSizes = 16; /* 16 data directories */
    fwrite(&opt, sizeof(opt), 1, f);

    /* --- Section Header: .text --- */
    MinSectionHeader text_sec = {0};
    memcpy(text_sec.Name, ".text", 5);
    text_sec.VirtualSize = (uint32_t)sizeof(code);
    text_sec.VirtualAddress = text_rva;
    text_sec.SizeOfRawData = text_raw_size;
    text_sec.PointerToRawData = text_raw_offset;
    text_sec.Characteristics = SCN_CNT_CODE | SCN_MEM_EXECUTE | SCN_MEM_READ;
    fwrite(&text_sec, sizeof(text_sec), 1, f);

    /* Pad to raw data offset */
    pos = ftell(f);
    while (pos < text_raw_offset) {
        fwrite(&zero, 1, 1, f);
        pos++;
    }

    /* Write .text section data */
    fwrite(code, sizeof(code), 1, f);

    /* Pad to file alignment */
    pos = ftell(f);
    long end = text_raw_offset + text_raw_size;
    while (pos < end) {
        fwrite(&zero, 1, 1, f);
        pos++;
    }

    fclose(f);
    printf("Created minimal PE32 executable: %s (%ld bytes)\n",
           output_path, end);
    return 0;
}
