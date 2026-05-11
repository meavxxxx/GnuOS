.section .text
.code64

.global _start
.type _start, @function

.extern ldso_stage0_bootstrap

/*
 * Stage-0 dynamic loader entry.
 * Future work: map PT_LOAD, relocate image, transfer control to entrypoint.
 */
_start:
    movq %rsp, %rdi
    call ldso_stage0_bootstrap

.Lhang:
    pause
    jmp .Lhang

.size _start, .-_start

.section .note.GNU-stack, "", @progbits
