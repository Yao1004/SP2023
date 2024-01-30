#include "threadtools.h"
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void fibonacci(int id, int arg) {
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) {
        if (RUNNING->i <= 2)
            RUNNING->x = RUNNING->y = 1;
        else {
            /* We don't need to save tmp, so it's safe to declare it here. */
            int tmp = RUNNING->y;
            RUNNING->y = RUNNING->x + RUNNING->y;
            RUNNING->x = tmp;
        }
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);   
        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void factorial(int id, int arg){
    // TODO
    thread_setup(id, arg);

    for (RUNNING->i = 1; ; RUNNING->i++) 
    {
        if (RUNNING->i <= 1)
            RUNNING->y = 1;
        else 
            RUNNING->y *= RUNNING->i;
        printf("%d %d\n", RUNNING->id, RUNNING->y);
        sleep(1);

        if (RUNNING->i == RUNNING->arg) {
            thread_exit();
        }
        else {
            thread_yield();
        }
    }
}

void bank_operation(int id, int arg) {
    // TODO
    thread_setup(id, arg);

    // * Step1
    lock();
    printf("%d acquired the lock\n", RUNNING->id);
    sleep(1);
    thread_yield();

    //printf("check:%d %d\n", RUNNING->id, RUNNING->arg);
    // * Step2
    if(RUNNING->arg > 0 || (RUNNING->arg <= 0 && bank.balance >= -RUNNING->arg))
    {
        bank.balance += RUNNING->arg;
        printf("%d %d %d\n", RUNNING->id, bank.balance-RUNNING->arg, bank.balance);
    }
    else
        printf("%d %d %d\n", RUNNING->id, bank.balance, bank.balance);
    sleep(1);
    thread_yield();

    // * Step3
    unlock();
    printf("%d released the lock\n", RUNNING->id);
    sleep(1);

    thread_exit();
}