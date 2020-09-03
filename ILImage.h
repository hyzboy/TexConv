#pragma once
#include<IL/il.h>
#include<hgl/type/String.h>

using namespace hgl;

class ILImage
{
    ILuint il_index;
    ILuint il_width,il_height,il_depth;
    ILuint il_bit,il_format,il_type;

    uint channel_count;

private:

    bool Convert(ILuint,ILuint);

    void *GetData(ILuint,ILuint);

public:

    const ILuint width  ()const{return il_width;}
    const ILuint height ()const{return il_height;}
    const ILuint depth  ()const{return il_depth;}
    const ILuint bit    ()const{return il_bit;}
    const ILuint format ()const{return il_format;}
    const ILuint type   ()const{return il_type;}

    const ILuint pixel_total()const{return il_width*il_height*il_depth;}

public:

    const uint channels()const{return channel_count;}         //通道数量

public:

    ILImage();
    ~ILImage();

    bool LoadFile(const OSString &);

    void Bind();

    bool Resize(uint,uint);

    void *ToRGB(ILuint type=IL_UNSIGNED_BYTE);
    void *ToGray(ILuint type=IL_UNSIGNED_BYTE);
    
    void *GetR(ILuint type);

    void *GetRG(ILuint type){return GetData(IL_LUMINANCE_ALPHA,type);}
    void *GetRGB(ILuint type){return GetData(IL_RGB,type);}
    void *GetRGBA(ILuint type);

    void *GetLum(ILuint type){return GetData(IL_LUMINANCE,type);}
};//class ILImage

bool SaveImageToFile(const OSString &filename,ILuint w,ILuint h,const float scale,ILuint c,ILuint t,void *data);

inline bool SaveImageToFile(const OSString &filename,ILuint w,ILuint h,ILuint c,ILuint t,void *data)
{
    return SaveImageToFile(filename,w,h,1.0,c,t,data);
}

inline bool SaveImageToFile(const OSString &filename,ILuint w,ILuint h,ILuint c,void *data)
{
    return SaveImageToFile(filename,w,h,1.0,c,IL_UNSIGNED_BYTE,data);
}
