#include "threads/loader.h"

.text
.globl syscall_entry
.type syscall_entry, @function
syscall_entry:
	movq %rbx, temp1(%rip)
	movq %r12, temp2(%rip)     /* callee saved registers */
	movq %rsp, %rbx            /* Store userland rsp    */
	movabs $tss, %r12		// 해당 커널 프로세스의 TSS의 주소를 받아온다. (작업 상태 세그먼트)
	movq (%r12), %r12
	movq 4(%r12), %rsp         /* Read ring0 rsp from the tss */ // TSS에 저장되어 있던 RSP0(커널 스택 시작주소)의 주소를 RSP로 옮긴다.
	/* Now we are in the kernel stack */ // OS(intr_enrty)가 커널 스택에 또 추가적인 레지스터 값을 담아준다.
	push $(SEL_UDSEG)      /* if->ss, 유저 스택 메모리 공간 주소 */
	push %rbx              /* if->rsp, RBX에 돌아가야 할 유저 메모리의 RSP값을 저장 */
	push %r11              /* if->eflags */
	push $(SEL_UCSEG)      /* if->cs, 유저 코드 세그먼트 메모리 시작 주소 */
	push %rcx              /* if->rip, RCX에 돌아가야 할 다음 PC (RIP 값)값을 저장 */
	subq $16, %rsp         /* skip error_code, vec_no */
	push $(SEL_UDSEG)      /* if->ds */
	push $(SEL_UDSEG)      /* if->es */
	push %rax
	movq temp1(%rip), %rbx
	push %rbx
	pushq $0
	push %rdx
	push %rbp
	push %rdi
	push %rsi
	push %r8
	push %r9
	push %r10
	pushq $0 /* skip r11 */
	movq temp2(%rip), %r12
	push %r12
	push %r13
	push %r14
	push %r15
	movq %rsp, %rdi

check_intr:
	btsq $9, %r11          /* Check whether we recover the interrupt */
	jnb no_sti
	sti                    /* restore interrupt */
no_sti:
	movabs $syscall_handler, %r12
	call *%r12
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rsi
	popq %rdi
	popq %rbp
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
	addq $32, %rsp
	popq %rcx              /* if->rip */
	addq $8, %rsp
	popq %r11              /* if->eflags */
	popq %rsp              /* if->rsp */
	sysretq

.section .data
.globl temp1
temp1:
.quad	0
.globl temp2
temp2:
.quad	0
