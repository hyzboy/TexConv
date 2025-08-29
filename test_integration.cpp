#include <iostream>
#include <IL/il.h>
#include "Lanczos3Scaler.h"

// Mock data types for testing
typedef unsigned int uint;

class TestILImage {
private:
    uint il_width, il_height, il_depth;
    uint il_format, il_type;
    uint channel_count;
    
public:
    TestILImage() : il_width(4), il_height(4), il_depth(1), 
                   il_format(IL_RGBA), il_type(IL_UNSIGNED_BYTE), 
                   channel_count(4) {}
    
    bool ResizeLanczos3(uint nw, uint nh) {
        if(nw==il_width&&nh==il_height)return(true);
        if(nw==0||nh==0)return(false);

        // Create dummy source data
        unsigned char testData[64]; // 4x4 RGBA
        for(int i = 0; i < 64; i++) {
            testData[i] = (i % 4 == 3) ? 255 : (i % 4) * 85; // Dummy pattern
        }
        
        // Scale using Lanczos3
        void* scaledData = Lanczos3Scaler::ScaleImage(testData, 
                                                     il_width, il_height, 
                                                     channel_count, il_type,
                                                     nw, nh);
        
        if (!scaledData) return false;
        
        std::cout << "Scaled from " << il_width << "x" << il_height 
                  << " to " << nw << "x" << nh << std::endl;
        
        // In real implementation, we would call ilTexImage here
        std::free(scaledData);
        
        il_width = nw;
        il_height = nh;
        
        return true;
    }
    
    uint width() const { return il_width; }
    uint height() const { return il_height; }
};

int main() {
    std::cout << "Testing ILImage integration with Lanczos3..." << std::endl;
    
    TestILImage image;
    std::cout << "Original size: " << image.width() << "x" << image.height() << std::endl;
    
    // Test resize
    if (image.ResizeLanczos3(8, 8)) {
        std::cout << "Success! New size: " << image.width() << "x" << image.height() << std::endl;
    } else {
        std::cout << "Failed to resize!" << std::endl;
    }
    
    return 0;
}