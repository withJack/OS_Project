#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

double leftShare = 100;

int
set_cpu_share(int share)
{
    // check if the input share is 0 or the total share is under 20%
    if((leftShare - share) < 20)
        return -1;
    if(share == 0)
        return -1;

    // proc.c
    cpu_share(share);

    return share;
}


// Wrapper for set_cpu_share
int
sys_set_cpu_share(void)
{
    int share;

    // Decode argument using argint
    if(argint(0, &share) < 0)
        return -1;

    return set_cpu_share(share);
}
