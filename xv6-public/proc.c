#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

double MLFQPassVal;
double MLFQStrideVal;

int checkpoint;
int levturn;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct {
  struct spinlock lock;
  struct tid tid[NPROC];
} ttable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 0;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  struct tid *t;
  int index;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  // initialize additional proc structure arguments
  p->isMLFQ = 1;
  p->pticks = 0;
  p->ptotalticks = 0;
  p->priority = 0;

  p->share = 0;
  p->stride = 0;
  p->pass = 0;

  p->isThread = 0;
  p->tid = 0;
  // acquire(&ttable.lock);
  // ttable.tid[0].state = EMBRYO;
  // release(&ttable.lock);
  acquire(&ttable.lock);
  for(index = 0, t = ttable.tid; t < &ttable.tid[NPROC]; t++, index++)
    if(t->state == UNUSED)
    {
      // cprintf("%d\n", index);
      p->tid = t->index = index;
      t->state = EMBRYO;
      break;
    }
  release(&ttable.lock);
  p->isMainThread = 1;
  p->cntThread = 1;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
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
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p;
  struct proc *curproc = myproc();

  sz = curproc->sz;

  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  // updates process's sz that has same pid (lwp, main thread)
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p == curproc)
      continue;
    if(p->pid == curproc->pid)
      p->sz += n;
  }
  release(&ptable.lock);

  curproc->sz = sz;

  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->usz = curproc->usz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);
// cprintf("%d \n", pid);
  return pid;
}

int
thread_create(thread_t *thread, void * (*start_routine)(void *), void *arg)
{
    struct proc *np;
    struct proc *curproc = myproc();
    // struct tid *t;
    struct proc *p;
    uint sz, sp, ustack[2];
    // int index;
    struct proc *tmp;

    tmp = findMinPass();

    // LWP cannot create new thread..
    if(curproc->isThread)
      return -1;

    // Allocate thread
    if((np = allocproc()) == 0)
        return -1;
    np->pid = curproc->pid;

    // allocate tid.
    // acquire(&ttable.lock);
    // for(index = 0, t = ttable.tid; t < &ttable.tid[NPROC]; t++, index++)
    //   if(t->state == UNUSED)
    //   {
    //     // cprintf("%d\n", index);
    //     np->tid = t->index = index;
    //     t->state = EMBRYO;
    //     break;
    //   }
    // release(&ttable.lock);

    //count lwp number.
    curproc->cntThread++;

    // if main thread follows stride scheduling,
    if(curproc->isMLFQ == 0)
    {
      acquire(&ptable.lock);
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if(p->pid == curproc->pid)
          {
            p->isMLFQ = 0;
            p->share = curproc->totalshare / (double)curproc->cntThread;
            p->stride = STRIDENUM / p->share;
            p->pass = tmp->pass;
            // cprintf("totalshare %d share %d mlfq %d\n", curproc->totalshare, (int)p->share, (int)leftShare);
          }
      release(&ptable.lock);
    }

    np->isThread = 1;
    np->isMainThread = 0;
    // reallocate proc structure
    np->pgdir = curproc->pgdir;
    np->parent = curproc;
    *np->tf = *curproc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for(int i = 0; i < NOFILE; i++)
      if(curproc->ofile[i])
        np->ofile[i] = filedup(curproc->ofile[i]);
        // np->ofile[i] = curproc->ofile[i];
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    sz = curproc->usz + (np->tid) * 2 * PGSIZE;
    // if((sz = allocuvm(np->pgdir, sz, sz + 2*PGSIZE)) == 0)
    // {
    //   cprintf("allocuvm\n");
    //   return -1;
    // }
    // cprintf("sz %d\n", sz);
    // clearpteu(np->pgdir, (char*)(sz- 2*PGSIZE));
    sp = sz;

// cprintf("123\n");
    // np->tf->esp = curproc->tf->esp + PGSIZE;

    ustack[0] = 0xffffffff;      // fake return PC
    ustack[1] = (uint)arg;       // argument for start_routine
    sp -= 8;

    if(copyout(np->pgdir, sp, ustack, 8) <0)
    {
      cprintf("copyout\n");
        np->state = UNUSED;
        np->kstack = 0;
        nexttid--;
        return -1;
    }
// cprintf("2create %d\n", np->tf->eip);
    *np->tf = *curproc->tf;
    // np->sz = curproc->sz = sz;
    np->usz = sz;
    np->sz = curproc->sz;
    np->tf->esp = sp;
    np->tf->eip = (uint)start_routine;
// cprintf("3create %d\n", np->parent->pid);
    np->state = RUNNABLE;
    *thread = np->tid;
// cprintf("pid %d tid %d\n", np->pid, np->tid);
    return 0;

}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd, check;

