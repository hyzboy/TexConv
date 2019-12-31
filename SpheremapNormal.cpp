#include<hgl/type/DataType.h>
#include<hgl/math/Vector.h>

/**
 * A_bit_more_deferred_-_CryEngine3

    Normal to GBufffer:

    G=normalize(N.xy)*sqrt(N.z*0.5+0.5)

    GBuffer to Normal:

    N.z=length2(G.xy)*2-1
    N.xy=normalize(G.xy)*sqrt(1-N.z*N.z)
*/

namespace hgl
{
    void normal_compress(uint8 *x,uint8 *y,const uint8 *rgb,const uint count)
    {
        Vector3f in;
        Vector2f in_xy;
        Vector2f out;

        for(uint i=0;i<count;i++)
        {
            in.x=float(*rgb)/255.0f;++rgb;
            in.y=float(*rgb)/255.0f;++rgb;
            in.z=float(*rgb)/255.0f;++rgb;

            in_xy=in.xy().Normalized();
            out=in_xy*sqrt(in.z*0.5+0.5);

            *x=out.x*255;++x;
            *y=out.y*255;++y;
        }
    }

    void normal_decompress(uint8 *rgb,const uint8 *x,const uint8 *y,const uint count)
    {
        Vector2f in;
        Vector2f in_normal;
        Vector3f out;
        float s;

        for(uint i=0;i<count;i++)
        {
            in.x=(*x)/255.0f;++x;
            in.y=(*y)/255.0f;++y;

            out.z=length(in)*2.0-1;
            in_normal=in.Normalized();
            s=sqrt(1-out.z*out.z);
            out.x=in_normal.x*s;
            out.y=in_normal.y*s;

            *rgb=out.x*255;++rgb;
            *rgb=out.y*255;++rgb;
            *rgb=out.z*255;++rgb;
        }
    }
}//namespace hgl
