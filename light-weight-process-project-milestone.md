# Light-weight Process

##### Lee Seung Jae (이승재)

##### 2014004948 Hanyang Univ. CSE

## Milestone 1

### 1. Process / Thread

__What is process? -Linux__

>In a very basic form, Linux process can be visualized as running instance of a program. It is an abstraction of a running program. For example, linux shell is an process. As shown below, bash shell is on background as pid(process ID) 1821. 
>
>![1525523297999](/uploads/9b324c1acc8e3ffb86ad270f6748f381/1525523297999.png)
>
>Processes are fundamental concept in Linux as OS does work by process unit(단위). Kernel provides system resources in independent memory space such as stack, code, heap, etc. Each processes is handled and executed  by scheduler(xv6). By `fork()` and `exec()` function, process is created. 

__What is Thread? -Linux__

>Thread in Linux is a flow of execution of the process. If there is no other threads in a process, the process itself may be called main-thread. In Linux environment, thread is the unit for scheduling. When there is multiple execution flows(thread), it is known as multi-threaded process. The difference between multi-process and multi-thread is mainly at memory distribution. Every processes have independent memory resources. On the other hand, threads in a single process shares same memory like code, data, heap. Each Threads still has separate stack. For this reason, memory sharing and context-switching between threads are lighter than between processes. Disadvantage of multi-threading is that it can occur race conditions due to data sharing. 



### 2. POSIX thread (Pthread) -based on man page

>POSIX threads(abbreviated as Pthread) are light-weight processes initially implemented in UNIX. It allows parallel execution in UNIX-like OS. 

- `int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *),void *arg);`
  - The `pthread_create()` function starts a new thread in the calling process. The new thread starts execution by invoking `start_routine();` arg is passed as the sole argument of `start_routine();`.
  - `pthread_t *thread`: newly made thread is accessed by this argument when successfully created. 
  - `const pthread_attr_t *attr`: It defines threads' attribute. When it is given NULL, the attribute is set to default thread attribute.
  - `void *(*start_routine)(void *)`: The function which will be executed in the new thread. Needs to be given by pointer of the function.
  - `void *arg`: Argument or arguments tossed to the thread function(start_routine).
  - Return value: On success, `pthread_create();` returns 0; on error, it returns an error number, and the contents of `*thread` are undefined. 
  - The thread is terminated on following conditions.
    1. `pthread_exit();`
    2. return on `start_routine()`
    3. `exit()` from the threads on the same process.
    4. maybe more...

  

- `int pthread_join(pthread_t thread, void **ret_val);`

  - The `pthread_join()` function waits for the thread specified by `pthread_t thread` to terminate. If that thread has already terminated, then `pthread_join()` returns immediately.

  - `pthread_t thread`: The thread to wait. 

  - `void **ret_val`: If it is not NULL, saves the exit status of the target thread.

  - Return value: On success, returns 0; on error, it returns an errno.

    ​
  

- `void pthread_exit(void *ret_val);`

  - The `pthread_exit()` function terminates the calling thread and returns a value via `retval` that (if the thread is joinable) is available to another thread in the same process that calls `pthread_join(3)`. 

  - Only thread-specific data are released, and process-shared resources are not released. 

  - After the last thread in a process terminates, the process terminates as by calling `exit(3)` with an exit status of zero. At this point, process-shared resources are released.

    ​

### 3. Design Basic LWP Operations for xv6 

#### Additional System Calls

`int thread_create(thread_t * thread, void * (*start_routine)(void *), void *arg);`

`void thread_exit(void *retval);`

`int thread_join(thread_t thread, void **retval);`

#### Very Brief Design 

> When new thread is created by `pthread_create()`, it is updated on ptable in `allocproc()` function in proc.c. To maintain the same address space with the main-thread, new page table is not assigned additionally. Instead it points `pgdir` of the main-thread. For independent stack area, make new stack in memory, and redirect corresponding pointers. (planning on getting hints from original functions `fork()`, `exec()` and `exit()`)

###### _Followed designs are very temporary and are not perfect. It may be implemented differently. Additional designs will be updated as the project is proceeding._

