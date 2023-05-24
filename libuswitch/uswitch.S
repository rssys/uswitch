#include <sys/syscall.h>
#include <asm-generic/mman-common.h>

#ifdef USWITCH_DISABLE_MPK
#define wrpkru
#endif

#define USWITCH_CLEAR_FPU
#ifdef USWITCH_CLEAR_FPU
#define zeroregs vzeroall
#else
#define zeroregs
#endif

#define offset_state_pkru 88
#define offset_state_signal_stack 80
#define offset_rostate_has_init 16

.section uswitch_trusted_code,"ax",@progbits
.global context_switch_start
context_switch_start:
    leaq .Lcontext_switch_start_resume(%rip), %rax
    zeroregs
    movq %rax, (%rdi)
    movq %rsp, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %rbx, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %r13, 40(%rdi)
    movq %r14, 48(%rdi)
    movq %r15, 56(%rdi)
    movq 56(%rsi), %r15
    movq 48(%rsi), %r14
    movq 40(%rsi), %r13
    movq 32(%rsi), %r12
    movq 24(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 8(%rsi), %r11
    movq (%rsi), %r10
    movq %rdx, %rdi
    movq %rcx, %rsi
    xorl %ecx, %ecx
    xorl %edx, %edx
    rdfsbase %r8
    addq $uswitch_rostate_buf@tpoff+0xfff, %r8
    andq $0xfffffffffffff000, %r8
    movl (%r8), %eax
    movl %eax, %fs:uswitch_state@tpoff+offset_state_pkru
    movq %r11, %rsp
    wrpkru
    rdfsbase %r9
    addq $uswitch_rostate_buf@tpoff+0xfff, %r9
    andq $0xfffffffffffff000, %r9
    movl (%r9), %edx
    cmp %eax, %edx
    je .Lcontext_switch_start_cont
    movl $__NR_exit, %eax
    xorl %edi, %edi
    syscall
.Lcontext_switch_start_cont:
    jmp *%r10
.Lcontext_switch_start_resume:
    retq

.section uswitch_trusted_code,"ax",@progbits
.global context_switch_resume
context_switch_resume:
    leaq .Lcontext_switch_resume_resume(%rip), %rax
    zeroregs
    movq %rax, (%rdi)
    movq %rsp, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %rbx, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %r13, 40(%rdi)
    movq %r14, 48(%rdi)
    movq %r15, 56(%rdi)
    movq 56(%rsi), %r15
    movq 48(%rsi), %r14
    movq 40(%rsi), %r13
    movq 32(%rsi), %r12
    movq 24(%rsi), %rbx
    movq 16(%rsi), %rbp
    movq 8(%rsi), %r11
    movq (%rsi), %r10
    movq %rdx, %rdi
    movq %rcx, %rsi
    xorl %ecx, %ecx
    xorl %edx, %edx
    rdfsbase %r8
    addq $uswitch_rostate_buf@tpoff+0xfff, %r8
    andq $0xfffffffffffff000, %r8
    movl (%r8), %eax
    movl %eax, %fs:uswitch_state@tpoff+offset_state_pkru
    movq %r11, %rsp
    wrpkru
    rdfsbase %r9
    addq $uswitch_rostate_buf@tpoff+0xfff, %r9
    andq $0xfffffffffffff000, %r9
    movl (%r9), %edx
    cmp %eax, %edx
    je .Lcontext_switch_resume_cont
    movl $__NR_exit, %eax
    xorl %edi, %edi
    syscall
.Lcontext_switch_resume_cont:
    jmp *%r10
.Lcontext_switch_resume_resume:
    retq

.section uswitch_trusted_code,"ax",@progbits
.global uswitch_ret
uswitch_ret:
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %eax, %eax
    wrpkru
    rdfsbase %rax
    movq %rax, %rsi
    addq $uswitch_rostate_buf@tpoff+0xfff, %rax
    andq $0xfffffffffffff000, %rax
    movl $0, 4(%rax)
    addq $uswitch_state@tpoff, %rsi
    movq (%rsi), %rax
    movl $0, (%rax)
    movq 64(%rsi), %r15
    movq 56(%rsi), %r14
    movq 48(%rsi), %r13
    movq 40(%rsi), %r12
    movq 32(%rsi), %rbx
    movq 24(%rsi), %rbp
    movq 16(%rsi), %rsp
    movl $0, %fs:uswitch_state@tpoff+offset_state_pkru
    movq 8(%rsi), %r10
    movl $1, %eax
    jmp *%r10

.section uswitch_trusted_code,"ax",@progbits
.global uswitch_yield
uswitch_yield:
    xorl %ecx, %ecx
    xorl %edx, %edx
    xorl %eax, %eax
    wrpkru
    rdfsbase %rdi
    movq %rdi, %rsi
    addq $uswitch_rostate_buf@tpoff+0xfff, %rdi
    andq $0xfffffffffffff000, %rdi
    movl $0, 4(%rdi)
    addq $uswitch_state@tpoff, %rsi
    movq 8(%rdi), %rdi
    movq (%rsi), %rax
    movl $0, (%rax)
    leaq .Luswitch_yield_resume(%rip), %rax
    movq %rax, (%rdi)
    movq %rsp, 8(%rdi)
    movq %rbp, 16(%rdi)
    movq %rbx, 24(%rdi)
    movq %r12, 32(%rdi)
    movq %r13, 40(%rdi)
    movq %r14, 48(%rdi)
    movq %r15, 56(%rdi)
    movq 64(%rsi), %r15
    movq 56(%rsi), %r14
    movq 48(%rsi), %r13
    movq 40(%rsi), %r12
    movq 32(%rsi), %rbx
    movq 24(%rsi), %rbp
    movq 16(%rsi), %rsp
    movl $0, %fs:uswitch_state@tpoff+offset_state_pkru
    movq 8(%rsi), %r10
    xor %eax, %eax
    jmp *%r10
.Luswitch_yield_resume:
    xor %eax, %eax
    ret

.section uswitch_trusted_code,"ax",@progbits
.global ld_so_xrstor_then_verify
ld_so_xrstor_then_verify:
    movl $0xee, %eax
    xor %edx, %edx
    xrstor 0x48(%rsp)
    cmpl $0xee, %eax
    je .Lld_so_xrstor_then_verify_cont
    movl $__NR_exit, %eax
    xorl %edi, %edi
    syscall
.Lld_so_xrstor_then_verify_cont:
    ret

.section uswitch_trusted_code,"ax",@progbits
.global uswitch_reset_pkru
uswitch_reset_pkru:
    movzbl uswitch_prstate(%rip), %eax
    testl %eax, %eax
    jnz .Luswitch_reset_pkru_cont
    testl %edi, %edi
    jnz .Luswitch_reset_pkru_cont
    notl %eax
    ret
.Luswitch_reset_pkru_cont:
    xorl %eax, %eax
    xorl %edx, %edx
    xorl %ecx, %ecx
    wrpkru
    rdfsbase %rdx
    addq $uswitch_rostate_buf@tpoff+0xfff, %rdx
    andq $0xfffffffffffff000, %rdx
    movl (%rdx), %edx
    cmp %eax, %edx
    je .Luswitch_reset_pkru_cont2
    movl $__NR_exit, %eax
    xorl %edi, %edi
    syscall
.Luswitch_reset_pkru_cont2:
    xorl %eax, %eax
    ret

.section uswitch_trusted_code,"ax",@progbits
.global uswitch_signal_handler_trampoline
uswitch_signal_handler_trampoline:
    movl %fs:uswitch_state@tpoff+offset_state_pkru, %ebx
    movl $0, %fs:uswitch_state@tpoff+offset_state_pkru
    movq %rdx, %r8
    xorl %eax, %eax
    xorl %edx, %edx
    xorl %ecx, %ecx
    wrpkru
    movl %fs:uswitch_state@tpoff+offset_state_pkru, %edx
    cmpl %eax, %edx
    je .Luswitch_signal_handler_trampoline_cont
    movl $__NR_exit, %eax
    xorl %edi, %edi
    syscall
.Luswitch_signal_handler_trampoline_cont:
    rdfsbase %rdx
    addq $uswitch_rostate_buf@tpoff+0xfff, %rdx
    andq $0xfffffffffffff000, %rdx
    movl offset_rostate_has_init(%rdx), %edx
    testl %edx, %edx
    jz .Luswitch_signal_handler_trampoline_noinit
    movq %rsp, %rdx
    movq %fs:uswitch_state@tpoff+offset_state_signal_stack, %rsp
    andq $0xfffffffffffffff0, %rsp
    movl %ebx, %fs:uswitch_state@tpoff+offset_state_pkru
    call uswitch_signal_handler
    movq %rax, %rsp
    movl $__NR_rt_sigreturn, %eax
    syscall
.Luswitch_signal_handler_trampoline_noinit:
    movq %r8, %rdx
    jmp uswitch_signal_handler_noinit

.text
.global rewrite_glibc_wrpkru
rewrite_glibc_wrpkru:
    movl $__NR_mprotect, %eax
    movq %rdx, %r8
    movl $(PROT_READ | PROT_WRITE), %edx
    syscall
    testl %eax, %eax
    jnz .Lrewrite_glibc_wrpkru_fail
    movw $0x9090, 0(%r8)
    movb $0x90, 3(%r8)
    movl $__NR_mprotect, %eax
    movl $(PROT_READ | PROT_EXEC), %edx
    syscall
    testl %eax, %eax
    jnz .Lrewrite_glibc_wrpkru_fail
    movl $1, %eax
    ret
.Lrewrite_glibc_wrpkru_fail:
    xorl %eax, %eax
    ret

//  rewrite
//      movl $0xee, %eax
//      xorl %edx, %rdx
//      xrstor 0x40(%rsp)
//  to
//      movabsq &ld_so_xrstor_then_verify, %rax
//      call *%rax

.text
.global rewrite_ld_so_xrstor
rewrite_ld_so_xrstor:
    movl $__NR_mprotect, %eax
    movq %rdx, %r8
    movl $(PROT_READ | PROT_WRITE), %edx
    syscall
    testl %eax, %eax
    jnz .Lrewrite_ld_so_xrstor_fail
    movw $0xb848, 0(%r8)
    leaq ld_so_xrstor_then_verify(%rip), %r9
    movq %r9, 2(%r8)
    movw $0xd0ff, 10(%r8)
    movl $__NR_mprotect, %eax
    movl $(PROT_READ | PROT_EXEC), %edx
    syscall
    testl %eax, %eax
    jnz .Lrewrite_ld_so_xrstor_fail
    movl $1, %eax
    ret
.Lrewrite_ld_so_xrstor_fail:
    xorl %eax, %eax
    ret
