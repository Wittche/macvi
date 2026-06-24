/**
 * @file test_pe_parser.c
 * @brief Unit tests for the PE parser (macwi_pe_parse_headers, macwi_pe_validate).
 *
 * Creates minimal valid and invalid PE headers in memory and verifies that
 * the parser correctly accepts or rejects them.  Uses simple assert-based
 * testing — no external test framework required.
 *
 * SPDX-License-Identifier: MIT
 */

#include "macwi/pe.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Helper: build a minimal valid PE32 image in memory
 *
 * Layout:
 *   [0x0000]  DOS_HEADER (e_magic=MZ, e_lfanew=0x0080)
 *   [0x0080]  PE Signature + File Header + Optional Header
 *   [after optional header]  One PE_SECTION_HEADER (.text)
 *
 * The image is SizeOfImage = 0x2000 (8 KiB), with one .text section at
 * VirtualAddress = 0x1000, VirtualSize = 0x100.
 * ============================================================================ */

/** Total size of the synthetic buffer — must be large enough for all headers. */
#define SYNTHETIC_PE_SIZE 4096

static size_t build_minimal_pe(uint8_t* buf, size_t buf_size) {
    memset(buf, 0, buf_size);

    /* ---- DOS Header at offset 0 ----------------------------------------- */
    DOS_HEADER* dos = (DOS_HEADER*)buf;
    dos->e_magic  = DOS_MAGIC;  /* 0x5A4D */
    dos->e_lfanew = 0x0080;     /* PE headers at offset 128 */

    /* ---- NT Headers at offset 0x0080 ------------------------------------ */
    IMAGE_NT_HEADERS_32* nt = (IMAGE_NT_HEADERS_32*)(buf + 0x0080);
    nt->Signature = PE_SIGNATURE;  /* 0x00004550 */

    /* File Header */
    nt->FileHeader.Machine              = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.NumberOfSections     = 1;
    nt->FileHeader.TimeDateStamp        = 0x60000000;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(PE_OPTIONAL_HEADER_32);
    nt->FileHeader.Characteristics      = 0x0102;  /* EXECUTABLE_IMAGE | 32BIT_MACHINE */

    /* Optional Header */
    nt->OptionalHeader.Magic                = PE32_MAGIC;  /* 0x010B */
    nt->OptionalHeader.AddressOfEntryPoint  = 0x1000;      /* RVA of entry */
    nt->OptionalHeader.ImageBase            = 0x00400000;
    nt->OptionalHeader.SectionAlignment     = 0x1000;
    nt->OptionalHeader.FileAlignment        = 0x0200;
    nt->OptionalHeader.SizeOfImage          = 0x2000;      /* 8 KiB */
    nt->OptionalHeader.SizeOfHeaders        = 0x0200;
    nt->OptionalHeader.Subsystem            = 3;  /* IMAGE_SUBSYSTEM_WINDOWS_CUI */
    nt->OptionalHeader.NumberOfRvaAndSizes   = PE_MAX_DATA_DIRECTORIES;

    /* ---- Section Header (immediately after optional header) ------------- */
    size_t section_table_offset = 0x0080 + sizeof(DWORD) +
                                  sizeof(PE_FILE_HEADER) +
                                  sizeof(PE_OPTIONAL_HEADER_32);

    PE_SECTION_HEADER* sec = (PE_SECTION_HEADER*)(buf + section_table_offset);
    memcpy(sec->Name, ".text\0\0\0", PE_SECTION_NAME_SIZE);
    sec->VirtualSize     = 0x0100;
    sec->VirtualAddress  = 0x1000;
    sec->SizeOfRawData   = 0x0200;
    sec->PointerToRawData = 0x0200;
    sec->Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;

    return section_table_offset + sizeof(PE_SECTION_HEADER);
}

/* ============================================================================
 * Test cases
 * ============================================================================ */

/**
 * Test 1: Successfully parse a valid minimal PE32 image.
 */
static void test_parse_valid_pe(void) {
    printf("  [TEST] parse valid PE32 image ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    size_t used = build_minimal_pe(buf, sizeof(buf));
    (void)used;

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);

    assert(status == MACWI_SUCCESS);
    assert(image.dos_header != NULL);
    assert(image.nt_headers != NULL);
    assert(image.section_headers != NULL);
    assert(image.num_sections == 1);
    assert(image.image_base == 0x00400000);
    assert(image.entry_point == 0x00401000);  /* ImageBase + AddressOfEntryPoint */
    assert(image.nt_headers->FileHeader.Machine == IMAGE_FILE_MACHINE_I386);

    printf("OK\n");
}