## Milestone 2

#### Basic Concept of LWP 

> ![LWP](/uploads/239a22fc47c7a6f6fc5fb4e082dbb642/LWP.png)
>
> LWPs are created only by `thread_create()`. 
>
> If the process is created by `fork()`, their parent process is the process who called `fork()`. For LWP, it is the same. Their  parent process is the process who called `thread_create()`. The difference is that they have the same pid as their parent process. Instead, they have identical tid(thread ID). 
>
> In my design, tid is maintained by ttable, very similar to ptable from xv6. 

> ![proc](/uploads/af263ec4400f7961941e63c6b07502ff/proc.png)
>
> This is my proc structure. ```int retval``` is used for `thread_exit()`, `thread_join()`. retval is saved in each process. There are `isThread`, `isMainThread` for determining whether the process is main thread or LWP. `usz` is used for each start point of stack of processes. Rather, sz is used for start point of heap area(more explanation would be presented later on). `isThereExec` is used in `exec()` system call. `cntThread` is the number of processes that has same pid. It is only managed by main thread for less confusion. `totalshare` is variable for stride scheduling issues. It is total number of share for all processes that has same pid. 



#### Basic LWP Operations (create, exit, join +a)

#### +a

> In my design, I allocate 64 stack space for future lwps in `exec()`. So when shell is created by `exec()` in init, it creates 64 stack space using `allocuvm()`. Each stack space has size of 2 * PGSIZE. One area size of PGSIZE is used for stack space and the other is used for boundary protection using `clearpteu()`. Also, the `sz` proc structure variable is set on top of 64 stack space. `usz` is set the very first stack space of 64 stack space. 

##### `thread_create()`

> `int thread_create(thread_t * thread, void * (*start_routine)(void *), void *arg);`
>
> * My design does not allow LWPs create new LWPs. 
>
> First, it calls `allocproc()` just like regular processes. Copy pid and file status from parent process.  Then, it looks up ttable for new tid.  It requires its `usz`, which shows the process' stack space, using its tid. Since it has to start `start_routine` function, change its eip value to the function address. To hand over `arg`, we put fake return PC and `arg` into newly assigned stack space. Make new LWP RUNNABLE, change `thread` argument into tid. 

`thread_exit()`

>`void thread_exit(void *retval)`
>
>When exiting, LWP closes all open file stream. Save `retval` in proc structure variable `retval`. Change the state into ZOMBIE, and wakeup its parent to collect from `wait()` or `thread_join()`. 

`thread_join()`

> `int thread_join(thread_t thread, void **retval)`
>
> `thread_join()` is very much similar to `wait()`. The main thread sleeps until the same tid as `thread` is found ZOMBIE state. it changes `retval` to proc structure variable `retval` saved from `thread_exit()`. It returns its tid back to ttable. 
>
> * The difference between `wait()` and `thread_join()` is that it does not call `freevm()`. Only main thread free page directory at the end. 



#### Interaction with other services in xv6 (results)

##### System calls (recording to testcases)

##### 0. racing test

> result
>
> ![racingtest](/uploads/6c847f8134c7628ee727551796812989/racingtest.png)
>
> Because the test code didn't consider racing condition(no locks), the result is more than 10000000. `4 0` is just my debugging code. 

##### 1. basic test

> result
>
> ![basictest](/uploads/858af710b43173596327b0d6e486f720/basictest.png)
>
> Each number shows that LWPs is nicely created and scheduled(MLFQ scheduling). 

##### 2. join test 1, 2

> result
>
> ![jointest](/uploads/a171193197141a00dec880569a55b705/jointest.png)
>
> jointest 1 checks if `thread_join()` wait for each `thread_exit()` by retval. 
>
> jointest 2 checks if `thread_join()` works even after all lwps has called `thread_exit()`.

##### 3. stress test

> result
>
> ![stresstest](/uploads/7d2797c4a70d4e13b5a89fbf24d2e2d4/stresstest.png)
>
> This test checks if the LWP basic operations works continueously after many repetition. Key of this test is to allocate and deallocate stack space and heap space nicely.

