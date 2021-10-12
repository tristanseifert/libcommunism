/// Offset into the Cothread class to get to the "current" stack pointer value.
#define COTHREAD_OFF_CONTEXT_TOP        (0x0)

#ifndef __cplusplus
///////////////////////////////////////////////////////////////////////////////////////////////////
// Define the exported symbol names.
//.global _ZN12libcommunism8internal3Arm6SwitchEPNS_8CothreadES3_
.global _ZN12libcommunism8internal3Arm6SwitchEPvS2_
.global ArmhfEntryStub

///////////////////////////////////////////////////////////////////////////////////////////////////
// Performs a context switch between two threads
//
// Arguments, in order:
// - from (r0): Pointer to the cothread context buffer we're switching from
// -   to (r1): Pointer to the cothread context buffer we're switching to
//
// Instead of trying to build a nice pretty stack frame, we just yeet these registers at the top
// of the stack memory region allocated for the stack, rather than trying to be smart about it
// and building a stack frame. This is stored in the top 0x100 bytes of the allocated stack and is
// reserved for this purpose.
//
// We have to save the callee saved registers, which includes the integer registers r4..r11, and
// the stack pointer and link register.
//
// void libcommunism::internal::Arm::Switch(Cothread *from, Cothread *to)
.balign 0x40
_ZN12libcommunism8internal3Arm6SwitchEPvS2_:
//_ZN12libcommunism8internal3Arm6SwitchEPNS_8CothreadES3_:
    stmia       r0!, {r4-r11,sp,lr}
    ldmia       r1!, {r4-r11,sp,pc}
    bx          lr

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Moves the argument from r4 (the first restored register) into r0, then jumps to the method
/// that actually begins execution of the thread's main.
ArmhfEntryStub:
    mov         r0, r4
    bl _ZN12libcommunism8internal3Arm19DereferenceCallInfoEPNS1_8CallInfoE

#endif
