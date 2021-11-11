// Define the offsets into the `cothread_amd64::Wrapper` structure. These _MUST_ match to the actual
// offsets of the field in the struct.
#define COTHREAD_OFF_CONTEXT_TOP        (0x20)

#ifndef __cplusplus
////////////////////////////////////////////////////////////////////////////////////////////////////
// Define the proper symbol names, based on what compiler we have. For GNU, we do not need an extra
// leading underscore, but macOS requires it. We assume that clang is the only compiler that needs
// this treatment.
#ifdef __clang__
.global __ZN12libcommunism8internal5Amd646SwitchEPS1_S2_
.global __ZN12libcommunism8internal5Amd6411JumpToEntryEv
.global __ZN12libcommunism8internal5Amd6417EntryReturnedStubEv
#else
.global _ZN12libcommunism8internal5Amd646SwitchEPS1_S2_
.global _ZN12libcommunism8internal5Amd6411JumpToEntryEv
.global _ZN12libcommunism8internal5Amd6417EntryReturnedStubEv
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Performs a context switch between two cothreads. This is the System V implementation
// Arguments, in order:
// - from (%rdi): Pointer to the wrapper of the cothread we're switching from
// -   to (%rsi): Pointer to the wrapper of the cothread we're switching to
//
// We need to ensure that the registers RBP, RBX, and R12-R15 are saved as these are callee-saved
// under the System V ABI.
// void libcommunism::internal::Amd64::Switch(Cothread *from, Cothread *to)
.balign 0x40
#ifdef __clang__
__ZN12libcommunism8internal5Amd646SwitchEPS1_S2_:
#else
_ZN12libcommunism8internal5Amd646SwitchEPS1_S2_:
#endif
    // save current state to stack and record the stack frame
    push        %rbp
    push        %rbx
    push        %r12
    push        %r13
    push        %r14
    push        %r15
    mov         %rsp, COTHREAD_OFF_CONTEXT_TOP(%rdi)

    // restore the previous cothread's state
    mov         COTHREAD_OFF_CONTEXT_TOP(%rsi), %rsp
    pop         %r15
    pop         %r14
    pop         %r13
    pop         %r12
    pop         %rbx
    pop         %rbp

    ret


////////////////////////////////////////////////////////////////////////////////////////////////////
// Invokes a cothread's main function.
//
// This pulls the arguments off the stack, since that's the only real nice way we can pass
// information to this function (the registers aren't necessarily initialized here)
// void libcommunism::internal::Amd64::JumpToEntry(void)
#ifdef __clang__
__ZN12libcommunism8internal5Amd6411JumpToEntryEv:
#else
_ZN12libcommunism8internal5Amd6411JumpToEntryEv:
#endif
    pop         %rax
    pop         %rdi
    jmp         *%rax


////////////////////////////////////////////////////////////////////////////////////////////////////
// Method invoked when a cothread's main returns. It fixes stack alignment and stack frame before
// we invoke the library handler.
//
// void libcommunism::internal::Amd64::EntryReturnedStub(void)
#ifdef __clang__
__ZN12libcommunism8internal5Amd6417EntryReturnedStubEv:
#else
_ZN12libcommunism8internal5Amd6417EntryReturnedStubEv:
#endif
    subq        $8, %rsp
    push        %rbp
    mov         %rsp, %rbp

    // libcommunism::internal::Amd64::CothreadReturned()
#ifdef __clang__
    call        __ZN12libcommunism8internal5Amd6416CothreadReturnedEv
#else
    call        _ZN12libcommunism8internal5Amd6416CothreadReturnedEv
#endif

#endif
