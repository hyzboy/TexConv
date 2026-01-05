#include<iostream>
#include"ImageLoader.h"
#include<hgl/color/sRGBConvert.h>
#include<hgl/filesystem/Filename.h>

using namespace hgl;
using namespace hgl::filesystem;

int convert(const OSString &filename)
{
    ImageLoader hdr;
    
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
        source=(float *)hdr.GetRGBA(IMAGE_FLOAT);
    }
    else
    if(channels==3)
    {
        source=(float *)hdr.GetRGB(IMAGE_FLOAT);
    }
    else
    if(channels==2)
    {
        source=(float *)hdr.GetRG(IMAGE_FLOAT);
    }
    else
    if(channels==1)
    {
        source=(float *)hdr.GetR(IMAGE_FLOAT);
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

    if(!SaveImageToFile(png_filename,width,height,channels,IMAGE_UNSIGNED_SHORT,dest))
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
    
    InitImageLibrary(nullptr);

    int result=convert(argv[1]);

    ShutdownImageLibrary();

    return result;
}