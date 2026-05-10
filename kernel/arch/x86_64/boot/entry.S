.set CR0_PE, 0x00000001
.set CR0_PG, 0x80000000
.set CR4_PAE, 0x00000020
.set EFER_MSR, 0xC0000080
.set EFER_LME, 0x00000100
.set PAGE_PRESENT, 0x00000001
.set PAGE_WRITABLE, 0x00000002
.set PAGE_PAGE_SIZE, 0x00000080

.section .bss
.align 4
boot_magic32:
    .long 0
boot_info32:
    .long 0

.align 4096
pml4_table:
    .skip 4096
pdpt_table:
    .skip 4096
pd_table:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .rodata
.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

.section .text
.code32
.global _start
.type _start, @function
_start:
    cli
    movl %eax, boot_magic32
    movl %ebx, boot_info32
    movl $stack_top, %esp

    call setup_page_tables

    movl %cr4, %eax
    orl $CR4_PAE, %eax
    movl %eax, %cr4

    movl $EFER_MSR, %ecx
    rdmsr
    orl $EFER_LME, %eax
    wrmsr

    movl $pml4_table, %eax
    movl %eax, %cr3

    movl %cr0, %eax
    orl $(CR0_PE | CR0_PG), %eax
    movl %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $long_mode_entry

setup_page_tables:
    movl $pml4_table, %edi
    xorl %eax, %eax
    movl $(4096 * 3 / 4), %ecx
    rep stosl

    movl $pdpt_table, %eax
    orl $(PAGE_PRESENT | PAGE_WRITABLE), %eax
    movl %eax, pml4_table
    movl $0, pml4_table + 4

    movl $pd_table, %eax
    orl $(PAGE_PRESENT | PAGE_WRITABLE), %eax
    movl %eax, pdpt_table
    movl $0, pdpt_table + 4

    xorl %ebx, %ebx
    xorl %ecx, %ecx
1:
    movl %ebx, %eax
    orl $(PAGE_PRESENT | PAGE_WRITABLE | PAGE_PAGE_SIZE), %eax
    movl %eax, pd_table(, %ecx, 8)
    movl $0, pd_table + 4(, %ecx, 8)
    addl $0x200000, %ebx
    incl %ecx
    cmpl $512, %ecx
    jne 1b

    ret

.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    movq $stack_top, %rsp
    movq %rsp, %rbp

    movl boot_magic32(%rip), %edi
    movl boot_info32(%rip), %esi
    call kmain

halt:
    cli
    hlt
    jmp halt

.section .note.GNU-stack, "", @progbits
