#include "threads/init.h"
#include <console.h>
#include <debug.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devices/kbd.h"
#include "devices/input.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#endif
#include "tests/threads/tests.h"
#ifdef VM
#include "vm/vm.h"
#endif
#ifdef FILESYS
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif

/* Page-map-level-4 with kernel mappings only. */
uint64_t *base_pml4;

#ifdef FILESYS
/* -f: Format the file system? */
static bool format_filesys;
#endif

/* -q: Power off after kernel tasks complete? */
bool power_off_when_done;

bool thread_tests;

static void bss_init (void);
static void paging_init (uint64_t mem_end);

static char **read_command_line (void);
static char **parse_options (char **argv);
static void run_actions (char **argv);
static void usage (void);

static void print_stats (void);


int main (void) NO_RETURN;

/* Pintos main program. */
int
main (void) {
	uint64_t mem_end; // 시스템 메모리의 끝을 나타내는 64비트 변수
	char **argv; // 명렬줄(command line) 인수의 문자열 배열에 대한 포인터

	/* Clear BSS and get machine's RAM size. */
	/*
	BSS(Block started by Symbol) 영역 초기화. 시스템의 RAM 크기를 가져옴. 
	BSS는 초기화되지 않은 전역 변수를 저장하는 메모리 영역
	*/
	bss_init ();

	/* Break command line into arguments and parse options. */
	argv = read_command_line (); // 커맨드 라인을 읽고 인수로 나눔. 나눈 인수를 argv에 저장함.
	argv = parse_options (argv); // argv에 저장된 커맨드 라인 인수를 분석하여 필요한 옵션을 생성함.

	/* Initialize ourselves as a thread so we can use locks,
	   then enable console locking. */
	thread_init ();
	console_init (); // 콘솔 입출력 시스템 초기화, 콘솔의 락 활성화

	/* Initialize memory system. */
	mem_end = palloc_init ();
	malloc_init ();
	paging_init (mem_end);

#ifdef USERPROG
	tss_init (); // 컨텍스트 스위칭에 사용되는 task state segment 초기화.
	gdt_init (); // 메모리 세그먼트 관리를 위한 구조인 global descriptor table 초기화
#endif

	/* Initialize interrupt handlers. */
	intr_init (); // 인터럽트 초기화
	timer_init (); // 타이머 초기화
	kbd_init (); // 키보드 시스템 초기화
	input_init (); // 입력 장치 시스템 초기화
#ifdef USERPROG
	exception_init (); // ✅예외 처리 시스템 초기화
	syscall_init (); // ✅시스템 콜 초기화
#endif
	/* Start thread scheduler and enable interrupts. */
	thread_start (); // 스레드 스케줄러 시작하고 인터럽트 활성화
	serial_init_queue ();
	timer_calibrate ();

#ifdef FILESYS
	/* Initialize file system. */
	disk_init ();
	filesys_init (format_filesys);
#endif

#ifdef VM
	vm_init ();
#endif

	printf ("Boot complete.\n");

	/* Run actions specified on kernel command line. */
	run_actions (argv); // 커맨드 라인에서 지정된 작업을 실행

	/* Finish up. */
	if (power_off_when_done)
		power_off ();
	thread_exit ();
}

/* Clear BSS */
static void
bss_init (void) {
	/* The "BSS" is a segment that should be initialized to zeros.
	   It isn't actually stored on disk or zeroed by the kernel
	   loader, so we have to zero it ourselves.

	   The start and end of the BSS segment is recorded by the
	   linker as _start_bss and _end_bss.  See kernel.lds. */
	extern char _start_bss, _end_bss;
	memset (&_start_bss, 0, &_end_bss - &_start_bss);
}

/* Populates the page table with the kernel virtual mapping,
 * and then sets up the CPU to use the new page directory.
 * Points base_pml4 to the pml4 it creates. */
static void
paging_init (uint64_t mem_end) {
	uint64_t *pml4, *pte;
	int perm;
	pml4 = base_pml4 = palloc_get_page (PAL_ASSERT | PAL_ZERO);

	extern char start, _end_kernel_text;
	// Maps physical address [0 ~ mem_end] to
	//   [LOADER_KERN_BASE ~ LOADER_KERN_BASE + mem_end].
	for (uint64_t pa = 0; pa < mem_end; pa += PGSIZE) {
		uint64_t va = (uint64_t) ptov(pa);

		perm = PTE_P | PTE_W;
		if ((uint64_t) &start <= va && va < (uint64_t) &_end_kernel_text)
			perm &= ~PTE_W;

		if ((pte = pml4e_walk (pml4, va, 1)) != NULL)
			*pte = pa | perm;
	}

	// reload cr3
	pml4_activate(0);
}

