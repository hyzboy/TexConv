#include <iostream>
#include <vector>
#include "Lanczos3Scaler.h"

void testPixelFormat(const char* formatName, ILuint ilType, int bytesPerPixel) {
    std::cout << "Testing " << formatName << "..." << std::endl;
    
    const int width = 2, height = 2, channels = 4;
    const int srcSize = width * height * channels * bytesPerPixel;
    
    // Create test data based on format
    std::vector<unsigned char> testData(srcSize);
    
    if (ilType == IL_UNSIGNED_BYTE) {
        unsigned char* data = testData.data();
        // Create gradient pattern
        for (int i = 0; i < width * height * channels; i++) {
            data[i] = (i % 255);
        }
    } else if (ilType == IL_UNSIGNED_SHORT) {
        unsigned short* data = reinterpret_cast<unsigned short*>(testData.data());
        for (int i = 0; i < width * height * channels; i++) {
            data[i] = (i % 65535);
        }
    } else if (ilType == IL_FLOAT) {
        float* data = reinterpret_cast<float*>(testData.data());
        for (int i = 0; i < width * height * channels; i++) {
            data[i] = (float)(i % 100) / 100.0f;
        }
    } else if (ilType == IL_HALF) {
        // For half precision, just use simple pattern
        unsigned short* data = reinterpret_cast<unsigned short*>(testData.data());
        for (int i = 0; i < width * height * channels; i++) {
            data[i] = 0x3C00 + (i % 1024); // Base half-float + offset
        }
    }
    
    // Scale using Lanczos3
    void* scaledData = Lanczos3Scaler::ScaleImage(
        testData.data(), width, height, channels, ilType, 4, 4);
    
    if (scaledData) {
        std::cout << "  ✓ " << formatName << " scaling successful!" << std::endl;
        std::free(scaledData);
    } else {
        std::cout << "  ✗ " << formatName << " scaling failed!" << std::endl;
    }
}

int main() {
    std::cout << "Testing Lanczos3 with different pixel formats..." << std::endl;
    
    testPixelFormat("8-bit RGBA", IL_UNSIGNED_BYTE, 1);
    testPixelFormat("16-bit RGBA", IL_UNSIGNED_SHORT, 2);
    testPixelFormat("Half-float RGBA", IL_HALF, 2);
    testPixelFormat("Float RGBA", IL_FLOAT, 4);
    
    std::cout << "\nTesting edge cases..." << std::endl;
    
    // Test no scaling (same size)
    unsigned char simpleData[] = {255, 0, 0, 255};
    void* noScale = Lanczos3Scaler::ScaleImage(
        simpleData, 1, 1, 4, IL_UNSIGNED_BYTE, 1, 1);
    if (noScale) {
        std::cout << "  ✓ No-scale case handled correctly" << std::endl;
        std::free(noScale);
    }
    
    // Test invalid parameters
    void* invalid = Lanczos3Scaler::ScaleImage(
        nullptr, 1, 1, 4, IL_UNSIGNED_BYTE, 2, 2);
    if (!invalid) {
        std::cout << "  ✓ Invalid parameters handled correctly" << std::endl;
    }
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}