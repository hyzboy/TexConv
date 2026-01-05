#include<iostream>
#include<hgl/type/DataType.h>
#include<hgl/filesystem/FileSystem.h>
#include"ImageLoader.h"

namespace df
{
    struct Point
    {
        uint32 x_offset,y_offset;

    public:

        const uint DistSq()const{return x_offset*x_offset+y_offset*y_offset;}
    };//struct Point;

    static Point inside_point{0,0};
    static Point outside_point{9999,9999};

    class Grid
    {
        int width,height;
        Point *data;

    public:

        Grid(const int w,const int h)
        {
            data=new Point[w*h];

            width=w;
            height=h;
        }

        ~Grid()
        {
            delete[] data;
        }

        const uint Distance(const int col,const int row)const
        {
            return sqrt(data[col+row*width].DistSq());
        }

        const Point &Get(const int col,const int row)
        {
            if(col<0||col>=width
             ||row<0||row>=height)
            return outside_point;

            return data[col+row*width];
        }

        bool Put(const int col,const int row,const Point &p)
        {
            if(col<0||col>=width
             ||row<0||row>=height)
             return(false);

            data[col+row*width]=p;
            return(true);
        }

        void Compare(Point &p,int col,int row,int offset_col,int offset_row)
        {
            Point other=Get(col+offset_col,row+offset_row);

            other.x_offset+=offset_col;
            other.y_offset+=offset_row;

            if(other.DistSq()<p.DistSq())
                p=other;
        }

        void GenerateSDF()
        {
            Point p;

            // pass 0
            for(int row=0;row<height;row++)
            {
                for(int col=0;col<width;col++)
                {
                    p=Get(col,row);

                    Compare(p,col,row,-1, 0);
                    Compare(p,col,row, 0,-1);
                    Compare(p,col,row,-1,-1);
                    Compare(p,col,row, 1,-1);

                    Put(col,row,p);
                }

                for(int col=width-1;col>=0;col--)
                {
                    p=Get(col,row);
                    Compare(p,col,row,1,0);
                    Put(col,row,p);
                }
            }

            //pass 1
            for(int row=height-1;row>=0;row--)
            {
                for(int col=width-1;col>=0;col--)
                {
                    p=Get(col,row);
                    Compare(p,col,row, 1, 0);
                    Compare(p,col,row, 0, 1);
                    Compare(p,col,row,-1, 1);
                    Compare(p,col,row, 1, 1);
                    Put(col,row,p);
                }

                for(int col=0;col<width;col++)
                {
                    p=Get(col,row);
                    Compare(p,col,row,-1,0);
                    Put(col,row,p);
                }
            }
        }
    };//class Grid
}//namespace df

int os_main(int argc,os_char **argv)
{
    os_out<<OS_TEXT("Distance Field Generater v1.1")<<std::endl;
    os_out<<std::endl;

    if(argc<2)
    {
        os_out<<OS_TEXT("example: DFGen 1.png [alpha]")<<std::endl<<std::endl;
        return 0;
    }

    logger::InitLogger(OS_TEXT("DFGen"));    

    InitImageLibrary(nullptr);

    ImageLoader img;
    bool use_alpha=false;

    if(!img.LoadFile(argv[1]))
    {
        os_err<<OS_TEXT("open file <")<<argv[1]<<OS_TEXT("> failed.")<<std::endl;
        return -1;
    }

    if(img.channels()==1)
    {
    }
    else
    if(img.channels()==4)
    {
        if(argc==3)
        {
            if(argv[2][0]=='a'
             ||argv[2][0]=='A')
                use_alpha=true;
        }
    }

    os_out <<(use_alpha?OS_TEXT("use alpha data."):OS_TEXT("use luminance data."))<<std::endl;

    const uint8 *op=(const uint8 *)(use_alpha?img.GetAlpha(IMAGE_UNSIGNED_BYTE):img.ToGray());

    AutoDelete<df::Grid> grid1=new df::Grid(img.width(),img.height());
    AutoDelete<df::Grid> grid2=new df::Grid(img.width(),img.height());

    df::outside_point.x_offset=img.width();
    df::outside_point.y_offset=img.height();

    for(int row=0;row<img.height();row++)
    {
        for(int col=0;col<img.width();col++)
        {
            if(*op<128)
            {
                grid1->Put(col,row,df::inside_point);
                grid2->Put(col,row,df::outside_point);
            }
            else
            {
                grid2->Put(col,row,df::inside_point);
                grid1->Put(col,row,df::outside_point);
            }

            ++op;
        }
    }

    grid1->GenerateSDF();
    grid2->GenerateSDF();

    AutoDeleteArray<uint8> df_bitmap(img.pixel_total());
    uint8 *tp=df_bitmap;

    uint32 dist;
    int c;

    for(int row=0;row<img.height();row++)
    {
        for(int col=0;col<img.width();col++)
        {
            dist=grid1->Distance(col,row)-grid2->Distance(col,row);

            c=dist*3+128;

            if(c<0)c=0;
            if(c>255)c=255;

            *tp++=c;
        }
    }

    OSString filename=hgl::filesystem::ClipFileMainname(OSString(argv[1]));

    filename+=OS_TEXT("_df.png");

    os_out<<OS_TEXT("output: ")<<filename.c_str()<<std::endl;

    SaveImageToFile(filename,img.width(),img.height(),1,IMAGE_UNSIGNED_BYTE,df_bitmap);

    ShutdownImageLibrary();
    return(0);
}
