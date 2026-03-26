#pragma once

#include "MagickImage.h"

using ImageLoader = MagickImage;

inline void InitImageLibrary(const char* path = nullptr)
{
    Magick::InitializeMagick(path);
}

inline void ShutdownImageLibrary()
{
    // ImageMagick doesn't require explicit shutdown
}

constexpr unsigned int IMAGE_UNSIGNED_BYTE = IL_UNSIGNED_BYTE;
constexpr unsigned int IMAGE_UNSIGNED_SHORT = IL_UNSIGNED_SHORT;
constexpr unsigned int IMAGE_FLOAT = IL_FLOAT;
constexpr unsigned int IMAGE_HALF = IL_HALF;
