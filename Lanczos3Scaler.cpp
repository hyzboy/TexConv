#include "Lanczos3Scaler.h"
#include <algorithm>
#include <memory>
#include <cstring>

float Lanczos3Scaler::Lanczos3Kernel(float x) 
{
    if (x == 0.0f) return 1.0f;
    
    float absX = std::abs(x);
    if (absX >= 3.0f) return 0.0f;
    
    const float pi = 3.14159265358979323846f;
    float piX = pi * absX;
    float pi3X = piX / 3.0f;
    
    return (3.0f * std::sin(piX) * std::sin(pi3X)) / (piX * piX);
}

std::vector<float> Lanczos3Scaler::ConvertToFloat(const void* data, int width, int height, 
                                                 int channels, ILuint type) 
{
    int totalPixels = width * height * channels;
    std::vector<float> result(totalPixels);
    
    switch (type) {
        case IL_UNSIGNED_BYTE: {
            const unsigned char* src = static_cast<const unsigned char*>(data);
            for (int i = 0; i < totalPixels; ++i) {
                result[i] = src[i] / 255.0f;
            }
            break;
        }
        case IL_UNSIGNED_SHORT: {
            const unsigned short* src = static_cast<const unsigned short*>(data);
            for (int i = 0; i < totalPixels; ++i) {
                result[i] = src[i] / 65535.0f;
            }
            break;
        }
        case IL_HALF: {
            // DevIL half precision floats - convert to float
            const unsigned short* src = static_cast<const unsigned short*>(data);
            for (int i = 0; i < totalPixels; ++i) {
                // Basic half to float conversion
                unsigned short h = src[i];
                unsigned int sign = (h >> 15) & 0x1;
                unsigned int exp = (h >> 10) & 0x1f;
                unsigned int frac = h & 0x3ff;
                
                unsigned int f;
                if (exp == 0) {
                    if (frac == 0) {
                        f = sign << 31;
                    } else {
                        // Denormalized
                        exp = 1;
                        while ((frac & 0x400) == 0) {
                            frac <<= 1;
                            exp--;
                        }
                        frac &= 0x3ff;
                        f = (sign << 31) | ((exp + 112) << 23) | (frac << 13);
                    }
                } else if (exp == 31) {
                    f = (sign << 31) | 0x7f800000 | (frac << 13);
                } else {
                    f = (sign << 31) | ((exp + 112) << 23) | (frac << 13);
                }
                result[i] = *reinterpret_cast<float*>(&f);
            }
            break;
        }
        case IL_FLOAT: {
            const float* src = static_cast<const float*>(data);
            std::copy(src, src + totalPixels, result.begin());
            break;
        }
        default:
            // Fallback - treat as unsigned byte
            const unsigned char* src = static_cast<const unsigned char*>(data);
            for (int i = 0; i < totalPixels; ++i) {
                result[i] = src[i] / 255.0f;
            }
            break;
    }
    
    return result;
}

void* Lanczos3Scaler::ConvertFromFloat(const std::vector<float>& floatData, int width, int height,
                                      int channels, ILuint type) 
{
    int totalPixels = width * height * channels;
    
    switch (type) {
        case IL_UNSIGNED_BYTE: {
            unsigned char* result = new unsigned char[totalPixels];
            for (int i = 0; i < totalPixels; ++i) {
                float val = std::clamp(floatData[i], 0.0f, 1.0f);
                result[i] = static_cast<unsigned char>(val * 255.0f + 0.5f);
            }
            return result;
        }
        case IL_UNSIGNED_SHORT: {
            unsigned short* result = new unsigned short[totalPixels];
            for (int i = 0; i < totalPixels; ++i) {
                float val = std::clamp(floatData[i], 0.0f, 1.0f);
                result[i] = static_cast<unsigned short>(val * 65535.0f + 0.5f);
            }
            return result;
        }
        case IL_HALF: {
            unsigned short* result = new unsigned short[totalPixels];
            for (int i = 0; i < totalPixels; ++i) {
                float f = floatData[i];
                unsigned int fi = *reinterpret_cast<unsigned int*>(&f);
                
                unsigned int sign = (fi >> 31) & 0x1;
                unsigned int exp = (fi >> 23) & 0xff;
                unsigned int frac = fi & 0x7fffff;
                
                unsigned short h;
                if (exp == 0) {
                    h = sign << 15;
                } else if (exp == 255) {
                    h = (sign << 15) | 0x7c00 | (frac >> 13);
                } else {
                    int newExp = exp - 127 + 15;
                    if (newExp >= 31) {
                        h = (sign << 15) | 0x7c00;
                    } else if (newExp <= 0) {
                        h = sign << 15;
                    } else {
                        h = (sign << 15) | (newExp << 10) | (frac >> 13);
                    }
                }
                result[i] = h;
            }
            return result;
        }
        case IL_FLOAT: {
            float* result = new float[totalPixels];
            std::copy(floatData.begin(), floatData.end(), result);
            return result;
        }
        default: {
            // Fallback - treat as unsigned byte
            unsigned char* result = new unsigned char[totalPixels];
            for (int i = 0; i < totalPixels; ++i) {
                float val = std::clamp(floatData[i], 0.0f, 1.0f);
                result[i] = static_cast<unsigned char>(val * 255.0f + 0.5f);
            }
            return result;
        }
    }
}