/* Breaks the kernel command line into words and returns them as
   an argv-like array. */
/*
커널에 입력된 커맨드라인을 개별 단어로 파싱하고, 이를 argv 형식의 배열로 반환함.
*/
static char **
read_command_line (void) {
	static char *argv[LOADER_ARGS_LEN / 2 + 1]; // 명령 줄을 파싱한 인수를 저장할 포인터 배열 argv 선언.
	char *p, *end; // p: 명령줄 문자열을 가리키는 포인터, end: 명령줄의 끝을 가리킴.
	int argc; // 명령줄 인수의 개수(argument count)를 저장.
	int i;

	/*
	LOADER_ARG_CNT 메모리 위치에서 명령줄 인수의 개수를 읽어와 argc에 저장함.
	ptov 함수는 물리 주소를 가상 주소로 변환함.
	*/
	argc = *(uint32_t *) ptov (LOADER_ARG_CNT);
	p = ptov (LOADER_ARGS); // 명령줄 인수들이 저장된 메모리 위치를 가리키는 포인터 p
	end = p + LOADER_ARGS_LEN; // 명령줄 인수 영역의 끝을 가리키는 포인터
	/*
	명령줄 인수만큼 loop를 돎.
	argv 배열에 인수의 주소 위치 저장.
	다음 인수의 주소를 가리키기 위해 현재 가리키는 인수의 길이만큼 p 이동시켜 다음 인수를 가리킬 수 있도록 함.
	만약에 포인터가 영역 끝에 다다르거나 넘어가면 PANIC 호출
	*/
	for (i = 0; i < argc; i++) {
		if (p >= end)
			PANIC ("command line arguments overflow");

		argv[i] = p;
		p += strnlen (p, end - p) + 1;
	}
	argv[argc] = NULL; // 배열의 끝을 NULL로 표기하여 끝이라는 것을 표기함.

	/* Print kernel command line. */
	printf ("Kernel command line:");
	/*
	인자의 개수만큼 loop 돌면서 공백이 없으면 그대로 출력
	공백이 있으면 작은 따옴표로 감싸서 출력
	명령줄 끝나면 줄바꿈
	*/
	for (i = 0; i < argc; i++)
		if (strchr (argv[i], ' ') == NULL)
			printf (" %s", argv[i]);
		else
			printf (" '%s'", argv[i]);
	printf ("\n");

	return argv; // 파싱 끝나면 인수들이 저장된 배열 반환
}

/* Parses options in ARGV[]
   and returns the first non-option argument. */
/*
커널의 명령줄에서 옵션 파싱하고, 첫 번째 비옵션 인수를 반환함.
인수의 배열인 argv를 받아서 -로 시작하는 인수(옵션)을 처리하고 그 이후 비옵션 인수를 반환함.
*/
static char **
parse_options (char **argv) {
	/*
	argv 배열이 NULL이 아니고 argv 배열의 첫 번째가 -로 시작하는 경우에만 loop를 돎
	*/
	for (; *argv != NULL && **argv == '-'; argv++) {
		char *save_ptr; // strtok_r에서 상태를 유지하기 위한 포인터
		char *name = strtok_r (*argv, "=", &save_ptr); // 인수를 = 기준으로 나누어 name에 저장.
		char *value = strtok_r (NULL, "", &save_ptr); // = 이후의 값을 value에 저장.
		
		// 옵션이 -h면 usage() 호출하여 함수 사용법 출력함
		if (!strcmp (name, "-h"))
			usage ();
		// 옵션이 -q이면 커널이 작업 끝난 후에 시스템 종료되도록 설정
		else if (!strcmp (name, "-q"))
			power_off_when_done = true;
#ifdef FILESYS // 파일 시스템이 정의된 경우에만 아래 코드가 컴파일 됨
		// 옵션이 -f이면 파일 시스템 포맷되도록 설정
		else if (!strcmp (name, "-f"))
			format_filesys = true;
#endif
		// 옵션이 -rs이면 value를 정수로 변환한 후 랜덤 시드로 설정하여 난수 생성기를 초기화
		else if (!strcmp (name, "-rs"))
			random_init (atoi (value));
		// 옵션이 -mlfqs이면 멀티레벨 피드백 큐 스케줄링 활성화
		else if (!strcmp (name, "-mlfqs"))
			thread_mlfqs = true;
#ifdef USERPROG // 유저 프로그램 기능이 정의된 경우에 아래 코드 컴파일 됨
		/*
		옵션이 -ul이면 value를 정수로 변환하여 사용자 페이지 제한을 설정

		👉사용자 페이지 제한:
		pintos에서 사용자가 사용할 수 있는 페이지(메모리)의 최대 수를 제한하는 기능.
		프로세스가 사용할 수 있는 메모리의 양을 조절함. 그 이상의 메모리 할당을 시도하면 실패 혹은
		page fault가 발생할 수 있음.
		*/
		else if (!strcmp (name, "-ul"))
			user_page_limit = atoi (value);
		// 옵션이 -threads-tests이면 스레드 테스트 활성화
		else if (!strcmp (name, "-threads-tests"))
			thread_tests = true;
#endif
		// 인식할 수 없는 옵션 주어지면 오류 메시지 출력하고 시스템 중단
		else
			PANIC ("unknown option `%s' (use -h for help)", name);
	}

	return argv; // 이후의 프로그램 실행에서 사용하기 위해 첫 번째 비옵션 인수의 포인터 반환
}

