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
