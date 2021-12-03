#pragma once
#include<hgl/type/DataType.h>

using namespace hgl;

enum class PixelDataType
{
    U8,
    U16,
    U32,
    F32,
    F64
};

class Image2D
{
    uint width;
    uint height;
    uint channels;

    PixelDataType type;

    void *data;

public:

    const uint GetWidth()const{return width;}
    const uint GetHeight()const{return height;}
    const uint GetChannels()const{return channels;}
    const PixelDataType GetType()const{return type;}
    void *GetData()const{return data;}

public:

    Image2D()
    {
        width=height=channels=0;
    }

    Image2D(const uint w,const uint h,const uint c,const PixelDataType &pdt,void *ptr)
    {
        width=w;
        height=h;
        channels=c;
        type=pdt;
        data=ptr;
    }

    ~Image2D()
    {
        delete[] data;
    }

    Image2D *CreateHalfImage();
};//class Image2D