// cprintf("exit %d\n", curproc->pid);
  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  // if main thread or lwp calls exit(), seek for lwps that has same pid.
  // main thread doesn't become ZOMBIE unless curproc itself is main thread.
  for(check = 0, p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    // if(p == curproc)
    //   continue;
    // if(p->isMainThread == 1)
    //   continue;
    if(p->pid == curproc->pid)
    {
      if(p != curproc)
      {
        // Close all open files.
        for(fd = 0; fd < NOFILE; fd++){
          if(p->ofile[fd]){
            fileclose(p->ofile[fd]);
            p->ofile[fd] = 0;
          }
        }
        begin_op();
        iput(p->cwd);
        end_op();
        p->cwd = 0;
        p->state = ZOMBIE;
        check++;
        // if(!p->isMainThread)
          // p->parent = p->parent->parent;
      }
    }
  }
  // curproc->parent->cntThread--;
  // if(curproc->parent->cntThread == 1 )
  // {
  //   leftShare = 100;
  //   MLFQStrideVal = (double)STRIDENUM / leftShare;
  //   curproc->parent->share = 0;
  //   curproc->parent->stride = 0;
  //   curproc->parent->pass = 0;
  // }
  // cprintf("pid %d cntThread %d\n", curproc->pid, curproc->parent->cntThread);

// cprintf("%d %d\n", curproc->pid, check);

  curproc->state = ZOMBIE;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  // cprintf("init\n");

  if(curproc->isThread)
    wakeup1(curproc->parent->parent);
  else
    wakeup1(curproc->parent);


  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    // cprintf("init\n");
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // cprintf("%d %d ", curproc->pid,check);

  // if(check != 0)
  // {
  //   release(&ptable.lock);
  //   // curproc doesn't become UNUSED.
  //   if(killsamepid(curproc->pid) != 0)
  //     panic("killsamepid error");
  //   acquire(&ptable.lock);
  // }

// cprintf("qwer\n");
  // Jump into the scheduler, never to return.

// cprintf("%d %d\n",curproc->isMainThread, curproc->parent->pid);
// cprintf("asdf");
  sched();
  panic("zombie exit");
}

void
thread_exit(void *retval)
{
    struct proc *curproc = myproc();
    struct proc *p;
    int fd;

    if(curproc == initproc)
        panic("init exiting");
// cprintf("exits\n");
    for(fd = 0; fd < NOFILE; fd++)
    {
        if(curproc->ofile[fd])
        {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    acquire(&ptable.lock);
    curproc->retval = (int)retval;
// cprintf("exit: tid %d retval %d \n", curproc->tid, (int)retval);

    // curproc->parent->cntThread--;
    // if(curproc->parent-> cntThread == 1 )
    // {
    //   leftShare += curproc->parent->totalshare;
    //   MLFQStrideVal = STRIDENUM / leftShare;
    // }
    // cprintf("pid %d cntThread %d\n", curproc->pid, curproc->parent->cntThread);

    // Parent might be sleeping in thread_join().
    wakeup1(curproc->parent);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
        if(p->parent == curproc)
        {
            p->parent = initproc;
            if(p->state == ZOMBIE)
                wakeup1(initproc);
        }
    }

// cprintf("exit\n");
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");

}

int
killsamepid(int pid)
{
  struct proc *p;
  // struct proc *curproc = myproc();

  // acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->pid != pid)
      continue;
    if(p->isMainThread)
      continue;
    // if(p->tid == 0)
    //   continue;
    // if(p == myproc())
    //   continue;
    if(p->state == ZOMBIE)
    {
// cprintf("pid %d tid %d\n", p->pid,p->tid);

      ttable.tid[p->tid].state = UNUSED;

      // if the process was following stride scheduler,
      // return the share to the MLFQ scheduler
      // and reset MLFQStrideVal.
      // if(p->isMLFQ == 0)
      // {
      //     p->parent->totalshare -= p->share;
      //
      //     leftShare += p->share;
      //     MLFQStrideVal = (double)STRIDENUM / leftShare;
      // }

      // cprintf("cntThread %d pid %d after totalshare %d mlfq %d\n", p->parent->cntThread ,p->pid, p->parent->totalshare, (int)leftShare);
      // if(p->tid > 0)
      //   thread_join(p->tid, 0);
      // reset proc structure
      p->pass = 0;
      p->stride = 0;
      p->share = 0;

      p->pticks = 0;
      p->ptotalticks = 0;
      p->priority = 0;
      p->isMLFQ = 0;

      p->tid = 0;

      kfree(p->kstack);
      // cprintf("asdf\n");
      p->kstack = 0;
      // if(p->tid == 0)
      //   freevm(p->pgdir);
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      p->state = UNUSED;
      // procdump();
    }
  }

  // release(&ptable.lock);
  return 0;
}
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

