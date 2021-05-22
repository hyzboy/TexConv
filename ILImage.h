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

    void Refresh();

private:

    bool Convert(ILuint,ILuint);

    void *GetData(ILuint,ILuint);

public:

    const ILuint width  ()const{return ilGetInteger(IL_IMAGE_WIDTH);}
    const ILuint height ()const{return ilGetInteger(IL_IMAGE_HEIGHT);}
    const ILuint depth  ()const{return ilGetInteger(IL_IMAGE_DEPTH);}
    const ILuint bit    ()const{return il_bit;}
    const ILuint format ()const{return il_format;}
    const ILuint type   ()const{return il_type;}

    const ILuint pixel_total()const{return width()*height()*depth();}

public:

    const uint channels()const{return channel_count;}         //通道数量

public:

    ILImage();
    ILImage(ILImage *);
    ~ILImage();

    bool LoadFile(const OSString &);

    void Bind();
    void Copy(ILImage *);

    bool Resize(uint,uint);

    bool GenMipmaps();
     int GetMipLevel();
    bool ActiveMipmap(ILuint mip);

    void *ToRGB(ILuint type=IL_UNSIGNED_BYTE);
    void *ToGray(ILuint type=IL_UNSIGNED_BYTE);
    
    void *GetR(ILuint type);

    void *GetRG(ILuint type){return GetData(IL_LUMINANCE_ALPHA,type);}
    void *GetRGB(ILuint type){return GetData(IL_RGB,type);}
    void *GetRGBA(ILuint type);

    void *GetLum(ILuint type){return GetData(IL_LUMINANCE,type);}

    bool ConvertToR(ILuint type){return (il_format==IL_LUMINANCE?Convert(IL_LUMINANCE,type):Convert(IL_ALPHA,type));}
    bool ConvertToRG(ILuint type){return Convert(IL_LUMINANCE_ALPHA,type);}
    bool ConvertToRGB(ILuint type){return Convert(IL_RGB,type);}
    bool ConvertToRGBA(ILuint type){return Convert(IL_RGBA,type);}
    bool ConvertToLum(ILuint type){return Convert(IL_LUMINANCE,type);}
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
