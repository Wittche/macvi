/**
 * @file pe_parser.c
 * @brief PE header parsing and structural validation.
 *
 * Implements the header-parsing half of the PE loader: reading and validating
 * the DOS header, PE signature, COFF file header, optional header, and
 * section table.  This code does NOT perform file I/O or memory mapping —
 * it operates on a raw byte buffer provided by the caller.
 *
 * Endianness note: PE files are little-endian.  ARM64 (AArch64) is also
 * little-endian by default on macOS, so we read multi-byte fields directly
 * without byte-swapping.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/**
 * Log a PE validation error to stderr with a consistent prefix.
 */
static void internal_pe_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[macwi:pe_parser] ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/* ============================================================================
 * macwi_pe_parse_headers
 * ============================================================================ */

macwi_status_t macwi_pe_parse_headers(const uint8_t* data, size_t size,
                                      PE_IMAGE* out_image) {
    if (!data || !out_image) {
        internal_pe_error("NULL argument passed to macwi_pe_parse_headers");
        return MACWI_ERROR_INVALID_PARAM;
    }

    /* Zero-initialize the output structure */
    memset(out_image, 0, sizeof(PE_IMAGE));

    /* ---- Step 1: Validate DOS Header ------------------------------------ */

    if (size < sizeof(DOS_HEADER)) {
        internal_pe_error("File too small for DOS header (%zu bytes)", size);
        return MACWI_ERROR_INVALID_PE;
    }

    const DOS_HEADER* dos = (const DOS_HEADER*)data;

    if (dos->e_magic != DOS_MAGIC) {
        internal_pe_error("Invalid DOS magic: 0x%04X (expected 0x%04X)",
                          dos->e_magic, DOS_MAGIC);
        return MACWI_ERROR_INVALID_PE;
    }

    /* ---- Step 2: Locate and validate PE signature ----------------------- */

    uint32_t pe_offset = (uint32_t)dos->e_lfanew;

    if (pe_offset == 0 || pe_offset >= size) {
        internal_pe_error("Invalid e_lfanew offset: 0x%08X (file size: %zu)",
                          pe_offset, size);
        return MACWI_ERROR_INVALID_PE;
    }

    if (pe_offset + sizeof(IMAGE_NT_HEADERS_32) > size) {
        internal_pe_error("File too small for NT headers at offset 0x%08X",
                          pe_offset);
        return MACWI_ERROR_INVALID_PE;
    }

    const IMAGE_NT_HEADERS_32* nt = (const IMAGE_NT_HEADERS_32*)(data + pe_offset);

    if (nt->Signature != PE_SIGNATURE) {
        internal_pe_error("Invalid PE signature: 0x%08X (expected 0x%08X)",
                          nt->Signature, PE_SIGNATURE);
        return MACWI_ERROR_INVALID_PE;
    }

    /* ---- Step 3: Validate COFF File Header ------------------------------ */

    const PE_FILE_HEADER* file_hdr = &nt->FileHeader;

    if (file_hdr->Machine != IMAGE_FILE_MACHINE_I386) {
        internal_pe_error("Unsupported machine type: 0x%04X (only i386/0x%04X supported)",
                          file_hdr->Machine, IMAGE_FILE_MACHINE_I386);
        return MACWI_ERROR_UNSUPPORTED;
    }

    if (file_hdr->NumberOfSections == 0) {
        internal_pe_error("PE file has zero sections");
        return MACWI_ERROR_INVALID_PE;
    }

    if (file_hdr->SizeOfOptionalHeader < sizeof(PE_OPTIONAL_HEADER_32)) {
        internal_pe_error("Optional header too small: %u bytes (need at least %zu)",
                          file_hdr->SizeOfOptionalHeader,
                          sizeof(PE_OPTIONAL_HEADER_32));
        return MACWI_ERROR_INVALID_PE;
    }

    /* ---- Step 4: Validate PE32 Optional Header -------------------------- */

    const PE_OPTIONAL_HEADER_32* opt = &nt->OptionalHeader;

    if (opt->Magic != PE32_MAGIC) {
        internal_pe_error("Unsupported optional header magic: 0x%04X (need PE32/0x%04X)",
                          opt->Magic, PE32_MAGIC);
        return MACWI_ERROR_UNSUPPORTED;
    }

    if (opt->SectionAlignment == 0 || opt->FileAlignment == 0) {
        internal_pe_error("Invalid alignment: SectionAlignment=0x%X, FileAlignment=0x%X",
                          opt->SectionAlignment, opt->FileAlignment);
        return MACWI_ERROR_INVALID_PE;
    }

    /* ---- Step 5: Locate section headers --------------------------------- */

    /* Section headers immediately follow the optional header */
    size_t section_table_offset = pe_offset + sizeof(DWORD) +
                                  sizeof(PE_FILE_HEADER) +
                                  file_hdr->SizeOfOptionalHeader;

    size_t section_table_size = (size_t)file_hdr->NumberOfSections *
                                sizeof(PE_SECTION_HEADER);

    if (section_table_offset + section_table_size > size) {
        internal_pe_error("Section table extends past end of file "
                          "(offset=0x%zX, count=%u)",
                          section_table_offset, file_hdr->NumberOfSections);
        return MACWI_ERROR_INVALID_PE;
    }

    const PE_SECTION_HEADER* sections =
        (const PE_SECTION_HEADER*)(data + section_table_offset);

    /* ---- Step 6: Populate the output PE_IMAGE --------------------------- */

    out_image->mapped_base    = (uint8_t*)data;  /* Non-owning pointer */
    out_image->mapped_size    = size;
    out_image->dos_header     = dos;
    out_image->nt_headers     = nt;
    out_image->section_headers = sections;
    out_image->num_sections   = file_hdr->NumberOfSections;
    out_image->image_base     = opt->ImageBase;
    out_image->size_of_image  = opt->SizeOfImage;
    out_image->entry_point    = opt->ImageBase + opt->AddressOfEntryPoint;
    out_image->is_loaded      = false;
    out_image->file_path      = NULL;

    return MACWI_SUCCESS;
}

