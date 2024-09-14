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

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
/* Projects 2 and later. */
typedef int pid_t;

void halt_handler 	(void) NO_RETURN;
void exit_handler 	(int status) NO_RETURN;
pid_t fork_handler	(const char *thread_name);
int exec_handler 	(const char *file);
// void wait_handler (pid_t);
void wait_handler 	(struct intr_frame *f);
void create_handler (struct intr_frame *f);
bool remove_handler	(const char *file);
void open_haddler	(struct intr_frame *f);
int filesize_handler(int fd);
int read_handler	(int fd, void *buffer, unsigned length);
void write_handler 	(struct intr_frame *f);
void seek_handelr	(int fd, unsigned position);
unsigned tell_handler(int fd);
void close_handler	(int fd);

/*  */

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	switch (f->R.rax){ 
		case SYS_HALT:
			halt_handler();
			break;
		case SYS_EXIT:
			exit_handler(f->R.rdi);
			break;
		case SYS_FORK:
		
		case SYS_EXEC:

		case SYS_WAIT:
		
		case SYS_CREATE:
			create_handler(f);
			break;
		case SYS_REMOVE:
			// f->R.rax = remove(f->R.rdi);
			break;

		case SYS_OPEN:
			open_haddler(f);
			break;
		case SYS_FILESIZE:

		case SYS_READ:

		case SYS_WRITE:
			write_handler(f);
			break;
		case SYS_SEEK:

		case SYS_TELL:

		case SYS_CLOSE:

		default:
			// exit(-1);
			thread_exit ();
			break;
	}

	/* 
	SYS_HALT,                  
	SYS_EXIT,                
	SYS_FORK,             
	SYS_EXEC,                   
	SYS_WAIT,                 
	SYS_CREATE,                
	SYS_REMOVE,                 
	SYS_OPEN,                   
	SYS_FILESIZE,               
	SYS_READ,                   
	SYS_WRITE,                  
	SYS_SEEK,                   
	SYS_TELL,                   
	SYS_CLOSE,                  
	 */
	
}
void halt_handler (void){
	power_off();
}

void exit_handler (int status){
	struct thread *cur = thread_current();
	cur->exit_status = status;
	thread_exit();
}
void
write_handler (struct intr_frame *f) {
	// putbuf(buffer, size);
	int fd			= f->R.rdi;
	void *buffer 	= f->R.rsi;
	unsigned size	= f->R.rdx;

	if(fd = 1){
		putbuf(buffer, size);
	}
	f->R.rax = size;
}

void
wait_handler(struct intr_frame *f){
	process_wait();
}

void
create_handler(struct intr_frame *f){
	char *file = f->R.rdi;
	
	if(check_file(file)!=0){
		unsigned initial_size = f->R.rsi;
		f->R.rax = filesys_create(file, initial_size);
	}
}

void
open_haddler(struct intr_frame *f){
	char *file_name = f->R.rdi;
	// filesys_init(file);
	if(check_file(file_name)!=0){
		struct file *file= filesys_open(file_name);
		if (!file){
			f->R.rax=-1;
			exit_handler(-1);
		}
		f->R.rax= handling_fd(file_name);
	}
}

struct fd_struct{
	int fd;
	struct file *file;
};

unsigned int fd_arr[128] ={0};

int
handling_fd(char *file_name){
	struct fd_struct new_fd;
	int i;
	for(i=2;i<sizeof(fd_arr);i++){
		if(fd_arr[i]==0){
			fd_arr[i]=&file_name;
			return i;
		}
	}
	return -1;
}

int
check_file(char *file){
	if(file == NULL || !is_user_vaddr(file) || pml4_get_page(thread_current()->pml4, file)==NULL){
		exit_handler(-1);
		return 0;
	}
	return 1;
}