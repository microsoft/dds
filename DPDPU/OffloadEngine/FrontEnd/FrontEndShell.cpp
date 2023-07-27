#include <iostream>
#include "FrontEnd.h"

int main() {
    FrontEnd frontEnd;
    size_t capacity = 1024 * 1024 * 32;

    std::cout << "Allocating " << capacity << " bytes of DMA buffer..." << std::endl;
    char* dmaBuffer = frontEnd.AllocateBuffer(capacity);
    if (dmaBuffer) {
        std::cout << "DMA buffer has been allocated" << std::endl;
    }
    else {
        std::cout << "DMA buffer allocation failed" << std::endl;
    }
}