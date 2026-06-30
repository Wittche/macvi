#include "macwi/emu.h"
#include <FEXCore/Core/FEXLibrary.h>
#include <FEXCore/HLE/SyscallHandler.h>
#include <FEXCore/Utils/TypeDefines.h>
#include "macwi/thunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <FEXCore/Core/CoreState.h>

struct EMU_CONTEXT {
    FEX_Context* fex_ctx;
    FEX_Thread*  fex_thread;
    FEXCore::Core::CpuStateFrame* current_frame;
};

class MacWISyscallHandler : public FEXCore::HLE::SyscallHandler {
public:
    MacWISyscallHandler(EMU_CONTEXT* ctx) : m_ctx(ctx) {
        // We use OS_LINUX32 so FEXCore properly parses int 0x80 syscalls 
        // and passes the Syscall Number (EAX) into Args->Argument[0].
        OSABI = FEXCore::HLE::SyscallOSABI::OS_LINUX32;
    }
    
    virtual ~MacWISyscallHandler() = default;

    uint64_t HandleSyscall(FEXCore::Core::CpuStateFrame* Frame, FEXCore::HLE::SyscallArguments* Args) override {
        m_ctx->current_frame = Frame;
        
        // EAX contains the API index
        uint32_t api_index = (uint32_t)Args->Argument[0];
        
        // Dispatch to the native host implementation
        macwi_thunk_handle_syscall(m_ctx, api_index);
        
        // The return value is typically already set in EAX by the host callback.
        uint64_t eax = Frame->State.gregs[0];
        
        m_ctx->current_frame = nullptr;
        return eax;
    }

    FEXCore::HLE::ExecutableRangeInfo QueryGuestExecutableRange(FEXCore::Core::InternalThreadState* Thread, uint64_t Address) override {
        return {0, UINT64_MAX, true};
    }

    std::optional<FEXCore::ExecutableFileSectionInfo> LookupExecutableFileSection(FEXCore::Core::InternalThreadState* Thread, uint64_t GuestAddr) override {
        return std::nullopt;
    }

private:
    EMU_CONTEXT* m_ctx;
};

#include <FEXCore/Config/Config.h>

