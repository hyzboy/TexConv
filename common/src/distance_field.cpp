#include "texconv/tex_distance_field.h"

#include <math.h>

namespace
{
    /// 网格点:记录到目标区域最近点的偏移(结构对齐旧 DFGen 的 df::Point,
    /// 但使用有符号偏移——旧实现的 uint32 偏移在 {0,0}+(-1) 时会回绕成巨大值,
    /// 导致紧邻目标像素的最近候选被错误丢弃;旧代码从未编译发布过,此处修正)
    struct DFPoint
    {
        int32_t x_offset, y_offset;

        uint64_t DistSq() const
        {
            const int64_t x = x_offset, y = y_offset;
            return uint64_t(x * x + y * y);
        }
    };

    /// 两遍扫描距离变换网格(扫描结构与旧 DFGen 的 df::Grid 一致)
    class DFGrid
    {
        int      width, height;
        DFPoint *data;
        DFPoint  outside;   // 越界返回值

    public:

        DFGrid(const int w, const int h, const DFPoint &out)
        {
            data    = new DFPoint[w * h];
            width   = w;
            height  = h;
            outside = out;
        }

        ~DFGrid() { delete[] data; }

        uint32_t Distance(const int col, const int row) const
        {
            return uint32_t(sqrt(double(data[col + row * width].DistSq())));
        }

        const DFPoint &Get(const int col, const int row) const
        {
            if(col < 0 || col >= width
             ||row < 0 || row >= height)
                return outside;

            return data[col + row * width];
        }

        void Put(const int col, const int row, const DFPoint &p)
        {
            data[col + row * width] = p;
        }

        void Compare(DFPoint &p, int col, int row, int offset_col, int offset_row)
        {
            DFPoint other = Get(col + offset_col, row + offset_row);

            other.x_offset += offset_col;
            other.y_offset += offset_row;

            if(other.DistSq() < p.DistSq())
                p = other;
        }

        void GenerateSDF()
        {
            DFPoint p;

            // pass 0
            for(int row = 0; row < height; row++)
            {
                for(int col = 0; col < width; col++)
                {
                    p = Get(col, row);

                    Compare(p, col, row, -1,  0);
                    Compare(p, col, row,  0, -1);
                    Compare(p, col, row, -1, -1);
                    Compare(p, col, row,  1, -1);

                    Put(col, row, p);
                }

                for(int col = width - 1; col >= 0; col--)
                {
                    p = Get(col, row);

                    Compare(p, col, row, 1, 0);

                    Put(col, row, p);
                }
            }

            // pass 1
            for(int row = height - 1; row >= 0; row--)
            {
                for(int col = width - 1; col >= 0; col--)
                {
                    p = Get(col, row);

                    Compare(p, col, row,  1,  0);
                    Compare(p, col, row,  0,  1);
                    Compare(p, col, row, -1,  1);
                    Compare(p, col, row,  1,  1);

                    Put(col, row, p);
                }

                for(int col = 0; col < width; col++)
                {
                    p = Get(col, row);

                    Compare(p, col, row, -1, 0);

                    Put(col, row, p);
                }
            }
        }
    };//class DFGrid
}//namespace

extern "C"
{
    int TexDF_Generate(const uint8_t *src, uint8_t *dst,
                       uint32_t width, uint32_t height,
                       uint8_t threshold, int scale, int bias)
    {
        if(!src || !dst || width == 0 || height == 0)
            return TEX_ERR_PARAM;

        if(scale <= 0) scale = 3;       // 对齐旧 DFGen:dist*3+128
        if(bias <= 0) bias = 128;

        const DFPoint inside {0, 0};
        const DFPoint outside{int32_t(width), int32_t(height)};

        DFGrid grid1(int(width), int(height), outside);
        DFGrid grid2(int(width), int(height), outside);

        // grid1:到暗部(值<threshold)的距离;grid2:到亮部的距离
        // 对齐旧 DFGen:暗部像素 grid1=inside/grid2=outside,亮部相反
        for(uint32_t row = 0; row < height; row++)
        {
            for(uint32_t col = 0; col < width; col++)
            {
                const uint8_t v = src[col + row * width];

                if(v < threshold)
                {
                    grid1.Put(int(col), int(row), inside);
                    grid2.Put(int(col), int(row), outside);
                }
                else
                {
                    grid2.Put(int(col), int(row), inside);
                    grid1.Put(int(col), int(row), outside);
                }
            }
        }

        grid1.GenerateSDF();
        grid2.GenerateSDF();

        // 输出:(到暗部距离 - 到亮部距离) —— 亮部(形状)内部为正
        for(uint32_t row = 0; row < height; row++)
        {
            for(uint32_t col = 0; col < width; col++)
            {
                const int dist = int(grid1.Distance(int(col), int(row)))
                               - int(grid2.Distance(int(col), int(row)));

                int c = dist * scale + bias;

                if(c < 0)  c = 0;
                if(c > 255)c = 255;

                dst[col + row * width] = uint8_t(c);
            }
        }

        return TEX_OK;
    }
}//extern "C"
