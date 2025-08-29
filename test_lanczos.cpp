#include <iostream>
#include <cmath>
#include "Lanczos3Scaler.h"

// Simple test data
void testLanczos3() {
    std::cout << "Testing Lanczos3 Scaler..." << std::endl;
    
    // Create simple 2x2 test image (4 pixels, RGBA)
    unsigned char testData[] = {
        255, 0, 0, 255,    // Red pixel
        0, 255, 0, 255,    // Green pixel  
        0, 0, 255, 255,    // Blue pixel
        255, 255, 0, 255   // Yellow pixel
    };
    
    // Scale 2x2 to 4x4
    void* scaledData = Lanczos3Scaler::ScaleImage(
        testData, 2, 2, 4, IL_UNSIGNED_BYTE, 4, 4);
    
    if (scaledData) {
        std::cout << "Lanczos3 scaling successful!" << std::endl;
        
        // Print some output pixels
        unsigned char* pixels = static_cast<unsigned char*>(scaledData);
        std::cout << "First pixel: R=" << (int)pixels[0] 
                  << " G=" << (int)pixels[1] 
                  << " B=" << (int)pixels[2] 
                  << " A=" << (int)pixels[3] << std::endl;
        
        delete[] pixels;
    } else {
        std::cout << "Lanczos3 scaling failed!" << std::endl;
    }
}

int main() {
    testLanczos3();
    return 0;
}