std::vector<float> Lanczos3Scaler::ScaleFloatData(const std::vector<float>& src,
                                                 int srcWidth, int srcHeight,
                                                 int dstWidth, int dstHeight,
                                                 int channels) 
{
    std::vector<float> result(dstWidth * dstHeight * channels);
    
    // Scale factors
    float scaleX = static_cast<float>(srcWidth) / dstWidth;
    float scaleY = static_cast<float>(srcHeight) / dstHeight;
    
    // Kernel radius
    const int radius = GetKernelRadius();
    
    for (int dstY = 0; dstY < dstHeight; ++dstY) {
        for (int dstX = 0; dstX < dstWidth; ++dstX) {
            // Map destination pixel to source space
            float srcCenterX = (dstX + 0.5f) * scaleX - 0.5f;
            float srcCenterY = (dstY + 0.5f) * scaleY - 0.5f;
            
            // Calculate kernel bounds
            int srcMinX = static_cast<int>(std::floor(srcCenterX - radius + 1));
            int srcMaxX = static_cast<int>(std::floor(srcCenterX + radius));
            int srcMinY = static_cast<int>(std::floor(srcCenterY - radius + 1));
            int srcMaxY = static_cast<int>(std::floor(srcCenterY + radius));
            
            // Clamp to source image bounds
            srcMinX = std::max(0, srcMinX);
            srcMaxX = std::min(srcWidth - 1, srcMaxX);
            srcMinY = std::max(0, srcMinY);
            srcMaxY = std::min(srcHeight - 1, srcMaxY);
            
            // Calculate weighted sum for each channel
            for (int ch = 0; ch < channels; ++ch) {
                float sum = 0.0f;
                float weightSum = 0.0f;
                
                for (int srcY = srcMinY; srcY <= srcMaxY; ++srcY) {
                    for (int srcX = srcMinX; srcX <= srcMaxX; ++srcX) {
                        float weightX = Lanczos3Kernel(srcX - srcCenterX);
                        float weightY = Lanczos3Kernel(srcY - srcCenterY);
                        float weight = weightX * weightY;
                        
                        int srcIndex = (srcY * srcWidth + srcX) * channels + ch;
                        sum += src[srcIndex] * weight;
                        weightSum += weight;
                    }
                }
                
                // Normalize and store result
                int dstIndex = (dstY * dstWidth + dstX) * channels + ch;
                result[dstIndex] = (weightSum > 0.0f) ? (sum / weightSum) : 0.0f;
            }
        }
    }
    
    return result;
}

void* Lanczos3Scaler::ScaleImage(const void* srcData, 
                                int srcWidth, int srcHeight, 
                                int srcChannels, ILuint srcType,
                                int dstWidth, int dstHeight) 
{
    if (!srcData || srcWidth <= 0 || srcHeight <= 0 || 
        dstWidth <= 0 || dstHeight <= 0 || srcChannels <= 0 || srcChannels > 4) {
        return nullptr;
    }
    
    // If no scaling needed, just copy the data
    if (srcWidth == dstWidth && srcHeight == dstHeight) {
        size_t dataSize = srcWidth * srcHeight * srcChannels;
        switch (srcType) {
            case IL_UNSIGNED_BYTE: dataSize *= sizeof(unsigned char); break;
            case IL_UNSIGNED_SHORT: case IL_HALF: dataSize *= sizeof(unsigned short); break;
            case IL_FLOAT: dataSize *= sizeof(float); break;
            default: dataSize *= sizeof(unsigned char); break;
        }
        
        void* result = std::malloc(dataSize);
        if (result) {
            std::memcpy(result, srcData, dataSize);
        }
        return result;
    }
    
    // Convert to float
    std::vector<float> floatData = ConvertToFloat(srcData, srcWidth, srcHeight, srcChannels, srcType);
    
    // Scale using Lanczos3
    std::vector<float> scaledData = ScaleFloatData(floatData, srcWidth, srcHeight, 
                                                  dstWidth, dstHeight, srcChannels);
    
    // Convert back to original format
    return ConvertFromFloat(scaledData, dstWidth, dstHeight, srcChannels, srcType);
}