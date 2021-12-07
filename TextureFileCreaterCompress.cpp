#include"TextureFileCreater.h"
#include"ILImage.h"
#include<hgl/log/LogInfo.h>
#include<hgl/TypeFunc.h>
#include"CMP_CompressonatorLib/Common.h"
#include"CMP_CompressonatorLib/Compressonator.h"

class TextureFileCreaterCompress:public TextureFileCreater
{
    MipSet MipSetIn;
    MipSet MipSetOut;
    CMP_CMIPS CMips;
    KernelOptions kernel_options;

    int channels;
    int type;
    uint pixel_bytes;
    CMP_ChannelFormat cf;
    CMP_TextureDataType tdt;
    CMP_FORMAT source_fmt;
    CMP_FORMAT target_fmt;
    std::string target_fmt_name;

public:

    using TextureFileCreater::TextureFileCreater;

public:

    bool InitFormat(ILImage* img) override
    {
        image = img;

        channels=image->channels();
        type=image->type();
        
        constexpr CMP_FORMAT fmt_list[]=
        {
            CMP_FORMAT_BC1,     //ColorFormat::BC1
            CMP_FORMAT_BC1,     //ColorFormat::BC1
            CMP_FORMAT_BC2,     //ColorFormat::BC2
            CMP_FORMAT_BC3,     //ColorFormat::BC3
            CMP_FORMAT_BC4,     //ColorFormat::BC4
            CMP_FORMAT_BC5,     //ColorFormat::BC5
            CMP_FORMAT_BC6H,    //ColorFormat::BC6H
            CMP_FORMAT_BC6H_SF, //ColorFormat::BC6H_SF
            CMP_FORMAT_BC7,     //ColorFormat::BC7
        };

        constexpr char fmt_name_list[][8]=
        {
            "BC1RGB",
            "BC1RGBA",
            "BC2",
            "BC3",
            "BC4",
            "BC5",
            "BC6H",
            "BC6H_SF",
            "BC7"
        };

        const int fmt_index=size_t(pixel_format->format)-size_t(ColorFormat::BC1RGB);

        target_fmt=fmt_list[fmt_index];

        target_fmt_name=fmt_name_list[fmt_index];
        
        std::cout<<"Compress Image to "<<target_fmt_name.c_str()<<" Format."<<std::endl;

        if(type==IL_UNSIGNED_BYTE   ){cf=CF_8bit;    pixel_bytes=1;}else
        if(type==IL_UNSIGNED_SHORT  ){cf=CF_16bit;   pixel_bytes=2;}else
        if(type==IL_UNSIGNED_INT    ){cf=CF_32bit;   pixel_bytes=4;}else
        if(type==IL_HALF            ){cf=CF_Float16; pixel_bytes=2;}else
        if(type==IL_FLOAT           ){cf=CF_Float32; pixel_bytes=4;}else
        {
            std::cerr<<"unknow type: "<<type<<std::endl;
            return(false);
        }
        
        if(channels==1)
        {
            tdt=TDT_R;
            
            if(type==IL_UNSIGNED_BYTE   )source_fmt=CMP_FORMAT_R_8;else
            if(type==IL_UNSIGNED_SHORT  )source_fmt=CMP_FORMAT_R_16;else
            if(type==IL_HALF            )source_fmt=CMP_FORMAT_R_16F;else
            if(type==IL_FLOAT           )source_fmt=CMP_FORMAT_R_32F;else
            {
                std::cerr<<"channels 1 unknow type: "<<type<<std::endl;
                return(false);
            }

            image->ConvertToLum(type);
        }
        else
        if(channels==2)
        {
            tdt=TDT_RG;
            
            if(type==IL_UNSIGNED_BYTE   )source_fmt=CMP_FORMAT_RG_8;else
            if(type==IL_UNSIGNED_SHORT  )source_fmt=CMP_FORMAT_RG_16;else
            if(type==IL_HALF            )source_fmt=CMP_FORMAT_RG_16F;else
            if(type==IL_FLOAT           )source_fmt=CMP_FORMAT_RG_32F;else
            {
                std::cerr<<"channels 2 unknow type: "<<type<<std::endl;
                return(false);
            }
            
            image->ConvertToRG(type);
        }
        else
        if(channels==3)
        {
            tdt=TDT_XRGB;
            
            if(type==IL_UNSIGNED_BYTE   )source_fmt=CMP_FORMAT_RGBA_8888;else
            if(type==IL_UNSIGNED_SHORT  )source_fmt=CMP_FORMAT_RGBA_16;else
            if(type==IL_HALF            )source_fmt=CMP_FORMAT_RGBA_16F;else
            if(type==IL_FLOAT           )source_fmt=CMP_FORMAT_RGBA_32F;else
            {
                std::cerr<<"channels 3 unknow type: "<<type<<std::endl;
                return(false);
            }

            image->ConvertToRGBA(type);
        }
        else
        if(channels==4)
        {
            tdt=TDT_ARGB;
            
            if(type==IL_UNSIGNED_BYTE   )source_fmt=CMP_FORMAT_RGBA_8888;else
            if(type==IL_UNSIGNED_SHORT  )source_fmt=CMP_FORMAT_RGBA_16;else
            if(type==IL_HALF            )source_fmt=CMP_FORMAT_RGBA_16F;else
            if(type==IL_FLOAT           )source_fmt=CMP_FORMAT_RGBA_32F;else
            {
                std::cerr<<"channels 4 unknow type: "<<type<<std::endl;
                return(false);
            }            
            
            image->ConvertToRGBA(type);
        }
        else
        {
            std::cerr<<"unknow channels: "<<channels<<std::endl;
            return(false);
        }

        return(true);
    }

