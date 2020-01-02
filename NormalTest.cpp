#include<iostream>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::filesystem;

namespace hgl
{
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
    uint8 *x=new uint8[pixels];

    normal_compress(x,y,rgb,pixels);
    normal_decompress(rgb,x,y,pixels);

    OSString filename;

    filename=ClipFileMainname<os_char>(argv[1]);

    filename+=OS_TEXT("_XY.png");

    if(SaveImageToFile(filename,rgb_image.width(),rgb_image.height(),3,IL_UNSIGNED_BYTE,rgb))
        std_cout<<OS_TEXT("Save To ")<<filename.c_str()<<OS_TEXT(" successed!")<<std::endl;

    ilShutDown();
}