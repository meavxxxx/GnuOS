.section .text
.code64

.global _start
.type _start, @function

.extern main
.weak _exit

_start:
    xor %ebp, %ebp
    movq (%rsp), %rdi
    leaq 8(%rsp), %rsi
    leaq 16(%rsp, %rdi, 8), %rdx
    andq $-16, %rsp

    call main

    movl %eax, %edi
    leaq _exit(%rip), %rax
    testq %rax, %rax
    jz .Lhang
    call *%rax

.Lhang:
    pause
    jmp .Lhang

.size _start, .-_start

.section .note.GNU-stack, "", @progbits
