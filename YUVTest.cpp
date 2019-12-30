#include<iostream>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::filesystem;

namespace hgl
{
    void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,const uint w,const uint h);
    void YUV2RGB(uint8 *rgb,const uint8 *y,const uint8 *u,const uint8 *v,const uint w,const uint h);

    void InitYUV2RGBDecode();
}

#if HGL_OS == HGL_OS_Windows
#define std_cout    std::wcout

int wmain(int argc,wchar_t **argv)
#else

#define std_cout    std::cout
int main(int argc,char **argv)
#endif//
{
    if(argc<1)
        return 0;

    InitYUV2RGBDecode();
    
    ilInit();

    ILImage rgb_image;

    if(!rgb_image.LoadFile(argv[1]))
    {
        std::cout<<"open source file failed!"<<std::endl;
        return(1);
    }
    
    uint8 *rgb=(uint8 *)rgb_image.GetRGB(IL_UNSIGNED_BYTE);

    const uint pixels=rgb_image.pixel_total();

    uint8 *y=new uint8[pixels];
    uint8 *u=new uint8[pixels/4];
    uint8 *v=new uint8[pixels/4];

    RGB2YUV(y,u,v,rgb,rgb_image.width(),rgb_image.height());
    
    YUV2RGB(rgb,y,u,v,rgb_image.width(),rgb_image.height());

    OSString filename;

    filename=ClipFileMainname<os_char>(argv[1]);

    filename+=OS_TEXT("_YUV.png");

    if(ilSaveImage(filename.c_str()))
        std_cout<<OS_TEXT("Save To ")<<filename.c_str()<<OS_TEXT(" successed!")<<std::endl;

    ilShutDown();
}