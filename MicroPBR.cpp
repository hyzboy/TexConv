#include<iostream>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/util/cmd/CmdParse.h>

using namespace hgl;
using namespace hgl::util;

void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,int w,int h);

class PBRComponent
{
    OSString name;

    ILImage img;

public:

    PBRComponent(const OSString &cn)
    {
        name=cn;
    }

    virtual ~PBRComponent()=default;

    bool Load(const OSString &fn)
    {
    }
};//class PBRComponent

#if HGL_OS == HGL_OS_Windows
#define std_cout    std::wcout

int wmain(int argc,wchar_t **argv)
#else

#define std_cout    std::cout
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

    OSString    color_filename,
                normal_filename,
                metallic_filename,
                roughness_filename;

    bool    has_color,
            has_normal,
            has_metallic,
            has_roughness;

    has_color       =cp.GetString(OS_TEXT("/C:"),color_filename);
    has_normal      =cp.GetString(OS_TEXT("/N:"),normal_filename);
    has_metallic    =cp.GetString(OS_TEXT("/M:"),metallic_filename);
    has_roughness   =cp.GetString(OS_TEXT("/R:"),roughness_filename);

    if(has_color        )std_cout<<OS_TEXT("BaseColor: ")<<color_filename.c_str()<<std::endl;
    if(has_normal       )std_cout<<OS_TEXT("   Normal: ")<<normal_filename.c_str()<<std::endl;
    if(has_metallic     )std_cout<<OS_TEXT(" Metallic: ")<<metallic_filename.c_str()<<std::endl;
    if(has_roughness    )std_cout<<OS_TEXT("Roughness: ")<<roughness_filename.c_str()<<std::endl;

    std_cout<<OS_TEXT("Output: ")<<argv[1]<<OS_TEXT("_YNormal.png")<<std::endl;
    std_cout<<OS_TEXT("Output: ")<<argv[1]<<OS_TEXT("_UVMR.png")<<std::endl;

    ILImage color,normal,metallic,roughness;
    ILImage yn,uvmr;

    if(has_color&&!color.LoadFile(color_filename))return(1);
    if(has_normal&&!normal.LoadFile(normal_filename))return(2);
    if(has_metallic&&!metallic.LoadFile(metallic_filename))return(3);
    if(has_roughness&&!roughness.LoadFile(roughness_filename))return(4);



    return 0;
}
