// Harkeerat Singh Sawhney , Mak Fazlic , Guerrero Toro Cindy

#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

typedef void (*handler) (struct intr_frame *);
static void syscall_exit (struct intr_frame *f);
static void syscall_write (struct intr_frame *f);
typedef void (*handler) (struct intr_frame *);
static void syscall_execute (struct intr_frame *f);
static void syscall_wait (struct intr_frame *f);
typedef int pid_t; /* Process identifier. */                                        //  id of the process

#define SYSCALL_MAX_CODE 19                                                         // max number of system calls 
static handler call[SYSCALL_MAX_CODE + 1];                                          // array of system call handlers

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* Any syscall not registered here should be NULL (0) in the call array. */
  memset(call, 0, SYSCALL_MAX_CODE + 1);

  /* Check file lib/syscall-nr.h for all the syscall codes and file
   * lib/user/syscall.c for a short explanation of each system call. */
  call[SYS_EXIT]  = syscall_exit;                                                  // Terminate this process.
  call[SYS_WRITE] = syscall_write;                                                 // Write to a file.
  call[SYS_EXEC]  = syscall_execute;                                               // Execute a task.
  call[SYS_WAIT]  = syscall_wait;                                                  // Wait for a child process.

}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_code = *((int*)f->esp);
  call[syscall_code](f);
}

static void
syscall_exit (struct intr_frame *f)
{
  int *stack = f->esp;
  struct thread* t = thread_current ();
  t->exit_status = *(stack+1);
  thread_exit ();
}

static void
syscall_write (struct intr_frame *f)
{
  int *stack = f->esp;
  ASSERT (*(stack+1) == 1); // fd 1
  char * buffer = *(stack+2);
  int    length = *(stack+3);
  putbuf (buffer, length);
  f->eax = length;
}

static void syscall_wait (struct intr_frame *f) {
  int *stack = f->esp;
  process_wait(* ((int *) (stack+1)));
}

static void syscall_execute (struct intr_frame *f) {
  const char * cmd_line = (const char *) *((int *) f->esp + 1);
  uint32_t * pagedir = thread_current()->pagedir;

  if (cmd && is_user_vaddr (cmd_line) &&       // Check valid pointer and virtual address
      pagedir_get_pFage(pagedir, cmd_line)) {   // Check virtual address
    f -> eax = process_execute(cmd_line);
  } else {
    f->eax = -1;
  }
}
