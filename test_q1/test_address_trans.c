#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SYS_GET_PHY 449

unsigned long my_get_physical_addresses(void *va) {
    long ret = syscall(SYS_GET_PHY, va);
    return ret < 0 ? 0 : (unsigned long)ret;
}

int main(void) {
    const size_t PAGE = 4096;
    const size_t N = 4;
    const size_t len = N * PAGE;
    char *addr = malloc(len);
    if (!addr) { perror("malloc"); return 1; }

    printf("malloc() returned address = %p\n\n", addr);
    puts("Check VA -> PA BEFORE access:");

    for (size_t i = 0; i < len; i += PAGE) {
        unsigned long pa = my_get_physical_addresses(addr + i);
        printf("  VA: %p -> PA: 0x%lx\n", addr + i, pa);
    }

    puts("\nTouching memory (write 1 byte per page)...");
    for (size_t i = 0; i < len; i += PAGE) addr[i] = 42;

    puts("\nCheck VA -> PA AFTER access:");
    for (size_t i = 0; i < len; i += PAGE) {
        unsigned long pa = my_get_physical_addresses(addr + i);
        printf("  VA: %p -> PA: 0x%lx\n", addr + i, pa);
    }

    return 0;
}
