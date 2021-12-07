#pragma once

#include"ILImage.h"
#include"pixel_format.h"
#include<hgl/io/FileOutputStream.h>
#include<hgl/io/DataOutputStream.h>

using namespace hgl;

bool ToILType(ILuint &type,const uint8 bits,const ColorDataType cdt);

enum class TextureFileType
{
    Tex1D=0,
    Tex2D,
    Tex3D,
    TexCubemap,
    Tex1DArray,
    Tex2DArray,
    TexCubemapArray,

    ENUM_CLASS_RANGE(Tex1D,TexCubemapArray)
};//

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

	virtual bool CreateTexFile(const OSString &, const TextureFileType &);
    virtual bool WriteSize1D(const uint miplevel,const uint length);
    virtual bool WriteSize2D(const uint miplevel, const uint width,const uint height);
    virtual bool WritePixelFormat();
    
    virtual bool InitFormat(ILImage *)=0;
    virtual uint32 Write()=0;

    virtual void Close();
    virtual void Delete();
};//class TextureFileCreater

TextureFileCreater *CreateTFC(const PixelFormat *,const int channels);
