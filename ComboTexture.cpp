#include<iostream>
#include<iomanip>
#include<ILImage.h>
#include<hgl/type/DataType.h>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/type/Smart.h>
#include<hgl/graph/ColorSpace.h>

using namespace hgl;
using namespace hgl::util;

#if HGL_OS == HGL_OS_Windows
    #define std_cout    std::wcout
#else
    #define std_cout    std::cout
#endif//

namespace hgl
{
    template<typename T>
    const T clamp(const T &in,const T min_v,const T max_v)
    {
        if(in<min_v)return min_v;
        if(in>max_v)return max_v;

        return in;
    }

    const double clamp(const double &in)
    {
        return clamp<double>(in,0.0f,1.0f);
    }

	constexpr double clamp_u=0.436*255.0;
	constexpr double clamp_v=0.615*255.0;
		
	void RGB2YUV(uint8 *y,uint8 *u,uint8 *v,const uint8 *rgb,const uint count)
	{
		uint8 r,g,b;

		for(uint i=0;i<count;i++)
		{
			r=*rgb++;
			g=*rgb++;
			b=*rgb++;

			*y++=				  0.299		* r	+ 0.587 	* g + 0.114 	* b;
			*u++=(clamp_u		- 0.14713 	* r	- 0.28886 	* g	+ 0.436 	* b) / 0.872;
			*v++=(clamp_v		+ 0.615 	* r	- 0.51499 	* g	- 0.10001 	* b) / 1.230;
		}
	}

    template<typename T>
    void MixRGB(T *rgb,const T *r,const T *g,const T *b,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *rgb=*r;++rgb;++r;
            *rgb=*g;++rgb;++g;
            *rgb=*b;++rgb;++b;
        }
    }

    template<typename T>
    void MixRGBA(T *rgba,const T *r,const T *g,const T *b,const T *a,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *rgba=*r;++rgba;++r;
            *rgba=*g;++rgba;++g;
            *rgba=*b;++rgba;++b;
            *rgba=*a;++rgba;++a;
        }
    }

    template<typename T>
    void MixRGBA(T *rgba,const T *rgb,const T *a,const uint count)
    {
        for(uint i=0;i<count;i++)
        {
            *rgba=*rgb;++rgba;++rgb;
            *rgba=*rgb;++rgba;++rgb;
            *rgba=*rgb;++rgba;++rgb;
            *rgba=*a;++rgba;++a;
        }
    }

    bool SaveRGBAFile(const OSString &filename,const uint w,const uint h,const float scale,const uint8 *rgb,const uint8 *a,const OSString &flag)
    {
        const OSString out_filename=OSString(filename)+OS_TEXT("_")+flag+OS_TEXT(".png");

        AutoDeleteArray<uint8> pixels=new uint8[w*h*4];

        MixRGBA<uint8>(pixels,rgb,a,w*h);

        if(SaveImageToFile(out_filename,w,h,scale,4,IL_UNSIGNED_BYTE,pixels))
        {
            std_cout<<OS_TEXT("Output ")<<flag.c_str()<<OS_TEXT(": ")<<out_filename.c_str()<<std::endl;
            return(true);
        }

        return(false);
    }

    bool SaveRGBAFile(const OSString &filename,const uint w,const uint h,const float scale,const uint8 *r,const uint8 *g,const uint8 *b,const uint8 *a,const OSString &flag)
    {
        const OSString out_filename=OSString(filename)+OS_TEXT("_")+flag+OS_TEXT(".png");

        AutoDeleteArray<uint8> pixels=new uint8[w*h*4];

        MixRGBA<uint8>(pixels,r,g,b,a,w*h);

        if(SaveImageToFile(out_filename,w,h,scale,4,IL_UNSIGNED_BYTE,pixels))
        {
            std_cout<<OS_TEXT("Output ")<<flag.c_str()<<OS_TEXT(": ")<<out_filename.c_str()<<std::endl;
            return(true);
        }

        return(false);
    }

    bool SaveRGBFile(const OSString &filename,const uint w,const uint h,const float scale,const uint8 *r,const uint8 *g,const uint8 *b,const OSString &flag)
    {
        const OSString out_filename=OSString(filename)+OS_TEXT("_")+flag+OS_TEXT(".png");

        AutoDeleteArray<uint8> pixels=new uint8[w*h*3];

        MixRGB<uint8>(pixels,r,g,b,w*h);

        if(SaveImageToFile(out_filename,w,h,scale,3,IL_UNSIGNED_BYTE,pixels))
        {
            std_cout<<OS_TEXT("Output ")<<flag.c_str()<<OS_TEXT(": ")<<out_filename.c_str()<<std::endl;
            return(true);
        }

        return(false);
    }
}//namespace hgl

