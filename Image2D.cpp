#include"Image2D.h"

const uint GetStride(const enum class PixelDataType type)
{
    const uint stride[]={0,1,2,4,4,8};

    return stride[(size_t)type];
}
   
Image2D::Image2D(const uint w,const uint h,const uint c,const PixelDataType &pdt,void *ptr)
{
    width=w;
    height=h;
    channels=c;
    type=pdt;
    data=ptr;
}

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

class PixelStream
{
public:

    virtual const double get()=0;
    virtual void put(const double &)=0;
};//

template<typename T> class PISImple:public PixelStream
{
protected:

    T *pointer;

public:

    PISImple(void *ptr){pointer=(T *)ptr;}

    const double get() override;
    void put(const double &) override;
};

template<typename T> class PISFloat:public PISImple<T>
{
public:

    using PISImple<T>::PISImple;

    const double get() override
    {
        double result=double(*pointer);

        ++pointer;

        return result;
    }

    void put(const double &value)
    {
        *pointer=(T)value;

        ++pointer;
    }
};

template<typename T,const T max_value> class PISUinteger:public PISImple<T>
{
public:

    using PISImple<T>::PISImple;

    const double get() override
    {
        double result=double(*pointer)/double(max_value);

        ++pointer;

        return result;
    }

    void put(const double &value)
    {
        *pointer=T(value*max_value);

        ++pointer;
    }
};

using PISF32=PISFloat<float>;
using PISF64=PISFloat<double>;
using PISU8 =PISUinteger<uint8,HGL_U8_MAX>;
using PISU16=PISUinteger<uint16,HGL_U16_MAX>;
using PISU32=PISUinteger<uint32,HGL_U32_MAX>;

PixelStream *CreatePIS(const PixelDataType &type,void *data)
{
    if(type==PixelDataType::F32)return(new PISF32(data));else
    if(type==PixelDataType::F64)return(new PISF64(data));else
    if(type==PixelDataType::U8 )return(new PISU8 (data));else
    if(type==PixelDataType::U16)return(new PISU16(data));else
    if(type==PixelDataType::U32)return(new PISU32(data));else
        return(nullptr);
}

void *CreateData(const PixelDataType &type,const uint total)
{
    if(type==PixelDataType::F32)return(new float [total]);else
    if(type==PixelDataType::F64)return(new double[total]);else
    if(type==PixelDataType::U8 )return(new uint8 [total]);else
    if(type==PixelDataType::U16)return(new uint16[total]);else
    if(type==PixelDataType::U32)return(new uint32[total]);else
        return(nullptr);
}

void Image2D::TypeConvert(const PixelDataType &new_type)
{
    if(new_type==type)return;

    uint total=GetPixelsTotal()*channels;

    void *new_data=CreateData(new_type,total);

    PixelStream *src=CreatePIS(type,data);
    PixelStream *tar=CreatePIS(new_type,new_data);

    while(total--)
        tar->put(src->get());

    delete src;
    delete[] data;

    data=new_data;
    type=new_type;
}