/* Runs the task specified in ARGV[1]. */
static void
run_task (char **argv) {
	const char *task = argv[1];

	printf ("Executing '%s':\n", task);
#ifdef USERPROG
	if (thread_tests){
		run_test (task);
	} else {
		process_wait (process_create_initd (task));
	}
#else
	run_test (task);
#endif
	printf ("Execution of '%s' complete.\n", task);
}

/* Executes all of the actions specified in ARGV[]
   up to the null pointer sentinel. */
/*
argv 배열에 정의된 명령어의 인자들을 null pointer sentinel(\0)까지 모두 순차적으로 실행함.
*/
static void
run_actions (char **argv) {
	/* An action. */
	struct action {
		char *name;                       /* Action name. */
		int argc;                         /* # of args, including action name. */
		void (*function) (char **argv);   /* Function to execute action. */
	};

	/* 
	Table of supported actions.
	지원하는 모든 동작을 정의한 테이블. 
	각 동작은 이름과 함께 필요한 인수의 수, 해당 동작을 수행하는 함수로 구성됨.
	"run"이라는 이름의 동작이 주어지면 run_task 함수가 실행됨.
	*/
	static const struct action actions[] = {
		{"run", 2, run_task},
#ifdef FILESYS
		{"ls", 1, fsutil_ls},
		{"cat", 2, fsutil_cat},
		{"rm", 2, fsutil_rm},
		{"put", 2, fsutil_put},
		{"get", 2, fsutil_get},
#endif
		{NULL, 0, NULL},
	};

	// argv 배열이 NULL이 될 때까지 loop
	while (*argv != NULL) {
		const struct action *a; // action 배열을 순회하기 위한 포인터
		int i;

		/* Find action name. */
		/*
		a는 현재 동작을 가리키는 포인터
		*/
		for (a = actions; ; a++)
			if (a->name == NULL) 
				PANIC ("unknown action `%s' (use -h for help)", *argv);
			else if (!strcmp (*argv, a->name))
				break;

		/* Check for required arguments. */
		for (i = 1; i < a->argc; i++)
			if (argv[i] == NULL)
				PANIC ("action `%s' requires %d argument(s)", *argv, a->argc - 1);

		/* Invoke action and advance. */
		a->function (argv);
		argv += a->argc;
	}

}

/* Prints a kernel command line help message and powers off the
   machine. */
static void
usage (void) {
	printf ("\nCommand line syntax: [OPTION...] [ACTION...]\n"
			"Options must precede actions.\n"
			"Actions are executed in the order specified.\n"
			"\nAvailable actions:\n"
#ifdef USERPROG
			"  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
			"  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
			"  ls                 List files in the root directory.\n"
			"  cat FILE           Print FILE to the console.\n"
			"  rm FILE            Delete FILE.\n"
			"Use these actions indirectly via `pintos' -g and -p options:\n"
			"  put FILE           Put FILE into file system from scratch disk.\n"
			"  get FILE           Get FILE from file system into scratch disk.\n"
#endif
			"\nOptions:\n"
			"  -h                 Print this help message and power off.\n"
			"  -q                 Power off VM after actions or on panic.\n"
			"  -f                 Format file system disk during startup.\n"
			"  -rs=SEED           Set random number seed to SEED.\n"
			"  -mlfqs             Use multi-level feedback queue scheduler.\n"
#ifdef USERPROG
			"  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
			);
	power_off ();
}


/* Powers down the machine we're running on,
   as long as we're running on Bochs or QEMU. */
void
power_off (void) {
#ifdef FILESYS
	filesys_done ();
#endif

	print_stats ();

	printf ("Powering off...\n");
	outw (0x604, 0x2000);               /* Poweroff command for qemu */
	for (;;);
}

/* Print statistics about Pintos execution. */
static void
print_stats (void) {
	timer_print_stats ();
	thread_print_stats ();
#ifdef FILESYS
	disk_print_stats ();
#endif
	console_print_stats ();
	kbd_print_stats ();
#ifdef USERPROG
	exception_print_stats ();
#endif
}
