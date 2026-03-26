#pragma once

#include "MagickImage.h"

inline void InitImageLibrary(const char* path = nullptr)
{
    Magick::InitializeMagick(path);
}

inline void ShutdownImageLibrary()
{
    // ImageMagick doesn't require explicit shutdown
}
