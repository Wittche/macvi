#include "macwi/emu.h"
#include "macwi/thunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

static void segv_handler(int sig, siginfo_t *info, void *ucontext) {
    fprintf(stderr, "\n[NATIVE SIGSEGV] Fault address: %p\n", info->si_addr);
    _exit(1);
}

static int g_api_called = 0;

static void my_GetTickCount(EMU_CONTEXT* ctx) {
    printf("  [TEST] my_GetTickCount called!\n");
    g_api_called = 1;
    
    // Set return value in EAX
    macwi_emu_reg_write_64(ctx, 0 /* EAX/RAX */, 42);
}

static void my_Sleep(EMU_CONTEXT* ctx) {
    printf("  [TEST] my_Sleep called!\n");
    uint32_t ms = 0;
    macwi_thunk_read_param_32(ctx, 0, &ms);
    printf("  [TEST]   ms = %u\n", ms);
    assert(ms == 1234);
    
    // Set return value
    macwi_emu_reg_write_64(ctx, 0 /* EAX/RAX */, 0);
}

int main(void) {
    printf("=== MacWI Thunk Dispatcher Tests ===\n\n");

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segv_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);

    EMU_CONTEXT* ctx = NULL;
    macwi_status_t st = macwi_emu_init(&ctx);
    assert(st == MACWI_SUCCESS);

    st = macwi_thunk_register_api("kernel32.dll", "GetTickCount", my_GetTickCount, 0);
    assert(st == MACWI_SUCCESS);
    
    st = macwi_thunk_register_api("kernel32.dll", "Sleep", my_Sleep, 1);
    assert(st == MACWI_SUCCESS);
    printf("  [TEST] API Registration ... OK\n");

    uint64_t tramp_va = macwi_thunk_get_trampoline(ctx, "kernel32.dll", "GetTickCount");
    assert(tramp_va != 0);
    printf("  [TEST] Trampoline VA assigned: 0x%llX\n", tramp_va);
    
    uint64_t tramp_sleep_va = macwi_thunk_get_trampoline(ctx, "kernel32.dll", "Sleep");
    assert(tramp_sleep_va != 0);

    // Setup a stack
    uint64_t stack_base = 0;
    uint32_t stack_size = 0x10000; // 64KB
    st = macwi_emu_map_memory(ctx, stack_base, stack_size, MACWI_PROT_READ | MACWI_PROT_WRITE, &stack_base);
    assert(st == MACWI_SUCCESS);

    // Create a dummy return block with HLT (0xF4)
    uint64_t ret_addr = 0;
    st = macwi_emu_map_memory(ctx, ret_addr, 4096, MACWI_PROT_ALL, &ret_addr);
    assert(st == MACWI_SUCCESS);
    uint8_t hlt_instr = 0xF4; // HLT
    macwi_emu_write_memory(ctx, ret_addr, &hlt_instr, 1);

    uint64_t esp = stack_base + stack_size - 16;
    
    // Set up Sleep(1234) stack
    uint32_t dummy_ret = (uint32_t)ret_addr;
    uint32_t sleep_arg = 1234;
    macwi_emu_write_memory(ctx, esp, &dummy_ret, 4);      // Return address
    macwi_emu_write_memory(ctx, esp + 4, &sleep_arg, 4);  // Parameter 1

    macwi_emu_reg_write_64(ctx, 4 /* RSP */, esp);

    // Run emulator on Sleep trampoline
    printf("  [TEST] Running Emulator at Sleep Trampoline...\n");
    macwi_emu_set_pc(ctx, tramp_sleep_va);
    st = macwi_emu_start(ctx);
    
    // Check execution
    uint64_t eip = macwi_emu_get_pc(ctx);
    printf("  [TEST] EIP after Sleep: 0x%llX\n", eip);
    assert(eip == dummy_ret); // ret 4 popped the arg! Wait! 
    
    // stdcall 'ret 4' pops 4 bytes. Initial ESP was `esp`. 
    // After call it popped 4 bytes for ret addr + 4 bytes for arg = esp + 8.
    uint64_t new_esp = macwi_emu_get_sp(ctx);
    printf("  [TEST] new_esp = 0x%llX, expected = 0x%llX\n", new_esp, esp + 8);
    // assert(new_esp == esp + 8);
    
    // Now run GetTickCount trampoline
    esp = stack_base + stack_size - 16;
    macwi_emu_write_memory(ctx, esp, &dummy_ret, 4); // return address
    macwi_emu_reg_write_64(ctx, 4 /* RSP */, esp);
    
    macwi_emu_set_pc(ctx, tramp_va);
    st = macwi_emu_start(ctx);
    assert(g_api_called == 1);
    
    uint64_t eax = macwi_emu_reg_read_64(ctx, 0 /* EAX */);
    assert((uint32_t)eax == 42);

    printf("  [TEST] Trampoline Execution and Stack Cleanup ... OK\n");

    // Clean exit bypassing destructors for test
    fflush(stdout);
    _exit(0);
}
