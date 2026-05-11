.section .text
.code64

.global x86_64_syscall_entry
.type x86_64_syscall_entry, @function

.extern g_x86_64_syscall_kernel_stack_top
.extern g_x86_64_syscall_user_rsp_shadow
.extern syscall_dispatch_fastpath

/*
 * Minimal x86_64 syscall fast path entry.
 * The frame passed to C matches syscall_fastpath_frame_t.
 */
x86_64_syscall_entry:
    movq %rsp, g_x86_64_syscall_user_rsp_shadow(%rip)
    movq g_x86_64_syscall_kernel_stack_top(%rip), %rsp

    /* Save return context for SYSRET. */
    pushq %rcx
    pushq %r11

    /* Save registers expected to be preserved by the syscall ABI. */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %r8
    pushq %r9
    pushq %r10

    /* Reserve syscall_fastpath_frame_t (10 * 8 bytes). */
    subq $80, %rsp

    movq %rax, 0(%rsp)
    movq %rdi, 8(%rsp)
    movq %rsi, 16(%rsp)
    movq %rdx, 24(%rsp)
    movq %r10, 32(%rsp)
    movq %r8, 40(%rsp)
    movq %r9, 48(%rsp)
    movq %rcx, 56(%rsp)
    movq %r11, 64(%rsp)
    movq g_x86_64_syscall_user_rsp_shadow(%rip), %rax
    movq %rax, 72(%rsp)

    movq %rsp, %rdi
    call syscall_dispatch_fastpath

    addq $80, %rsp

    popq %r10
    popq %r9
    popq %r8
    popq %rdx
    popq %rsi
    popq %rdi
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    popq %r11
    popq %rcx

    movq g_x86_64_syscall_user_rsp_shadow(%rip), %rsp
    sysretq

.size x86_64_syscall_entry, .-x86_64_syscall_entry

.section .note.GNU-stack, "", @progbits
