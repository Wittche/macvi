// SPDX-License-Identifier: MIT
/*
$info$
tags: backend|arm64
$end_info$
*/

#include "Interface/Context/Context.h"
#include "Interface/Core/Dispatcher/Dispatcher.h"
#include "Interface/Core/JIT/JITClass.h"
#include <FEXCore/Utils/TypeDefines.h>

namespace FEXCore::CPU {

// Helper macro to apply GlobalMemoryBase shifting for atomic operations on macOS.
// HostReg: The newly declared register variable name that will hold the final host address (type ARMEmitter::Register).
// GuestReg: The register containing the guest address (untouched after this macro).
// TmpReg: A scratch register used for loading the constant if needed.
#ifdef __APPLE__
#define APPLY_ATOMIC_MEM_SHIFT(HostReg, GuestReg, TmpReg) \
  ARMEmitter::Register HostReg = TmpReg; \
  do { \
    if (FEXCore::Utils::GlobalMemoryBase != 0) { \
      ARMEmitter::Register _ShiftTmp = TmpReg; \
      if (GuestReg == TmpReg) { \
        _ShiftTmp = TMP4; \
        HostReg = TMP4; \
      } \
      ARMEmitter::ForwardLabel _ZeroLabel; \
      mov(ARMEmitter::Size::i64Bit, HostReg, GuestReg); \
      (void)cbz(ARMEmitter::Size::i64Bit, GuestReg, &_ZeroLabel); \
      LoadConstant(ARMEmitter::Size::i64Bit, _ShiftTmp, FEXCore::Utils::GlobalMemoryBase); \
      add(ARMEmitter::Size::i64Bit, HostReg, GuestReg, _ShiftTmp); \
      (void)Bind(&_ZeroLabel); \
    } else { \
      mov(ARMEmitter::Size::i64Bit, HostReg, GuestReg); \
    } \
  } while (0)
#else
#define APPLY_ATOMIC_MEM_SHIFT(HostReg, GuestReg, TmpReg) \
  ARMEmitter::Register HostReg = GuestReg
#endif


DEF_OP(CASPair) {
  auto Op = IROp->C<IR::IROp_CASPair>();
  LOGMAN_THROW_A_FMT(IROp->ElementSize == IR::OpSize::i32Bit || IROp->ElementSize == IR::OpSize::i64Bit, "Wrong element size");
  // Size is the size of each pair element
  auto Dst0 = GetReg(Op->OutLo);
  auto Dst1 = GetReg(Op->OutHi);
  auto Expected0 = GetReg(Op->ExpectedLo);
  auto Expected1 = GetReg(Op->ExpectedHi);
  auto Desired0 = GetReg(Op->DesiredLo);
  auto Desired1 = GetReg(Op->DesiredHi);
  auto MemSrc = GetReg(Op->Addr);

  const auto EmitSize = IROp->ElementSize == IR::OpSize::i64Bit ? ARMEmitter::Size::i64Bit : ARMEmitter::Size::i32Bit;

  if (CTX->HostFeatures.SupportsAtomics) {
#ifdef __APPLE__
    ARMEmitter::Register HostMemSrc = MemSrc;
    if (FEXCore::Utils::GlobalMemoryBase != 0) {
      // Use TMP1 to hold the shifted address. TMP1 is safe here as it's before any Desired/CaspalDst setup.
      ARMEmitter::ForwardLabel _ZeroLabel;
      mov(ARMEmitter::Size::i64Bit, TMP1, MemSrc);
      (void)cbz(ARMEmitter::Size::i64Bit, MemSrc, &_ZeroLabel);
      LoadConstant(ARMEmitter::Size::i64Bit, TMP1, FEXCore::Utils::GlobalMemoryBase);
      add(ARMEmitter::Size::i64Bit, TMP1, MemSrc, TMP1);
      (void)Bind(&_ZeroLabel);
      HostMemSrc = TMP1;
    }
#else
    const auto HostMemSrc = MemSrc;
#endif

    // RA has heuristics to try to pair sources, but we need to handle the cases
    // where they fail. We do so by moving to temporaries. Note we use 64-bit
    // moves here even for 32-bit cmpxchg, for the Firestorm register renamer.
    if (Desired1.Idx() != (Desired0.Idx() + 1) || Desired0.Idx() & 1) {
      // Use TMP1/TMP2 for desired pairs. 
      // If HostMemSrc is TMP1, we need to be careful.
#ifdef __APPLE__
      if (HostMemSrc == TMP1) {
        // Shift HostMemSrc to TMP4 (which is free here)
        mov(ARMEmitter::Size::i64Bit, TMP4, TMP1);
        HostMemSrc = TMP4;
      }
#endif
      mov(ARMEmitter::Size::i64Bit, TMP1, Desired0);
      mov(ARMEmitter::Size::i64Bit, TMP2, Desired1);
      Desired0 = TMP1;
      Desired1 = TMP2;
    }

    auto CaspalDst0 = Dst0;
    auto CaspalDst1 = Dst1;
    if (CaspalDst1.Idx() != (CaspalDst0.Idx() + 1) || CaspalDst0.Idx() & 1) {
      CaspalDst0 = TMP3;
      CaspalDst1 = TMP4;
    }

    // We can't clobber the source, these moves are inherently required due to
    // ISA limitations. But by making them 64-bit, Firestorm can rename.
    mov(ARMEmitter::Size::i64Bit, CaspalDst0, Expected0);
    mov(ARMEmitter::Size::i64Bit, CaspalDst1, Expected1);

    caspal(EmitSize, CaspalDst0, CaspalDst1, Desired0, Desired1, HostMemSrc);

    if (CaspalDst0 != Dst0) {
      mov(ARMEmitter::Size::i64Bit, Dst0, CaspalDst0);
      mov(ARMEmitter::Size::i64Bit, Dst1, CaspalDst1);
    }
  } else {
    mrs(TMP1, ARMEmitter::SystemRegister::NZCV);

    APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP4);

    ARMEmitter::BackwardLabel LoopTop;
    ARMEmitter::ForwardLabel LoopNotExpected;
    ARMEmitter::ForwardLabel LoopExpected;
    (void)Bind(&LoopTop);

    ldaxp(EmitSize, TMP2, TMP3, HostMemSrc);
    cmp(EmitSize, TMP2, Expected0);
    ccmp(EmitSize, TMP3, Expected1, ARMEmitter::StatusFlags::None, ARMEmitter::Condition::CC_EQ);
    (void)b(ARMEmitter::Condition::CC_NE, &LoopNotExpected);
    stlxp(EmitSize, TMP2, Desired0, Desired1, HostMemSrc);
    (void)cbnz(EmitSize, TMP2, &LoopTop);
    mov(EmitSize, Dst0, Expected0);
    mov(EmitSize, Dst1, Expected1);

    (void)b(&LoopExpected);

    (void)Bind(&LoopNotExpected);
    mov(EmitSize, Dst0, TMP2.R());
    mov(EmitSize, Dst1, TMP3.R());
    clrex();
    (void)Bind(&LoopExpected);

    msr(ARMEmitter::SystemRegister::NZCV, TMP1);
  }
}

