.section .text
.code64

.global _start
.type _start, @function

.extern main
.weak _exit
.weak __gnuos_store_startup

_start:
    xor %ebp, %ebp
    movq (%rsp), %rdi
    leaq 8(%rsp), %rsi
    leaq 16(%rsp, %rdi, 8), %rdx
    movq %rdi, %r12
    movq %rsi, %r13
    movq %rdx, %r14
    andq $-16, %rsp

    leaq __gnuos_store_startup(%rip), %rax
    testq %rax, %rax
    jz .Lstart_main
    movq %r12, %rdi
    movq %r13, %rsi
    movq %r14, %rdx
    call *%rax

.Lstart_main:
    movq %r12, %rdi
    movq %r13, %rsi
    movq %r14, %rdx
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
