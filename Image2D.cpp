#include"Image2D.h"
struct HalfImage
{
    virtual void Process(void *,void *,const uint,const uint,const uint)=0;
};

template<typename T>
struct HalfImageImple:public HalfImage
{
    void Process(void *target,void *source,const uint width,const uint height,const uint channels) override
    {
        const uint line_length=width*channels;

        T *tp=(T *)target;

        T *sp0=(T *)source;
        T *sp1=sp0+channels;
        T *sp2=sp0+line_length;
        T *sp3=sp1+line_length;

        for(uint row=0;row<height;row+=2)
        {
            for(uint col=0;col<=width;col+=2)
            {
                for(uint i=0;i<channels;i++)
                {
                    *tp=((*sp0)+(*sp1)+(*sp2)+(*sp3))/T(4);
                    ++tp;

                    ++sp0;
                    ++sp1;
                    ++sp2;
                    ++sp3;
                }

                sp0+=channels;
                sp1+=channels;
                sp2+=channels;
                sp3+=channels;
            }

            sp0+=line_length;
            sp1+=line_length;
            sp2+=line_length;
            sp3+=line_length;
        }
    }
};

Image2D *Image2D::CreateHalfImage()
{
    HalfImage *hi;
    
    if(type==PixelDataType::U8 )hi=new HalfImageImple<uint8>;else
    if(type==PixelDataType::U16)hi=new HalfImageImple<uint16>;else
    if(type==PixelDataType::U32)hi=new HalfImageImple<uint32>;else
    if(type==PixelDataType::F32)hi=new HalfImageImple<float>;else
    if(type==PixelDataType::F64)hi=new HalfImageImple<double>;else
        return(nullptr);

    uint hw=width>>1;
    uint hh=height>>1;

    void *new_data=new uint8[hw*hh*channels];

    hi->Process(new_data,data,width,height,channels);

    delete hi;

    return(new Image2D(hw,hh,channels,type,new_data));
}