// cprintf("wait %d\n", curproc->pid);
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      if(p->isThereExec)
      {
        p->isThereExec = 0;
        // cprintf("a\n");
        // procdump();
        continue;
      }
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        // procdump();
// cprintf("wait %d %d %d %d %d\n", p->pid, p->tid, p->isThread, p->isMainThread, p->state);
// cprintf("wait %d\n", curproc->pid);
// procdump();
        if(killsamepid(p->pid) != 0)
          panic("killsamepid error");
        if(p->isThread && !p->isMainThread)
          continue;

        ttable.tid[p->tid].state = UNUSED;
// cprintf("wait %d %d %d %d %d\n", p->pid, p->tid, p->isThread, p->isMainThread, p->state);
        // if the process was following stride scheduler,
        // return the share to the MLFQ scheduler
        // and reset MLFQStrideVal.
        if(p->isMLFQ == 0)
        {
            leftShare += p->totalshare;
            MLFQStrideVal = STRIDENUM / leftShare;
        }
        // cprintf("cntThread %d\n",p->cntThread);

        // cprintf("pid %d cntThread %d\n", curproc->pid, curproc->parent->cntThread);

        p->totalshare = 0;
        // if(p->tid > 0)
        //   thread_join(p->tid, 0);
        // reset proc structure
        p->pass = 0;
        p->stride = 0;
        p->share = 0;

        p->pticks = 0;
        p->ptotalticks = 0;
        p->priority = 0;
        p->isMLFQ = 0;

        p->tid = 0;

        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        if(p->isMainThread)
          freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        // cprintf("waitend\n");s
        // procdump();
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int
thread_join(thread_t thread, void **retval)
{
    struct proc *p;
    int havekids;
    struct proc *curproc = myproc();

    acquire(&ptable.lock);
    for(;;)
    {
        havekids = 0;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
            if(p->parent != curproc)
                continue;
            havekids = 1;
            while(p->state != ZOMBIE && p->pid == curproc->pid && p->tid == thread)
              sleep(curproc, &ptable.lock);

              // procdump();
                // if(retval != 0)
                  *retval = (void*)p->retval;
                // Found one.
// cprintf("join: tid %d retval %d\n", p->tid, (int)retval);

                // cprintf("pid %d cntThread %d\n", p->pid, curproc->cntThread);
                ttable.tid[p->tid].state = UNUSED;
                // cprintf("after %d curproc %d\n", p->sz, curproc->sz);

                // if the process was following stride scheduler,
                // return the share to the MLFQ scheduler
                // and reset MLFQStrideVal.
                if(p->isMLFQ == 0)
                {
                    // p->parent->totalshare += p->share;
                    leftShare += p->share;
                    MLFQStrideVal = (double)STRIDENUM / leftShare;
                }
                // p->parent->cntThread--;
                // reset proc structure
                p->pass = 0;
                p->stride = 0;
                p->share = 0;

                p->pticks = 0;
                p->ptotalticks = 0;
                p->priority = 0;
                p->isMLFQ = 0;

                p->tid = 0;

                // pid = p->pid;
                kfree(p->kstack);
                p->kstack = 0;
                //freevm(p->pgdir);
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);
                return 0;

        }

            // No point waiting if we don't have any children.
            if(!havekids || curproc->killed){
              cprintf("havekids\n");
              release(&ptable.lock);
              return -1;
            }

            // Wait for children to exit.  (See wakeup1 call in proc_exit.)
            // sleep(curproc, &ptable.lock);  //DOC: wait-sleep
    }
}


