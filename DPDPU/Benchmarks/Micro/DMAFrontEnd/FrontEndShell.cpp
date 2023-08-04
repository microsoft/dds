#include <iostream>
#include "FrontEnd.h"

int main() {
    FrontEnd frontEnd;
    size_t capacity = 1024 * 1024 * 32;

    std::cout << "Connecting to the back end" << std::endl;
    if (!frontEnd.Connect()) {
        std::cout << "Failed to connect" << std::endl;
        return -1;
    }

    std::cout << "Connected to the back end" << std::endl;

    std::cout << "Allocating " << capacity << " bytes of DMA buffer..." << std::endl;
    char* dmaBuffer = NULL;
    dmaBuffer = frontEnd.AllocateBuffer(capacity);
    if (dmaBuffer) {
        std::cout << "DMA buffer has been allocated" << std::endl;
    }
    else {
        std::cout << "DMA buffer allocation failed" << std::endl;
    }

    frontEnd.DeallocateBuffer();

    frontEnd.Disconnect();
}