##### 4. exit test 1, 2

>result 
>
>![exittest](/uploads/7617ba9a5d3bd303bb9fce899e39c5f1/exittest.png)
>
>exit test 1 checks if the LWPs are handled correctly when main thread calls `exit()` instead of LWPs calling `thread_exit()`. 
>
>exit test 2 checks same thing, but just when LWPs calls `exit()`
>
>key of this test is making exceptions in `exit()` and `wait()`. 
>
>I made a new function call `killsamepid()`. This function collects LWPs that has same pid. When main thread is ZOMBIE and is being collected in `wait()`, it also kills LWPs. This is necessary because the parent of LWPs can be ZOMBIE, which means LWPs can become zombie processes(고아 process). 
>
>In `exit()`, I made all the processes that has same pid into ZOMBIE. This way, whether the main thread or the LWPs call `exit()`, they both can kill all the processes that has same pid. 

##### 5. exec test

> result
>
> ![exectest](/uploads/cd72a27354de6391be677d9933eb2105/exectest.png)
>
> exec test checks the case when `exec()` is called by LWP. When `exec()` is called, it should kill all the other processes that has same pid and itself should become main thread(which means its parent process is threadtest process). I made exceptions in `exec()`, `exit()`, and `wait()`. I made all the other LWPs' parent process to threadtest (parent's parent) and make the original main thread into non-main thread. 
>
> Also, I handled page directory, since when `exec()` is called, the process is assigned new page directory. 
>
> this result seems good, but something I don't know is wrong. whenever I do other tests after exec test, the problem pops.

##### 6. sbrk test

> result
>
> ![sbrktest](/uploads/688e05e97d5ffede24c237ab266f9a00/sbrktest.png)
>
> This test is to check if the LWPs are sharing the virtual memory(except stack space). 

##### 7. kill test

> result
>
> ![killtest](/uploads/f02f54fc4e81561cc935d803006ea5a3/killtest.png)
>
> This test checks the case when one of the process calls `kill()`. When `kill()` is called, it needs to send kill sign to all the processes that has same pid. 

##### 8. pipe test

> result
>
> ![pipetest](/uploads/2103a263106e99092fd5643fff7c002a/pipetest.png)
>
> This test checks if pipe works in multi threading. 
>
> As the result shows, it doesn't work.  I surely did all `filedup()` and `fileclose()` in both `thread_create()` and `thread_exit()`. Also in `exit()`. 

##### 9. sleep test

> result
>
> ![sleeptest](/uploads/853499f255e97156bf7b214ae1dad555/sleeptest.png)
>
> This test checks if LWPs still dies in SLEEPING state. 

##### 10. stride test (1, 2, test_master from last project)

> result
>
> ![stridetest1](/uploads/079ff8e5074a7bfedb8fc7257c799cc0/stridetest1.png)
>
> ![stridetest2](/uploads/aae3c9fd43391e3a2fbf6de7bbcf30fa/stridetest2.png)
>
> stride test 1 calls `set_cpu_share()` before creation of LWPs. It checks if the share is distributed when `thread_create()` is called. 
>
> stride test 2 calls `set_cpu_share()` after all the creation of LWPs. It checks if the share is distributed to current LWPs. 
>
> My result has fine result of cpu sharing. But, the test wouldn't finish. I printed `procdump()` to find the bug. It seems the stridetest process wouldn't wakeup.
>
> result of last project test case 
>
> ![testmaster](/uploads/0dba3003c976c1f4578c49e1a36e74b0/testmaster.png)
>
> This is irrelevent to multi-threading, so it works fine.

##### 11. fork test 

> result
>
> ![forktest](/uploads/c9444cf24b4775423b575d9787c37783/forktest.png)
>
> This test checks for case when LWP calls `fork()`.
>
> When LWP calls `fork()`, the parent of newly forked process is the LWP who called the system call. 
>
> `printf()` function calls are not implemented with locks like `cprintf()`. so the result may seem awkward.
> 
> The number of parent and child printed is same(10). 



