/**
 * @file pe_parser.c
 * @brief PE header parsing and structural validation.
 *
 * Implements the header-parsing half of the PE loader: reading and validating
 * the DOS header, PE signature, COFF file header, optional header, and
 * section table for both PE32 (32-bit) and PE32+ (64-bit).
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

macwi_status_t macwi_pe_parse_headers(const uint8_t* data, size_t size, PE_IMAGE* out_image) {
    if (!data || !out_image) return MACWI_ERROR_INVALID_PARAM;

    memset(out_image, 0, sizeof(PE_IMAGE));

    if (size < sizeof(DOS_HEADER)) {
        internal_pe_error("File too small for DOS header");
        return MACWI_ERROR_INVALID_PE;
    }

    const DOS_HEADER* dos = (const DOS_HEADER*)data;
    if (dos->e_magic != DOS_MAGIC) {
        internal_pe_error("Invalid DOS magic");
        return MACWI_ERROR_INVALID_PE;
    }

    uint32_t pe_offset = (uint32_t)dos->e_lfanew;
    if (pe_offset == 0 || pe_offset >= size || pe_offset + sizeof(DWORD) + sizeof(PE_FILE_HEADER) > size) {
        internal_pe_error("Invalid e_lfanew offset");
        return MACWI_ERROR_INVALID_PE;
    }

    const DWORD* signature = (const DWORD*)(data + pe_offset);
    if (*signature != PE_SIGNATURE) {
        internal_pe_error("Invalid PE signature");
        return MACWI_ERROR_INVALID_PE;
    }

    const PE_FILE_HEADER* file_hdr = (const PE_FILE_HEADER*)(data + pe_offset + sizeof(DWORD));
    
    // We only support x86_64 and maybe i386 if needed
    if (file_hdr->Machine != IMAGE_FILE_MACHINE_I386 && file_hdr->Machine != IMAGE_FILE_MACHINE_AMD64) {
        internal_pe_error("Unsupported machine type: 0x%04X", file_hdr->Machine);
        return MACWI_ERROR_UNSUPPORTED;
    }

    if (file_hdr->SizeOfOptionalHeader < 2) {
        internal_pe_error("Optional header too small");
        return MACWI_ERROR_INVALID_PE;
    }

    const WORD* magic = (const WORD*)(data + pe_offset + sizeof(DWORD) + sizeof(PE_FILE_HEADER));
    
    out_image->mapped_base = (uint8_t*)data;
    out_image->mapped_size = size;
    out_image->dos_header = dos;
    out_image->file_header = (PE_FILE_HEADER*)file_hdr;
    out_image->num_sections = file_hdr->NumberOfSections;
    out_image->is_loaded = false;

    size_t section_table_offset = pe_offset + sizeof(DWORD) + sizeof(PE_FILE_HEADER) + file_hdr->SizeOfOptionalHeader;

    if (*magic == PE32_MAGIC) {
        out_image->is_64bit = false;
        out_image->nt_headers_32 = (const IMAGE_NT_HEADERS_32*)(data + pe_offset);
        out_image->optional_header.opt_32 = (PE_OPTIONAL_HEADER_32*)&out_image->nt_headers_32->OptionalHeader;
        out_image->image_base = out_image->optional_header.opt_32->ImageBase;
        out_image->size_of_image = out_image->optional_header.opt_32->SizeOfImage;
        out_image->entry_point = out_image->image_base + out_image->optional_header.opt_32->AddressOfEntryPoint;
    } else if (*magic == PE32PLUS_MAGIC) {
        out_image->is_64bit = true;
        out_image->nt_headers_64 = (const IMAGE_NT_HEADERS_64*)(data + pe_offset);
        out_image->optional_header.opt_64 = (PE_OPTIONAL_HEADER_64*)&out_image->nt_headers_64->OptionalHeader;
        out_image->image_base = out_image->optional_header.opt_64->ImageBase;
        out_image->size_of_image = out_image->optional_header.opt_64->SizeOfImage;
        out_image->entry_point = out_image->image_base + out_image->optional_header.opt_64->AddressOfEntryPoint;
    } else {
        internal_pe_error("Unsupported optional header magic: 0x%04X", *magic);
        return MACWI_ERROR_UNSUPPORTED;
    }

    size_t section_table_size = (size_t)file_hdr->NumberOfSections * sizeof(PE_SECTION_HEADER);
    if (section_table_offset + section_table_size > size) {
        internal_pe_error("Section table extends past end of file");
        return MACWI_ERROR_INVALID_PE;
    }

    out_image->section_headers = (const PE_SECTION_HEADER*)(data + section_table_offset);

    return MACWI_SUCCESS;
}

macwi_status_t macwi_pe_validate(const PE_IMAGE* image) {
    if (!image || !image->dos_header || !image->file_header || !image->section_headers) {
        return MACWI_ERROR_INVALID_PARAM;
    }

    uint32_t SizeOfImage = image->size_of_image;
    uint32_t SectionAlignment = image->is_64bit ? image->optional_header.opt_64->SectionAlignment : image->optional_header.opt_32->SectionAlignment;
    uint32_t SizeOfHeaders = image->is_64bit ? image->optional_header.opt_64->SizeOfHeaders : image->optional_header.opt_32->SizeOfHeaders;

    if (SectionAlignment > 0 && (SizeOfImage % SectionAlignment) != 0) {
        internal_pe_error("SizeOfImage is not aligned to SectionAlignment");
        return MACWI_ERROR_INVALID_PE;
    }

    if (SizeOfHeaders > SizeOfImage) {
        internal_pe_error("SizeOfHeaders exceeds SizeOfImage");
        return MACWI_ERROR_INVALID_PE;
    }

    for (uint16_t i = 0; i < image->num_sections; i++) {
        const PE_SECTION_HEADER* sec = &image->section_headers[i];
        uint32_t section_end = sec->VirtualAddress + sec->VirtualSize;
        if (section_end < sec->VirtualAddress) return MACWI_ERROR_INVALID_PE;
        if (sec->VirtualAddress < SizeOfHeaders && sec->VirtualSize > 0) return MACWI_ERROR_INVALID_PE;
    }

    return MACWI_SUCCESS;
}
