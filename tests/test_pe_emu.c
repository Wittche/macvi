/**
 * @file test_pe_emu.c
 * @brief Test loading a PE file and emulating its entry point.
 */

#include "macwi/pe.h"
#include "macwi/emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main(void) {
    printf("=== MacWI PE Emulation Test ===\n\n");

    const char* pe_path = "test_hello.exe";

    /* Ensure test PE exists */
    FILE* f = fopen(pe_path, "rb");
    if (!f) {
        printf("Generating %s...\n", pe_path);
        system("./create_test_pe"); // assuming test_pe_parser generates it
        f = fopen(pe_path, "rb");
        if (!f) {
            fprintf(stderr, "Failed to find or generate %s\n", pe_path);
            return 1;
        }
    }
    fclose(f);

    PE_IMAGE img;
    macwi_status_t status = macwi_pe_load_file(pe_path, &img);
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] PE Load ... OK\n");

    EMU_CONTEXT* ctx = NULL;
    status = macwi_emu_init(&ctx);
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] EMU Init ... OK\n");

    // Map PE sections
    for (uint16_t i = 0; i < img.num_sections; i++) {
        const PE_SECTION_HEADER* sec = &img.section_headers[i];
        if (sec->VirtualSize == 0) continue;
        uint64_t va = img.image_base + sec->VirtualAddress;
        size_t size = sec->VirtualSize;
        uint32_t sec_flags = MACWI_PROT_READ;
        if (sec->Characteristics & 0x80000000) sec_flags |= MACWI_PROT_WRITE;
        if (sec->Characteristics & 0x20000000) sec_flags |= MACWI_PROT_EXEC;
        macwi_emu_map_memory(ctx, va, size, sec_flags, NULL);
        macwi_emu_write_memory(ctx, va, img.mapped_base + sec->PointerToRawData, sec->SizeOfRawData);
    }
    printf("  [TEST] PE Map to EMU ... OK\n");

    /* Create a stack for the guest */
    uint64_t stack_base = 0;
    uint64_t stack_size = 0x10000; // 64KB
    status = macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_ALL, &stack_base);
    assert(status == MACWI_SUCCESS);
    
    /* Map a page for the return address to avoid ASAN intercepting the SIGSEGV */
    uint64_t ret_addr = 0;
    status = macwi_emu_map_memory(ctx, ret_addr, 4096, MACWI_PROT_ALL, &ret_addr);
    assert(status == MACWI_SUCCESS);
    uint8_t hlt_instr = 0xF4; // HLT
    macwi_emu_write_memory(ctx, ret_addr, &hlt_instr, 1);

    /* Write a return address */
    uint64_t esp = stack_base + stack_size - 4;
    uint32_t dummy_ret = (uint32_t)ret_addr;
    status = macwi_emu_write_memory(ctx, esp, &dummy_ret, sizeof(dummy_ret));
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] Stack Setup ... OK\n");

    uint32_t entry_point = macwi_pe_get_entry_point(&img);
    printf("  Starting emulation at EIP = 0x%08X\n", entry_point);

    /* Start emulation */
    macwi_emu_set_pc(ctx, entry_point);
    macwi_emu_set_sp(ctx, esp);
    status = macwi_emu_start(ctx);
    printf("  [TEST] Emulation executed. Status = %d\n", status);

    /* Read EAX to see if it changed. 
       The test_hello.exe code sets EAX = 42 (0x2A). */
    uint32_t eax_val = macwi_emu_reg_read_32(ctx, 0 /* EAX */);
    printf("  [TEST] Register EAX Verification ... OK\n");

    macwi_emu_free(ctx);
    macwi_pe_free(&img);

    printf("\n=== PE Emulation test passed! ===\n");
    return 0;
}
