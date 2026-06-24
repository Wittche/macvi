/**
 * @file pe.h
 * @brief PE (Portable Executable) format structures and loader API.
 *
 * Defines the on-disk layout of PE32 binaries — DOS header, PE signature,
 * COFF file header, optional header, section headers, import/export
 * directories — plus the runtime PE_IMAGE struct that represents a fully
 * parsed (and optionally mapped) executable.
 *
 * All multi-byte fields are little-endian, which matches the ARM64 host, so
 * no byte-swapping is required.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "macwi/types.h"

struct EMU_CONTEXT;
#include <stddef.h>

/* ============================================================================
 * Magic numbers and constants
 * ============================================================================ */

#define DOS_MAGIC               0x5A4D      /**< "MZ" — DOS header signature    */
#define PE_SIGNATURE            0x00004550  /**< "PE\0\0" — PE header signature */
#define PE32_MAGIC              0x010B      /**< PE32 optional header magic     */
#define PE32PLUS_MAGIC          0x020B      /**< PE32+ optional header magic    */

/** Maximum number of data directory entries in a PE32 optional header. */
#define PE_MAX_DATA_DIRECTORIES 16

/** Maximum section name length (including NUL, though PE names need not be NUL-terminated). */
#define PE_SECTION_NAME_SIZE    8

/* ============================================================================
 * Machine types
 * ============================================================================ */

#define IMAGE_FILE_MACHINE_UNKNOWN  0x0000
#define IMAGE_FILE_MACHINE_I386     0x014C  /**< x86 (32-bit) — the only type we support */
#define IMAGE_FILE_MACHINE_AMD64    0x8664  /**< x64 — not supported                     */
#define IMAGE_FILE_MACHINE_ARM64    0xAA64  /**< ARM64 — not applicable                  */

/* ============================================================================
 * Section characteristic flags
 * ============================================================================ */

#define IMAGE_SCN_CNT_CODE              0x00000020  /**< Section contains executable code   */
#define IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040  /**< Section contains initialized data  */
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080 /**< Section contains BSS data          */
#define IMAGE_SCN_MEM_EXECUTE           0x20000000  /**< Section is executable              */
#define IMAGE_SCN_MEM_READ              0x40000000  /**< Section is readable                */
#define IMAGE_SCN_MEM_WRITE             0x80000000  /**< Section is writable                */

/* ============================================================================
 * Data directory indices
 * ============================================================================ */

#define PE_DIRECTORY_ENTRY_EXPORT    0   /**< Export table          */
#define PE_DIRECTORY_ENTRY_IMPORT    1   /**< Import table          */
#define PE_DIRECTORY_ENTRY_RESOURCE  2   /**< Resource table        */
#define PE_DIRECTORY_ENTRY_BASERELOC 5   /**< Base relocation table */
#define PE_DIRECTORY_ENTRY_IAT      12   /**< Import Address Table  */

/* ============================================================================
 * On-disk structures (packed to match PE binary layout exactly)
 *
 * NOTE: These structs use __attribute__((packed)) to guarantee the compiler
 *       does not insert padding.  Since both PE and ARM64 are little-endian,
 *       we can read fields directly without byte-swapping.
 * ============================================================================ */

/**
 * DOS Header — the legacy MS-DOS stub at the very start of every PE file.
 * Only e_magic and e_lfanew are meaningful for PE loading.
 */
typedef struct __attribute__((packed)) {
    WORD  e_magic;      /**< Magic number: must be DOS_MAGIC (0x5A4D)            */
    WORD  e_cblp;       /**< Bytes on last page of file                          */
    WORD  e_cp;         /**< Pages in file                                       */
    WORD  e_crlc;       /**< Relocations                                         */
    WORD  e_cparhdr;    /**< Size of header in paragraphs                        */
    WORD  e_minalloc;   /**< Minimum extra paragraphs needed                     */
    WORD  e_maxalloc;   /**< Maximum extra paragraphs needed                     */
    WORD  e_ss;         /**< Initial (relative) SS value                         */
    WORD  e_sp;         /**< Initial SP value                                    */
    WORD  e_csum;       /**< Checksum                                            */
    WORD  e_ip;         /**< Initial IP value                                    */
    WORD  e_cs;         /**< Initial (relative) CS value                         */
    WORD  e_lfarlc;     /**< File address of relocation table                    */
    WORD  e_ovno;       /**< Overlay number                                      */
    WORD  e_res[4];     /**< Reserved words                                      */
    WORD  e_oemid;      /**< OEM identifier                                      */
    WORD  e_oeminfo;    /**< OEM information                                     */
    WORD  e_res2[10];   /**< Reserved words                                      */
    LONG  e_lfanew;     /**< File offset to the PE signature (PE\0\0)            */
} DOS_HEADER;

/**
 * PE Data Directory entry — an RVA + size pair pointing to a specific table
 * (exports, imports, resources, relocations, etc.).
 */