#if HGL_OS == HGL_OS_Windows
    int wmain(int argc,wchar_t **argv)
#else
    int main(int argc,char **argv)
#endif//
{
    std::cout<<"Combo Texture Compression"<<std::endl<<std::endl;

    if(argc<2)
    {
        // FullTexture: Y+Normal
        // HalfTexture: U+V+M+R
        std::cout<<"Example: ComboTexture <output name> MPBR Color.png Normal.png Metallic.png Roughness.png"<<std::endl;

        // FullTexture: Y1+Y2+Y3+Y4
        // HalfTexture: U1+U2+U3+U4
        // HalfTexture: V1+V2+V3+V4
        std::cout<<"Example: ComboTexture <output name> 4RGB Color1.png Color2.png Color3.png Color4.png"<<std::endl;

        // FullTexture: Y1+Normal1
        // FullTexture: Y2+Normal2
        // HalfTexture: U1+V1+U2+V2
        std::cout<<"Example: ComboTexture <output name> 2CN Color1.png Normal1.png Color2.png Normal2.png"<<std::endl;

        //// FullTexture: Y1+Y2+G
        //// HalfTexture: U1+V1+U2+V2
        //std::cout<<"Example1: ComboTexture <output name> C2G1 Color1.png Color2.png Grayscale.png"<<std::endl;

        //// FullTexture: Y+M+R
        //// HalfTexture: U+V+G
        //std::cout<<"Example2: ComboTexture <output name> MPBR6 Color.png Matallic.png Roughness.png Grayscale.png"<<std::endl;

        //// FullTexture: Y+G1+G2
        //// HalfTexture: U+V+M+R
        //std::cout<<"Example3: ComboTexture <output name> MPBR7 Color.png Matallic.png Roughness.png Grayscale1.png Grayscale2.png"<<std::endl;

        return(0);
    }

    ilInit();

    if(stricmp(argv[2],"MPBR")==0)
    {
        ILImage color,normal,metallic,roughness;

        uint w,h;

        if(!color.LoadFile(argv[3]))return(1);
        if(!normal.LoadFile(argv[4]))return(1);
        if(!metallic.LoadFile(argv[5]))return(1);
        if(!roughness.LoadFile(argv[6]))return(1);

        color.ToRGB();
        normal.ToRGB();
        metallic.ToGray();
        roughness.ToGray();

        w=color.width();
        h=color.height();

        normal.Resize(w,h);
        metallic.Resize(w,h);
        roughness.Resize(w,h);

        const uint pixel_total=w*h;

        AutoDeleteArray<uint8> y=new uint8[pixel_total];
        AutoDeleteArray<uint8> u=new uint8[pixel_total];
        AutoDeleteArray<uint8> v=new uint8[pixel_total];

        RGB2YUV(y,u,v,(uint8 *)color.GetRGB(IL_UNSIGNED_BYTE),pixel_total);

        SaveRGBAFile(   argv[1],
                        w,h,1.0,
                        (uint8 *)normal.GetRGB(IL_UNSIGNED_BYTE),
                        y,
                        OS_TEXT("NormalLuma"));

        SaveRGBAFile(   argv[1],
                        w,h,0.5,
                        u,v,
                        (uint8 *)metallic.GetLum(IL_UNSIGNED_BYTE),
                        (uint8 *)roughness.GetLum(IL_UNSIGNED_BYTE),
                        OS_TEXT("CbCrMR"));
    }
    else
    if(stricmp(argv[2],"4RGB")==0)
    {
        ILImage rgb[4];

        uint w,h;

        if(!rgb[0].LoadFile(argv[3]))return 1;
        if(!rgb[1].LoadFile(argv[4]))return 1;
        if(!rgb[2].LoadFile(argv[5]))return 1;
        if(!rgb[3].LoadFile(argv[6]))return 1;

        rgb[0].ToRGB();
        rgb[1].ToRGB();
        rgb[2].ToRGB();
        rgb[3].ToRGB();

        w=rgb[0].width();
        h=rgb[0].height();

        rgb[1].Resize(w,h);
        rgb[2].Resize(w,h);
        rgb[3].Resize(w,h);
        
        const uint pixel_total=w*h;
        AutoDeleteArray<uint8> y[4],u[4],v[4];

        for(uint i=0;i<4;i++)
        {
            y[i]=new uint8[pixel_total];
            u[i]=new uint8[pixel_total];
            v[i]=new uint8[pixel_total];

            RGB2YUV(y[i],u[i],v[i],(uint8 *)rgb[i].GetRGB(IL_UNSIGNED_BYTE),pixel_total);
        }

        SaveRGBAFile(   argv[1],
                        w,h,1.0,
                        y[0],y[1],y[2],y[3],
                        OS_TEXT("4Luma"));

        SaveRGBAFile(   argv[1],
                        w,h,0.5,
                        u[0],u[1],u[2],u[3],
                        OS_TEXT("4Cb"));

        SaveRGBAFile(   argv[1],
                        w,h,0.5,
                        v[0],v[1],v[2],v[3],
                        OS_TEXT("4Cr"));
    }
    else
    if(stricmp(argv[2],"2CN")==0)
    {
        ILImage color[2],normal[2];

        uint w,h;

        if(!color[0].LoadFile(argv[3]))return(1);
        if(!normal[0].LoadFile(argv[4]))return(1);
        if(!color[1].LoadFile(argv[5]))return(1);
        if(!normal[1].LoadFile(argv[6]))return(1);

        color[0].ToRGB();
        normal[0].ToRGB();
        color[1].ToRGB();
        normal[1].ToRGB();

        w=color[0].width();
        h=color[0].height();
        
        color[1].Resize(w,h);
        normal[0].Resize(w,h);
        normal[1].Resize(w,h);

        const uint pixel_total=w*h;

        AutoDeleteArray<uint8> y[2],u[2],v[2];

        for(uint i=0;i<2;i++)
        {
            y[i]=new uint8[pixel_total];
            u[i]=new uint8[pixel_total];
            v[i]=new uint8[pixel_total];

            RGB2YUV(y[i],u[i],v[i],(uint8 *)color[i].GetRGB(IL_UNSIGNED_BYTE),pixel_total);
        }

        SaveRGBAFile(   argv[1],
                        w,h,1.0,
                        (uint8 *)normal[0].GetRGB(IL_UNSIGNED_BYTE),
                        y[0],
                        OS_TEXT("NormalLuma1"));

        SaveRGBAFile(   argv[1],
                        w,h,1.0,
                        (uint8 *)normal[1].GetRGB(IL_UNSIGNED_BYTE),
                        y[1],
                        OS_TEXT("NormalLuma2"));

        SaveRGBAFile(   argv[1],
                        w,h,0.5,
                        u[0],v[0],
                        u[1],v[1],
                        OS_TEXT("2CbCr"));
    }

    ilShutDown();
    return 0;
}
