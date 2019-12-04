#pragma once
#include<IL/il.h>
#include<hgl/type/BaseString.h>

using namespace hgl;

class ILImage
{
    ILuint il_index;
    ILuint il_width,il_height;
    ILuint il_bit,il_format,il_type;

    uint channel_count;

private:

    bool Convert(ILuint,ILuint);

    void *GetData(ILuint,ILuint);

public:

    const ILuint width   ()const{return il_width;}
    const ILuint height  ()const{return il_height;}
    const ILuint bit     ()const{return il_bit;}
    const ILuint format  ()const{return il_format;}
    const ILuint type    ()const{return il_type;}

    const ILuint pixel_total()const{return il_width*il_height;}

public:

    const uint channels()const{return channel_count;}         //通道数量

public:

    ILImage();
    ~ILImage();

    bool LoadFile(const OSString &);

    void Bind();
    
    void *GetR(ILuint type);

    void *GetRG(ILuint type){return GetData(IL_LUMINANCE_ALPHA,type);}
    void *GetRGB(ILuint type){return GetData(IL_RGB,type);}
//    void *GetBGR(ILuint type){return GetData(IL_BGR,type);}
    void *GetRGBA(ILuint type){return GetData(IL_RGBA,type);}
//    void *GetBGRA(ILuint type){return GetData(IL_BGRA,type);}
};//class ILImage
