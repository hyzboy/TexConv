#ifndef HGL_GRAPH_BITMAP2D_INCLUDE
#define HGL_GRAPH_BITMAP2D_INCLUDE

#include<hgl/type/DataType.h>
#include"pixel_format.h"
namespace hgl
{
    namespace graph
    {
        class Bitmap
        {
        protected:

            uint32 width,height;
            uint32 channel;
            uint32 bit;

            uint8 component[4];     //成份排列顺序

            uint32 total_pixels;
            uint32 line_bytes;
            uint32 total_bytes;

            uint8 *data;

        public:

            const uint32 GetWidth()const{return width;}
            const uint32 GetHeight()const{return height;}
            const uint32 GetCahnnel()const{return channel;}
            const uint32 GetBit()const{return bit;}
            const uint32 GetTotalPixels()const{return total_pixels;}
            const uint32 GetTotalBytes()const{return total_bytes;}

            void *GetData(){return data;}

        public:

            Bitmap()
            {
                hgl_zero(*this);
            }

            Bitmap(const uint32 w,const uint32 h,const uint32 c,const uint32 b)
            {
                width=w;
                height=h;
                channel=c;
                bit=b;

                total_pixels=width*height;
                line_bytes=(width*channel*bit)>>3;
                total_bytes=line_bytes*height;

                data=new uint8[total_bytes];
            }

            virtual ~Bitmap()
            {
                SAFE_CLEAR_ARRAY(data);
            }

            void SetComponentOrder(int n,const uint8 *c)
            {
                if(n<0||n>4)return;
                if(!c)return;

                memcpy(component,c,n);
            }

            void Flip()
            {
                uint8 *tmp=new uint8[line_bytes];

                uint8 *tp=tmp;
                uint8 *bp=tmp+line_bytes*(height-1);

                while(tp<bp)
                {
                    memcpy(tmp,tp,line_bytes);
                    memcpy(tp,bp,line_bytes);
                    memcpy(bp,tmp,line_bytes);

                    tp+=line_bytes;
                    bp-=line_bytes;
                }

                delete[] tmp;
            }
        };//class Bitmap2D

        template<typename T>
        class BitmapUInteger:public Bitmap
        {
        public:

            using Bitmap::Bitmap;
            virtual ~BitmapUInteger()=default;

            BitmapUInteger<T> *Half()
            {
            }
        };

        template<typename T>
        class BitmapR:public Bitmap
        {
            T *pixels;

        public:

            BitmapR():Bitmap()
            {
                pixels=nullptr;
            }

            BitmapR(const uint32 w,const uint32 h,const uint32 b):Bitmap(w,h,1,b)
            {
                pixels=data;
            }

            virtual ~BitmapR()=default;
        };//class BitmapR:public Bitmap

        template<typename T>
        class BitmapRG:public Bitmap
        {
            T *pixels;

        public:

            BitmapRG():Bitmap()
            {
                pixels=nullptr;
            }

            BitmapRG(const uint32 w,const uint32 h,const uint32 b):Bitmap(w,h,2,b)
            {
                pixels=data;
            }

            virtual ~BitmapRG()=default;
        };//class BitmapRG:public Bitmap

        enum class RGBOrder
        {
            RGB=0,
            BGR=1,
        };//

        template<typename T>
        class BitmapRGB:public Bitmap
        {
            T *pixels;

            RGBOrder component_order;

        public:

            BitmapRGB():Bitmap()
            {
                pixels=nullptr;
            }

            BitmapRGB(const uint32 w,const uint32 h,const uint32 b):Bitmap(w,h,3,b)
            {
                pixels=data;
            }

            virtual ~BitmapRGB()=default;

            bool SetComponentOrder(RGBOrder order)
            {
                if(order==RGBOrder::RGB)Bitmap::SetComponent(3,"RGB");else
                if(order==RGBOrder::BGR)Bitmap::SetComponent(3,"BGR");else
                return(false);

                component_order=order;
                return(true);
            }

            void ChangeComponentOrder(RGBOrder order)
            {
                if(component_order==order)return;

                T *p=pixels;
                T temp;

                for(uint32 i=0;i<total_pixels;i++)
                {
                    temp=p[0];
                    p[0]=p[2];
                    p[2]=temp;

                    p+=3;
                }

                SetComponentOrder(order);
            }

            void FlipG();        //Nomral中经常需要翻转Y值
        };//class BitmapRGB:public Bitmap

        template<> void BitmapRGB<uint8>::FlipG()
        {
            uint8 *p=pixels+1;

            for(uint i=0;i<total_pixels;i++)
            {
                *p=0xFF-(*p);
                p+=3;
            }
        }

        template<> void BitmapRGB<uint16>::FlipG()
        {
            uint16 *p=pixels+1;

            for(uint i=0;i<total_pixels;i++)
            {
                *p=0xFFFF-(*p);
                p+=3;
            }
        }