DEF_OP(AtomicSwap) {
  auto Op = IROp->C<IR::IROp_AtomicSwap>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    ldswpal(SubEmitSize, Src, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    stlxr(SubEmitSize, TMP3, Src, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchAdd) {
  auto Op = IROp->C<IR::IROp_AtomicFetchAdd>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    ldaddal(SubEmitSize, Src, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    add(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchSub) {
  auto Op = IROp->C<IR::IROp_AtomicFetchSub>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    neg(EmitSize, TMP2, Src);
    ldaddal(SubEmitSize, TMP2, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    sub(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchAnd) {
  auto Op = IROp->C<IR::IROp_AtomicFetchAnd>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    mvn(EmitSize, TMP2, Src);
    ldclral(SubEmitSize, TMP2, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    and_(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchCLR) {
  auto Op = IROp->C<IR::IROp_AtomicFetchCLR>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    ldclral(SubEmitSize, Src, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    bic(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchOr) {
  auto Op = IROp->C<IR::IROp_AtomicFetchOr>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    ldsetal(SubEmitSize, Src, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    orr(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchXor) {
  auto Op = IROp->C<IR::IROp_AtomicFetchXor>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  auto Src = GetReg(Op->Value);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    ldeoral(SubEmitSize, Src, GetReg(Node), HostMemSrc);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    eor(EmitSize, TMP3, TMP2, Src);
    stlxr(SubEmitSize, TMP3, TMP3, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(AtomicFetchNeg) {
  auto Op = IROp->C<IR::IROp_AtomicFetchNeg>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto MemSrc = GetReg(Op->Addr);
  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    // Use a CAS loop to avoid needing to emulate unaligned LLSC atomics
    ldr(SubEmitSize, TMP2, HostMemSrc);
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    mov(EmitSize, TMP4, TMP2);
    neg(EmitSize, TMP3, TMP2);
    casal(SubEmitSize, TMP2, TMP3, HostMemSrc);
    sub(EmitSize, TMP3, TMP2, TMP4);
    (void)cbnz(EmitSize, TMP3, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    neg(EmitSize, TMP3, TMP2);
    stlxr(SubEmitSize, TMP4, TMP3, HostMemSrc);
    (void)cbnz(EmitSize, TMP4, &LoopTop);
    mov(EmitSize, GetReg(Node), TMP2.R());
  }
}

DEF_OP(CAS) {
  auto Op = IROp->C<IR::IROp_CAS>();
  const auto EmitSize = ConvertSize(IROp);
  const auto SubEmitSize = ConvertSubRegSize8(IROp->Size);

  auto Dst = GetReg(Node);
  auto Expected = GetReg(Op->Expected);
  auto Desired = GetReg(Op->Desired);
  auto MemSrc = GetReg(Op->Addr);

  APPLY_ATOMIC_MEM_SHIFT(HostMemSrc, MemSrc, TMP1);

  if (CTX->HostFeatures.SupportsAtomics) {
    if (Expected == Dst && Dst != HostMemSrc && Dst != Desired) {
      casal(SubEmitSize, Dst, Desired, HostMemSrc);
    } else {
      mov(EmitSize, TMP2, Expected);
      casal(SubEmitSize, TMP2, Desired, HostMemSrc);
      mov(EmitSize, Dst, TMP2.R());
    }
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(SubEmitSize, TMP2, HostMemSrc);
    cmp(EmitSize, TMP2, Expected);
    ARMEmitter::ForwardLabel NotExpected;
    (void)b(ARMEmitter::Condition::CC_NE, &NotExpected);
    stlxr(SubEmitSize, TMP3, Desired, HostMemSrc);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);

    (void)Bind(&NotExpected);
    clrex();
    mov(EmitSize, Dst, TMP2.R());
  }
}

DEF_OP(TelemetrySetValue) {
#ifndef FEX_DISABLE_TELEMETRY
  auto Op = IROp->C<IR::IROp_TelemetrySetValue>();
  auto Src = GetReg(Op->Value);

  ldr(TMP2, STATE_PTR_IDX(CpuStateFrame, Pointers.TelemetryValueAddresses, Op->TelemetryValueIndex));

  // Cortex fuses cmp+cset.
  cmp(ARMEmitter::Size::i32Bit, Src, 0);
  cset(ARMEmitter::Size::i32Bit, TMP1, ARMEmitter::Condition::CC_NE);

  if (CTX->HostFeatures.SupportsAtomics) {
    stsetl(ARMEmitter::SubRegSize::i64Bit, TMP1, TMP2);
  } else {
    ARMEmitter::BackwardLabel LoopTop;
    (void)Bind(&LoopTop);
    ldaxr(ARMEmitter::SubRegSize::i64Bit, TMP3, TMP2);
    orr(ARMEmitter::Size::i32Bit, TMP3, TMP3, Src);
    stlxr(ARMEmitter::SubRegSize::i64Bit, TMP3, TMP3, TMP2);
    (void)cbnz(ARMEmitter::Size::i32Bit, TMP3, &LoopTop);
  }
#endif
}

} // namespace FEXCore::CPU
