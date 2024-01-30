# SP Homework
This my homeworks from System Programming, I took this course in Fall 2023.

## Hw1 : Locks & IO Multiplexing
This homework requires us to build a bulletin system, with client and server interacting.
Servers has to support the requirements from multiple clients, thus IO multiplexing is required.
There are multiple servers, thus we need lock to prevent race condition.

## HW2 : fork & IPC
This homework requires us to implement a hierarchical service management system.
There are multiple processes, we have to use different kind of IPC.
We can only use one process as the manager, which is the only process that we can type in.

There are three operations : Spawn, Kill, Exchange.
We have to fork or kill indicated processes, or exchange secrets using FIFO.

## HW3 : Signal & longjmp
This homework requires us to implement a user-level thread system.
We use user mode to simulate a thread system using signal and longjmp.
We have to implement signal handler and worker function.

## HW4 : Threads
This homework requires us to implement a thread pool.
We use the pthread function provided by POSIX to implement a thread management system.
