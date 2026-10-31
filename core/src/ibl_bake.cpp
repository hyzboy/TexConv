#include "internal.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace texcore
{
    static constexpr float PI = 3.14159265358979f;

    static constexpr float CUBE_FACE_AXES[6][3][3] =
    {
        {{  1, 0, 0},{ 0, 0,-1},{ 0,-1, 0}},    // +X
        {{ -1, 0, 0},{ 0, 0, 1},{ 0,-1, 0}},    // -X
        {{  0, 1, 0},{ 1, 0, 0},{ 0, 0, 1}},    // +Y
        {{  0,-1, 0},{ 1, 0, 0},{ 0, 0,-1}},    // -Y
        {{  0, 0, 1},{ 1, 0, 0},{ 0,-1, 0}},    // +Z
        {{  0, 0,-1},{-1, 0, 0},{ 0,-1, 0}},    // -Z
    };

    static void CubeFaceToDir(int face, float s, float t, float out[3])
    {
        const auto &a = CUBE_FACE_AXES[face];
        for(int i = 0; i < 3; i++)
            out[i] = a[0][i] + s * a[1][i] + t * a[2][i];
    }

    static void DirToFace(const float d[3], int &out_face, float &out_s, float &out_t)
    {
        float ax[3] = { std::fabs(d[0]), std::fabs(d[1]), std::fabs(d[2]) };
        if(ax[0] >= ax[1] && ax[0] >= ax[2])
        {
            out_face = d[0] > 0 ? 0 : 1;
            out_s = (d[0] > 0) ? -d[2]/ax[0] : d[2]/ax[0];
            out_t = -d[1]/ax[0];
        }
        else if(ax[1] >= ax[2])
        {
            out_face = d[1] > 0 ? 2 : 3;
            out_s = d[0]/ax[1];
            out_t = (d[1] > 0) ? d[2]/ax[1] : -d[2]/ax[1];
        }
        else
        {
            out_face = d[2] > 0 ? 4 : 5;
            out_s = (d[2] > 0) ? d[0]/ax[2] : -d[0]/ax[2];
            out_t = -d[1]/ax[2];
        }
    }

    // 双线性采样:最近邻会让低粗糙度 mip 出现明显块状,且面内插值才与
    // 面与面之间的方向采样一致。跨面接缝不处理(需要立方体邻接表),
    // 面内按 clamp 边界处理即可。
    static void SampleCubeRGB(const std::vector<float> faces[6], uint32_t w, uint32_t h,
                              const float dir[3], float out_rgb[3])
    {
        int f; float s, t;
        DirToFace(dir, f, s, t);

        // s,t ∈ [-1,1] → 纹素坐标(纹素中心在 .5 处)
        const float u = (s + 1.0f) * 0.5f * float(w) - 0.5f;
        const float v = (t + 1.0f) * 0.5f * float(h) - 0.5f;

        const float fu = std::floor(u), fv = std::floor(v);
        const float tu = u - fu, tv = v - fv;

        const int max_x = int(w) - 1, max_y = int(h) - 1;
        const int x0 = (std::clamp)(int(fu),     0, max_x);
        const int y0 = (std::clamp)(int(fv),     0, max_y);
        const int x1 = (std::clamp)(int(fu) + 1, 0, max_x);
        const int y1 = (std::clamp)(int(fv) + 1, 0, max_y);

        const std::vector<float> &face = faces[f];
        const float *c00 = &face[(size_t(y0)*w + size_t(x0))*4];
        const float *c10 = &face[(size_t(y0)*w + size_t(x1))*4];
        const float *c01 = &face[(size_t(y1)*w + size_t(x0))*4];
        const float *c11 = &face[(size_t(y1)*w + size_t(x1))*4];

        for(int i = 0; i < 3; i++)
        {
            const float a = c00[i] + (c10[i] - c00[i]) * tu;
            const float b = c01[i] + (c11[i] - c01[i]) * tu;
            out_rgb[i] = a + (b - a) * tv;
        }
    }

    // Van der Corput 基数反演(Hammersley 序列的第二维)
    static float RadicalInverseVdC(uint32_t bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return float(bits) * 2.3283064365386963e-10f;
    }

    // 逐像素确定性的整数散列,用于 Cranley–Patterson 旋转
    // (旧实现用 sin(s*12.9898)*43758.5453 取 fmod:同一 s 在任意像素给出同一个
    //  值,导致整张图共用一条射线序列,出现条带状结构;而且 fmod 会返回负值)
    static uint32_t PixelSeed(uint32_t face, uint32_t px, uint32_t py)
    {
        uint32_t h = face * 0x9E3779B1u;
        h ^= px * 0x85EBCA6Bu;
        h = (h << 13) | (h >> 19);
        h ^= py * 0xC2B2AE35u;
        h ^= h >> 15;
        return h;
    }

    struct Sample2D { float u1, u2; };

    static Sample2D HammersleyRotated(int index, int count, uint32_t seed)
    {
        const float r1 = float(seed & 0xFFFFu)       * (1.0f / 65536.0f);
        const float r2 = float((seed >> 16) & 0xFFFF) * (1.0f / 65536.0f);

        Sample2D r;
        r.u1 = (float(index) + 0.5f) / float(count);
        r.u2 = RadicalInverseVdC(uint32_t(index));

        // Cranley–Patterson 旋转后取小数部分
        r.u1 = r.u1 + r1; r.u1 -= std::floor(r.u1);
        r.u2 = r.u2 + r2; r.u2 -= std::floor(r.u2);
        return r;
    }

    static void BuildBasis(const float n[3], float t[3], float b[3])
    {
        // Duff 等人的无分支正交基。最后一维必须是 -n[1] 而非 -sg*n[1]:
        // 多带的 sg 会让 n[2] < 0 的那半球基向量不正交,采样半球被剪切。
        float sg = n[2] >= 0 ? 1.0f : -1.0f;
        float a  = -1.0f / (n[2] + sg);
        t[0] = 1 + sg*n[0]*n[0]*a;  t[1] = sg*n[0]*n[1]*a;  t[2] = -sg*n[0];
        b[0] = n[0]*n[1]*a;         b[1] = sg + n[1]*n[1]*a; b[2] = -n[1];
    }

    static void GGXSampleDir(float roughness, float u1, float u2,
                             const float rd[3], const float t[3], const float b[3],
                             float out_dir[3])
    {
        const float a = roughness * roughness;
        const float phi = 2 * PI * u2;
        const float ct = std::sqrt((1 - u1) / (1 + (a*a - 1) * u1));
        const float st = std::sqrt(1 - ct * ct);
        for(int i = 0; i < 3; i++)
            out_dir[i] = st * std::cos(phi) * t[i] + st * std::sin(phi) * b[i] + ct * rd[i];
    }

    bool BakeDiffuseIrradiance(const std::vector<float> src[6], uint32_t src_w, uint32_t src_h,
                               std::vector<float> dst[6], uint32_t dst_w, uint32_t dst_h,
                               int sample_count)
    {
        if(!src_w || !src_h || sample_count <= 0) return false;

        for(int face = 0; face < 6; face++)
        {
            for(uint32_t py = 0; py < dst_h; py++)
            for(uint32_t px = 0; px < dst_w; px++)
            {
                float n[3];
                CubeFaceToDir(face, (float(px)+.5f)/dst_w*2-1, (float(py)+.5f)/dst_h*2-1, n);
                float nl = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
                for(int i=0;i<3;i++) n[i]/=nl;

                // 切线基必须建在逐像素法线上:用面主轴建基会让面内偏心像素的
                // 采样半球被剪切(基向量不正交于 n),方向分布偏离余弦加权。
                float t[3], b[3];
                BuildBasis(n, t, b);

                float ir[3] = {};
                float wd[3];
                const uint32_t seed = PixelSeed(uint32_t(face), px, py);

                for(int s = 0; s < sample_count; s++)
                {
                    const Sample2D xi = HammersleyRotated(s, sample_count, seed);
                    const float u1 = xi.u1, u2 = xi.u2;
                    // 余弦加权半球采样:pdf = cosθ/π,
                    // 故 E[L·cosθ/pdf]/π 恰好等于平均辐射度(照明项已含 cosθ),
                    // 累加后取 1/N 即为漫反射 IBL 需要的 irradiance/π。
                    const float phi = 2*PI*u2;
                    const float st = std::sqrt(u1), ct = std::sqrt(1-u1);
                    float d[3] = { st*std::cos(phi), st*std::sin(phi), ct };

                    for(int i = 0; i < 3; i++)
                        wd[i] = d[0]*t[i] + d[1]*b[i] + d[2]*n[i];

                    const float wl = std::sqrt(wd[0]*wd[0]+wd[1]*wd[1]+wd[2]*wd[2]);
                    for(int i = 0; i < 3; i++) wd[i] /= wl;

                    float c[3];
                    SampleCubeRGB(src, src_w, src_h, wd, c);
                    ir[0]+=c[0]; ir[1]+=c[1]; ir[2]+=c[2];
                }
                float inv = 1.0f/sample_count;
                float *dp = &dst[face][(size_t(py)*dst_w+px)*4];
                dp[0]=ir[0]*inv; dp[1]=ir[1]*inv; dp[2]=ir[2]*inv; dp[3]=1;
            }
        }
        return true;
    }

    bool BakeGGXPrefilter(const std::vector<float> src[6], uint32_t src_w, uint32_t src_h,
                          std::vector<float> dst[6], uint32_t dst_w, uint32_t dst_h,
                          float roughness, int sample_count)
    {
        if(!src_w || !src_h || roughness < 0 || sample_count <= 0) return false;

        for(int face = 0; face < 6; face++)
        {
            for(uint32_t py = 0; py < dst_h; py++)
            for(uint32_t px = 0; px < dst_w; px++)
            {
                float rd[3];
                CubeFaceToDir(face, (float(px)+.5f)/dst_w*2-1, (float(py)+.5f)/dst_h*2-1, rd);
                float rl = std::sqrt(rd[0]*rd[0]+rd[1]*rd[1]+rd[2]*rd[2]);
                for(int i=0;i<3;i++) rd[i]/=rl;

                float t[3], b[3];
                BuildBasis(rd, t, b);

                float pre[3] = {};
                float wsum = 0.0f;
                const uint32_t seed = PixelSeed(uint32_t(face), px, py);

                for(int s = 0; s < sample_count; s++)
                {
                    const Sample2D xi = HammersleyRotated(s, sample_count, seed);
                    const float u1 = xi.u1, u2 = xi.u2;

                    // 按 GGX 法线分布采样半向量 H,再以 V = N = R 反射得到光线方向。
                    // 旧实现直接把 H 当作光线方向使用,高光收敛位置整体偏移。
                    float h[3];
                    GGXSampleDir(roughness, u1, u2, rd, t, b, h);

                    float ndoth = 0.0f;
                    for(int i = 0; i < 3; i++)
                        ndoth += h[i] * rd[i];

                    float ld[3];
                    for(int i = 0; i < 3; i++)
                        ld[i] = 2.0f * ndoth * h[i] - rd[i];

                    float ndotl = 0.0f;
                    for(int i = 0; i < 3; i++)
                        ndotl += ld[i] * rd[i];

                    if(ndotl <= 0.0f)
                        continue;

                    float c[3];
                    SampleCubeRGB(src, src_w, src_h, ld, c);
                    pre[0]+=c[0]*ndotl; pre[1]+=c[1]*ndotl; pre[2]+=c[2]*ndotl;
                    wsum += ndotl;
                }

                const float inv = (wsum > 1e-6f) ? 1.0f/wsum : 0.0f;
                float *dp = &dst[face][(size_t(py)*dst_w+px)*4];
                dp[0]=pre[0]*inv; dp[1]=pre[1]*inv; dp[2]=pre[2]*inv; dp[3]=1;
            }
        }
        return true;
    }
}//namespace texcore
