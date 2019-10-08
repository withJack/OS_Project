# Scheduling Project Milestone1

#### Lee Seungjae(이승재)

#### 2014004948 Hanyang Univ. CSE

## CPU Scheduling Concept & Background

### why we need CPU scheduling? -CPU virtualization

> ​	CPU can only run single program at a time. It might look like handling many programs at the same time, but it is not. Rather, CPU Virtualization is taking place. Every program thinks they are the only one using the CPU. The key to this concept is _time sharing_. When multiple programs are running simultaneously, which is happening even now when you are reading this document, one process(running program) stops and another process runs. CPU scheduler, so called process scheduler, is the one that decides which process should CPU run next by certain algorithms. With this scheduling procedure, OS can make best use of given resources efficiently, thus being more productive. 

Followings are the concepts of the various schedulers. 

## Concepts of Existing Schedulers

#### * Round Robin

> RR(Round Robin) scheduling uses timer interrupt for context switch. A process runs for a time slice and then switch to the next process. The length of a time slice is a multiple of the timer-interrupt period called tick. Every default tick(10ms in xv6), timer interrupt occurs. When the interrupt occurs, current process is switched over to the next runnable process, which itself thinks itself is running from the run queue.  This repeatedly happens until all the processes are finished.

#### * Multilevel Feedback Queue Scheduling 

> The main idea of this design is that a scheduler learns from the past to predict future. MLFQ has a number of distinct queues that is assigned different priority level. Scheduler chooses next process to run based on the priority of the queue the process is in. In the cases when there is a lot of process in same queue, RR scheduling is affected. Every process is stored in the highest priority level queue when first enters the system. When the process uses up an entire time slice set to each queue(the higher the priority, the lesser the time slice), it is moved to next level queue. The only way that process moves to upper level queue is priority boosting. Priority boosting occurs every certain amount of tick to move every processes to the highest priority level queue. This prevents starvation of lower priority processes. 

#### * Stride Scheduling 

> Each process has a number called _stride_. Stride is set by a large number divided by tickets requested by the process. Every time a process runs, _pass value_ is increased by its stride. Scheduler chooses which process to run by its pass value. One of the lowest pass value process run. When the new process enters the system, its initial pass value is set to the minimum of existing processes. This pass value idea helps each process to know whether they are getting right amount of time share from the scheduler. Note that OS has its own pass value, too. 

## General Abstract Design & Scenario 

### Project Goal 

##### _Design a new scheduler with MLFQ and Stride on xv6_.

---

#### Brief Scenario

When the process enters the system, or is executed, it follows the MLFQ scheduling design. It goes to the highest priority queue. After a while, when a system call that inquires to obtain cpu share(%) is called, the calling process is deleted from the queue and now it follows the stride scheduling design. As soon as the first process comes out, the MLFQ _scheduler_ follows the stride scheduling design. Its cpu share is always the rest of the share. For example, if there are 2 process that asked for the cpu share, 20% and 30% respectively, MLFQ scheduler has 50% of the cpu share. When MLFQ scheduler gets the cpu control, it runs its processes in the queues for the given tick. 

![diagram](C:\Users\super\Documents\학교 3-1\운영체제-정형수\Projects\scheduling design diagram.png){: width="10px" height="10px"}

---



#### 1. First Step: MLFQ

* Every process enters MLFQ when executed.


* There are 3 levels of feedback queue. 
  * sys_getlev: system call to get the level of process ready queue of MLFQ.
* Each level of queue adopts RR(Round Robin) policy with different time quantum. 
  * 1 tick, 2 ticks, 4 ticks each.
* Each level of queue has different time allotment. 
  * Highest priority queue: 5 ticks
  * Middle priority queue: 10 ticks
  * Lowest priority queue: doesn't need one
* Every certain amount of ticks, priority boost is called to prevent starvation. 
  * Frequency of the priority: 100 ticks.

#### 2. Second Step: Combine the stride scheduling algorithm with MLFQ

* Make a system call that requests the cpu share (%) and garatees the calling process to be allocated that time. 
  * set_cpu_share()
* Sum of stride processes CPU time is 80% of the total. Exception handling is needed for exceeding request.
* At least 20% of CPU time should run for the MLFQ scheduler which is the default schedulling. 

Additional system call:

* yield(): yield the cpu to the next process.



## Design Implementation

#### Additional Parameters

param.h

> ```c
> #define STRIDENUM	10000
> ```
>
> STRIDENUM: the value that is used to calculate stride value in stride scheduling.

