.code

?CothreadReturned@Amd64@internal@libcommunism@@SAXXZ    proto

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This performs a context switch between two cothreads.
;
; - from (RCX)
; -   to (RDX)
;
; void libcommunism::internal::Amd64::Switch(Cothread *from, Cothread *to)
?Switch@Amd64@internal@libcommunism@@SAXPEAVCothread@3@0@Z PROC
    ; save integer state
    push    RBP
    push    RBX
    push    RDI
    push    RSI
    push    R12
    push    R13
    push    R14
    push    R15

    ; save SSE state (TODO: look at using MOVAPS instead of MOVDQU?)
    movdqu  [rsp-010h], XMM6
    movdqu  [rsp-020h], XMM7
    movdqu  [rsp-030h], XMM8
    movdqu  [rsp-040h], XMM9 
    movdqu  [rsp-050h], XMM10
    movdqu  [rsp-060h], XMM11
    movdqu  [rsp-070h], XMM12
    movdqu  [rsp-080h], XMM13
    movdqu  [rsp-090h], XMM14
    movdqu  [rsp-0A0h], XMM15
    sub     RSP, 0A0h

    ; save stack
    mov     [RCX+20h], RSP

    ; restore stack
    mov     RSP, [RDX+20h]

    ; restore SSE state
    add     RSP, 0A0h
    movdqu  XMM15, [rsp-0A0h]
    movdqu  XMM14, [rsp-090h]
    movdqu  XMM13, [rsp-080h]
    movdqu  XMM12, [rsp-070h]
    movdqu  XMM11, [rsp-060h]
    movdqu  XMM10, [rsp-050h]
    movdqu  XMM9, [rsp-040h]
    movdqu  XMM8, [rsp-030h]
    movdqu  XMM7, [rsp-020h]
    movdqu  XMM6, [rsp-010h]

    ; restore integer state
    pop     R15
    pop     R14
    pop     R13
    pop     R12
    pop     RSI
    pop     RDI
    pop     RBX
    pop     RBP

    ; return to caller
    ret

?Switch@Amd64@internal@libcommunism@@SAXPEAVCothread@3@0@Z ENDP

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Pops two fields off the stack (for the entry point and its context) then jumps to that
; function.
;
; Because Windows is cool I guess we have to reserve 4 words (32 bytes) on the stack for arguments
; that we don't have.
;
; void libcommunism::internal::Amd64::JumpToEntry(void)
?JumpToEntry@Amd64@internal@libcommunism@@SAXXZ PROC
    pop     RAX
    pop     RCX
    jmp     RAX
?JumpToEntry@Amd64@internal@libcommunism@@SAXXZ ENDP
    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Fixes the stack frame and calls the C++ routine indicating that we've exited a cothread's main
; routine.
;
; void libcommunism::internal::Amd64::EntryReturnedStub(void)
?EntryReturnedStub@Amd64@internal@libcommunism@@SAXXZ PROC
    sub     RSP, 8h
    push    RBP
    mov     RBP, RSP
    
    sub     RSP, 20h
    call    ?CothreadReturned@Amd64@internal@libcommunism@@SAXXZ
?EntryReturnedStub@Amd64@internal@libcommunism@@SAXXZ ENDP

end
