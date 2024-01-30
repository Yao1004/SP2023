#include "threadtools.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Print out the signal you received.
 * If SIGALRM is received, reset the alarm here.
 * This function should not return. Instead, call siglongjmp(sched_buf, 1).
 */
void sighandler(int signo) {
    // TODO
    if( signo == SIGALRM )
    {
        printf("caught SIGALRM\n");
        alarm(timeslice);
    }
    else if (signo == SIGTSTP )
        printf("caught SIGTSTP\n");
    longjmp(sched_buf, 1);
}

/*
 * Prior to calling this function, both SIGTSTP and SIGALRM should be blocked.
 */
void scheduler() {
    // TODO
    int val = setjmp(sched_buf);
    /*printf("val : %d\nrq ", val);
    for(int i=0;i<rq_size;i++) printf("%d ", ready_queue[i]->id);
    printf("\nwq ");
    for(int i=0;i<wq_size;i++) printf("%d ", waiting_queue[i]->id);
    printf("\n\n");*/
    if(rq_size <= 0 && wq_size <= 0)
        return;
    if(val == 0)
    {
        rq_current = 0;
        longjmp(RUNNING->environment, 1);
    }
    // * Step 1 & 2
    if(bank.lock_owner == -1 && wq_size != 0)
    {
        //printf("give lock\n");
        ready_queue[rq_size] = waiting_queue[0];
        bank.lock_owner = ready_queue[rq_size]->id;
        rq_size++;
        wq_size--;
        for(int i=0;i<wq_size;i++)
            waiting_queue[i] = waiting_queue[i+1];
    }
    // * Step 3 & 4
    if(val == 2)
    {
        waiting_queue[wq_size] = RUNNING;
        wq_size++;
        RUNNING = ready_queue[rq_size-1];
        rq_size--;
    }
    else if(val == 3)
    {
        free(RUNNING);
        RUNNING = ready_queue[rq_size-1];
        rq_size--;
    }
    //printf("ans:%d %d\n", rq_size, wq_size);
    // * Step 6
    if(rq_size <= 0 && wq_size <= 0)
        return;
    // * Step 5
    if(val == 1)
    {
        if(rq_current == rq_size-1)
            rq_current = 0;
        else
            rq_current++;
        longjmp(RUNNING->environment, 1);
    }
    else if(val == 2 || val == 3)
    {
        if(rq_current == rq_size)
            rq_current = 0;
        longjmp(RUNNING->environment, 1);
    }
}

    