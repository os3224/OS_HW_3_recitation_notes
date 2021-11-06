#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void pinit(void) { initlock(&ptable.lock, "ptable"); }

// PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// PAGEBREAK: 32
// Set up first user process.
void userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// ****** we also need to grow thread's memory size ******
// Return 0 on success, -1 on failure.
int growproc(int n) {
  uint sz;
  // get size of process' memory in bytes by accessing the process' sz field in the proc structure
  sz = proc->sz;
  // check whether increase or decrease of memory has been requested
  // if increase
  if (n > 0) {

    // call allocuvm():
    // Allocate page tables and physical memory to grow process from oldsz to
    // newsz, which need not be page aligned.  Returns new size or 0 on error.
    // int allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
    // return - 1 if the allocation failed

    if ((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } 
  // decrease case
  else if (n < 0) {

    // Deallocate user pages to bring the process size from oldsz to
    // newsz.  oldsz and newsz need not be page-aligned, nor does newsz
    // need to be less than oldsz.  oldsz can be larger than the actual
    // process size.  Returns the new process size.
    // int deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
    // return - 1 if the deallocation failed

    if ((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  // set the new size of the process in the process table
  proc->sz = sz;

  // ****** Acquire the process table lock ******

  // ****** initialize a pointer to the process structure (proc) ******

  // ****** loop over the process table ******

    // ****** if the "process" we are currently looking at is a thread 
    // " for simplicity, threads each have their own process ID. ""
    // i.e, if p is a thread it will have the current process as its parent AND will share the same address space as the parent
    // p->parent == proc && p->pgdir == proc->pgdir 
    // then: update the thread's memory size
    // ******

  // ****** release the process table lock

  // load the process for execution 
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
  int i, pid;

  // pointer to the proc struct
  struct proc *np;

  // Allocate process.
  if ((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  // AND initialize the new process' page directory to the parent's page directory 
  // copyuvm() :
  // Given a parent process's page table, create a copy
  // of it for a child.
  // pde_t *copyuvm(pde_t *pgdir, uint sz)
  // returns 0 on failure 

  if ((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0) {
    // free the resources in case of failure 
    kfree(np->kstack);
    np->kstack = 0;
    // reset the process' picked slot in the stack  table back to unused
    np->state = UNUSED;
    // return the failure code
    return -1;
  }

  // if copyuvm succeeded

  // set the new process' size
  np->sz = proc->sz;
  // set the new process' parent pointer 
  np->parent = proc;
  // initilaize the new process' trap frame 
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  // copy over the files that were opened by the parent process
  for (i = 0; i < NOFILE; i++)
    if (proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  // initize the new process' current working directory
  np->cwd = idup(proc->cwd);

  // give the child process the parent's name
  safestrcpy(np->name, proc->name, sizeof(proc->name));

  // set up the pid for the new process 
  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
  struct proc *p;
  int fd;

  if (proc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++) {
    if (proc->ofile[fd]) {
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  // clear the file system from the existing process
  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  // init will adopt children 
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    // ****** make sure that we've found the actual children rather than threads
    // p's parent is proc, but p is not proc's thread
    // p->parent == proc && p->pgdir != proc->pgdir ******
    if (p->parent == proc) { // ***** careful /: *****
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  // equivalent to while(True)
  for (;;) {
    // Scan through table looking for zombie children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      // ***** same thing as in exit(), make sure we are dealing with processes, not the threads *****
      // skip the "process" if it's actually a thread-
      if (p->parent != proc)
        continue;
      // if we've found a child process
      havekids = 1;
      if (p->state == ZOMBIE) {
        // Found one.
        // deallocate its resources
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        // freevm() :
        // Free a page table and all the physical memory pages
        // in the user part.
        // void freevm(pde_t *pgdir)

        freevm(p->pgdir); // **** careful :/ ***

        // **** loop over the process table and make sure that there's no thread that is currently 
        // active and is using the child's page directory 
        // only then call freevm() to free the page directory ****

        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || proc->killed) {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock); // DOC: wait-sleep
  }
}

// PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void) {
  struct proc *p;
  int foundproc = 1;

  for (;;) {
    // Enable interrupts on this processor.
    sti();

    if (!foundproc)
      hlt();

    foundproc = 0;

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      foundproc = 1;
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void sched(void) {
  int intena;

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (cpu->ncli != 1)
    panic("sched locks");
  if (proc->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  acquire(&ptable.lock); // DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
  if (proc == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock) { // DOC: sleeplock0
    acquire(&ptable.lock);  // DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock) { // DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1(void *chan) {
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
  static char *states[] = {
      [UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
      [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING) {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// **** clone() system call ****
int clone(void (*fcn)(void *, void *), void *arg1, void *arg2,
          void *user_stack) {

  // **** similar to fork() ****

  int i, pid;
  struct proc *np; // New process

  // **** allocate a new "process" ****

  np->sz = proc->sz;   // Set sz to to current proc sz
  np->parent = proc;   // Set parent to current proc
  *np->tf = *proc->tf; // Set tf to current proc's tf

  // **** initialize the thread's page directory to process' page directory ****

  // **** "The new thread starts executing at the address specified by fcn"
  // access new process' instructionn pointer (eip), which lives inside the process' trap frame and initlize it to fcn ****

  // **** initilaize the user_stack_pointer (should be added to the proc struct first) to the user stack ****

  // **** initialize the stack pointer, note that the user stack grows downward to lower addresses
  // " the stack should be one page in size " ****
  uint *stack_pointer = user_stack + PGSIZE;

  stack_pointer--;
  // **** push the second argument onto the stack
  *stack_pointer = (uint)arg2; // Copy arg2 to stack
  stack_pointer--;

  // **** push the first argument onto the stack ****

  // **** " uses a fake return PC (0xffffffff) "
  // set that return PC ****

  // **** update the stack pointer (esp), which also lives inside trap frame ****
  np->tf->esp = (uint)stack_pointer; // esp register: stack pointer

  // **** the rest is similar to fork() ****

}


// **** join() system call ****

int join(void **stack) {

  struct proc *p;

  int havethread, pid; // like havekids in wait()

  // **** acquire the process table lock ****

  for (;;) {
    // Scan through table looking for exited thread.
    havethread = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {

      // **** find child process' thread ****
      // 1. p's parent is not proc
      // 2. p's page directory is different from proc's page directory
  
      // **** if we've found a thread ****
      havethread = 1;
      if (p->state == ZOMBIE) {
        // **** the location of the child's user stack is copied into the argument stack ****

        // **** careful about freevm() /: ****

        return pid;
      }
    }

    // **** again, similar to wait() ****
  }
}
