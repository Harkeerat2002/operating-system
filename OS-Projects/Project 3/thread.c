// Harkeerat Singh Sawhney , Mak Fazlic , Guerrero Toro Cindy
#include "threads/thread.h" 
#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame {
    void *eip;             /* Return address. */
    thread_func *function; /* Function to call. */
    void *aux;             /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);
static FPReal average_load;                                                                // average_load

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().
   
   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);

    lock_init(&tid_lock);
    list_init(&ready_list);
    list_init(&all_list);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}
/* Modified version of the original is_less function.
/* Returns true only if the thread priority in list elements A is less
   than the thread priority in list elements A given auxiliary data AUX. */
bool under_prioritize(struct list_elem *a, struct list_elem *b, void *aux) {
    ASSERT(aux == NULL);                                                                  // aux is NULL
    struct thread *t1, *t2;                                                               // making 2 new threads t1 and t2
    bool is_least = false;                                                                // Making a boolean is_least false

    t1 = list_entry(a, struct thread, elem);                                              // t1 is the thread in the list
    t2 = list_entry(b, struct thread, elem);                                              // t2 is the thread in the list

    is_least = t1->priority < t2->priority;                                               // is_least boolean assingned, check if t1 is less than t2 (their priorities)
    return is_least;                                                                      // return is_least
}

/*
 latest_cpu() - uses FPReal to find the thread with the highest priority
 */
void latest_cpu(struct thread * t, void * aux UNUSED){
  FPReal recent_cpu = t->recent_cpu;                                                      // recent_cpu is assigned to the recent_cpu of the thread
  FPReal load_multiply_by_two = FPR_MUL_INT(average_load, 2);                             // load_multiply_by_two is assigned to the average_load * 2
  FPReal load_multiply_plus_one = FPR_ADD_INT(load_multiply_by_two, 1);                   // load_multiply_plus_one is assigned to the load_multiply_by_two + 1
  FPReal load_division = FPR_DIV_FPR(load_multiply_by_two, load_multiply_plus_one);       // load_division is assigned to the load_multiply_by_two / load_multiply_plus_one
  t->recent_cpu = FPR_ADD_INT(FPR_MUL_FPR(load_division, recent_cpu), t->nice);           // recent_cpu is assigned to the load_division * recent_cpu + nice
}

/*
 update_priority() - updates the priority of the thread
*/
void update_priority(struct thread * t, void *aux UNUSED){
    t->priority = PRI_MAX - FPR_TO_INT(FPR_DIV_INT(t->recent_cpu, 4)) - (t->nice * 2);     // priority is assigned to PRI_MAX - recent_cpu / 4 - nice * 2 (using the prioritu formula)
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread) idle_ticks++;
#ifdef USERPROG
    else if (t->pagedir != NULL)
        user_ticks++;
#endif
    else
        kernel_ticks++;






    if(thread_mlfqs) {                                                                      // if thread_mlfqs is true (Set by the OS in boot)
        /* Update recent_cpu for every tick */
        t->recent_cpu = FPR_ADD_INT(t->recent_cpu, 1);                                      // recent_cpu is assigned to recent_cpu + 1

        /* Update recent_cpu and average_load for all threads, every second */
        if(timer_ticks()%100 == 0){                                                         // Our cpu is 100 hz, so we check to see if a second has passed.
            int ready_threads;                                                              // ready_threads is assigned to the number of threads in the ready list
            if (t == idle_thread) {                                                         // Checking to see if the current thread is ready or not
                ready_threads = (int)(list_size(&ready_list));                              // How many are ready without itself
            } else {
                ready_threads = (int)(list_size(&ready_list)) + 1;                          // How many are ready with itself
            }
            average_load =                                                                   // average_load is assigned to the average_load (Formula)
                FPR_ADD_FPR(FPR_MUL_FPR(average_load, INT_DIV_INT(59, 60)),
                            FPR_MUL_INT(INT_DIV_INT(1, 60), ready_threads));
            enum intr_level old_level = intr_disable();                                      // Disables interrupts and equalts intr_level to old_level
            thread_foreach(latest_cpu, NULL);                                                // For all threads it calls the latest_cpu function and updates the recent_cpu
            intr_set_level(old_level);                                                       // Enables the interrupts
        }
        /* Updates the priority for all threads, every 4 ticks*/
        if (++thread_ticks >= TIME_SLICE){                                                   // If the thread_ticks is greater than or equal to TIME_SLICE (4)
            enum intr_level old_level = intr_disable();                                      // Disables interrupts and equalts intr_level to old_level
            thread_foreach(update_priority, NULL);                                           // For all threads it calls the update_priority function and updates the priority
            intr_set_level(old_level);                                                       // Enables the interrupts
        }
    }







        /* Enforce preemption. */
        if (thread_ticks >= TIME_SLICE) intr_yield_on_return();
    
}

/* Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.
   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.
   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
    struct thread *t_thread;
    struct kernel_thread_frame *kernel_f;
    struct switch_entry_frame *entry_f;
    struct switch_threads_frame *switch_f;
    tid_t tid;
    enum intr_level old_level;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t_thread = palloc_get_page(PAL_ZERO);
    if (t_thread == NULL) return TID_ERROR;

    /* Initialize thread. */
    init_thread(t_thread, name, priority);
    tid = t_thread->tid = allocate_tid();

    /* Prepare thread for first run by initializing its stack.
       Do this atomically so intermediate values for the 'stack'
       member cannot be observed. */
    old_level = intr_disable();


    /* Stack frame for kernel_thread(). */
    kernel_f = alloc_frame(t_thread, sizeof *kernel_f);
    kernel_f->eip = NULL;
    kernel_f->function = function;
    kernel_f->aux = aux;

    /* Stack frame for switch_entry(). */
    entry_f = alloc_frame(t_thread, sizeof *entry_f);
    entry_f->eip = (void (*)(void))kernel_thread;

    /* Stack frame for switch_threads(). */
    switch_f = alloc_frame(t_thread, sizeof *switch_f);
    switch_f->eip = switch_entry;
    switch_f->ebp = 0;

    
    t_thread->sleep_for = 0;                                                                 // sleep_for is assigned to 0    
    t_thread->nice = 0;                                                                      // nice is assigned to 0
    t_thread->recent_cpu = INT_TO_FPR(0);                                                    // recent_cpu is assigned to 0




    intr_set_level(old_level);

    thread_unblock(t_thread);

    if(!thread_mlfqs){                                                                        // if thread_mlfqs is false (Set by the OS in boot)
        yielding();                                                                           // yielding function is called
    }
    return tid;
}

/*
    yield() function is called by the thread_create() function.
*/
void yielding(void){
    enum intr_level old_level = intr_disable();                                               // Disables interrupts and equalts intr_level to old_level
 
    if (!list_empty(&ready_list)) {                                                           // If the ready list is not empty
        struct thread *thread_priority =                                                      // Equates thread_priority with the highest priority thread in the ready list
            list_entry(list_max(&ready_list, under_prioritize, NULL),
                       struct thread, elem);

        struct thread * curr = thread_current();                                              // Equates curr with the current thread
        if (curr->priority < thread_priority->priority) {                                     // If the current thread's priority is less than the highest priority thread's priority then the current thread is yielded
            thread_yield();
        }
    }
    intr_set_level(old_level);                                                                // Enables the interrupts
}



void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);
    list_push_back(&ready_list, &t->elem);
    t->status = THREAD_READY;
    intr_set_level(old_level);
}

/* Returns the name of the running thread. */
const char *thread_name(void) { return thread_current()->name; }

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *thread_current(void) {
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void) { return thread_current()->tid; }

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void) {
    ASSERT(!intr_context());

#ifdef USERPROG
    process_exit();
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable();
    list_remove(&thread_current()->allelem);
    thread_current()->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());

    old_level = intr_disable();
    if (cur != idle_thread) list_push_back(&ready_list, &cur->elem);
    cur->status = THREAD_READY;
    schedule();
    intr_set_level(old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
    thread_current()->priority = new_priority;
    yielding();                                                               // to check if the current thread's priority is higher than the highest priority thread in the ready list
}

