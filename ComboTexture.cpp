#include<iostream>
#include<iomanip>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/type/Smart.h>

using namespace hgl;
using namespace hgl::util;

#if HGL_OS == HGL_OS_Windows
    #define std_cout    std::wcout
#else
    #define std_cout    std::cout
#endif//

namespace hgl
{
    void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,const uint count);
    void normal_compress(uint8 *x,uint8 *y,const uint8 *rgb,const uint count);

    template<typename T>
    void MixRGB(T *rgb,const T *r,const T *g,const T *b,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *rgb=*r;++rgb;++r;
            *rgb=*g;++rgb;++g;
            *rgb=*b;++rgb;++b;
        }
    }

    template<typename T>
    void MixRGBA(T *rgba,const T *r,const T *g,const T *b,const T *a,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *rgba=*r;++rgba;++r;
            *rgba=*g;++rgba;++g;
            *rgba=*b;++rgba;++b;
            *rgba=*a;++rgba;++a;
        }
    }
        
    bool SaveRGBAFile(const OSString &filename,const uint w,const uint h,const float scale,const uint8 *r,const uint8 *g,const uint8 *b,const uint8 *a,const OSString &flag)
    {
        const OSString out_filename=OSString(filename)+OS_TEXT("_")+flag+OS_TEXT(".png");

        AutoDeleteArray<uint8> pixels=new uint8[w*h*4];

        MixRGBA<uint8>(pixels,r,g,b,a,w*h);
        
        if(SaveImageToFile(out_filename,w,h,scale,4,IL_UNSIGNED_BYTE,pixels))
        {
            std_cout<<OS_TEXT("Output ")<<flag.c_str()<<OS_TEXT(": ")<<out_filename.c_str()<<std::endl;
            return(true);
        }

        return(false);    
    }
    bool SaveRGBFile(const OSString &filename,const uint w,const uint h,const float scale,const uint8 *r,const uint8 *g,const uint8 *b,const OSString &flag)
    {
        const OSString out_filename=OSString(filename)+OS_TEXT("_")+flag+OS_TEXT(".png");

        AutoDeleteArray<uint8> pixels=new uint8[w*h*3];

        MixRGB<uint8>(pixels,r,g,b,w*h);
        
        if(SaveImageToFile(out_filename,w,h,scale,3,IL_UNSIGNED_BYTE,pixels))
        {
            std_cout<<OS_TEXT("Output ")<<flag.c_str()<<OS_TEXT(": ")<<out_filename.c_str()<<std::endl;
            return(true);
        }

        return(false);    
    }
}//namespace hgl

#if HGL_OS == HGL_OS_Windows
    int wmain(int argc,wchar_t **argv)
#else
    int main(int argc,char **argv)
#endif//
{
    std::cout<<"Combo Texture Compression"<<std::endl<<std::endl;

    if(argc<2)
    {
        // FullTexture: Y1+Y2+G
        // HalfTexture: U1+V1+U2+V2
        std::cout<<"Example1: ComboTexture <output name> C2G1 Color1.png Color2.png Grayscale.png"<<std::endl;

        // FullTexture: Y+M+R
        // HalfTexture: U+V+G
        std::cout<<"Example2: ComboTexture <output name> MPBR6 Color.png Matallic.png Roughness.png Grayscale.png"<<std::endl;

        // FullTexture: Y+G1+G2
        // HalfTexture: U+V+M+R
        std::cout<<"Example3: ComboTexture <output name> MPBR7 Color.png Matallic.png Roughness.png Grayscale1.png Grayscale2.png"<<std::endl;

        return(0);
    }
    
    ilInit();

    if(stricmp(argv[2],"C2G1")==0)
    {
        ILImage c1,c2,g;
        uint w,h;

        if(!c1.LoadFile(argv[3]))return(1);
        if(!c2.LoadFile(argv[4]))return(2);
        if(!g.LoadFile(argv[5]))return(3);

        c1.ToRGB();
        c2.ToRGB();
        g.ToGray();

        if((c1.width()!=c2.width())||(c2.width()!=g.width())||(c1.width()!=g.width())
         ||(c1.height()!=c2.height())||(c2.height()!=g.height())||(c1.width()!=g.height()))
        {
            w=hgl_min(c1.width(),hgl_min(c2.width(),g.width()));
            h=hgl_min(c1.height(),hgl_min(c2.height(),g.height()));

            c1.Resize(w,h);
            c2.Resize(w,h);
            g.Resize(w,h);
        }
        
        const uint pixel_total=w*h;
        const uint half_pixel_total=pixel_total>>2;

        AutoDeleteArray<uint8> y1=new uint8[pixel_total];
        AutoDeleteArray<uint8> y2=new uint8[pixel_total];
        AutoDeleteArray<uint8> u1=new uint8[half_pixel_total];
        AutoDeleteArray<uint8> u2=new uint8[half_pixel_total];
        AutoDeleteArray<uint8> v1=new uint8[half_pixel_total];
        AutoDeleteArray<uint8> v2=new uint8[half_pixel_total];

        RGB2YUV(y1,u1,v1,(uint8 *)c1.GetRGB(IL_UNSIGNED_BYTE),pixel_total);
        RGB2YUV(y2,u2,v2,(uint8 *)c2.GetRGB(IL_UNSIGNED_BYTE),pixel_total);

        SaveRGBFile(    argv[1],
                        w,h,1.0,
                        y1,y2,(uint8 *)g.GetLum(IL_UNSIGNED_BYTE),
                        OS_TEXT("Y2G"));

        SaveRGBAFile(   argv[1],
                        w,h,0.5,
                        u1,v1,u2,v2,
                        OS_TEXT("UV2"));
    }

    ilShutDown();
    return 0;
}
