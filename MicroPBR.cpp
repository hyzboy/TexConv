#include<iostream>
#include<iomanip>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/util/cmd/CmdParse.h>

using namespace hgl;
using namespace hgl::util;

#if HGL_OS == HGL_OS_Windows
    #define std_cout    std::wcout
#else
    #define std_cout    std::cout
#endif//

namespace hgl
{
    void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,const uint w,const uint h);
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

    class PBRComponent
    {
        OSString param_name;
        OSString name;
        OSString filename;

        bool has;
        ILImage img;

    public:

        const uint width()const{return img.width();}
        const uint height()const{return img.height();}

    public:

        PBRComponent(const OSString &cn,const OSString &pn)
        {
            param_name=pn;
            name=cn;
            has=false;
        }

        virtual ~PBRComponent()=default;

        const bool isHas()const{return has;}

        bool Parse(const CmdParse &cp)
        {
            has=cp.GetString(param_name,filename);
            if(!has)
            {
                std_cout<<OS_TEXT("no ")<<std::setw(10)<<name.c_str()<<std::endl;
                return(false);
            }

            std_cout<<std::setw(10)<<name.c_str()<<": "<<filename.c_str()<<std::endl;

            has=img.LoadFile(filename);

            if(!has)
                std_cout<<OS_TEXT("Load file failed, filename: ")<<filename.c_str()<<std::endl;

            return(has);
        }

        void Resize(const uint w,const uint h){img.Resize(w,h);}
        void ToRGB(){img.ToRGB();}
        void ToGray(){img.ToGray();}

        uint8 *GetRGB(){return (uint8 *)img.GetRGB(IL_UNSIGNED_BYTE);}
        uint8 *GetLum(){return (uint8 *)img.GetLum(IL_UNSIGNED_BYTE);}
    };//class PBRComponent
}//namespace hgl

#if HGL_OS == HGL_OS_Windows
    int wmain(int argc,wchar_t **argv)
#else
    int main(int argc,char **argv)
#endif//
{
    std::cout<<"MicroPBR Texture Compression"<<std::endl<<std::endl;

    if(argc<2)
    {
        std::cout<<"Example: MicroPBR <output name> /C:BaseColor.png /N:Normal.png /M:Metallic.png /R:Roughness.png"<<std::endl;

        return(0);
    }
    
    CmdParse cp(argc,argv);

    PBRComponent    color       (OS_TEXT("BaseColor"),OS_TEXT("/C:")),
                    normal      (OS_TEXT("Normal"   ),OS_TEXT("/N:")),
                    metallic    (OS_TEXT("Metallic" ),OS_TEXT("/M:")),
                    roughness   (OS_TEXT("Roughness"),OS_TEXT("/R:"));

    ilInit();

    color.Parse(cp);
    normal.Parse(cp);
    metallic.Parse(cp);
    roughness.Parse(cp);

    if(!color.isHas()||!normal.isHas())
        return(false);

    color.ToRGB();
    normal.ToRGB();

    if(color.width()!=normal.width()
     ||color.height()!=normal.height())
    {
        uint nw=hgl_min(color.width(),normal.width());
        uint nh=hgl_max(color.height(),normal.height());

        color.Resize(nw,nh);
    }

    const uint w=color.width();
    const uint h=color.height();
    const uint half_w=w>>1;
    const uint half_h=h>>1;

    if(metallic.isHas())
    {
        metallic.ToGray();
        metallic.Resize(half_w,half_h);
    }

    if(roughness.isHas())
    {
        roughness.ToGray();
        roughness.Resize(half_w,half_h);
    }

    uint8 *y,*u,*v,*nx,*ny;

    y=new uint8[w*h];
    u=new uint8[half_w*half_h];
    v=new uint8[half_w*half_h];
    nx=new uint8[w*h];
    ny=new uint8[w*h];

    RGB2YUV(y,u,v,color.GetRGB(),w,h);
    normal_compress(nx,ny,normal.GetRGB(),w*h);

    // BaseColor Y + Normal XY
    {
        ILImage img;
        const OSString out_filename=OSString(argv[1])+OS_TEXT("_YN.png");

        uint8 *pixels=new uint8[w*h*3];

        MixRGB<uint8>(pixels,y,nx,ny,w*h);

        img.Create(w,h,3,IL_UNSIGNED_BYTE,pixels);

        delete[] pixels;
        
        if(img.SaveFile(out_filename))
            std_cout<<OS_TEXT("Output 1: ")<<out_filename.c_str()<<std::endl;
    }

    if(metallic.isHas()&&roughness.isHas())
    {
        ILImage img;
        const OSString out_filename=OSString(argv[1])+OS_TEXT("_UVMR.png");

        uint8 *pixels=new uint8[half_w*half_h*4];

        MixRGBA<uint8>(pixels,u,v,metallic.GetLum(),roughness.GetLum(),half_w*half_h);

        img.Create(half_w,half_h,4,IL_UNSIGNED_BYTE,pixels);

        delete[] pixels;
        
        if(img.SaveFile(out_filename))
            std_cout<<OS_TEXT("Output 2: ")<<out_filename.c_str()<<std::endl;        
    }
    else
    if(metallic.isHas())
    {
        ILImage img;
        const OSString out_filename=OSString(argv[1])+OS_TEXT("_UVM.png");

        uint8 *pixels=new uint8[half_w*half_h*3];

        MixRGB<uint8>(pixels,u,v,metallic.GetLum(),half_w*half_h);

        img.Create(half_w,half_h,3,IL_UNSIGNED_BYTE,pixels);

        delete[] pixels;
        
        if(img.SaveFile(out_filename))
            std_cout<<OS_TEXT("Output 2: ")<<out_filename.c_str()<<std::endl;
    }
    else
    if(roughness.isHas())
    {
        ILImage img;
        const OSString out_filename=OSString(argv[1])+OS_TEXT("_UVR.png");

        uint8 *pixels=new uint8[half_w*half_h*3];

        MixRGB<uint8>(pixels,u,v,roughness.GetLum(),half_w*half_h);

        img.Create(half_w,half_h,3,IL_UNSIGNED_BYTE,pixels);

        delete[] pixels;
        
        if(img.SaveFile(out_filename))
            std_cout<<OS_TEXT("Output 2: ")<<out_filename.c_str()<<std::endl;
    }

    delete[] y;
    delete[] u;
    delete[] v;
    delete[] nx;
    delete[] ny;

    ilShutDown();
    return 0;
}
