#include <stdio.h>
#include "my_syscall_p2.h"

int main(void)
{
    struct my_thread_info_record data;

    int ret = my_get_thread_kernel_info(&data);
    if (ret) {
        printf("=== Single-thread test ===\n");
        printf("pid                      = %lu\n",  data.pid);
        printf("tgid                     = %lu\n",  data.tgid);
        printf("process descriptor addr  = %p\n",   data.process_descriptor_address);
        printf("kernel mode stack addr   = %p\n",   data.kernel_mode_stack_address);
        printf("pgd table addr           = %p\n",   data.pgd_table_address);
    } else {
        printf("Cannot execute the new system call correctly\n");
    }

    return 0;
}
