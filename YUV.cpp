#include<hgl/type/DataType.h>
#include<string.h>

using namespace hgl;

namespace hgl
{
	const double clamp_u=0.436*255.0;
	const double clamp_v=0.615*255.0;
		
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
}//namespace hgl

namespace hgl
{
	static bool yuv2rgb=false;
	static int sAdjCrr[256];
	static int sAdjCrg[256];
	static int sAdjCbg[256];
	static int sAdjCbb[256];
	static int sAdjY[256];
	static unsigned char  sClampBuff[1024];
	static unsigned char *sClamp = sClampBuff + 384;

	static unsigned char YV_R[256][256];
	static unsigned char YU_B[256][256];

	inline int getMin(int a, int b)
	{
	   return a>b ? b : a;
	}

	inline int getMax(int a, int b)
	{
	   return a>b ? a : b;
	}

	inline int mClamp(int val, int low, int high)
	{
	   return getMax(getMin(val, high), low);
	}

	void InitYUV2RGBDecode()
	{
		int i;

		for(i = 0; i < 256; i++)
		{
			sAdjCrr[i] = (409 * (i - 128) + 128) >> 8;
			sAdjCrg[i] = (208 * (i - 128) + 128) >> 8;
			sAdjCbg[i] = (100 * (i - 128) + 128) >> 8;
			sAdjCbb[i] = (516 * (i - 128) + 128) >> 8;
			sAdjY[i]   = (298 * (i -  16)      ) >> 8;
		}

		for(i = -384; i < 640; i++)
			sClamp[i] = mClamp(i, 0, 0xFF);

		 for(int v=0;v<256;v++)
			for(int y=0;y<256;y++)
			{
				YV_R[v][y]=sClamp[sAdjY[y]+sAdjCrr[v]];
				YU_B[v][y]=sClamp[sAdjY[y]+sAdjCbb[v]];
			}

		 yuv2rgb=true;
	}
	
	void YUV2RGB(uint8 *rgb,const uint8 *y,const uint8 *u,const uint8 *v,const uint w,const uint h)
	{
		const uint line_gap=w*3;

		const uint y_stride=w;
		const uint uv_stride=w>>1;

		const uint8 *pY0, *pY1, *pU, *pV;

		for(int _y = 0; _y < h; _y += 2)
		{
			pY0 = y +    _y		* y_stride;
			pY1 = y +   (_y | 1)* y_stride;
			pU  = u + (( _y		* uv_stride) >> 1);
			pV  = v + (( _y		* uv_stride) >> 1);

			unsigned __int8 *dst0=rgb;
			unsigned __int8 *dst1=rgb+line_gap;

			for(int _x = 0; _x < w; _x += 2)
			{
				const int G = sAdjCrg[*pV] + sAdjCbg[*pU];

				const uint8 *r =YV_R[*pV++];
				const uint8 *b =YU_B[*pU++];

				// pixel 0x0
				*dst0++ = r[*pY0];
				*dst0++ = sClamp[sAdjY[*pY0] - G];
				*dst0++ = b[*pY0++];

				// pixel 1x0
				*dst0++ = r[*pY0];
				*dst0++ = sClamp[sAdjY[*pY0] - G];
				*dst0++ = b[*pY0++];

				// pixel 0x1
				*dst1++ = r[*pY1];
				*dst1++ = sClamp[sAdjY[*pY1] - G];
				*dst1++ = b[*pY1++];

				// pixel 1x1
				*dst1++ = r[*pY1];
				*dst1++ = sClamp[sAdjY[*pY1] - G];
				*dst1++ = b[*pY1++];
			}

			rgb+=line_gap*2;
		}
	}
}//namespace hgl