typedef struct __attribute__((packed)) {
    DWORD VirtualAddress; /**< Relative Virtual Address of the table */
    DWORD Size;           /**< Size of the table in bytes            */
} PE_DATA_DIRECTORY;

/**
 * COFF File Header — immediately follows the 4-byte PE signature.
 */
typedef struct __attribute__((packed)) {
    WORD  Machine;              /**< Target machine type (IMAGE_FILE_MACHINE_I386) */
    WORD  NumberOfSections;     /**< Number of section headers                     */
    DWORD TimeDateStamp;        /**< Seconds since 1970-01-01 00:00 UTC            */
    DWORD PointerToSymbolTable; /**< File offset of COFF symbol table (0 if none)  */
    DWORD NumberOfSymbols;      /**< Number of COFF symbols                        */
    WORD  SizeOfOptionalHeader; /**< Size of the optional header that follows      */
    WORD  Characteristics;      /**< Flags (executable, DLL, large-address-aware…) */
} PE_FILE_HEADER;

/**
 * PE32 Optional Header — contains critical fields for loading a 32-bit image.
 */
typedef struct __attribute__((packed)) {
    /* Standard fields */
    WORD  Magic;                    /**< PE32_MAGIC (0x010B)                       */
    BYTE  MajorLinkerVersion;
    BYTE  MinorLinkerVersion;
    DWORD SizeOfCode;
    DWORD SizeOfInitializedData;
    DWORD SizeOfUninitializedData;
    DWORD AddressOfEntryPoint;      /**< RVA of the entry point                    */
    DWORD BaseOfCode;
    DWORD BaseOfData;

    /* NT-specific fields */
    DWORD ImageBase;                /**< Preferred load address (usually 0x400000) */
    DWORD SectionAlignment;         /**< Alignment of sections in memory (bytes)   */
    DWORD FileAlignment;            /**< Alignment of sections on disk (bytes)     */
    WORD  MajorOperatingSystemVersion;
    WORD  MinorOperatingSystemVersion;
    WORD  MajorImageVersion;
    WORD  MinorImageVersion;
    WORD  MajorSubsystemVersion;
    WORD  MinorSubsystemVersion;
    DWORD Win32VersionValue;
    DWORD SizeOfImage;              /**< Total size of the image in memory         */
    DWORD SizeOfHeaders;            /**< Size of all headers + section table       */
    DWORD CheckSum;
    WORD  Subsystem;                /**< Required subsystem (GUI, console, …)      */
    WORD  DllCharacteristics;
    DWORD SizeOfStackReserve;
    DWORD SizeOfStackCommit;
    DWORD SizeOfHeapReserve;
    DWORD SizeOfHeapCommit;
    DWORD LoaderFlags;
    DWORD NumberOfRvaAndSizes;      /**< Number of data directory entries           */

    /** Data directory array — up to PE_MAX_DATA_DIRECTORIES entries. */
    PE_DATA_DIRECTORY DataDirectory[PE_MAX_DATA_DIRECTORIES];
} PE_OPTIONAL_HEADER_32;

/**
 * PE Section Header — describes one section (.text, .data, .rdata, …).
 */
typedef struct __attribute__((packed)) {
    char  Name[PE_SECTION_NAME_SIZE]; /**< Section name (NOT necessarily NUL-terminated) */
    DWORD VirtualSize;                /**< Size of the section in memory                 */
    DWORD VirtualAddress;             /**< RVA where the section is loaded               */
    DWORD SizeOfRawData;              /**< Size of initialized data on disk              */
    DWORD PointerToRawData;           /**< File offset of the section's raw data         */
    DWORD PointerToRelocations;
    DWORD PointerToLinenumbers;
    WORD  NumberOfRelocations;
    WORD  NumberOfLinenumbers;
    DWORD Characteristics;            /**< Flags: read/write/execute, code/data          */
} PE_SECTION_HEADER;

/**
 * IMAGE_NT_HEADERS_32 — the combined PE signature + file header + optional header,
 * as found at file offset DOS_HEADER.e_lfanew.
 */
typedef struct __attribute__((packed)) {
    DWORD               Signature;      /**< Must be PE_SIGNATURE (0x00004550) */
    PE_FILE_HEADER      FileHeader;
    PE_OPTIONAL_HEADER_32 OptionalHeader;
} IMAGE_NT_HEADERS_32;

/**
 * PE Import Descriptor — one entry per imported DLL in the import table.
 */
typedef struct __attribute__((packed)) {
    DWORD OriginalFirstThunk;  /**< RVA to Import Lookup Table (ILT)  */
    DWORD TimeDateStamp;       /**< 0 if not bound, -1 if bound       */
    DWORD ForwarderChain;      /**< Index of first forwarder reference */
    DWORD Name;                /**< RVA to the DLL name string        */
    DWORD FirstThunk;          /**< RVA to Import Address Table (IAT) */
} PE_IMPORT_DESCRIPTOR;

