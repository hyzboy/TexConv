#include<iostream>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::filesystem;

namespace hgl
{
	void InitYUV2RGBDecode();
    void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,const uint count);
    void YUV2RGB(uint8 *rgb,const uint8 *y,const uint8 *u,const uint8 *v,const uint w,const uint h);
   
    void normal_compress(uint8 *x,uint8 *y,const uint8 *rgb,const uint count);
    void normal_decompress(uint8 *rgb,const uint8 *x,const uint8 *y,const uint count);
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

    const uint pixel_total=rgb_image.pixel_total();

    uint8 *y=new uint8[pixel_total];
    uint8 *u=new uint8[pixel_total>>2];
    uint8 *v=new uint8[pixel_total>>2];

    RGB2YUV(y,u,v,rgb,pixel_total);
    
    YUV2RGB(rgb,y,u,v,rgb_image.width(),rgb_image.height());

    OSString filename;

    filename=ClipFileMainname<os_char>(argv[1]);
    filename+=OS_TEXT("_YUV.png");

    if(SaveImageToFile(filename,rgb_image.width(),rgb_image.height(),1.0,3,IL_UNSIGNED_BYTE,rgb))
        std_cout<<OS_TEXT("Save To ")<<filename.c_str()<<OS_TEXT(" successed!")<<std::endl;

    ilShutDown();
}