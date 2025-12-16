#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/syscall.h>   // __NR_gettid
#include <unistd.h>
#include "my_syscall_p2.h"

struct data_ {
    int  id;
    char name[16];
};
typedef struct data_ sdata;

/* thread-local 變數示範：每個 thread 有自己的 tx */
static __thread sdata tx;

/* 給每個 thread 用來裝 syscall 回傳資料 */
static __thread struct my_thread_info_record info;

void hello(int tid)
{
    int ret = my_get_thread_kernel_info(&info);
    if (ret) {
        printf("  [tid=%d, name=%s]\n", tid, tx.name);
        printf("    pid                      = %lu\n",  info.pid);
        printf("    tgid                     = %lu\n",  info.tgid);
        printf("    process descriptor addr  = %p\n",   info.process_descriptor_address);
        printf("    kernel mode stack addr   = %p\n",   info.kernel_mode_stack_address);
        printf("    pgd table addr           = %p\n",   info.pgd_table_address);
    } else {
        printf("  [tid=%d] Cannot execute the new system call correctly\n", tid);
    }
}

void *func1(void *arg)
{
    char *p = (char *)arg;
    int tid = (int)syscall(__NR_gettid);

    tx.id = tid;
    strncpy(tx.name, p, sizeof(tx.name) - 1);
    tx.name[sizeof(tx.name) - 1] = '\0';

    printf("I am thread with ID %d executing func1().\n", tid);
    hello(tid);

    while (1) {
        // 可以開下面這行看 TLS 變數
        // printf("(func1) (%d)(%s)\n", tx.id, tx.name);
        sleep(1);
    }
    return NULL;
}

void *func2(void *arg)
{
    char *p = (char *)arg;
    int tid = (int)syscall(__NR_gettid);

    tx.id = tid;
    strncpy(tx.name, p, sizeof(tx.name) - 1);
    tx.name[sizeof(tx.name) - 1] = '\0';

    printf("I am thread with ID %d executing func2().\n", tid);
    hello(tid);

    while (1) {
        // printf("(func2) (%d)(%s)\n", tx.id, tx.name);
        sleep(2);
    }
    return NULL;
}

int main(void)
{
    pthread_t id[2];
    char p[2][16];

    /* 建 thread1 */
    strncpy(p[0], "Thread1", sizeof(p[0]) - 1);
    p[0][sizeof(p[0]) - 1] = '\0';
    pthread_create(&id[0], NULL, func1, (void *)p[0]);

    /* 建 thread2 */
    strncpy(p[1], "Thread2", sizeof(p[1]) - 1);
    p[1][sizeof(p[1]) - 1] = '\0';
    pthread_create(&id[1], NULL, func2, (void *)p[1]);

    /* main thread 自己也當一個 thread 來測 */
    int tid = (int)syscall(__NR_gettid);
    tx.id = tid;
    strncpy(tx.name, "MAIN", sizeof(tx.name) - 1);
    tx.name[sizeof(tx.name) - 1] = '\0';

    printf("I am main thread with ID %d.\n", tid);
    hello(tid);

    /* main 不要退出，不然整個 process 會結束 */
    while (1) {
        // printf("(MAIN) (%d)(%s)\n", tx.id, tx.name);
        sleep(5);
    }

    return 0;
}
