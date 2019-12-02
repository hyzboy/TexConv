#include"pixel_format.h"
#include<hgl/type/StrChar.h>
#include<iostream>
#include<iomanip>

namespace hgl
{
    constexpr char *ColorDataName[]=
    {
        "NONE",

        "UNORM",
        "SNORM",
        "USCALED",
        "SSCALED",
        "UINT",
        "SINT",
        "UFLOAT",
        "SFLOAT"
    };

    constexpr PixelFormat pf_list[]=
    {
        {ColorFormat::BGRA4,      "BGRA4",     {'B','G','R','A'},{ 4, 4, 4, 4},ColorDataType::UNORM},
        {ColorFormat::RGB565,     "RGB565",    {'R','G','B', 0 },{ 4, 5, 5, 0},ColorDataType::UNORM},
        {ColorFormat::A1RGB5,     "A1RGB5",    {'A','R','G','B'},{ 1, 5, 5, 5},ColorDataType::UNORM},
        {ColorFormat::R8UN,       "R8UN",      {'R', 0 , 0 , 0 },{ 8, 0, 0, 0},ColorDataType::UNORM},
        {ColorFormat::RG8UN,      "RG8UN",     {'R','G', 0 , 0 },{ 8, 8, 0, 0},ColorDataType::UNORM},
        {ColorFormat::RGBA8UN,    "RGBA8UN",   {'R','G','B','A'},{ 8, 8, 8, 8},ColorDataType::UNORM},
        {ColorFormat::ABGR8UN,    "ABGR8UN",   {'A','B','G','R'},{ 8, 8, 8, 8},ColorDataType::UNORM},
        {ColorFormat::A2BGR10UN,  "A2BGR10UN", {'A','B','G','R'},{ 2,10,10,10},ColorDataType::UNORM},
        {ColorFormat::R16UN,      "R16UN",     {'R', 0 , 0 , 0 },{16, 0, 0, 0},ColorDataType::UNORM},
        {ColorFormat::R16F,       "R16F",      {'R', 0 , 0 , 0 },{16, 0, 0, 0},ColorDataType::SFLOAT},
        {ColorFormat::RG16UN,     "RG16UN",    {'R','G', 0 , 0 },{16,16, 0, 0},ColorDataType::UNORM},
        {ColorFormat::RG16F,      "RG16F",     {'R','G', 0 , 0 },{16,16, 0, 0},ColorDataType::SFLOAT},
        {ColorFormat::RGBA16UN,   "RGBA16UN",  {'R','G','B','A'},{16,16,16, 0},ColorDataType::UNORM},
        {ColorFormat::RGBA16F,    "RGBA16F",   {'R','G','B','A'},{16,16,16, 0},ColorDataType::SFLOAT},
        {ColorFormat::R32U,       "R32U",      {'R', 0 , 0 , 0 },{32, 0, 0, 0},ColorDataType::UINT},
        {ColorFormat::R32I,       "R32I",      {'R', 0 , 0 , 0 },{32, 0, 0, 0},ColorDataType::SINT},
        {ColorFormat::R32F,       "R32F",      {'R', 0 , 0 , 0 },{32, 0, 0, 0},ColorDataType::SFLOAT},
        {ColorFormat::RG32U,      "RG32U",     {'R','G', 0 , 0 },{32,32, 0, 0},ColorDataType::UINT},
        {ColorFormat::RG32I,      "RG32I",     {'R','G', 0 , 0 },{32,32, 0, 0},ColorDataType::SINT},
        {ColorFormat::RG32F,      "RG32F",     {'R','G', 0 , 0 },{32,32, 0, 0},ColorDataType::SFLOAT},
        {ColorFormat::RGB32U,     "RGB32U",    {'R','G','B', 0 },{32,32,32, 0},ColorDataType::UINT},
        {ColorFormat::RGB32I,     "RGB32I",    {'R','G','B', 0 },{32,32,32, 0},ColorDataType::SINT},
        {ColorFormat::RGB32F,     "RGB32F",    {'R','G','B', 0 },{32,32,32, 0},ColorDataType::SFLOAT},
        {ColorFormat::RGBA32U,    "RGBA32U",   {'R','G','B','A'},{32,32,32,32},ColorDataType::UINT},
        {ColorFormat::RGBA32I,    "RGBA32I",   {'R','G','B','A'},{32,32,32,32},ColorDataType::SINT},
        {ColorFormat::RGBA32F,    "RGBA32F",   {'R','G','B','A'},{32,32,32,32},ColorDataType::SFLOAT}
    };

    constexpr uint PixelFormatCount=sizeof(pf_list)/sizeof(PixelFormat);
}//namespace

void PrintFormatList()
{
    const PixelFormat *pf=pf_list;

    for(uint i=0;i<PixelFormatCount;i++)
    {
        std::cout<<std::setw(10)<<pf->name<<" "<<std::setw(3)<<pf->GetPixelBytes()<<" bits "<<ColorDataName[(uint)(pf->type)]<<std::endl;

        ++pf;
    }
}

const PixelFormat *GetPixelFormat(ColorFormat fmt)
{
    if(fmt<=ColorFormat::NONE
     ||fmt>=ColorFormat::END)return(nullptr);

    const PixelFormat *pf=pf_list;

    for(uint i=0;i<PixelFormatCount;i++)
    {
        if(pf->format==fmt)
            return pf;

        ++pf;
    }

    return nullptr;
}

const PixelFormat *GetPixelFormat(const os_char *name)
{
    if(!name||!*name)return(nullptr);

    const PixelFormat *pf=pf_list;

    for(uint i=0;i<PixelFormatCount;i++)
    {
        if(hgl::stricmp(name,pf->name)==0)
            return pf;

        ++pf;
    }

    return nullptr;
}

/**
 * 检测是否所有ALPHA一致
 */
bool CheckSameAlpha(uint8 *data,uint count)
{
    uint8 *p=data;

    while(count--)
    {
        if(*p!=*data)return(false);

        p++;
    }

    return(true);
}

/**
* 检测RGB数据是否一致
*/
bool CheckSameRGB(uint8 *data,int color,uint count)
{
    uint8 *p=data;

    while(count--)
    {
        if(memcmp(data,p,color-1))
            return(false);

        p+=color;
    }

    return(true);
}

void MixLA(uint8 *lum,uint8 *alpha,int size)
{
    int i;

    for(i=0;i<size;i++)
    {
        lum++;
        *lum++=*alpha++;
    }
}

void MixRGBA(uint8 *rgba,uint8 *alpha,int size)
{
    int i;

    for(i=0;i<size;i++)
    {
        rgba+=3;
        *rgba++=*alpha++;
    }
}