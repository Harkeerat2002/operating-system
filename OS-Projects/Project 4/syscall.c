#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (struct intr_frame *f);                          // exit
static void syscall_write (struct intr_frame *f);                         // write


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  memset(&syscall_table, 0, sizeof(syscall_table));                       // allocating memory for syscall_table

  call[SYS_EXIT] = syscall_exit;                                          // register syscall_exit
  call[SYS_WRITE] = syscall_write;                                        // register syscall_write
}

static void
syscall_handler (struct intr_frame *f UNUSED)                             
{
  // printf ("system call!\n");
  // thread_exit ();
  int syscall_num = *(int *)f->esp;                                       // get syscall number
  call[syscall_num](f);                                                   // call syscall
}

// pg 31 

// ESP is a saved stack pointer.
static void
syscall_write (struct intr_frame *f)                                      // system call for write
{
  int fd = f->esp[1];                                                     // gets the stack pointer (esp)
  char *buffer = (const char *) f->esp[2];                                // gets the buffer from the esp
  int size = f->esp[3];                                                   // gets the size from the esp
  if (fd == 1) {
    putbuf (buffer, size);
  }
  else
  {
    f->eax = size;
  }
}


static void
syscall_exit (struct intr_frame *f)                                        // System call for exit
{
  int exit_status = *(int *)f->esp;                                        // gets the exit status from the esp
  thread_current ()->exit_status = exit_status;                            // sets the current exits status for thread_current to the exit_status
  thread_exit ();                                                          // exists from the thread
}
