# FILE SYSTEM PROJECT

##### Lee Seung Jae (이승재)

##### 2014004948 Hanyang Univ. CSE

## Milestone 1

### 1. Original xv6 implementation about file system

> ![image](/uploads/30eef69cbb6415914c2a7a7a72a5565d/image.png)
>
> There is total 13 addresses to point data blocks. 12 addresses are pointing direct blocks. These can have at most size of a single block. A single block is size of 512 bytes. 1 address(index 12) is used for indirect block. As you can see on the image, this address can have 128 data blocks. 
>
> As total, original xv6 can have 12 + 128 data blocks. 



### 2. Huge File System (expanded maximum size of a file) 

> ![image](/uploads/ed5ff775db40f08f1a1b810a337d4e76/image.png)
>
> (image from scalable computing systems laboratory)
>
> Number of directly linked blocks is 10. 
>
> Index of single indirect is 10. It can have 128 data blocks.
>
> Index of doubly indirect is 11. It can have 128 * 128 data blocks.
>
> Index of triple indirect is 12. It can have 128 * 128 * 128 data blocks. 
>
> As total, newly implemented file system can hold at most 10 + 128 + 128^2 + 128^3 data blocks. This is 2,113,674 data blocks, and is size of 1,082,201,088 bytes. 



### 3. Implementation

#### param.h

> ```#define FSSIZE 40000``` 
>
> ---
>
> FSSIZE is the maximum number of data blocks that a single file can have. 
>
> This means a file can have size of 40000 * 512 bytes (20MB) at the maximum. 

####file.h

> ```uint addrs[NDIRECT+3]``` 
>
> ---
>
> `inode` and `dinode` structure has uint array call addrs, size of 13(0-12).

#### fs.h

> ``` #define NDIRECT 10```
>
> ``` #define NINDIRECT (BSIZE / sizeof(uint))```
>
> ```#define NDOUBINDIRECT (NINDIRECT * NINDIRECT)```
>
> ``` #define NTRIPINDIRECT (NDOUBINDIRECT * NINDIRECT)```
>
> ``` #define MAXFILE (NDIRECT + NINDIRECT + NDOUBINDIRECT + NTRIPINDIRECT)```
>
> ---
>
> These are additional constants needed for functions in `fs.c`. 
>
> ```NDIRECT``` is used for index of addresses in inode. 
>
> ```BSIZE``` is single block size, which is 512.
>
> ```MAXFILE``` is the total data blocks that the file system has. 

#### fs.c

> ```static uint bmap(struct inode *ip, uint bn)```
>
> ```static void itrunc(struct inode *ip)```
>
> ---
>
> ```bmap()``` function is to acheive the disk block address. If there is no such block, `bmap()` allocates one. Original `bmap()` was capable of block number less than 128. I implemented `bmap()` to handle block numbers up to `NTIRPINDIRECT` which is 128 * 128 * 128.
>
> ```itrunc()``` function truncates(discard) inodes. It is called when inode has no links to it, which means no directory entries referring to it. Original ```itrunc()``` freed data blocks of direct and indirect blocks. I used `for` loop to implement `itrunc()` to free data blocks of  doubly indirect and triple indirect blocks in addition to the original xv6 `itrunc()`. 



### 4. Test results

#### create test

> ![create](/uploads/7e8777e9cc46bbc6cb02c3da8ea69eac/create.png)

#### read test

> ![read](/uploads/062071296b7853e0f4dc660517387168/read.png)

#### stress tests

> ![stress0](/uploads/aa4b22b6b721f10f07f2668e23258c2d/stress0.png)
>
> ![stress1](/uploads/2a4d2f6aa66377fb56a6e5985a9ed886/stress1.png)
>
> ![stress2](/uploads/e2bbd66476c7ae2cdfff0c025d428e5c/stress2.png)
>
> ![stress3](/uploads/1b395bf8b5d513c268c4ebb35a460c67/stress3.png)
>
> ![stress4](/uploads/b888866b85d8f3ceae0fedaef832aee2/stress4.png)



### 5. Little Bug...?

> #### main-loop: WARNING: I/O thread spun for 1000 iterations.
>
> Above sign is printed sometimes when the test cases are executed. 
>
> It seems there is problem with handling I/O either in xv6 or qemu. 
>
> This problem does not have any relation with FILE SYSTEM PROJECT. 



## Milestone 2

### Linux(POSIX) pread, pwrite implementation.

> `pread(int fd, void* buf, size_t count, off_t offset)`
>
> `pread()` reads up to count bytes from file descriptor fd at offset into the buffer starting at buf. The file offset is not changed
>
> `pwrite(int fd, void* buf, size_t count, off_t offset) `
>
> `pwrite()` writes up to count bytes from the buffer starting at buf to the file descriptor fd at offset. The file offset is not changed.



### Implementation of pread, pwrite system call in xv6.

#### `pwrite(int fd, void* addr, int n, int off);`

> When`pwrite()` system call is called, OS calls the function called `pfilewrite()` in file.c. `pfilewrite()` has 4 arguments. file structure pointer, address, two integer variables. This function get specific offset value from the user. Offset is passed on to `writei()` function which actually writes on physical data. Not like `write()` system call, it does not change the files' offset.  
>
> In `write()`, if the offset is bigger than the size of inode, it updates inode size using `iupdate(ip)` function. 

#### `pread(int fd, void* addr, int n, int off);`

> When `pread()` system call is called, OS calls the function called `pfileread()` in file.c. `pfileread()` gets offset value from user and pass it on to `readi()` function. `readi()` reads the physical data. Difference between `read()` and `pread()` system call is that `pread()` does not move the files' offset.



### Testcode results

![pwritetest](/uploads/2e213b47039a2282a247199c8c0e6116/pwritetest.png)

> The testcode provided by other classmate by piazza, checks if `pread()`, `pwrite()` system call works. It simply makes 10 thread(0-9) and simultaneously writes and reads with separated offsets. When `pread()` test, it compares if `pwrite()` function has written data correctly. 
>
> After the pwritetest, myfile file has been created. 
>
> ![lsmyfile](/uploads/bfad38933bd9fbbc24fb7438d3bf350b/lsmyfile.png)



### Bug 

> When the user writes file with multiple threads, because threads doesn't garauntee order, it may write from the offset bigger than the file size. This is called __hole__ in linux. This hole may not be read correctly. Linux implemented this problem by filling zeros in hole. 


