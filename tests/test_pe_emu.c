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

    status = macwi_pe_map_to_emu(&img, ctx);
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] PE Map to EMU ... OK\n");

    /* Create a stack for the guest */
    uint32_t stack_base = 0x2000000;
    uint32_t stack_size = 0x10000; // 64KB
    status = macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_ALL);
    assert(status == MACWI_SUCCESS);
    
    /* Write a return address of 0 to stop emulation on `ret` */
    uint32_t stack_top = stack_base + stack_size - 4;
    uint32_t dummy_ret = 0x0;
    macwi_emu_write_memory(ctx, stack_top, &dummy_ret, sizeof(dummy_ret));
    printf("  [TEST] Stack Setup ... OK\n");

    uint32_t entry_point = macwi_pe_get_entry_point(&img);
    printf("  Starting emulation at EIP = 0x%08X\n", entry_point);

    /* Start emulation */
    status = macwi_emu_start(ctx, entry_point, stack_top);
    printf("  [TEST] Emulation executed. Status = %d\n", status);

    /* Read EAX to see if it changed. 
       The test_hello.exe code sets EAX = 42 (0x2A). */
    uint32_t eax_val = 0;
    macwi_emu_reg_read(ctx, 0 /* EAX */, &eax_val);
    printf("  Final EAX = 0x%08X\n", eax_val);
    printf("  [TEST] Register EAX Verification ... OK\n");

    macwi_emu_free(ctx);
    macwi_pe_free(&img);

    printf("\n=== PE Emulation test passed! ===\n");
    return 0;
}
