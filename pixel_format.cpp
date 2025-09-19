#include"pixel_format.h"
#include<hgl/type/StrChar.h>
#include<iostream>
#include<iomanip>

namespace hgl
{
    using namespace graph;

    constexpr const char *VulkanNumberTypeName[]=
    {
        "NONE",
        
        "UINT",
        "SINT",
        "UNORM",
        "SNORM",
        "USCALED",
        "SSCALED",
        "UFLOAT",
        "SFLOAT",
        "sRGB"
    };

    constexpr PixelFormat pf_list[]=
    {
        {ColorFormat::RGBA4,    "RGBA4",    4,{'R','G','B','A'},{ 4, 4, 4, 4}, 16,VulkanBaseType::UNORM},
        {ColorFormat::BGRA4,    "BGRA4",    4,{'B','G','R','A'},{ 4, 4, 4, 4}, 16,VulkanBaseType::UNORM},
        {ColorFormat::RGB565,   "RGB565",   3,{'R','G','B', 0 },{ 5, 6, 5, 0}, 16,VulkanBaseType::UNORM},
        {ColorFormat::A1RGB5,   "A1RGB5",   4,{'A','R','G','B'},{ 1, 5, 5, 5}, 16,VulkanBaseType::UNORM},
        {ColorFormat::R8,       "R8",       1,{'R', 0 , 0 , 0 },{ 8, 0, 0, 0},  8,VulkanBaseType::UNORM},
        {ColorFormat::RG8,      "RG8",      2,{'R','G', 0 , 0 },{ 8, 8, 0, 0}, 16,VulkanBaseType::UNORM},
        {ColorFormat::RGBA8,    "RGBA8",    4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,VulkanBaseType::UNORM},
        {ColorFormat::RGBA8SN,  "RGBA8S",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,VulkanBaseType::SNORM},
        {ColorFormat::RGBA8U,   "RGBA8U",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,VulkanBaseType::UINT},
        {ColorFormat::RGBA8I,   "RGBA8I",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,VulkanBaseType::SINT},
        {ColorFormat::ABGR8,    "ABGR8",    4,{'A','B','G','R'},{ 8, 8, 8, 8}, 32,VulkanBaseType::UNORM},
        {ColorFormat::A2BGR10,  "A2BGR10",  4,{'A','B','G','R'},{ 2,10,10,10}, 32,VulkanBaseType::UNORM},
        {ColorFormat::R16,      "R16",      1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,VulkanBaseType::UNORM},
        {ColorFormat::R16U,     "R16U",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,VulkanBaseType::UINT},
        {ColorFormat::R16I,     "R16I",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,VulkanBaseType::SINT},
        {ColorFormat::R16F,     "R16F",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,VulkanBaseType::SFLOAT},
        {ColorFormat::RG16,     "RG16",     2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,VulkanBaseType::UNORM},
        {ColorFormat::RG16U,    "RG16U",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,VulkanBaseType::UINT},
        {ColorFormat::RG16I,    "RG16I",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,VulkanBaseType::SINT},
        {ColorFormat::RG16F,    "RG16F",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,VulkanBaseType::SFLOAT},
        {ColorFormat::RGBA16,   "RGBA16",   4,{'R','G','B','A'},{16,16,16,16}, 64,VulkanBaseType::UNORM},
        {ColorFormat::RGBA16SN, "RGBA16S",  4,{'R','G','B','A'},{16,16,16,16}, 64,VulkanBaseType::SNORM},
        {ColorFormat::RGBA16U,  "RGBA16U",  4,{'R','G','B','A'},{16,16,16,16}, 64,VulkanBaseType::UINT},
        {ColorFormat::RGBA16I,  "RGBA16I",  4,{'R','G','B','A'},{16,16,16,16}, 64,VulkanBaseType::SINT},
        {ColorFormat::RGBA16F,  "RGBA16F",  4,{'R','G','B','A'},{16,16,16,16}, 64,VulkanBaseType::SFLOAT},
        {ColorFormat::R32U,     "R32U",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,VulkanBaseType::UINT},
        {ColorFormat::R32I,     "R32I",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,VulkanBaseType::SINT},
        {ColorFormat::R32F,     "R32F",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,VulkanBaseType::SFLOAT},
        {ColorFormat::RG32U,    "RG32U",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,VulkanBaseType::UINT},
        {ColorFormat::RG32I,    "RG32I",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,VulkanBaseType::SINT},
        {ColorFormat::RG32F,    "RG32F",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,VulkanBaseType::SFLOAT},
        {ColorFormat::RGB32U,   "RGB32U",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,VulkanBaseType::UINT},
        {ColorFormat::RGB32I,   "RGB32I",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,VulkanBaseType::SINT},
        {ColorFormat::RGB32F,   "RGB32F",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,VulkanBaseType::SFLOAT},
        {ColorFormat::RGBA32U,  "RGBA32U",  4,{'R','G','B','A'},{32,32,32,32},128,VulkanBaseType::UINT},
        {ColorFormat::RGBA32I,  "RGBA32I",  4,{'R','G','B','A'},{32,32,32,32},128,VulkanBaseType::SINT},
        {ColorFormat::RGBA32F,  "RGBA32F",  4,{'R','G','B','A'},{32,32,32,32},128,VulkanBaseType::SFLOAT},
        {ColorFormat::B10GR11UF,"B10GR11UF",3,{'B','G','R', 0 },{10,11,11, 0}, 32,VulkanBaseType::UFLOAT},

        {ColorFormat::COMPRESS, "COMPRESS", 0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  0,VulkanBaseType::NONE},

        {ColorFormat::BC1RGB,   "BC1RGB",   0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,VulkanBaseType::NONE},
        {ColorFormat::BC1RGBA,  "BC1RGBA",  0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,VulkanBaseType::NONE},
        {ColorFormat::BC2,      "BC2",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,VulkanBaseType::NONE},
        {ColorFormat::BC3,      "BC3",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,VulkanBaseType::NONE},
        {ColorFormat::BC4,      "BC4",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,VulkanBaseType::NONE},
        {ColorFormat::BC5,      "BC5",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,VulkanBaseType::NONE},
        {ColorFormat::BC6,      "BC6",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,VulkanBaseType::NONE},
        {ColorFormat::BC7,      "BC7",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,VulkanBaseType::NONE}
    };

    constexpr uint PixelFormatCount=sizeof(pf_list)/sizeof(PixelFormat);
}//namespace

void PrintFormatList()
{
    const PixelFormat *pf=pf_list;

    for(uint i=0;i<PixelFormatCount;i++)
    {
        if(pf->format<ColorFormat::COMPRESS)
            std::cout<<int(pf->channels)<<": "<<std::setw(10)<<pf->name<<" "<<std::setw(3)<<pf->total_bits<<" bits "<<VulkanNumberTypeName[(uint)(pf->type)]<<std::endl;
        else
        if(pf->format>ColorFormat::COMPRESS)
            std::cout<<std::setw(13)<<pf->name<<" Compress Format"<<std::endl;
        else
            std::cout<<std::endl;

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
