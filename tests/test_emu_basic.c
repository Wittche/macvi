/**
 * @file test_emu_basic.c
 * @brief Basic test for the x86 emulation engine.
 */

#include "macwi/emu.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Minimal x86 code:
 * mov eax, 0x1234
 * add eax, 0x1000
 * ret
 */
static const uint8_t X86_CODE[] = {
    0xB8, 0x34, 0x12, 0x00, 0x00, // mov eax, 0x1234
    0x05, 0x00, 0x10, 0x00, 0x00, // add eax, 0x1000
    0xC3                          // ret
};

int main(void) {
    printf("=== MacWI Basic Emulation Test ===\n\n");

    EMU_CONTEXT* ctx = NULL;
    macwi_status_t status = macwi_emu_init(&ctx);
    assert(status == MACWI_SUCCESS && ctx != NULL);
    printf("  [TEST] emu_init ... OK\n");

    /* Map 4KB for code at 0x1000000 */
    uint32_t code_addr = 0x1000000;
    status = macwi_emu_map_memory(ctx, code_addr, 4096, MACWI_PROT_ALL);
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] emu_map_memory ... OK\n");

    /* Write code */
    status = macwi_emu_write_memory(ctx, code_addr, X86_CODE, sizeof(X86_CODE));
    assert(status == MACWI_SUCCESS);
    printf("  [TEST] emu_write_memory ... OK\n");

    /* Map 4KB for stack at 0x2000000 */
    uint32_t stack_addr = 0x2000000;
    status = macwi_emu_map_memory(ctx, stack_addr, 4096, MACWI_PROT_ALL);
    assert(status == MACWI_SUCCESS);
    uint32_t stack_top = stack_addr + 4096 - 4;

    /* Write a return address of 0 so ret instruction faults and stops emulation,
     * or we just let it execute since uc_emu_start handles reaching unmapped memory */
    uint32_t dummy_ret = 0x0;
    macwi_emu_write_memory(ctx, stack_top, &dummy_ret, sizeof(dummy_ret));

    /* Start emulation */
    status = macwi_emu_start(ctx, code_addr, stack_top);
    // Unicorn returns error when jumping to unmapped memory (0x0), which is fine for our hacky return
    printf("  [TEST] emu_start ... OK (finished/faulted at RET)\n");

    /* Verify EAX = 0x2234 */
    uint32_t eax_val = 0;
    status = macwi_emu_reg_read(ctx, 0 /* EAX */, &eax_val);
    assert(status == MACWI_SUCCESS);
    assert(eax_val == 0x2234);
    printf("  [TEST] reg_read (EAX == 0x2234) ... OK\n");

    macwi_emu_free(ctx);
    printf("\n=== All basic emulation tests passed! ===\n");
    return 0;
}
