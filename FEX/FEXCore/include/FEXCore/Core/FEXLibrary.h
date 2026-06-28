// SPDX-License-Identifier: MIT
/*
$info$
category: Library ~ FEX as a Library API for external consumers (e.g. Wine)
desc: Provides a stable C API for embedding FEX in external processes
$end_info$
*/

#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file FEXLibrary.h
 * @brief FEX as a Library - Stable C API for embedding FEX JIT in external processes like Wine.
 *
 * This API allows external applications (such as Wine's CPU translator plugin)
 * to initialize FEX, execute x86/x86_64 code, and manage the emulation context
 * without linking directly to FEXCore internals.
 */

/* Opaque handle types */
typedef struct FEX_Context FEX_Context;
typedef struct FEX_Thread   FEX_Thread;

/**
 * @brief Memory region permissions
 */
enum FEX_MemoryPerms {
  FEX_MEM_READ    = 1,
  FEX_MEM_WRITE   = 2,
  FEX_MEM_EXEC    = 4,
};

/**
 * @brief Initialization flags for FEX context
 */
enum FEX_InitFlags {
  FEX_INIT_DEFAULT      = 0,
  FEX_INIT_64BIT_MODE   = 1,  ///< Emulate x86_64 (default: x86 32-bit)
  FEX_INIT_ENABLE_JIT   = 2,  ///< Enable JIT compilation (default: interpreter only)
};

/**
 * @brief Result codes for FEX API functions
 */
typedef enum FEX_Result {
  FEX_SUCCESS       = 0,
  FEX_ERROR_GENERIC = -1,
  FEX_ERROR_INVALID_ARG = -2,
  FEX_ERROR_OUT_OF_MEMORY = -3,
  FEX_ERROR_ALREADY_INIT = -4,
  FEX_ERROR_NOT_INIT = -5,
} FEX_Result;

/**
 * @brief Initialize the FEX library globally (must be called once per process).
 *
 * @param Flags Initialization flags from FEX_InitFlags.
 * @return FEX_SUCCESS on success, error code otherwise.
 */
FEX_Result FEX_Initialize(int Flags);

/**
 * @brief Shutdown the FEX library globally.
 */
void FEX_Shutdown(void);
bool FEX_HandleSIGBUS(FEX_Thread* Thread, void* ucontext);
void FEX_SetSyscallHandler(FEX_Context* Ctx, void* Handler);

/**
 * @brief Create a new emulation context for running guest code.
 *
 * @param Flags Context flags from FEX_InitFlags.
 * @return Opaque context pointer, or NULL on failure.
 */
FEX_Context* FEX_ContextCreate(int Flags);

/**
 * @brief Destroy an emulation context.
 *
 * @param Ctx The context to destroy.
 */
void FEX_ContextDestroy(FEX_Context* Ctx);

/**
 * @brief Map memory in the guest address space.
 *
 * @param Ctx The FEX context.
 * @param GuestAddr Desired guest address (0 for any).
 * @param Size Size of the mapping.
 * @param Perms Permissions (bitmask of FEX_MemoryPerms).
 * @return Guest address of the mapping, or 0 on failure.
 */
uint64_t FEX_MapMemory(FEX_Context* Ctx, uint64_t GuestAddr, uint64_t Size, int Perms);

/**
 * @brief Unmap memory in the guest address space.
 *
 * @param Ctx The FEX context.
 * @param GuestAddr Guest address to unmap.
 * @param Size Size of the region to unmap.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_UnmapMemory(FEX_Context* Ctx, uint64_t GuestAddr, uint64_t Size);

/**
 * @brief Write data to guest memory.
 *
 * @param Ctx The FEX context.
 * @param GuestAddr Guest address to write to.
 * @param Data Pointer to host data.
 * @param Size Number of bytes to write.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_WriteMemory(FEX_Context* Ctx, uint64_t GuestAddr, const void* Data, uint64_t Size);

/**
 * @brief Read data from guest memory.
 *
 * @param Ctx The FEX context.
 * @param GuestAddr Guest address to read from.
 * @param Data Pointer to host buffer.
 * @param Size Number of bytes to read.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_ReadMemory(FEX_Context* Ctx, uint64_t GuestAddr, void* Data, uint64_t Size);

/**
 * @brief Create a new guest thread.
 *
 * @param Ctx The FEX context.
 * @param EntryPoint Guest RIP to start execution at.
 * @param StackPointer Guest RSP value.
 * @return Opaque thread handle, or NULL on failure.
 */
FEX_Thread* FEX_ThreadCreate(FEX_Context* Ctx, uint64_t EntryPoint, uint64_t StackPointer);

/**
 * @brief Destroy a guest thread.
 *
 * @param Thread The thread to destroy.
 */
void FEX_ThreadDestroy(FEX_Thread* Thread);

/**
 * @brief Execute a guest thread on the current host thread.
 *
 * This function blocks until the guest code returns or hits an exit condition.
 *
 * @param Thread The thread to execute.
 * @return FEX_SUCCESS on normal exit.
 */
FEX_Result FEX_ThreadExecute(FEX_Thread* Thread);

/**
 * @brief Set a guest register value.
 *
 * @param Thread The guest thread.
 * @param RegName Register name (e.g. "rax", "rbx", "rip", "rsp").
 * @param Value The value to set.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_ThreadSetReg(FEX_Thread* Thread, const char* RegName, uint64_t Value);

/**
 * @brief Get a guest register value.
 *
 * @param Thread The guest thread.
 * @param RegName Register name (e.g. "rax", "rbx", "rip", "rsp").
 * @param OutValue Pointer to store the register value.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_ThreadGetReg(FEX_Thread* Thread, const char* RegName, uint64_t* OutValue);

FEX_Result FEX_ThreadSetTLSBase(FEX_Thread* Thread, uint64_t Base);

/**
 * @brief Initializes the Windows-specific environment (TEB, PEB, GS segment).
 */
FEX_Result FEX_InitWindowsEnvironment(FEX_Context* Ctx, FEX_Thread* Thread, uint64_t ImageBase, int argc, char** argv);

/**
 * @brief Load an ELF binary into the guest address space.
 *
 * @param Ctx The FEX context.
 * @param Path Path to the ELF binary.
 * @param OutEntryPoint Pointer to store the entry point address.
 * @return FEX_SUCCESS on success.
 */
FEX_Result FEX_LoadELF(FEX_Context* Ctx, const char* Path, uint64_t* OutEntryPoint);

/**
 * @brief Creates a guest-side COM proxy for a host-side object.
 */
#ifdef __cplusplus
uint64_t CreateCOMProxy(FEX_Context* Ctx, void* HostObj, uint32_t InterfaceID, uint32_t MethodCount, uint64_t Extra = 0);
#else
uint64_t CreateCOMProxy(FEX_Context* Ctx, void* HostObj, uint32_t InterfaceID, uint32_t MethodCount, uint64_t Extra);
#endif

void FEX_RegisterStubAddress(uint64_t Address, const char* Name);
const char* FEX_GetStubName(uint64_t Address);

#ifdef __cplusplus
}
#endif

