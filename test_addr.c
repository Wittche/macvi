#include <stdio.h>
#include <dlfcn.h>
int main() {
    void* handle = dlopen("./build/lib/libFEXCore_shared.dylib", RTLD_NOW);
    void* func = dlsym(handle, "FEX_Initialize");
    printf("FEX_Initialize is at %p\n", func);
    return 0;
}
