;; --*- nasm -*-
section .text

global edk2_revert_stack

edk2_revert_stack:
        mov rax, [rsp]
        add rsp, 0x200-0x8
        mov rsp, [rsp]
        mov [rsp], rax
        ret
        
global edk2_switch_stack
        
edk2_switch_stack:
        add rdi, rsi
        mov rax, 0
        mov [rdi-0x8], rax        
        mov [rdi-0x10], rsp        
        mov rax, [rsp]        
        mov [rdi-0x208], rax
        lea rsp, [rdi-0x208]
        ret
