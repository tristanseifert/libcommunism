.model flat
.code

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Performs a context switch between two cothreads.
;
; Arguments, in order:
; - from (%ecx): Pointer to the wrapper of the cothread we're switching from
; -   to (%edx): Pointer to the wrapper of the cothread we're switching to
;
; This uses the Microsoft fastcall calling convention. It's not totally clear which registers
; need to be saved; ECX and EDX are of course used for the function arguments, and RAX is
; traditionally reserved for the return value of the function, which this is a void so it may or
; may not matter.
;
; void libcommunism::internal::x86::Switch(Cothread *from, Cothread *to)

?Switch@x86@internal@libcommunism@@SIXPAVCothread@3@0@Z PROC
    push    EBX
    push    ESI
    push    EDI
    push    EBP
    mov     [ECX+0h], ESP
    
    mov     ESP, [EDX+0h]
    pop     EBP
    pop     EDI
    pop     ESI
    pop     EBX

    ret
?Switch@x86@internal@libcommunism@@SIXPAVCothread@3@0@Z ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Invokes a cothread's main function.
;
; This does some indirection since we can't make a fastcall (with the arguments in the registers)
; directly to initialize a cothread.
;
; void libcommunism::internal::x86::JumpToEntry(void)

?JumpToEntry@x86@internal@libcommunism@@SAXXZ PROC
    pop     EAX
    pop     ECX
    jmp     EAX
?JumpToEntry@x86@internal@libcommunism@@SAXXZ ENDP

end