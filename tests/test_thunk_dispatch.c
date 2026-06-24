#include "macwi/emu.h"
#include "macwi/thunk.h"
#include <stdio.h>
#include <assert.h>

static int g_api_called = 0;

static void my_GetTickCount(EMU_CONTEXT* ctx) {
    printf("  [TEST] my_GetTickCount called!\n");
    g_api_called = 1;
    
    // Set return value
    macwi_emu_reg_write(ctx, 0 /* EAX */, 42);
}

int main(void) {
    printf("=== MacWI Thunk Dispatcher Tests ===\n\n");

    EMU_CONTEXT* ctx = NULL;
    macwi_status_t st = macwi_emu_init(&ctx);
    assert(st == MACWI_SUCCESS);

    st = macwi_thunk_init_dispatcher(ctx);
    assert(st == MACWI_SUCCESS);
    printf("  [TEST] Dispatcher Init ... OK\n");

    st = macwi_thunk_register_api("kernel32.dll", "GetTickCount", my_GetTickCount, 0);
    assert(st == MACWI_SUCCESS);
    printf("  [TEST] API Registration ... OK\n");

    uint32_t magic_va = macwi_thunk_get_magic_va("kernel32.dll", "GetTickCount");
    assert(magic_va != 0);
    printf("  [TEST] Magic VA assigned: 0x%08X\n", magic_va);

    // Setup a dummy stack
    uint32_t stack_base = 0x80000000;
    uint32_t stack_size = 0x10000; // 64KB
    st = macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_READ | MACWI_PROT_WRITE);
    assert(st == MACWI_SUCCESS);

    uint32_t esp = stack_base + stack_size - 4;
    uint32_t dummy_ret = 0xDEADBEEF;
    macwi_emu_write_memory(ctx, esp, &dummy_ret, 4);

    macwi_emu_reg_write(ctx, 7 /* ESP */, esp);

    // Run emulator. It will hit the magic VA, run our callback, and jump to 0xDEADBEEF
    // which will cause a memory fetch error.
    printf("  [TEST] Running Emulator at Magic VA...\n");
    st = macwi_emu_start(ctx, magic_va, esp);
    
    // We expect it to fail at 0xDEADBEEF
    assert(g_api_called == 1);
    
    uint32_t eax = 0;
    macwi_emu_reg_read(ctx, 0 /* EAX */, &eax);
    assert(eax == 42);

    uint32_t eip = 0;
    macwi_emu_reg_read(ctx, 8 /* EIP */, &eip);
    assert(eip == dummy_ret);

    printf("  [TEST] Hook Execution and Stack Cleanup ... OK\n");

    macwi_emu_free(ctx);

    printf("\n=== All thunk tests passed! ===\n");
    return 0;
}
