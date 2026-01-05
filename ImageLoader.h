#pragma once

#ifdef USE_IMAGEMAGICK
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

    // Type definitions for compatibility
    constexpr unsigned int IMAGE_UNSIGNED_BYTE = IL_UNSIGNED_BYTE;
    constexpr unsigned int IMAGE_UNSIGNED_SHORT = IL_UNSIGNED_SHORT;
    constexpr unsigned int IMAGE_FLOAT = IL_FLOAT;
    constexpr unsigned int IMAGE_HALF = IL_HALF;

#else
    #include "ILImage.h"
    #include <IL/il.h>
    #include <IL/ilu.h>
    
    using ImageLoader = ILImage;
    
    inline void InitImageLibrary(const char* = nullptr)
    {
        ilInit();
        iluInit();
        iluImageParameter(ILU_FILTER, ILU_SCALE_MITCHELL);
    }
    
    inline void ShutdownImageLibrary()
    {
        ilShutDown();
    }

    // Type definitions for compatibility
    constexpr unsigned int IMAGE_UNSIGNED_BYTE = IL_UNSIGNED_BYTE;
    constexpr unsigned int IMAGE_UNSIGNED_SHORT = IL_UNSIGNED_SHORT;
    constexpr unsigned int IMAGE_FLOAT = IL_FLOAT;
    constexpr unsigned int IMAGE_HALF = IL_HALF;

#endif
