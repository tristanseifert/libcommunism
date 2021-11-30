// Define the offsets into the `x86` structure. These _MUST_ match to the actual offsets of the
// field in the struct.
#define COTHREAD_OFF_CONTEXT_TOP        (0x10)

#ifndef __cplusplus
////////////////////////////////////////////////////////////////////////////////////////////////////
// Define the exported symbols.

.global _ZN12libcommunism8internal3x866SwitchEPS1_S2_
.global _ZN12libcommunism8internal3x8611JumpToEntryEv

////////////////////////////////////////////////////////////////////////////////////////////////////
// Performs a context switch between two cothreads.
//
// Arguments, in order:
// - from (%ecx): Pointer to the wrapper of the cothread we're switching from
// -   to (%edx): Pointer to the wrapper of the cothread we're switching to
//
// This uses the Microsoft fastcall calling convention. It's not totally clear which registers
// need to be saved; ECX and EDX are of course used for the function arguments, and RAX is
// traditionally reserved for the return value of the function, which this is a void so it may or
// may not matter.
//
// void libcommunism::internal::x86::Switch(x86 *from, x86 *to)
.balign 0x40
_ZN12libcommunism8internal3x866SwitchEPS1_S2_:
    // save current state to stack and record the stack frame
    push        %ebx
    push        %esi
    push        %edi
    push        %ebp
    mov         %esp, COTHREAD_OFF_CONTEXT_TOP(%ecx)

    // restore the previous cothread's state
    mov         COTHREAD_OFF_CONTEXT_TOP(%edx), %esp
    pop         %ebp
    pop         %edi
    pop         %esi
    pop         %ebx

    ret

////////////////////////////////////////////////////////////////////////////////////////////////////
// Invokes a cothread's main function.
//
// This does some indirection since we can't make a fastcall (with the arguments in the registers)
// directly to initialize a cothread.
//
// void libcommunism::internal::x86::JumpToEntry(void)
_ZN12libcommunism8internal3x8611JumpToEntryEv:
    pop         %eax
    pop         %ecx
    jmp         *%eax

#endif
