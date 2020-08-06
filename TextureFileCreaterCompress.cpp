#include"TextureFileCreater.h"
#include"ILImage.h"
#include<hgl/log/LogInfo.h>
#include<CMP_CompressonatorLib/Common.h>
#include<CMP_CompressonatorLib/Compressonator.h>

void CMP_RegisterHostPlugins();
CMP_ERROR CMP_API CMP_DestroyComputeLibrary(bool forceclose = false);

class TextureFileCreaterCompress:public TextureFileCreater
{
    MipSet MipSetIn;
    MipSet MipSetOut;
    CMP_CMIPS CMips;
    KernelOptions kernel_options;

public:

    using TextureFileCreater::TextureFileCreater;

public:

    void InitMipSetIn()
    {
        const int channels=image->channels();
        const int type=image->type();
        
        memset(&MipSetIn, 0, sizeof(MipSet));

        CMP_ChannelFormat cf;
        CMP_TextureDataType tdt;
        CMP_FORMAT fmt;
        uint8 pixel_bytes;

        void *source_data=nullptr;

        if(type==IL_UNSIGNED_BYTE   ){cf=CF_8bit;    pixel_bytes=1;}else
        if(type==IL_UNSIGNED_SHORT  ){cf=CF_16bit;   pixel_bytes=2;}else
        if(type==IL_UNSIGNED_INT    ){cf=CF_32bit;   pixel_bytes=4;}else
        if(type==IL_HALF            ){cf=CF_Float16; pixel_bytes=2;}else
        if(type==IL_FLOAT           ){cf=CF_Float32; pixel_bytes=4;}else
        {
            std::cerr<<"unknow type: "<<type<<std::endl;
            return;
        }

        if(channels==1)
        {
            tdt=TDT_R;
            
            if(type==IL_UNSIGNED_BYTE   )fmt=CMP_FORMAT_R_8;else
            if(type==IL_UNSIGNED_SHORT  )fmt=CMP_FORMAT_R_16;else
            if(type==IL_HALF            )fmt=CMP_FORMAT_R_16F;else
            if(type==IL_FLOAT           )fmt=CMP_FORMAT_R_32F;else
            {
                std::cerr<<"channels 1 unknow type: "<<type<<std::endl;
                return;
            }

            source_data=image->GetLum(type);
        }
        else
        if(channels==2)
        {
            tdt=TDT_RG;
            
            if(type==IL_UNSIGNED_BYTE   )fmt=CMP_FORMAT_RG_8;else
            if(type==IL_UNSIGNED_SHORT  )fmt=CMP_FORMAT_RG_16;else
            if(type==IL_HALF            )fmt=CMP_FORMAT_RG_16F;else
            if(type==IL_FLOAT           )fmt=CMP_FORMAT_RG_32F;else
            {
                std::cerr<<"channels 2 unknow type: "<<type<<std::endl;
                return;
            }
            
            source_data=image->GetRG(type);
        }
        else
        if(channels==3)
        {
            tdt=TDT_XRGB;
            
            if(type==IL_UNSIGNED_BYTE   )fmt=CMP_FORMAT_RGBA_8888;else
            if(type==IL_UNSIGNED_SHORT  )fmt=CMP_FORMAT_RGBA_16;else
            if(type==IL_HALF            )fmt=CMP_FORMAT_RGBA_16F;else
            if(type==IL_FLOAT           )fmt=CMP_FORMAT_RGBA_32F;else
            {
                std::cerr<<"channels 3 unknow type: "<<type<<std::endl;
                return;
            }

            source_data=image->GetRGBA(type);
        }
        else
        if(channels==4)
        {
            tdt=TDT_ARGB;
            
            if(type==IL_UNSIGNED_BYTE   )fmt=CMP_FORMAT_RGBA_8888;else
            if(type==IL_UNSIGNED_SHORT  )fmt=CMP_FORMAT_RGBA_16;else
            if(type==IL_HALF            )fmt=CMP_FORMAT_RGBA_16F;else
            if(type==IL_FLOAT           )fmt=CMP_FORMAT_RGBA_32F;else
            {
                std::cerr<<"channels 3 unknow type: "<<type<<std::endl;
                return;
            }
            
            source_data=image->GetRGBA(type);
        }
        else
        {
            std::cerr<<"unknow channels: "<<channels<<std::endl;
            return;
        }

        if(!CMips.AllocateMipSet(&MipSetIn,cf,tdt,TT_2D,image->width(),image->height(),1))
        {
            std::cerr<<"CMP_ERR_MEM_ALLOC_FOR_MIPSET"<<std::endl;
            return;
        }

        if(!CMips.AllocateMipLevelData(CMips.GetMipLevel(&MipSetIn, 0), image->width(), image->height(), cf, tdt))
        {
            std::cerr<<"CMP_ERR_MEM_ALLOC_FOR_MIPSET"<<std::endl;
            return;
        }

        MipSetIn.m_nMipLevels = 1;
        MipSetIn.m_format     = CMP_FORMAT_RGBA_8888;

        CMP_BYTE* pData = CMips.GetMipLevel(&MipSetIn,0)->m_pbData;

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
        memset(&kernel_options,0,sizeof(KernelOptions));

        constexpr CMP_FORMAT fmt_list[]=
        {
            CMP_FORMAT_BC1,     //ColorFormat::BC1RGB
            CMP_FORMAT_BC1,     //ColorFormat::BC1RGBA
            CMP_FORMAT_BC2,     //ColorFormat::BC2
            CMP_FORMAT_BC3,     //ColorFormat::BC3
            CMP_FORMAT_BC4,     //ColorFormat::BC4
            CMP_FORMAT_BC5,     //ColorFormat::BC5
            CMP_FORMAT_BC6H,    //ColorFormat::BC6H
            CMP_FORMAT_BC6H_SF, //ColorFormat::BC6H_SF
            CMP_FORMAT_BC7,     //ColorFormat::BC7
        };
            
        kernel_options.height       =image->height();
        kernel_options.width        =image->width();
        kernel_options.fquality     =1.0f;        
        kernel_options.format       =fmt_list[size_t(fmt->format)-size_t(ColorFormat::BC1RGB)];
        kernel_options.encodeWith   =CMP_HPC;
        kernel_options.threads      =4;
        kernel_options.getPerfStats =false;
        kernel_options.getDeviceInfo=false;
    }

    static bool CMP_API CMP_Feedback_Proc(CMP_FLOAT fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
    {
        std::cout<<"progress: "<<fProgress<<std::endl;
        return(true);
    }
 
    bool Write() override
    {
        InitOption();
        InitMipSetIn();
        CMP_RegisterHostPlugins();
        CMP_InitializeBCLibrary();

        memset(&MipSetOut,0,sizeof(MipSet));

        CMP_ProcessTexture(&MipSetIn,&MipSetOut,kernel_options,&TextureFileCreaterCompress::CMP_Feedback_Proc);

        CMP_ShutdownBCLibrary();

        total_bytes=MipSetOut.dwDataSize;
        return TextureFileCreater::Write(MipSetOut.pData);
    }
};//class TextureFileCreaterCompress:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterCompress(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterCompress(pf,image));
}
