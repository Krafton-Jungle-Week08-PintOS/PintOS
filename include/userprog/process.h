#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* for user stack */
void setup_user_stack(struct intr_frame *if_, char **arg_value, int arg_count);
int parsing_arg(char *file_name, char **arg_value);
#endif /* userprog/process.h */