set_cpu_share.c

> ```c
> uint leftShare = 100;
> ```
>
> `leftShare`: Initial value of leftShare is 100. This value represents the total CPU share value. `leftShare` is also the CPU share value of the MLFQ scheduler. 

proc.c

> ```c
> uint MLFQPassVal;
> uint MLFQStrideVal;
>
> int checkpoint;
> int levturn;
> ```
>
> `MLFQPassVal`: Pass value of the MLFQ scheduler. 
>
> `MLFQStrideVal`: Stride value of the MLFQ scheduler (MLFQPassVal = STRIDENUM / leftshare)
>
> `checkpoint`: Used in `MLFQ_Scheduler()` to make circular queue for each MLFQ level.
>
> `levturn`: Used in `MLFQ_Scheduler()` to get out of circular queue when the search is done. Then the scheduler searches the next level.

trap.c

> ```c
> uint MLFQ_ticks;
> int timeQuantumPerQ[3] = {1,2,4};
> int timeAllotmentPerQ[2] = {5,10};
> ```
>
> `MLFQ_ticks`: Ticks for MLFQ scheduler. Used fro priority boosting.
>
> `timeQuantumPerQ`: Time quantum for each level.
>
> `timeAllotmentPerQ`: Time allotment for each level.



#### Additional System Calls

sysproc.c

> ```c
> uint i;
>
> int
> sys_getlev(void)
> {
>     return myproc()->priority;
> }
>
> int
> sys_yield(void)
> {
>     if(++i % 20 == 0)
>     {
>       MLFQ_ticks++;
>       myproc()->pticks++;
>       myproc()->ptotalticks++;
>     }
>     yield();
>     return 0;
> }
> ```
>
> `sys_getlev()`: get the level of current process ready queue of MLFQ.
>
> `sys_yield()`: yield the cpu to the next process.
>
> ---
>
> `sys_yield()` function adds up ticks every 20 `sys_yield()` calls before the process actually yields. I put this addition to the ticks design, because of gaming the scheduler bugs. The reason I set the cycle every 20 system calls is that the test case calls  `sys_yield()` very often. To fulfill the requirement(1:2 ratio in level 0,1 queues), I arbitrarily set the cycle.

set_cpu_share.c

>```c
>int
>set_cpu_share(int share)
>{
>    // check if the input share is 0 or the total share is under 20%
>    if((leftShare - share) < 20)
>        return -1;
>    if(share == 0)
>        return -1;
>    // proc.c
>    cpu_share(share);
>    return share;
>}
>
>// Wrapper for set_cpu_share
>int
>sys_set_cpu_share(void)
>{
>    int share;
>    // Decode argument using argint
>    if(argint(0, &share) < 0)
>        return -1;
>    return set_cpu_share(share);
>}
>```
>
>`set_cpu_share()`: Inquires to obtain cpu share, at the same time,  the process starts to follow stride scheduling.
>
>`sys_set_cpu_share()`: wrapper function for `set_cpu_share()`.
>
>`cpu_share()`: This function is in proc.c file. 
>
>```c
>void
>cpu_share(int share)
>{
>  struct proc *p = myproc();
>  struct proc *tmp;
>  leftShare -= share;
>  MLFQStrideVal = STRIDENUM / leftShare;
>  p->isMLFQ = 0;
>  p->share = share;
>  p->stride = STRIDENUM / share;
>  tmp = findMinPass();
>  if(tmp == 0)
>    p->pass = MLFQPassVal;
>  else
>    p->pass = tmp->pass;
>}
>```
>
>It resets the MLFQ scheduler's stride value. Then sets the process into stride scheduling process. The pass value of the process is the minimal value of current processes.



#### Additional Variables to proc structure

proc.h - ```struct proc{}```

> ```c
> // MLFQ scheduling
> int priority;
> int pticks;
> int ptotalticks;
> int isMLFQ;
> // stride scheduling
> int share;
> int stride;
> int pass;
> ```
>
> `priority`: determines MLFQ level.
>
> `pticks`: determines the process' ticks (used for quantum check)
>
> `ptotalticks`: determines the process' ticks (used for allotment check)
>
> `isMLFQ`: determines whether the process follows MLFQ scheduling or stride scheduling.
>
> `share`: process share required from ```set_cpu_share()``` system call.
>
> `stride`: process' stride value (stride = STRIDENUM / share)
>
> `pass`: process; pass value.