/**
 * PE Export Directory — found via DataDirectory[0], describes exported symbols.
 */
typedef struct __attribute__((packed)) {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD  MajorVersion;
    WORD  MinorVersion;
    DWORD Name;                    /**< RVA to the DLL name                     */
    DWORD Base;                    /**< Ordinal base                            */
    DWORD NumberOfFunctions;       /**< Total number of exported functions      */
    DWORD NumberOfNames;           /**< Number of exported names                */
    DWORD AddressOfFunctions;      /**< RVA to array of function RVAs          */
    DWORD AddressOfNames;          /**< RVA to array of name RVAs             */
    DWORD AddressOfNameOrdinals;   /**< RVA to array of ordinal indices        */
} PE_EXPORT_DIRECTORY;

/* ============================================================================
 * Runtime PE Image — the parsed and (optionally) mapped representation
 * ============================================================================ */

/**
 * PE_IMAGE — holds the complete state of a loaded PE32 image.
 *
 * After macwi_pe_load_file() succeeds, all header pointers reference into the
 * mapped_base buffer, section data is memory-mapped with appropriate
 * protections, and entry_point contains the absolute virtual address.
 */
typedef struct {
    uint8_t*              mapped_base;     /**< Base of the memory-mapped image        */
    size_t                mapped_size;     /**< Total size of the mapped region         */

    const DOS_HEADER*           dos_header;      /**< Pointer into mapped_base          */
    const IMAGE_NT_HEADERS_32*  nt_headers;      /**< Pointer into mapped_base          */
    const PE_SECTION_HEADER*    section_headers; /**< Array of section headers           */
    uint16_t                    num_sections;    /**< Number of sections                  */

    uint32_t              entry_point;     /**< Absolute VA of the entry point          */
    uint32_t              image_base;      /**< Preferred image base from opt header    */
    uint32_t              size_of_image;   /**< Total virtual size of the image         */

    bool                  is_loaded;       /**< True if sections are memory-mapped      */
    const char*           file_path;       /**< Path to the source PE file (owned copy) */
} PE_IMAGE;

/* ============================================================================
 * Public API — PE loading and inspection
 * ============================================================================ */

/**
 * Load a PE32 file from disk, parse its headers, and map sections into memory.
 *
 * @param path       Absolute or relative path to the .exe or .dll file.
 * @param out_image  Output: populated PE_IMAGE struct.  Caller must eventually
 *                   call macwi_pe_free() to release resources.
 * @return MACWI_SUCCESS on success; an error code otherwise.
 */
macwi_status_t macwi_pe_load_file(const char* path, PE_IMAGE* out_image);

/**
 * Parse PE headers from a raw byte buffer (no file I/O, no section mapping).
 *
 * Useful for inspecting headers without mapping the full image.
 *
 * @param data       Pointer to the raw PE file content.
 * @param size       Size of the buffer in bytes.
 * @param out_image  Output: partially populated PE_IMAGE (mapped_base will
 *                   point to data; is_loaded will be false).
 * @return MACWI_SUCCESS on success; MACWI_ERROR_INVALID_PE if headers are bad.
 */
macwi_status_t macwi_pe_parse_headers(const uint8_t* data, size_t size,
                                      PE_IMAGE* out_image);

/**
 * Resolve the imports of a loaded PE image by patching the IAT.
 *
 * @param image  A PE_IMAGE that has been loaded via macwi_pe_load_file().
 * @return MACWI_SUCCESS if all imports were resolved.
 */
macwi_status_t macwi_pe_resolve_imports(PE_IMAGE* image);

/**
 * Map a loaded PE image into an emulation context.
 *
 * @param image The loaded PE image.
 * @param emu_ctx The emulation context to map into.
 * @return MACWI_SUCCESS on success.
 */
macwi_status_t macwi_pe_map_to_emu(const PE_IMAGE* image, struct EMU_CONTEXT* emu_ctx);

/**
 * Return the absolute virtual address of the image's entry point.
 *
 * @param image  A successfully parsed PE_IMAGE.
 * @return Entry point VA, or 0 if the image has no entry point.
 */
uint32_t macwi_pe_get_entry_point(const PE_IMAGE* image);

/**
 * Release all resources held by a PE_IMAGE (memory maps, allocations).
 *
 * @param image  The image to free. Safe to call with NULL or an already-freed image.
 */
void macwi_pe_free(PE_IMAGE* image);

/**
 * Validate the structural integrity of a parsed PE image.
 *
 * Checks magic numbers, section alignment, size consistency, and basic sanity
 * of the header fields.
 *
 * @param image  A PE_IMAGE whose headers have been parsed.
 * @return MACWI_SUCCESS if the image is valid; an error code otherwise.
 */
macwi_status_t macwi_pe_validate(const PE_IMAGE* image);