extern "C" {

macwi_status_t macwi_emu_init(EMU_CONTEXT** out_ctx) {
    if (!out_ctx) return MACWI_ERROR_INVALID_PARAM;

    FEXCore::Config::Initialize();
    
    // 2. SMC & JIT Cache Koruması: Track and invalidate JIT code on self-modification
    // FEXCore::Config::Set(FEXCore::Config::CONFIG_SMCCHECKS, "full");
    
    // 3. Apple Silicon TSO: Enable x86 strict memory ordering
    // FEXCore::Config::Set(FEXCore::Config::CONFIG_TSOENABLED, "1");

    int init_flags = FEX_INIT_ENABLE_JIT;
    if (FEX_Initialize(init_flags) != FEX_SUCCESS) {
        fprintf(stderr, "[macwi:emu] FEX_Initialize failed\n");
        return MACWI_ERROR_MEMORY;
    }

    FEX_Context* fex_ctx = FEX_ContextCreate(init_flags);
    if (!fex_ctx) {
        fprintf(stderr, "[macwi:emu] FEX_ContextCreate failed\n");
        return MACWI_ERROR_MEMORY;
    }

    EMU_CONTEXT* ctx = (EMU_CONTEXT*)calloc(1, sizeof(EMU_CONTEXT));
    ctx->fex_ctx = fex_ctx;
    
    // Register our Syscall Handler for API Dispatch
    MacWISyscallHandler* handler = new MacWISyscallHandler(ctx);
    FEX_SetSyscallHandler(fex_ctx, handler);

    ctx->fex_thread = FEX_ThreadCreate(fex_ctx, 0, 0);

    *out_ctx = ctx;
    return MACWI_SUCCESS;
}

void macwi_emu_free(EMU_CONTEXT* ctx) {
    if (!ctx) return;
    // FEXCore destruction is unstable in embedded mode
    // if (ctx->fex_thread) FEX_ThreadDestroy(ctx->fex_thread);
    // if (ctx->fex_ctx) FEX_ContextDestroy(ctx->fex_ctx);
    free(ctx);
}

macwi_status_t macwi_emu_map_memory(EMU_CONTEXT* ctx, uint64_t address, size_t size, int perms, uint64_t* out_address) {
    if (!ctx || !ctx->fex_ctx) return MACWI_ERROR_INVALID_PARAM;
    int fex_perms = 0;
    if (perms & MACWI_PROT_READ)  fex_perms |= FEX_MEM_READ;
    if (perms & MACWI_PROT_WRITE) fex_perms |= FEX_MEM_WRITE;
    if (perms & MACWI_PROT_EXEC)  fex_perms |= FEX_MEM_EXEC;

    uint64_t mapped = FEX_MapMemory(ctx->fex_ctx, address, size, fex_perms);

    if (mapped == 0) {
        fprintf(stderr, "[macwi:emu] FEX_MapMemory failed at 0x%llX\n", address);
        return MACWI_ERROR_MEMORY;
    }
    if (out_address) *out_address = mapped;
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_unmap_memory(EMU_CONTEXT* ctx, uint64_t address, size_t size) {
    if (!ctx || !ctx->fex_ctx) return MACWI_ERROR_INVALID_PARAM;
    if (FEX_UnmapMemory(ctx->fex_ctx, address, size) != FEX_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_write_memory(EMU_CONTEXT* ctx, uint64_t address, const void* data, size_t size) {
    if (!ctx || !ctx->fex_ctx) return MACWI_ERROR_INVALID_PARAM;
    if (FEX_WriteMemory(ctx->fex_ctx, address, data, size) != FEX_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_read_memory(EMU_CONTEXT* ctx, uint64_t address, void* out_data, size_t size) {
    if (!ctx || !ctx->fex_ctx) return MACWI_ERROR_INVALID_PARAM;
    if (FEX_ReadMemory(ctx->fex_ctx, address, out_data, size) != FEX_SUCCESS) {
        return MACWI_ERROR_MEMORY;
    }
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_init_windows_env(EMU_CONTEXT* ctx, uint64_t image_base, int argc, char** argv) {
    if (!ctx || !ctx->fex_ctx || !ctx->fex_thread) return MACWI_ERROR_INVALID_PARAM;
    if (FEX_InitWindowsEnvironment(ctx->fex_ctx, ctx->fex_thread, image_base, argc, argv) != 0) {
        return MACWI_ERROR_IO;
    }
    return MACWI_SUCCESS;
}

typedef struct EmuNewThreadArgs {
    EMU_CONTEXT* parent_ctx;
    FEX_Context* fex_ctx;
    FEX_Thread* fex_thread;
    uint64_t entry_point;
    uint64_t param;
    uint64_t stack_base;
    uint64_t stack_size;
} EmuNewThreadArgs;

static void* emu_new_thread_func(void* arg) {
    EmuNewThreadArgs* args = (EmuNewThreadArgs*)arg;
    
    // Set SP
    uint64_t rsp = args->stack_base + args->stack_size - 0x1000;
    
    // stdcall: Push parameter, push dummy return address
    uint32_t param32 = (uint32_t)args->param;
    uint32_t ret32 = (uint32_t)macwi_thunk_get_trampoline(args->parent_ctx, "kernel32.dll", "ExitThread");
    rsp -= 4;
    FEX_WriteMemory(args->fex_ctx, rsp, &param32, 4);
    rsp -= 4;
    FEX_WriteMemory(args->fex_ctx, rsp, &ret32, 4);
    
    // Set Registers
    FEX_ThreadSetReg(args->fex_thread, "rsp", rsp);
    FEX_ThreadSetReg(args->fex_thread, "rip", args->entry_point);
    
    // Execute
    FEX_ThreadSetTLSBase(args->fex_thread, 0x40000000); // Share main TEB for now
    
    FEX_ThreadExecute(args->fex_thread);
    
    // Cleanup
    FEX_UnmapMemory(args->fex_ctx, args->stack_base, args->stack_size);
    FEX_ThreadDestroy(args->fex_thread);
    free(args);
    return NULL;
}

macwi_status_t macwi_emu_create_thread(EMU_CONTEXT* ctx, uint64_t entry_point, uint64_t param, uint64_t stack_size, uint64_t* out_thread_id) {
    if (!ctx || !ctx->fex_ctx) return MACWI_ERROR_INVALID_PARAM;
    if (stack_size == 0) stack_size = 1024 * 1024; // 1MB default
    
    // Allocate stack
    static uint64_t next_thread_stack = 0x71000000;
    uint64_t stack_base = next_thread_stack;
    next_thread_stack += stack_size + 0x10000; // Plus guard page space
    
    if (FEX_MapMemory(ctx->fex_ctx, stack_base, stack_size, FEX_MEM_READ | FEX_MEM_WRITE) == 0) {
        return MACWI_ERROR_MEMORY;
    }
    
    FEX_Thread* new_fex_thread = FEX_ThreadCreate(ctx->fex_ctx, 0, 0);
    if (!new_fex_thread) {
        FEX_UnmapMemory(ctx->fex_ctx, stack_base, stack_size);
        return MACWI_ERROR_MEMORY;
    }
    
    EmuNewThreadArgs* args = (EmuNewThreadArgs*)malloc(sizeof(EmuNewThreadArgs));
    args->parent_ctx = ctx;
    args->fex_ctx = ctx->fex_ctx;
    args->fex_thread = new_fex_thread;
    args->entry_point = entry_point;
    args->param = param;
    args->stack_base = stack_base;
    args->stack_size = stack_size;
    
    pthread_t pt;
    if (pthread_create(&pt, NULL, emu_new_thread_func, args) != 0) {
        FEX_UnmapMemory(ctx->fex_ctx, stack_base, stack_size);
        FEX_ThreadDestroy(new_fex_thread);
        free(args);
        return MACWI_ERROR_IO;
    }
    
    if (out_thread_id) *out_thread_id = (uint64_t)pt;
    fprintf(stderr, "[macwi:emu] Created thread with tid=%p\n", (void*)pt);
    return MACWI_SUCCESS;
}

static const char* get_reg_name_32(int reg_id) {
    switch (reg_id) {
        case 0: return "rax"; // FEX Library mostly uses 64-bit names even for 32-bit queries if we cast it
        case 1: return "rcx";
        case 2: return "rdx";
        case 3: return "rbx";
        case 4: return "rsp";
        case 5: return "rbp";
        case 6: return "rsi";
        case 7: return "rdi";
        default: return NULL;
    }
}

static const char* get_reg_name_64(int reg_id) {
    switch (reg_id) {
        case 0: return "rax";
        case 1: return "rcx";
        case 2: return "rdx";
        case 3: return "rbx";
        case 4: return "rsp";
        case 5: return "rbp";
        case 6: return "rsi";
        case 7: return "rdi";
        case 8: return "r8";
        case 9: return "r9";
        case 10: return "r10";
        case 11: return "r11";
        case 12: return "r12";
        case 13: return "r13";
        case 14: return "r14";
        case 15: return "r15";
        default: return NULL;
    }
}

uint32_t macwi_emu_reg_read_32(EMU_CONTEXT* ctx, int reg_id) {
    if (!ctx || !ctx->fex_thread) return 0;
    if (ctx->current_frame && reg_id >= 0 && reg_id < 16) {
        return (uint32_t)ctx->current_frame->State.gregs[reg_id];
    }
    const char* name = get_reg_name_32(reg_id);
    if (!name) return 0;
    uint64_t val = 0;
    FEX_ThreadGetReg(ctx->fex_thread, name, &val);
    return (uint32_t)val;
}

void macwi_emu_reg_write_32(EMU_CONTEXT* ctx, int reg_id, uint32_t value) {
    if (!ctx || !ctx->fex_thread) return;
    if (ctx->current_frame && reg_id >= 0 && reg_id < 16) {
        ctx->current_frame->State.gregs[reg_id] = (uint64_t)value;
        return;
    }
    const char* name = get_reg_name_32(reg_id);
    if (!name) return;
    
    // For 32-bit writes to 64-bit registers, the upper 32-bits are zeroed in x86_64
    FEX_ThreadSetReg(ctx->fex_thread, name, (uint64_t)value);
}

uint64_t macwi_emu_reg_read_64(EMU_CONTEXT* ctx, int reg_id) {
    if (!ctx || !ctx->fex_thread) return 0;
    if (ctx->current_frame && reg_id >= 0 && reg_id < 16) {
        return ctx->current_frame->State.gregs[reg_id];
    }
    const char* name = get_reg_name_64(reg_id);
    if (!name) return 0;
    uint64_t val = 0;
    FEX_ThreadGetReg(ctx->fex_thread, name, &val);
    return val;
}

void macwi_emu_reg_write_64(EMU_CONTEXT* ctx, int reg_id, uint64_t value) {
    if (!ctx || !ctx->fex_thread) return;
    if (ctx->current_frame && reg_id >= 0 && reg_id < 16) {
        ctx->current_frame->State.gregs[reg_id] = value;
        return;
    }
    const char* name = get_reg_name_64(reg_id);
    if (!name) return;
    FEX_ThreadSetReg(ctx->fex_thread, name, value);
}

void macwi_emu_set_pc(EMU_CONTEXT* ctx, uint64_t pc) {
    if (!ctx || !ctx->fex_thread) return;
    if (ctx->current_frame) {
        ctx->current_frame->State.rip = pc;
        return;
    }
    FEX_ThreadSetReg(ctx->fex_thread, "rip", pc);
}

uint64_t macwi_emu_get_pc(EMU_CONTEXT* ctx) {
    if (!ctx || !ctx->fex_thread) return 0;
    if (ctx->current_frame) return ctx->current_frame->State.rip;
    uint64_t val = 0;
    FEX_ThreadGetReg(ctx->fex_thread, "rip", &val);
    return val;
}

void macwi_emu_set_sp(EMU_CONTEXT* ctx, uint64_t sp) {
    if (!ctx || !ctx->fex_thread) return;
    if (ctx->current_frame) {
        ctx->current_frame->State.gregs[4] = sp;
        return;
    }
    FEX_ThreadSetReg(ctx->fex_thread, "rsp", sp);
}

uint64_t macwi_emu_get_sp(EMU_CONTEXT* ctx) {
    if (!ctx || !ctx->fex_thread) return 0;
    if (ctx->current_frame) return ctx->current_frame->State.gregs[4];
    uint64_t val = 0;
    FEX_ThreadGetReg(ctx->fex_thread, "rsp", &val);
    return val;
}

uint64_t macwi_emu_get_dispatcher_loop(EMU_CONTEXT* ctx) {
    if (!ctx || !ctx->fex_thread) return 0;
    return FEX_ThreadGetDispatcherLoop(ctx->fex_thread);
}

uint64_t macwi_emu_get_global_memory_base(void) {
    return FEXCore::Utils::GlobalMemoryBase;
}

macwi_status_t macwi_emu_start(EMU_CONTEXT* ctx) {
    if (!ctx || !ctx->fex_thread) return MACWI_ERROR_INVALID_PARAM;
    if (FEX_ThreadExecute(ctx->fex_thread) != FEX_SUCCESS) {
        return MACWI_ERROR_IO;
    }
    return MACWI_SUCCESS;
}

void macwi_emu_stop(EMU_CONTEXT* ctx) {
    // FEXLibrary doesn't expose a simple ThreadStop in the C API yet.
    // Usually handled via setting an exit flag or handling a signal.
}

macwi_status_t macwi_emu_hook_unmapped(EMU_CONTEXT* ctx, macwi_emu_hook_cb cb, void* user_data) {
    // TODO: Connect this to FEX_HandleSIGBUS or FEX_SetSyscallHandler.
    // For now, FEXCore handles hooks differently. We might need a syscall handler.
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_save_state(EMU_CONTEXT* ctx, void** out_state) {
    if (!ctx || !ctx->current_frame || !out_state) return MACWI_ERROR_INVALID_PARAM;
    FEXCore::Core::CPUState* state = new FEXCore::Core::CPUState();
    memcpy(state, &ctx->current_frame->State, sizeof(FEXCore::Core::CPUState));
    *out_state = state;
    return MACWI_SUCCESS;
}

macwi_status_t macwi_emu_restore_state(EMU_CONTEXT* ctx, void* state) {
    if (!ctx || !ctx->current_frame || !state) return MACWI_ERROR_INVALID_PARAM;
    memcpy(&ctx->current_frame->State, state, sizeof(FEXCore::Core::CPUState));
    return MACWI_SUCCESS;
}

void macwi_emu_free_state(void* state) {
    if (state) delete (FEXCore::Core::CPUState*)state;
}

} // extern "C"