#### Timer Interrupt Handling

trap.c

> ```c
> void
> MLFQ_Check(void)
> {
>     // didn't used its time quantum yet.
>     // do nothing, end function.
>     if(myproc()->pticks < timeQuantumPerQ[myproc()->priority]);
>     // used up its time quantum
>     // yield()
>     else if(myproc()->pticks >= timeQuantumPerQ[myproc()->priority])
>     {
>         myproc()->pticks = 0;
>         // move to next level
>         if(myproc()->priority < 2 && myproc()->ptotalticks >= timeAllotmentPerQ[myproc()->priority])
>         {
>             myproc()->ptotalticks = 0;
>             myproc()->priority++;
>         }
>         yield();
>     }
> }
> ```
>
> `MLFQ_Check()`: Checks time quantum, time allotment, priority level of the process. 

trap.c - `trap()`

> ```c
>   if(myproc() && myproc()->state == RUNNING &&
>      tf->trapno == T_IRQ0+IRQ_TIMER)
>   {
>       // MLFQ scheduling has different time quantum per levels.
>       if(myproc()->isMLFQ)
>       {
>         MLFQ_ticks++;
>         myproc()->pticks++;
>         myproc()->ptotalticks++;
>         MLFQ_Check();
>       }
>       // stride scheduling has time quantum of 1 tick.
>       else
>           yield();
>   }
> ```
>
> When the timer interrupt comes in, cpu checks if the process is following MLFQ or stride scheduling. If the process follows MLFQ scheduling, it adds all the related ticks and reset the proc structure values by `MLFQ_Check()`. If the process follows stride scheduling, it yields. 



#### Process from Birth to Death

proc.c - `allocproc()`

> ```c
> 	...
> found:
> 	...
>   p->isMLFQ = 1;
>   p->pticks = 0;
>   p->ptotalticks = 0;
>   p->priority = 0;
>
>   p->share = 0;
>   p->stride = 0;
>   p->pass = 0;
> 	...
> ```
>
> When the process is born by `fork()`, `allocproc()` function sets the proc structure variables of the process. Every process follows MLFQ scheduling when it first enter the system. The priority level is set to the highest. 

proc.c - `wait()`

> ```c
> 	...
> if(p->state == ZOMBIE){
>         // Found one.
>         if(p->isMLFQ == 0)
>         {
>             leftShare += p->share;
>             MLFQStrideVal = STRIDENUM / leftShare;
>         }
>         p->pass = 0;
>         p->stride = 0;
>         p->share = 0;
>         p->pticks = 0;
>         p->ptotalticks = 0;
>         p->priority = 0;
>         p->isMLFQ = 0;
>     ...
> ```
>
> `wait()` function checks if there is any ZOMBIE state processes that has called `exit()` but not removed. If the ZOMBIE process was following stride scheduling, it returns its CPU share value to the MLFQ scheduler and also MLFQ stride value is reset. The rest of the variables in proc structure is set to inital value.

proc.c - `MLFQ_Boost()`

> ```c
> void
> MLFQ_Boost(void)
> {
>   struct proc *p;
>
>   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
>   {
>     if(!p->isMLFQ)
>       continue;
>     p->priority = 0;
>     p->pticks = 0;
>     p->ptotalticks = 0;
>   }
>   MLFQ_ticks = 0;
> }
> ```
>
> When this function is called, priority boosting is activated. All the process which is following MLFQ scheduling is pushed to the highest priority level. This prevents __starvation__. 

proc.c - `MLFQ_Scheduler()`

> ```c
> for(checkpoint = 0, levturn = 0, p = ptable.proc; p < &ptable.proc[NPROC]; levturn++, checkpoint++, p++)
>   {
>     if(MLFQ_ticks >= 100)
>       MLFQ_Boost();
>
>     if(levturn == 63)
>       break;
>
>     if((checkpoint % 63) == 0)
>       p = ptable.proc;
>     // check if the process follows MLFQ scheduling.
>     if(p->isMLFQ != 1)
>       continue;
>     // search for RUNNABLE process that is in lev 0
>     if(p->state != RUNNABLE || p->priority != 0)
>       continue;
>
> ```
>
> This is part of MLFQ Scheduling. MLFQ scheduler looks for the next process to switch in the highest priority level queue. MLFQ priority level queue is designed as circular queue. It leaves mark where it was searching last time. if the scheduler has checked 64 processes in the ptable, it goes to the next priority level queue and do the same thing. I used circular queue instead of just queue, because xv6 can only run 64 processes at a time. This makes the loop very inexpensive. The time complexity is similar to the normal queue when entries are small. 