/* ============================================================================
 * macwi_pe_validate
 * ============================================================================ */

macwi_status_t macwi_pe_validate(const PE_IMAGE* image) {
    if (!image) {
        internal_pe_error("NULL image passed to macwi_pe_validate");
        return MACWI_ERROR_INVALID_PARAM;
    }

    if (!image->dos_header || !image->nt_headers || !image->section_headers) {
        internal_pe_error("Image has NULL header pointers — was it parsed?");
        return MACWI_ERROR_INVALID_PE;
    }

    /* Re-check magic numbers for safety */
    if (image->dos_header->e_magic != DOS_MAGIC) {
        internal_pe_error("DOS magic mismatch during validation");
        return MACWI_ERROR_INVALID_PE;
    }

    if (image->nt_headers->Signature != PE_SIGNATURE) {
        internal_pe_error("PE signature mismatch during validation");
        return MACWI_ERROR_INVALID_PE;
    }

    const PE_OPTIONAL_HEADER_32* opt = &image->nt_headers->OptionalHeader;

    /* Check that SizeOfImage is a multiple of SectionAlignment */
    if (opt->SectionAlignment > 0 &&
        (opt->SizeOfImage % opt->SectionAlignment) != 0) {
        internal_pe_error("SizeOfImage (0x%X) is not aligned to SectionAlignment (0x%X)",
                          opt->SizeOfImage, opt->SectionAlignment);
        return MACWI_ERROR_INVALID_PE;
    }

    /* Check that SizeOfHeaders does not exceed SizeOfImage */
    if (opt->SizeOfHeaders > opt->SizeOfImage) {
        internal_pe_error("SizeOfHeaders (0x%X) exceeds SizeOfImage (0x%X)",
                          opt->SizeOfHeaders, opt->SizeOfImage);
        return MACWI_ERROR_INVALID_PE;
    }

    /* Validate each section */
    for (uint16_t i = 0; i < image->num_sections; i++) {
        const PE_SECTION_HEADER* sec = &image->section_headers[i];

        /* Section must not extend beyond SizeOfImage */
        uint32_t section_end = sec->VirtualAddress + sec->VirtualSize;
        if (section_end < sec->VirtualAddress) {
            /* Overflow check */
            internal_pe_error("Section %u: VirtualAddress + VirtualSize overflows", i);
            return MACWI_ERROR_INVALID_PE;
        }

        /* VirtualAddress should be >= SizeOfHeaders */
        if (sec->VirtualAddress < opt->SizeOfHeaders && sec->VirtualSize > 0) {
            internal_pe_error("Section %u: VirtualAddress (0x%X) overlaps headers",
                              i, sec->VirtualAddress);
            return MACWI_ERROR_INVALID_PE;
        }
    }

    return MACWI_SUCCESS;
}
