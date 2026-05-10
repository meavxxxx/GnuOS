.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    movq $stack_top, %rsp
    movq %rsp, %rbp
    call kmain

halt:
    hlt
    jmp halt

