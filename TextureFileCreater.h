#pragma once

#include"ILImage.h"
#include"pixel_format.h"
#include<hgl/io/FileOutputStream.h>
#include<hgl/io/DataOutputStream.h>

using namespace hgl;

bool ToILType(ILuint &type,const uint8 bits,const ColorDataType cdt);

class TextureFileCreater
{
protected:

    const PixelFormat *fmt;

    ILImage *image;

    uint pixel_bytes;       //单像素字节数
    uint pixel_total;       //象素总量=width*height
    uint total_bytes;       //总字节数

protected:

    OSString filename;

    io::FileOutputStream fos;
    io::DataOutputStream *dos;

    bool Write(void *);

public:

    TextureFileCreater(const PixelFormat *pf,ILImage *);
    virtual ~TextureFileCreater();

    virtual bool WriteFileHeader(const OSString &);
    
    virtual bool Write()=0;

    virtual void Close();
    virtual void Delete();
};//class TextureFileCreater