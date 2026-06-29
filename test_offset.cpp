#include <iostream>
#include <cstddef>
#include "FEX/FEXCore/include/FEXCore/Debug/InternalThreadState.h"
int main() {
    std::cout << "Offset of CurrentFrame: " << offsetof(FEXCore::Core::InternalThreadState, CurrentFrame) << std::endl;
    return 0;
}
