;; --*- nasm -*-
section .text

;; size_t OPENSSL_ia32_rdseed_bytes(unsigned char *buf, size_t len);
;; size_t OPENSSL_ia32_rdrand_bytes(unsigned char *buf, size_t len);
        
global OPENSSL_ia32_rdseed_bytes

OPENSSL_ia32_rdseed_bytes:
        push rcx
        push rbx

        mov rbx, rsi
        mov rcx, rsi
.l1:
        rdseed rax
        mov [rdi], al
        inc rdi
        
        loop .l1

        mov rax, rbx
        
        pop rbx
        pop rcx
        ret
        
global OPENSSL_ia32_rdrand_bytes
        
OPENSSL_ia32_rdrand_bytes:
        push rcx
        push rbx

        mov rbx, rsi
        mov rcx, rsi
.l1:
        rdrand rax
        mov [rdi], al
        inc rdi
        
        loop .l1

        mov rax, rbx
        
        pop rbx
        pop rcx
        ret
