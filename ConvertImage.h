#pragma once

#include<hgl/type/BaseString.h>
#include<hgl/log/LogInfo.h>
#include"pixel_format.h"

using namespace hgl;

class ILImage;

class ConvertImage
{
    OSString filename;

    ILImage *image;

private:

    bool CreateTextureFile(const PixelFormat *);

private:

public:

    ConvertImage();
    ~ConvertImage();

    bool Load(const OSString &fn);
    bool Convert(const PixelFormat **);
};//class ConvertImage