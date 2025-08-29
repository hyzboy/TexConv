#pragma once

#include <vector>
#include <cmath>
#include <IL/il.h>

/**
 * High-quality Lanczos3 image scaling implementation
 * Supports various pixel formats by converting to float, scaling, then converting back
 */
class Lanczos3Scaler 
{
public:
    /**
     * Scale image data using Lanczos3 algorithm
     * @param srcData Source image data
     * @param srcWidth Source image width
     * @param srcHeight Source image height
     * @param srcChannels Number of channels (1-4)
     * @param srcType DevIL type (IL_UNSIGNED_BYTE, IL_FLOAT, etc.)
     * @param dstWidth Target width
     * @param dstHeight Target height
     * @return Scaled image data (caller owns memory)
     */
    static void* ScaleImage(const void* srcData, 
                           int srcWidth, int srcHeight, 
                           int srcChannels, ILuint srcType,
                           int dstWidth, int dstHeight);

private:
    // Lanczos3 kernel function
    static float Lanczos3Kernel(float x);
    
    // Convert various formats to float array
    static std::vector<float> ConvertToFloat(const void* data, int width, int height, 
                                           int channels, ILuint type);
    
    // Convert float array back to original format
    static void* ConvertFromFloat(const std::vector<float>& floatData, int width, int height,
                                 int channels, ILuint type);
    
    // Core Lanczos3 scaling for float data
    static std::vector<float> ScaleFloatData(const std::vector<float>& src,
                                           int srcWidth, int srcHeight,
                                           int dstWidth, int dstHeight,
                                           int channels);
    
    // Get kernel support radius (3 for Lanczos3)
    static constexpr int GetKernelRadius() { return 3; }
};