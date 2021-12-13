#pragma once

#include"ILImage.h"
#include"pixel_format.h"
#include<vulkan/vulkan.h>
#include<hgl/io/FileOutputStream.h>
#include<hgl/io/DataOutputStream.h>

using namespace hgl;

bool ToILType(ILuint &type,const uint8 bits,const ColorDataType cdt);

class TextureFileCreater
{
protected:

    ILImage *image;
    const PixelFormat *pixel_format;

protected:

    OSString filename;

    io::FileOutputStream fos;
    io::DataOutputStream *dos;

    uint32 Write(void *,const uint);

public:

    TextureFileCreater(const PixelFormat *pf);
    virtual ~TextureFileCreater();

	virtual bool CreateTexFile(const OSString &, const VkImageViewType &);

    virtual bool WriteSize1D(const uint32 length);
    virtual bool WriteSize2D(const uint32 width,const uint32 height);
    virtual bool WriteSize3D(const uint32 width,const uint32 height,const uint32 depth);
            bool WriteSizeCube(const uint32 width,const uint32 height){return WriteSize2D(width,height);}
    virtual bool WriteSize1DArray(const uint32 length,const uint32 layers);
    virtual bool WriteSize2DArray(const uint32 width,const uint32 height,const uint32 layers);
            bool WriteSizeCubeArray(const uint32 width,const uint32 height,const uint32 layers){return WriteSize2DArray(width,height,layers);}

    virtual bool WritePixelFormat(const uint miplevel);
    
    virtual bool InitFormat(ILImage *)=0;
    virtual uint32 Write()=0;

    virtual void Close();
    virtual void Delete();
};//class TextureFileCreater

TextureFileCreater *CreateTFC(const PixelFormat *,const int channels);
