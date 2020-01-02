#include<hgl/type/DataType.h>
#include<hgl/math/Vector.h>

/**
 * A_bit_more_deferred_-_CryEngine3

    Normal to GBufffer:

    G=normalize(N.xy)*sqrt(-N.z*0.5+0.5)

    GBuffer to Normal:

    N.z=length2(G.xy)*2-1
    N.xy=normalize(G.xy)*sqrt(1-N.z*N.z)
*/

namespace hgl
{
    void normal_compress(uint8 *x,uint8 *y,const uint8 *rgb,const uint count)
    {
        Vector3f in;
        float f;
        Vector2f out;

        for(uint i=0;i<count;i++)
        {
            in.x=float(*rgb)/255.0f;++rgb;
            in.y=float(*rgb)/255.0f;++rgb;
            in.z=float(*rgb)/255.0f;++rgb;

//Spheremap Transform,PSNR 48.071 dB,15 GPU cycles
            f=sqrt(8*in.z+8);
            out=in.xy()/f+Vector2f(0.5);

            out=out.Clamp01();

            *x=out.x*255;++x;
            *y=out.y*255;++y;
        }
    }

    void normal_decompress(uint8 *rgb,const uint8 *x,const uint8 *y,const uint count)
    {
        Vector2f in;
        Vector2f fenc;
        Vector3f out;
        float f,g;

        for(uint i=0;i<count;i++)
        {
            in.x=(*x)/255.0f;++x;
            in.y=(*y)/255.0f;++y;
            
//Spheremap Transform,PSNR 48.071 dB,15 GPU cycles
            fenc=in*4.0f-Vector2f(2.0f);
            f=dot(fenc,fenc);
            g=sqrt(1.0-f/4.0f);

            out.x=fenc.x*g;
            out.y=fenc.y*g;
            out.z=1-f/2.0f;
            
            out=out.Clamp01();

            *rgb=out.x*255;++rgb;
            *rgb=out.y*255;++rgb;
            *rgb=out.z*255;++rgb;
        }
    }
}//namespace hgl
