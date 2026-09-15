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

    static void SampleCubeRGB(const std::vector<float> faces[6], uint32_t w, uint32_t h,
                              const float dir[3], float out_rgb[3])
    {
        int f; float s, t;
        DirToFace(dir, f, s, t);
        uint32_t px = (std::min)(uint32_t((s+1)*0.5f*w), w-1);
        uint32_t py = (std::min)(uint32_t((t+1)*0.5f*h), h-1);
        const float *p = &faces[f][(size_t(py)*w+px)*4];
        out_rgb[0]=p[0]; out_rgb[1]=p[1]; out_rgb[2]=p[2];
    }

    static void BuildBasis(const float n[3], float t[3], float b[3])
    {
        float sg = n[2] >= 0 ? 1.0f : -1.0f;
        float a  = -1.0f / (n[2] + sg);
        t[0] = 1 + sg*n[0]*n[0]*a;  t[1] = sg*n[0]*n[1]*a;  t[2] = -sg*n[0];
        b[0] = n[0]*n[1]*a;         b[1] = sg + n[1]*n[1]*a; b[2] = -sg*n[1];
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
            float t[3], b[3];
            BuildBasis(CUBE_FACE_AXES[face][0], t, b);

            for(uint32_t py = 0; py < dst_h; py++)
            for(uint32_t px = 0; px < dst_w; px++)
            {
                float n[3];
                CubeFaceToDir(face, (float(px)+.5f)/dst_w*2-1, (float(py)+.5f)/dst_h*2-1, n);
                float nl = std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
                for(int i=0;i<3;i++) n[i]/=nl;

                float ir[3] = {};
                float wd[3];

                for(int s = 0; s < sample_count; s++)
                {
                    float u1 = (float(s)+.5f)/sample_count;
                    float u2 = std::fmod(std::sin(s*12.9898f)*43758.5453f, 1.0f);
                    float phi = 2*PI*u2;
                    float st = std::sqrt(u1), ct = std::sqrt(1-u1);
                    float d[3] = { st*std::cos(phi), st*std::sin(phi), ct };

                    for(int i = 0; i < 3; i++)
                        wd[i] = d[0]*t[i] + d[1]*b[i] + d[2]*n[i];

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
        if(!src_w || !src_h || roughness <= 0 || sample_count <= 0) return false;

        for(int face = 0; face < 6; face++)
        {
            float t[3], b[3];
            BuildBasis(CUBE_FACE_AXES[face][0], t, b);

            for(uint32_t py = 0; py < dst_h; py++)
            for(uint32_t px = 0; px < dst_w; px++)
            {
                float rd[3];
                CubeFaceToDir(face, (float(px)+.5f)/dst_w*2-1, (float(py)+.5f)/dst_h*2-1, rd);
                float rl = std::sqrt(rd[0]*rd[0]+rd[1]*rd[1]+rd[2]*rd[2]);
                for(int i=0;i<3;i++) rd[i]/=rl;

                float pre[3] = {};
                for(int s = 0; s < sample_count; s++)
                {
                    float u1 = (float(s)+.5f)/sample_count;
                    float u2 = std::fmod(std::sin(s*12.9898f+roughness*78.233f)*43758.5453f, 1.0f);
                    float a = roughness*roughness;
                    float phi = 2*PI*u2;
                    float ct = std::sqrt((1-u1)/(1+(a*a-1)*u1));
                    float st = std::sqrt(1-ct*ct);

                    float ld[3];
                    GGXSampleDir(roughness, u1, u2, rd, t, b, ld);

                    float c[3];
                    SampleCubeRGB(src, src_w, src_h, ld, c);
                    pre[0]+=c[0]; pre[1]+=c[1]; pre[2]+=c[2];
                }
                float inv = 1.0f/sample_count;
                float *dp = &dst[face][(size_t(py)*dst_w+px)*4];
                dp[0]=pre[0]*inv; dp[1]=pre[1]*inv; dp[2]=pre[2]*inv; dp[3]=1;
            }
        }
        return true;
    }
}//namespace texcore
