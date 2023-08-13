// Harkeerat Singh Sawhney , Mak Fazlic , Guerrero Toro Cindy

#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
void arg_stack(void ** top, char * command);
void elem_stack(void ** top, void * info, int size);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