// function for priority boosting
// prevents starvation in MLFQ scheduling.
void
MLFQ_Boost(void)
{
  struct proc *p;

  // reset all the process that follows MLFQ Scheduling
  // to inital state.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(!p->isMLFQ)
      continue;
    p->priority = 0;
    p->pticks = 0;
    p->ptotalticks = 0;
  }

  MLFQ_ticks = 0;
}

// MLFQ Scheduling
void
MLFQ_Scheduler(void)
{
  struct proc *p = myproc();
  struct cpu *c = mycpu();

  c->proc = 0;

  for(checkpoint = 0, levturn = 0, p = ptable.proc; p < &ptable.proc[NPROC]; levturn++, checkpoint++, p++)
  {
    // frequency of priority boosting is 100ticks.
    if(MLFQ_ticks >= 100)
      MLFQ_Boost();

    // checks if the scheduler has searched
    // all the process in the queue
    if(levturn == 63)
      break;

    // keep track of where the scheduler has checked
    // the process table last time.
    if((checkpoint % 63) == 0)
      p = ptable.proc;

    // check if the process follows MLFQ scheduling.
    if(p->isMLFQ != 1)
      continue;

    // search for RUNNABLE process that is in lev 0
    if(p->state != RUNNABLE || p->priority != 0)
      continue;

    // if lev 0, add pass value 1 times.
    if(leftShare < 100)
      MLFQPassVal += MLFQStrideVal;

    // switch process for scheduler to next process.
    levturn = 0;

    c->proc = p;
    switchuvm(p);
// cprintf("pid %d tid %d eip %d\n", p->pid, p->tid, p->tf->eip);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
  }

  for(checkpoint = 0, levturn = 0, p = ptable.proc; p < &ptable.proc[NPROC]; levturn++, checkpoint++, p++)
  {
    // frequency of priority boosting is 100ticks.
    if(MLFQ_ticks >= 100)
      MLFQ_Boost();

    // checks if the scheduler has searched
    // all the process in the queue
    if(levturn == 63)
      break;

    // keep track of where the scheduler has checked
    // the process table last time.
    if((checkpoint % 63) == 0)
      p = ptable.proc;

    // check if the process follows MLFQ scheduling.
    if(p->isMLFQ != 1)
      continue;

    // search for RUNNABLE process that is in lev 1
    if(p->state != RUNNABLE || p->priority != 1)
      continue;

    // if lev 1, add pass value 2 times.
    if(leftShare < 100)
      MLFQPassVal += (MLFQStrideVal * 2);

    // switch process for scheduler to next process.
    levturn = 0;
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
  }

  for(checkpoint = 0, levturn = 0, p = ptable.proc; p < &ptable.proc[NPROC]; levturn++, checkpoint++, p++)
  {
    // frequency of priority boosting is 100ticks.
    if(MLFQ_ticks >= 100)
      MLFQ_Boost();

    // checks if the scheduler has searched
    // all the process in the queue
    if(levturn == 63)
      break;

    // keep track of where the scheduler has checked
    // the process table last time.
    if((checkpoint % 63) == 0)
      p = ptable.proc;

    // check if the process follows MLFQ scheduling.
    if(p->isMLFQ != 1)
      continue;

    // search for RUNNABLE process that is in lev 2
    if(p->state != RUNNABLE || p->priority != 2)
      continue;

    // if lev 2, add pass value 3 times.
    if(leftShare < 100)
      MLFQPassVal += (MLFQStrideVal * 4);

    // switch process for scheduler to next process.
    levturn = 0;
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    swtch(&(c->scheduler), p->context);
    switchkvm();
    c->proc = 0;
  }

  // exception handling for the following case.
  // if there is no RUNNABLE MLFQ processes,
  // process 0, 1(executed when xv6 is turned on)
  // runs but doesn't add the pass value
  if(leftShare < 100)
    MLFQPassVal += MLFQStrideVal;

}