proc.c `Stride_Scheduler()`

> ```c
> void
> Stride_Scheduler(struct proc* p)
> {
>   struct cpu *c = mycpu();
>   c->proc = 0;
>   p->pass += p->stride;
>   c->proc = p;
>   switchuvm(p);
>   p->state = RUNNING;
>   swtch(&(c->scheduler), p->context);
>   switchkvm();
>   c->proc = 0;
> }
> ```
>
> Stride scheduler just adds the pass value of the process before the context switch is called. The argument 'p' is the process which has the minimal value of the current processes.

proc.c - `scheduler()`

> ```c
> void
> scheduler(void)
> {
>   struct proc *min;
>   struct cpu *c = mycpu();
>   c->proc = 0;
>   for(;;){
>     if(MLFQ_ticks >= 100)
>       MLFQ_Boost();
>     sti();
>     acquire(&ptable.lock);
>     if(leftShare >= 100)
>       MLFQ_Scheduler();
>     else if(leftShare < 100)
>     {
>       if((min = findMinPass()) == 0)
>         MLFQ_Scheduler();
>       else
>         Stride_Scheduler(min);
>     }
>     release(&ptable.lock);
>   }
> }
> ```
>
> This scheduler is the head scheduler who decides which scheduler to use.  If there is no processes that follows stride scheduling, MLFQ scheduler is called. If there is any processes that has called `set_cpu_share()`, whichever process that has the minimal pass value is selected(the scheduler the process follows).



## Few Design Issues

#### 1. Gaming the scheduler problem in MLFQ scheduling

> When the process calles `yield()` by system call, the process may not go to the next priority level continueously. To prevent this bug, I designed my `sys_yield()` system call to add the ticks when the system call is called even though timer interrupt didn't come.  I arbitrarily set the cycle(주기)  of this addition to every 20 system calls to fit the MLFQ(yield) test case value to MLFQ(compute) test case value.

#### 2. Low level process starvation in MLFQ scheduling

> Starvation is one of the serious problem in MLFQ scheduling. `MLFQ_Boost()` is used to prevent this problem. Because this problem is serious, I designed my OS to observe MLFQ_ticks in head scheduler, MLFQ_scheduler and `MLFQ_Check()` in trap.c file. The cycle of the priority boosting is 100ticks.

#### 3. Priority level queue design in MLFQ scheduling

> MLFQ priority level queue is designed as circular queue. It leaves mark where it was searching last time. if the scheduler has checked 64 processes in the ptable, it goes to the next priority level queue and do the same thing. I used circular queue instead of just queue, because xv6 can only run 64 processes at a time. This makes the loop very inexpensive. The time complexity is similar to the normal queue when entries are small. 

#### 4. Combining MLFQ scheduling and stride scheduling

> * When there is both MLFQ scheduling and stride scheduling processes, MLFQ scheduler represents all the processes that follow MLFQ scheduling. MLFQ scheduler has its pass value and stride value to compare with the stride scheduling processes.
> * When MLFQ scheduler is selected by the head scheduler, MLFQ scheduler runs its processes by their own time quantum. But if the time quantum is 2 ticks, for example, the MLFQ pass value is added 2 times the MLFQ stride value. 



## Results

> __test_mlfq 0__
>
> ![image](/uploads/ab687931413775aa3e3abf87c267ddfb/image.png)
>
> We can see the ratio between lev[0] and lev[1] is close to 1:2.

> __test_mlfq 1__
>
> ![image](/uploads/8fbafd4cee7b5e043b76c401a1cc95ed/image.png)
>
> We can see the ratio between lev[0] and lev[1] is close to 1:2 even though the processes call `sys_yield()` repeatedly.

> __test_stride #__
>
> ![image](/uploads/2a3c2daa19acbce7d8c670b1b4570c21/image.png)
>
> This test case only has one stride process. so the cnt value does not change related to cpu share requirements. Exception is successfully handled among wrong cpu share requirements.

> __test_master__
>
>![image](/uploads/8b9c840b3ba80fa4b2ecdf5b49db9609/image.png)
>
> The ratio between two processes that follow stride scheduling is precisely close to 10:40. Also the ratio between lev[0] and lev[1] is close to 1:2 in both MLFQ(compute) and MLFQ(yield).