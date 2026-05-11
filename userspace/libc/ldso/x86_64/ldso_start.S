.section .text
.code64

.global _start
.type _start, @function

/*
 * Stage-0 dynamic loader entry.
 * Future work: parse auxiliary vector and relocate program image.
 */
_start:
    pause
    jmp _start

.size _start, .-_start

.section .note.GNU-stack, "", @progbits