    void InitMipSetIn()
    {
        const int width=image->width();
        const int height=image->height();
        
        hgl_zero(MipSetIn);

        void *source_data=nullptr;

        if(channels==1)
        {
            source_data=image->GetLum(type);
        }
        else
        if(channels==2)
        {  
            source_data=image->GetRG(type);
        }
        else
        if(channels==3)
        {
            source_data=image->GetRGBA(type);
        }
        else
        if(channels==4)
        {   
            source_data=image->GetRGBA(type);
        }
        else
        {
            std::cerr<<"unknow channels: "<<channels<<std::endl;
            return;
        }

        if(!CMips.AllocateMipSet(&MipSetIn,cf,tdt,TT_2D,width,height,1))
        {
            std::cerr<<"CMP_ERR_MEM_ALLOC_FOR_MIPSET"<<std::endl;
            return;
        }

        CMP_MipLevel *cmp_mip_level=CMips.GetMipLevel(&MipSetIn,0);

        if(!CMips.AllocateMipLevelData(cmp_mip_level, width, height, cf, tdt))
        {
            std::cerr<<"CMP_ERR_MEM_ALLOC_FOR_MIPSET"<<std::endl;
            return;
        }

        MipSetIn.m_nMipLevels = 1;
        MipSetIn.m_format     = source_fmt;

        CMP_BYTE* pData = cmp_mip_level->m_pbData;

        CMP_DWORD dwPitch;

        if(channels<3)
            dwPitch=pixel_bytes*channels*MipSetIn.m_nWidth;
        else
            dwPitch=pixel_bytes*4*MipSetIn.m_nWidth;
            
        CMP_DWORD dwSize  = dwPitch * MipSetIn.m_nHeight;

        memcpy(pData,source_data,dwSize);
    }

    void InitOption()
    {
        hgl_zero(kernel_options);

        kernel_options.height       =image->height();
        kernel_options.width        =image->width();
        kernel_options.fquality     =1.0f;        
        kernel_options.format       =target_fmt;
        kernel_options.encodeWith   =CMP_HPC;
        kernel_options.threads      =8;
        kernel_options.getPerfStats =false;
        kernel_options.getDeviceInfo=false;

        std::cout<<"Compress Image To: "<<image->width()<<"x"<<image->height()<<" "<<target_fmt_name.c_str()<<" format";
    }

    static bool CMP_API CMP_Feedback_Proc(CMP_FLOAT fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
    {
        putchar('.');
        return(false);
    }
 
    uint32 Write() override
    {
        InitOption();
        InitMipSetIn();

        hgl_zero(MipSetOut);

        CMP_ProcessTexture(&MipSetIn,&MipSetOut,kernel_options,&TextureFileCreaterCompress::CMP_Feedback_Proc);

        std::cout<<" "<<MipSetOut.dwDataSize<<" bytes."<<std::endl;

        uint32 result=TextureFileCreater::Write(MipSetOut.pData,MipSetOut.dwDataSize);

        CMips.FreeMipSet(&MipSetOut);
        CMips.FreeMipSet(&MipSetIn);

        return result;
    }
};//class TextureFileCreaterCompress:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterCompress(const PixelFormat *pf)
{
    return(new TextureFileCreaterCompress(pf));
}
