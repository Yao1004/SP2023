#include "my_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <assert.h>
typedef long long LL;
long long cn = 0;
pthread_mutex_t cnm = PTHREAD_MUTEX_INITIALIZER;
void *Collatz(void *args) {
  LL x = *(LL *)args;
  LL cnt = 0;
  while (x != 1) {
    if (x & 1)
      x = x * 3 + 1;
    else
      x /= 2;
    cnt++;
    // try uncomment printf
    // printf("%lld\n", x);
  }
  // try uncomment printf
  // printf("%lld\n", cnt);
  pthread_mutex_lock(&cnm);
  cn++;
  pthread_mutex_unlock(&cnm);
  return NULL;
}
int main(int argc , char** argv) 
{
    struct tms tmsstart, tmsend;
    clock_t start, end;
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    start = times(&tmsstart);

    tpool *pool = tpool_init(N);
    LL *arg = malloc(M * sizeof(LL));
    for (int i = 0; i < M; i++) {
      arg[i] = 0x10000000ll + i;
      tpool_add(pool, Collatz, (void *)&arg[i]);
    }
    tpool_wait(pool);
    tpool_destroy(pool);
    free(arg);

    end = times(&tmsend);
    long clktck = sysconf(_SC_CLK_TCK);
    printf("threads = %d\n", atoi(argv[1]));
    printf("real time : %7.2f\n", (end-start)/ (double) clktck);
    printf("user time : %7.2f\n",  (tmsend.tms_utime - tmsstart.tms_utime) / (double) clktck);
    printf("sys  time : %7.2f\n\n",(tmsend.tms_stime - tmsstart.tms_stime) / (double) clktck);
    assert(cn == M);

}