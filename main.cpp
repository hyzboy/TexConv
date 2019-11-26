#include<il/il.h>
#include<il/ilu.h>
#include<iostream>
#include"cmd_parse.h"
#include"pixel_format.h"
#include<hgl/type/DataType.h>
#include<hgl/type/StrChar.h>

using namespace hgl;

const	PixelFormat *   glfmt[4]	={NULL,NULL,NULL,NULL};				//选中格式
bool					gen_mipmaps	=false;								//是否产生mipmaps

bool					use_color_key=false;							//是否使用ColorKey
uint8					color_key[3];									//ColorKey颜色

const PixelFormat *CheckOpenGLCoreFormat(const cmd_parse &cmd,const char *flag,const PixelFormat *default_format)
{
    std::string fmtstr;

    if(!cmd.GetString(flag,fmtstr))return(nullptr);

    const PixelFormat *result=GetPixelFormat(fmtstr.c_str());

    if(result)return(result);

    return default_format;
}

void CheckOpenGLCoreFormat(const cmd_parse &cmd)
{
    //指定格式
    glfmt[0]=CheckOpenGLCoreFormat(cmd,"/R:",      &TextureFormatInfoList[HGL_SF_R8]);
    glfmt[1]=CheckOpenGLCoreFormat(cmd,"/RG:",     &TextureFormatInfoList[HGL_SF_RG8]);
    glfmt[2]=CheckOpenGLCoreFormat(cmd,"/RGB:",    &TextureFormatInfoList[HGL_SF_RGB8]);
    glfmt[3]=CheckOpenGLCoreFormat(cmd,"/RGBA:",   &TextureFormatInfoList[HGL_SF_RGBA8]);
}

void CheckColorKey(const cmd_parse &cmd)
{
    std::string ckstr;

    if(!cmd.GetString("/ColorKey:",ckstr))return;

    char rgbstr[6];

    hgl_cpy(rgbstr,ckstr.c_str(),6);		//注意：hgl_cpy是跨类型复制的，不要替换成strcpy或memcpy

    ParseHexStr(color_key[0],rgbstr+0);
    ParseHexStr(color_key[1],rgbstr+2);
    ParseHexStr(color_key[2],rgbstr+4);

    use_color_key=true;
}

int main(int argc,char **argv)
{
    std::cout<<"Image to Texture Convert tools 1.1"<<std::endl<<std::endl;

    if(argc<=1)
    {
        std::cout<< "Command format:\n"
                    "\tTexConv [/s] [/mip] <filename or pathname>\n"
                    "\n"
                    "Params:\n"
                    "\t/s : proc sub-directory\n"
                    "\n\n";

        return 0;
    }

    cmd_parse cp(argc,argv);

    if(cp.Find("/s")!=-1)sub=true;						    //检测是否处理子目录
    if(cp.Find("/mip")!=-1)gen_mipmaps=true;				//检测是否生成mipmaps
    
    CheckColorKey(cp);
    CheckFormat(cp);								        //检测推荐格式
   
    ilInit();



	ilShutDown();
}