        template<> void BitmapRGB<uint32>::FlipG()
        {
            uint32 *p=pixels+1;

            for(uint i=0;i<total_pixels;i++)
            {
                *p=0xFFFFFFFF-(*p);
                p+=3;
            }
        }

        template<> void BitmapRGB<float>::FlipG()
        {
            float *p=pixels+1;

            for(uint i=0;i<total_pixels;i++)
            {
                *p=-(*p);
                p+=3;
            }
        }

        template<> void BitmapRGB<double>::FlipG()
        {
            double *p=pixels+1;

            for(uint i=0;i<total_pixels;i++)
            {
                *p=-(*p);
                p+=3;
            }
        }

        enum class RGBAOrder
        {
            RGBA=0,
            BGRA=1,
            ARGB=2,
            ABGR=3
        };

        template<typename T>
        class BitmapRGBA:public Bitmap
        {
            T *pixels;
            
            RGBAOrder component_order;

        public:

            BitmapRGBA():Bitmap()
            {
                pixels=nullptr;
            }

            BitmapRGBA(const uint32 w,const uint32 h,const uint32 b):Bitmap(w,h,4,b)
            {
                pixels=data;
            }

            virtual ~BitmapRGBA()=default;

            bool SetComponentOrder(RGBAOrder order)
            {
                if(order==RGBAOrder::RGBA)Bitmap::SetComponent(4,"RGBA",b);else
                if(order==RGBAOrder::BGRA)Bitmap::SetComponent(4,"BGRA",b);else
                if(order==RGBAOrder::ARGB)Bitmap::SetComponent(4,"ARGB",b);else
                if(order==RGBAOrder::ABGR)Bitmap::SetComponent(4,"ABGR",b);else
                return(false);

                component_order=order;
                return(true);
            }

            void ChangeComponentOrder(RGBAOrder order)
            {
                if(component_order==order)return;

                T *p=pixels;
                T temp;

                if((component_order==RGBAOrder::RGBA&&order==RGBAOrder::BGRA)
                 ||(component_order==BGRAOrder::BGRA&&order==RGBAOrder::RGBA)){for(uint32 i=0;i<total_pixels;i++){temp=p[0];p[0]=p[2];p[2]=temp;p+=4;}}else
                if((component_order==RGBAOrder::RGBA&&order==RGBAOrder::ARGB)
                 ||(component_order==BGRAOrder::BGRA&&order==RGBAOrder::ABGR)){for(uint32 i=0;i<total_pixels;i++){temp=p[3];p[3]=p[2];p[2]=p[1];p[1]=p[0];p[0]=temp;p+=4;}}else
                if((component_order==RGBAOrder::RGBA&&order==RGBAOrder::ABGR)
                 ||(component_order==BGRAOrder::BGRA&&order==RGBAOrder::ARGB)){for(uint32 i=0;i<total_pixels;i++){temp=p[3];p[3]=p[0];p[2]=p[1];p[1]=p[2];p[0]=temp;p+=4;}}else
                if((component_order==RGBAOrder::ARGB&&order==RGBAOrder::RGBA)
                 ||(component_order==RGBAOrder::ABGR&&order==RGBAOrder::BGRA)){for(uint32 i=0;i<total_pixels;i++){temp=p[0];p[0]=p[1];p[1]=p[2];p[2]=p[3];p[3]=temp;p+=4;}}else
                if((component_order==RGBAOrder::ARGB&&order==RGBAOrder::BGRA)
                 ||(component_order==RGBAOrder::ABGR&&order==RGBAOrder::RGBA)){for(uint32 i=0;i<total_pixels;i++){temp=p[0];p[0]=p[3];p[1]=p[2];p[2]=p[1];p[3]=temp;p+=4;}}else
                if((component_order==RGBAOrder::ARGB&&order==RGBAOrder::ABGR)
                 ||(component_order==RGBAOrder::ABGR&&order==RGBAOrder::ARGB)){for(uint32 i=0;i<total_pixels;i++){temp=p[1];p[1]=p[3];p[3]=temp;p+=4;}}else

                SetComponentOrder(order);
            }
        };//class BitmapRGBA:public Bitmap

        #define DEF_BITMAP(n,t) \
        using Bitmap##n##b  =Bitmap##t<int8>;   \
        using Bitmap##n##ub =Bitmap##t<uint8>;  \
        using Bitmap##n##s  =Bitmap##t<int16>;  \
        using Bitmap##n##us =Bitmap##t<uint16>; \
        using Bitmap##n##i  =Bitmap##t<int32>;  \
        using Bitmap##n##u  =Bitmap##t<uint32>; \
        using Bitmap##n##f  =Bitmap##t<float>;  \
        using Bitmap##n##d  =Bitmap##t<double>; 

        DEF_BITMAP(1,R)
        DEF_BITMAP(2,RG)
        DEF_BITMAP(3,RGB)
        DEF_BITMAP(4,RGBA)

        #undef DEF_BITMAP
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BITMAP2D_INCLUDE
