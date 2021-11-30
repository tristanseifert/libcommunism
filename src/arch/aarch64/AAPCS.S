/// Offset into the Cothread class to get to the "current" stack pointer value.
#define COTHREAD_OFF_CONTEXT_TOP        (0x20)

#ifndef __cplusplus
///////////////////////////////////////////////////////////////////////////////////////////////////
// Define the exported symbol names.
#if defined(__clang__) && defined(__APPLE__)
.global __ZN12libcommunism8internal7Aarch646SwitchEPS1_S2_
.global _Aarch64AapcsEntryStub
#else
.global _ZN12libcommunism8internal7Aarch646SwitchEPS1_S2_
.global Aarch64AapcsEntryStub
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// Performs a context switch between two threads
//
// Arguments, in order:
// - from (r0): Pointer to the wrapper of the cothread we're switching from
// -   to (r1): Pointer to the wrapper of the cothread we're switching to
//
// Instead of trying to build a nice pretty stack frame, we just yeet these registers at the top
// of the stack memory region allocated for the stack, rather than trying to be smart about it
// and building a stack frame. This is stored in the top 0x100 bytes of the allocated stack and is
// reserved for this purpose.
//
// We have to save the callee saved registers, which includes the integer registers r19..r29, and
// of course the stack pointer. Additionally, r30 (LR, the return address) is saved so we can
// properly return to it.
//
// Also, the floating point registers v8..v15 must be saved; but of this, only the low 64 bits of
// each of these values has to be preserved.
//
// void libcommunism::internal::Aarch64::Switch(Cothread *from, Cothread *to)
.balign 0x40
#if defined(__clang__) && defined(__APPLE__)
__ZN12libcommunism8internal7Aarch646SwitchEPS1_S2_:
#else
_ZN12libcommunism8internal7Aarch646SwitchEPS1_S2_:
#endif
    // dereference the entry offset address
    ldr         x0, [x0, COTHREAD_OFF_CONTEXT_TOP]
    ldr         x1, [x1, COTHREAD_OFF_CONTEXT_TOP]

    // current stack ptr and return address
    mov         x16, sp
    stp         x16,  lr, [x0, 0x00]
    ldp         x16,  lr, [x1, 0x00]
    mov          sp, x16

    // general purpose registers
    stp         x19, x20, [x0, 0x10]
    ldp         x19, x20, [x1, 0x10]
    stp         x21, x22, [x0, 0x20]
    ldp         x21, x22, [x1, 0x20]
    stp         x23, x24, [x0, 0x30]
    ldp         x23, x24, [x1, 0x30]
    stp         x25, x26, [x0, 0x40]
    ldp         x25, x26, [x1, 0x40]
    stp         x27, x28, [x0, 0x50]
    ldp         x27, x28, [x1, 0x50]
    str         x29, [x0, 0x60]
    ldr         x29, [x1, 0x60]

    // floating point registers
    stp          d8,  d9, [x0, 0x70]
    ldp          d8,  d9, [x1, 0x70]
    stp         d10, d11, [x0, 0x80]
    ldp         d10, d11, [x1, 0x80]
    stp         d12, d13, [x0, 0x90]
    ldp         d12, d13, [x1, 0x90]
    stp         d14, d15, [x0, 0xA0]
    ldp         d14, d15, [x1, 0xA0]

    // and return back to the caller (we restored lr earlier)
    br          lr

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Translates the argument from register x19 into the first argument register, then jumps to the
/// proper entry thread entry point.
///
/// extern "C" void Aarch64AapcsEntryStub()
#if defined(__clang__) && defined(__APPLE__)
_Aarch64AapcsEntryStub:
#else
Aarch64AapcsEntryStub:
#endif
    mov         x0, x19
#if defined(__clang__) && defined(__APPLE__)
    bl  __ZN12libcommunism8internal7Aarch6419DereferenceCallInfoEPNS1_8CallInfoE
#else
    bl  _ZN12libcommunism8internal7Aarch6419DereferenceCallInfoEPNS1_8CallInfoE
#endif

#endif
