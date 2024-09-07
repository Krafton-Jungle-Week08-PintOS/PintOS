#ifndef THREADS_INTERRUPT_H
#define THREADS_INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

// 인터럽트가 비활성화 되거나 활성화 됨을 나타냄.
/* Interrupts on or off? */
enum intr_level {
	INTR_OFF,             /* Interrupts disabled. */
	INTR_ON               /* Interrupts enabled. */
};

enum intr_level intr_get_level (void);		// 현재 인터럽트 상태(켜져 있는지 꺼져 있는지)를 알려준다.
enum intr_level intr_set_level (enum intr_level);	// 레벨에 따라 인터럽트를 켜거나 끈다. 여기서 시스템의 인터럽트 상태를 변경할 수 있다.
enum intr_level intr_enable (void);
enum intr_level intr_disable (void);

// 문맥 교환이나 인터럽트 처리 중에 CPU 레지스터 상태를 저장하거나 복원하는 데 사용
/* Interrupt stack frame. */
struct gp_registers {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rbp;
	uint64_t rdx;
	uint64_t rcx;
	uint64_t rbx;
	uint64_t rax;
} __attribute__((packed));	// 구조체의 메모리 패딩을 없애 정확한 크기로 메모리에 저장되도록 한다.

struct intr_frame {
	/* Pushed by intr_entry in intr-stubs.S.
	   These are the interrupted task's saved registers. */
	struct gp_registers R;
	uint16_t es;
	uint16_t __pad1;
	uint32_t __pad2;
	uint16_t ds;
	uint16_t __pad3;
	uint32_t __pad4;
	/* Pushed by intrNN_stub in intr-stubs.S. */
	uint64_t vec_no; /* Interrupt vector number. */
/* Sometimes pushed by the CPU,
   otherwise for consistency pushed as 0 by intrNN_stub.
   The CPU puts it just under `eip', but we move it here. */
	uint64_t error_code;
/* Pushed by the CPU.
   These are the interrupted task's saved registers. */
	uintptr_t rip;
	uint16_t cs;
	uint16_t __pad5;
	uint32_t __pad6;
	uint64_t eflags;
	uintptr_t rsp;
	uint16_t ss;
	uint16_t __pad7;
	uint32_t __pad8;
} __attribute__((packed));

typedef void intr_handler_func (struct intr_frame *);

void intr_init (void);
void intr_register_ext (uint8_t vec, intr_handler_func *, const char *name);
void intr_register_int (uint8_t vec, int dpl, enum intr_level,
                        intr_handler_func *, const char *name);
bool intr_context (void);
void intr_yield_on_return (void);

void intr_dump_frame (const struct intr_frame *);
const char *intr_name (uint8_t vec);

#endif /* threads/interrupt.h */
