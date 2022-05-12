#include<iostream>
#include"ILImage.h"
#include<hgl/type/Color.h>
#include<hgl/filesystem/Filename.h>

using namespace hgl;
using namespace hgl::filesystem;

constexpr double SRGB_GAMMA         =1.0f/2.2f;
constexpr double SRGB_INVERSE_GAMMA =2.2f;
constexpr double SRGB_ALPHA         =0.055f;

template<typename T>
inline constexpr T sRGB2Linear(const T &in)
{
    if(in<=0.4045)
        return (double)in/12.92;
    else
        return pow((double(in)+SRGB_ALPHA)/(1.0f+SRGB_ALPHA),2.4f);
}

template<typename T>
inline constexpr T Linear2sRGB(const T &in)
{
    if(in<=0.0031308f)
        return double(in)*12.92f;
    else
        return (1.0f+SRGB_ALPHA)*pow(double(in),1.0f/2.4f)-SRGB_ALPHA;
}
        
int convert(const OSString &filename)
{
    ILImage hdr;
    
    if(!hdr.LoadFile(filename))
    {
        os_err<<"Can't load file: "<<filename.c_str()<<std::endl;
        return 1;
    }

    const uint width=hdr.width();
    const uint height=hdr.height();
    const uint channels=hdr.channels();

    float *source=nullptr;        
    
    if(channels==4)
    {
        source=(float *)hdr.GetRGBA(IL_FLOAT);
    }
    else
    if(channels==3)
    {
        source=(float *)hdr.GetRGB(IL_FLOAT);
    }
    else
    if(channels==2)
    {
        source=(float *)hdr.GetRG(IL_FLOAT);
    }
    else
    if(channels==1)
    {
        source=(float *)hdr.GetR(IL_FLOAT);
    }
    else
    {
        os_err<<"Unknow channels: "<<channels<<std::endl;
        return 2;
    }

    uint16 *dest=new uint16[width*height*channels];

    for(uint32 i=0;i<width*height*channels;i++)
    {
        *dest=uint16(Linear2sRGB(*source)*65535);

        ++source;
        ++dest;
    }

    const OSString png_filename=ReplaceExtName<os_char>(filename,OS_TEXT("png"));

    if(!SaveImageToFile(png_filename,width,height,channels,IL_UNSIGNED_SHORT,dest))
    {
        delete[] dest;
        os_err<<OS_TEXT("Save to file failed: ")<<png_filename.c_str()<<std::endl;
        return 3;
    }
    
    delete[] dest;
    os_err<<OS_TEXT("Save to file OK: ")<<png_filename.c_str()<<std::endl;

    return 0;
}

int os_main(int argc,os_char **argv)
{
    std::cout<<"HDR/EXR to PNG v1.0"<<std::endl<<std::endl;

    if(argc<2)
    {
        os_out<<"Example: HDR2PNG 1.hdr"<<std::endl;
        return 0;
    }
    
    ilInit();

    int result=convert(argv[1]);

    ilShutDown();

    return result;
}