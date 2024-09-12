#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	/* 할 일 1. 시스템 콜 번호 받아오기 */
	/* 2. 시스템 콜 인자들을 받아온다. */
	/* 3. 알맞은 액션을 취한다 (반환은 %rax에) */

	// printf ("system call!\n");
	// thread_exit ();

	int sys_num = f->R.rax;
	switch (sys_num)
	{
	case SYS_FORK:
		// fork_handler(f);
		break;
	case SYS_CREATE:
		create_handler(f);
		break;
	case SYS_READ:
		// read_handler(f);
		break;
	case SYS_WRITE:
		write_handler(f);
		break;
	case SYS_EXIT:
		exit_handler(f->R.rdi);
		break;
	case SYS_WAIT:
		wait_handler(f);
	default:
		thread_exit();
	}

	// printf("system call!\n");
}

void check_address(void *addr)
{
	/* 포인터가 가르키는 주소가 유저영역의 주소인지 확인
	1. 유저 가상 주소를 가리키는지 2. 주소가 유효한지 3. 유저 영역 내에 있지만 페이지로 할당하지 않은 영역
	잘못된 접근일 경우 프로세스 종료*/
	struct thread *t = thread_current();
	if (!is_user_vaddr(addr) || addr == NULL || pml4_get_page(t->pml4, addr) == NULL)
	{
		exit_handler(-1);
	}
}

static void halt_handler(struct intr_frame *f)
{
	int fd;
	struct file *file;

	fd = f->R.rdi;
	power_off();
}

void exit_handler(int status)
{
	// 현재 사용자 프로그램을 종료(process_exit)하고 상태를 커널로 되돌립니다.
	// 프로세스의 부모가 s를 기다리면(아래 참조) 이것이 반환되는 상태입니다.
	// 일반적으로 상태 0은 성공을 나타내고 0이 아닌 값은 오류를 나타냅니다.
	struct thread *t = thread_current();
	printf("%s: exit(%d)\n", t->name, status);
	// printf("exit\n"); // Process Termination Message
	t->exit_status = status;
	

	thread_exit();
}

void write_handler(struct intr_frame *f)
{
	int fd;
	void *buffer;
	unsigned size;
	struct file *file;

	fd = f->R.rdi;
	buffer = f->R.rsi;
	size = f->R.rdx;

	// 만약 fd가 표준 입출력이라면, shell에 쓰기
	// (shell은 user와 os가 소통하기위한 장치, ex 터미널.)
	// 0 , 1 , 2 먼가먼가임
	// 그리고 종료

	// 표준 입출력이 아니라면
	// fd로 file 찾아 올 수 있도록.
	if (STDOUT_FILENO)
	{
		putbuf(buffer, size);
	}

	else
	{
		// file에다 쓰기
		f->R.rax = file_write(file, buffer, size);
	}
}

void wait_handler(struct intr_frame *f)
{
}

void create_handler(struct intr_frame *f)
{
	char *file = f->R.rdi;
	unsigned initial_size = f->R.rsi;
	check_address(file);
	if (filesys_create(file, initial_size))
	{
		f->R.rax= true;
	}
	else
	{
		f->R.rax = false;
	}
}

bool remove_handler(const char *file)
{
	check_address(file);
	if (filesys_remove(file))
	{
		return true;
	}
	else
	{
		return false;
	}
}

// static void read_handler(struct intr_frame *f)
// {
// 	int fd;
// 	void *buffer;
// 	unsigned size;
// 	struct file *file;

// 	fd = f->R.rdi;
// 	buffer = f->R.rsi;
// 	size = f->R.rdx;

// 	// fd로 file 을 찾아오기
// 	// 1. fd 배열에서 유효한지 확인. ( 요청받은 fd가 존재하는지)
// 	// 2. 존재한다면 file = fds[fd] 가져오기

// 	f->R.rax = file_read(file, buffer, size);
// 	// rax 는 반환 값을 저장하는 레지스터 이기때문.
// }