// function to find the process
// who has minimal pass value.
struct proc*
findMinPass(void)
{
  struct proc *p, *min = 0;
  uint max = 3000000000;

  // search among the process that follows stride scheduler,
  // that is RUNNABLE,
  // that has minimal pass value
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->isMLFQ == 1)
      continue;
    if(p->state != RUNNABLE)
      continue;
    if(p->pass < max)
    {
      max = p->pass;
      min = p;
    }

  }

  // 0 : MLFQ scheduler
  // else : Stride scheduler
  if(MLFQPassVal < max)
    return 0;
  else
    return min;
}

// stride scheduling.
void
Stride_Scheduler(struct proc* p)
{
  struct cpu *c = mycpu();

  c->proc = 0;

  p->pass += p->stride;

  c->proc = p;
  switchuvm(p);
  p->state = RUNNING;
  swtch(&(c->scheduler), p->context);
  switchkvm();
  c->proc = 0;

}

// function for set_cpu_share() sys-call.
void
cpu_share(int share)
{
  struct proc *p = myproc();
  struct proc *t;
  struct proc *tmp;

  if(p->isMainThread){
    p->totalshare += share;
    if(p->totalshare > 100)
      panic("cpu share overflow");
    leftShare -= p->totalshare;
  }
  else{
    p->parent->totalshare += share;
    // cprintf("totalshare %d\n", p->parent->totalshare);
    if(p->parent->totalshare > 100)
      panic("cpu share overflow");
    leftShare -= p->parent->totalshare;
  }
  MLFQStrideVal = STRIDENUM / leftShare;

  tmp = findMinPass();

  if(p->isMainThread && p->cntThread > 1)
  {
    for(t = ptable.proc; t < &ptable.proc[NPROC]; t++)
      if(t->pid == p->pid)
        {
          t->isMLFQ = 0;
          t->share = p->totalshare / p->cntThread;
          t->stride = (double)STRIDENUM / t->share;
          // leftShare -= t->share;
          // MLFQStrideVal = STRIDENUM / leftShare;

          if(tmp == 0)
            t->pass = MLFQPassVal;
          else
            t->pass = tmp->pass;
        }
    // p->share = share / p->cntThread;
  }
  else if(p->isThread)
  {
    for(t = ptable.proc; t < &ptable.proc[NPROC]; t++)
      if(t->pid == p->pid)
        {
          t->isMLFQ = 0;
          t->share = share / p->parent->cntThread;
          t->stride = STRIDENUM / t->share;
          // leftShare -= t->share;
          // MLFQStrideVal = STRIDENUM / leftShare;

          if(tmp == 0)
            t->pass = MLFQPassVal;
          else
            t->pass = tmp->pass;
        }
  }
  else
  {
    p->isMLFQ = 0;
    p->share = share;
    p->stride = STRIDENUM / share;
    // leftShare -= share;
    // MLFQStrideVal = STRIDENUM / leftShare;

    if(tmp == 0)
      p->pass = MLFQPassVal;
    else
      p->pass = tmp->pass;

  }

  // tmp = findMinPass();
  // if(tmp == 0)
  //   p->pass = MLFQPassVal;
  // else
  //   p->pass = tmp->pass;
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *min;
  struct cpu *c = mycpu();


  c->proc = 0;

  for(;;){

    if(MLFQ_ticks >= 100)
      MLFQ_Boost();
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);

    if(leftShare >= 100)
      MLFQ_Scheduler();
    else if(leftShare < 100)
    {
      if((min = findMinPass()) == 0)
        MLFQ_Scheduler();
      else
        Stride_Scheduler(min);
    }

    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  // cprintf("pid %d, tid %d, state %d\n", p->pid, p->tid, p->state);
  // cprintf("");
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
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
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan){
      // cprintf("wakeup1 %d\n", p->pid);
      p->state = RUNNABLE;
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      // release(&ptable.lock);
      // return 0;
    }
  }
  release(&ptable.lock);
  return 0;
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s %d", p->pid, state, p->name, p->killed);
    if(p->state == ZOMBIE)
      cprintf(" %d", p->parent->pid);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