/**
 * Test 2: Reject a buffer that is too small for the DOS header.
 */
static void test_reject_too_small(void) {
    printf("  [TEST] reject buffer too small ... ");

    uint8_t buf[16];
    memset(buf, 0, sizeof(buf));

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);

    assert(status == MACWI_ERROR_INVALID_PE);

    printf("OK\n");
}

/**
 * Test 3: Reject a buffer with an invalid DOS magic.
 */
static void test_reject_bad_dos_magic(void) {
    printf("  [TEST] reject bad DOS magic ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    build_minimal_pe(buf, sizeof(buf));

    /* Corrupt the DOS magic */
    buf[0] = 0x00;
    buf[1] = 0x00;

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);

    assert(status == MACWI_ERROR_INVALID_PE);

    printf("OK\n");
}

/**
 * Test 4: Reject a buffer with an invalid PE signature.
 */
static void test_reject_bad_pe_signature(void) {
    printf("  [TEST] reject bad PE signature ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    build_minimal_pe(buf, sizeof(buf));

    /* Corrupt the PE signature at offset 0x0080 */
    buf[0x0080] = 0xFF;

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);

    assert(status == MACWI_ERROR_INVALID_PE);

    printf("OK\n");
}

/**
 * Test 5: Reject a PE with an unsupported machine type (e.g., AMD64).
 */
static void test_reject_wrong_machine(void) {
    printf("  [TEST] reject non-i386 machine type ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    build_minimal_pe(buf, sizeof(buf));

    /* Change the machine type to AMD64 */
    IMAGE_NT_HEADERS_32* nt = (IMAGE_NT_HEADERS_32*)(buf + 0x0080);
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);

    assert(status == MACWI_ERROR_UNSUPPORTED);

    printf("OK\n");
}

/**
 * Test 6: Validate a correctly parsed image.
 */
static void test_validate_valid_image(void) {
    printf("  [TEST] validate valid image ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    build_minimal_pe(buf, sizeof(buf));

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);
    assert(status == MACWI_SUCCESS);

    status = macwi_pe_validate(&image);
    assert(status == MACWI_SUCCESS);

    printf("OK\n");
}

/**
 * Test 7: Reject NULL arguments.
 */
static void test_reject_null_args(void) {
    printf("  [TEST] reject NULL arguments ... ");

    PE_IMAGE image;

    assert(macwi_pe_parse_headers(NULL, 100, &image) == MACWI_ERROR_INVALID_PARAM);
    assert(macwi_pe_parse_headers((uint8_t*)"x", 1, NULL) == MACWI_ERROR_INVALID_PARAM);
    assert(macwi_pe_validate(NULL) == MACWI_ERROR_INVALID_PARAM);

    printf("OK\n");
}

/**
 * Test 8: Verify macwi_pe_get_entry_point matches the parsed value.
 */
static void test_get_entry_point(void) {
    printf("  [TEST] get_entry_point ... ");

    uint8_t buf[SYNTHETIC_PE_SIZE];
    build_minimal_pe(buf, sizeof(buf));

    PE_IMAGE image;
    macwi_status_t status = macwi_pe_parse_headers(buf, sizeof(buf), &image);
    assert(status == MACWI_SUCCESS);

    uint32_t ep = macwi_pe_get_entry_point(&image);
    assert(ep == 0x00401000);

    /* NULL image should return 0 */
    assert(macwi_pe_get_entry_point(NULL) == 0);

    printf("OK\n");
}

/* ============================================================================
 * Main — run all tests
 * ============================================================================ */

int main(void) {
    printf("\n=== MacWI PE Parser Tests ===\n\n");

    test_parse_valid_pe();
    test_reject_too_small();
    test_reject_bad_dos_magic();
    test_reject_bad_pe_signature();
    test_reject_wrong_machine();
    test_validate_valid_image();
    test_reject_null_args();
    test_get_entry_point();

    printf("\n=== All tests passed! ===\n\n");
    return 0;
}
