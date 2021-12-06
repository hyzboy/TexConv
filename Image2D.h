#pragma once
#include<hgl/type/DataType.h>

using namespace hgl;

enum class PixelDataType
{
    Unknow=0,

    U8,
    U16,
    U32,
    F32,
    F64
};

const uint GetStride(const enum class PixelDataType);

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
    const uint GetPixelsTotal()const{return width*height;}
    const uint GetChannels()const{return channels;}

    const PixelDataType GetType()const{return type;}
    void *GetData()const{return data;}

public:

    Image2D()
    {
        width=height=channels=0;
    }

    Image2D(const uint w,const uint h,const uint c,const PixelDataType &pdt,void *ptr);

    ~Image2D()
    {
        delete[] data;
    }

    Image2D *CreateHalfImage();

    void TypeConvert(const PixelDataType &);
};//class Image2D
