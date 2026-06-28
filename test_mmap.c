#include <stdio.h>
#include <sys/mman.h>

int main() {
    void* addr = mmap((void*)0x10000, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }
    printf("mmap succeeded at %p\n", addr);
    return 0;
}