/* Returns the current thread's priority. */
int thread_get_priority(void) { return thread_current()->priority; }

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice){ 
    struct thread * now = thread_current();                                   // now is assigned to the current thread
    now->nice = nice;                                                         // now's nice is assigned to nice
    thread_set_priority(PRI_MAX - FPR_TO_INT(FPR_DIV_INT(now->recent_cpu, 4)) - (now->nice * 2));    // Update the priority because of the influence of nice
}

/* Returns the current thread's nice value. */
int thread_get_nice(void) {
    return thread_current()->nice;                                            // return the current thread's nice value
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    return FPR_TO_INT(FPR_MUL_INT(average_load, 100));                       // return 100 times the system load average
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
    return FPR_TO_INT(FPR_MUL_INT(thread_current()->recent_cpu, 100));       // return 100 times the current thread's recent_cpu value
}

/* Idle thread.  Executes when no other thread is ready to run.
   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.
           The `sti' instruction disables interrupts until the
           completion of the next instruction, so these two
           instructions are executed atomically.  This atomicity is
           important; otherwise, an interrupt could be handled
           between re-enabling interrupts and waiting for the next
           one to occur, wasting as much as one clock tick worth of
           time.
           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable(); /* The scheduler runs with interrupts off. */
    function(aux); /* Execute the thread function. */
    thread_exit(); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *running_thread(void) {
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm("mov %%esp, %0" : "=g"(esp));
    return pg_round_down(esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
    return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->stack = (uint8_t *)t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;
    list_push_back(&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *alloc_frame(struct thread *t, size_t size) {
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *next_thread_to_run(void) {

    struct thread *highest_priority_thread;
    struct list_elem *e;

    if (list_empty(&ready_list)) {
        return idle_thread;
    } else {
        e = list_max(&ready_list, under_prioritize, NULL);                               // find the highest priority thread in the ready list
        highest_priority_thread = list_entry(e, struct thread, elem);
        list_remove(e);                                                                  // remove the highest priority thread from the ready list
        return highest_priority_thread;                                                  // return the highest priority thread
    }

}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.
   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).
   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.
   After this function and its caller returns, the thread switch
   is complete. */
void thread_schedule_tail(struct thread *prev) {
    struct thread *cur = running_thread();

    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct
       thread.  This must happen late so that thread_exit() doesn't
       pull out the rug under itself.  (We don't free
       initial_thread because its memory was not obtained via
       palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING &&
        prev != initial_thread) {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void schedule(void) {
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    if (cur != next) prev = switch_threads(cur, next);
    thread_schedule_tail(prev);
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}

/* Blocks a thread until certain ticks have passed */
void thread_block_until(int64_t ticks) {
    enum intr_level old_level;                                          // save the old interrupt level
    struct thread *t;                                                   // the thread to be blocked
 
    old_level = intr_disable();                                         // disable interrupts
    t = thread_current();                                               // get the current thread
    t->sleep_for = ticks;                                               // set the sleep_for field of the thread

    thread_block();                                                     // block the thread
    intr_set_level(old_level);                                          // restore the interrupt level
}

/* Wakes up thread if timer has passed x ticks since blocked */
void thread_wake_up() {                                                 
    struct thread *t;                                                   // the thread to be woken up
    struct list_elem *pos;                                              // iterator for the list of all threads

    /*Disable interrupts*/
    enum intr_level old_level;                                          // save the old interrupt level
    old_level = intr_disable();                                         // disable interrupts

    int64_t ticks = timer_ticks();                                      // get the current ticks
    for (pos = list_begin(&all_list); pos != list_end(&all_list);       // iterate through the list of all threads
         pos = list_next(pos)) {                                        // get the next thread
        t = list_entry(pos, struct thread, allelem);                    // get the current thread

        if (t->sleep_for != 0 && ticks >= t->sleep_for) {               // if the thread is blocked and the ticks have passed
            t->sleep_for = 0;                                           // set the sleep_for field to 0
            thread_unblock(t);                                          // unblock the thread
        }
    }
    intr_set_level(old_level);                                          // restore the interrupt level
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);



