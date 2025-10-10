#include "TextureFileCreater.h"
#include "ILImage.h"
#include <hgl/log/log.h>
#include <hgl/TypeFunc.h>
#include <cstring>
#include <cstdlib>
#include "basis_universal/encoder/basisu_enc.h"
#include "basis_universal/encoder/basisu_comp.h"

using namespace basisu;

class TextureFileCreaterBasisU : public TextureFileCreater
{
    int channels;
    int type;
    uint pixel_bytes;
    std::string target_fmt_name;
    bool use_uastc;  // true for UASTC, false for ETC1S

public:
    using TextureFileCreater::TextureFileCreater;

public:
    bool InitFormat(ILImage* img) override
    {
        image = img;

        channels = image->channels();
        type = image->type();

        // Determine format based on pixel_format name
        if (strcmp(pixel_format->name, "BU_ETC1S") == 0)
        {
            target_fmt_name = "BASIS_ETC1S (BC1-5/6H/7 compatible)";
            use_uastc = false;
        }
        else if (strcmp(pixel_format->name, "BU_ASTC") == 0)
        {
            target_fmt_name = "BASIS_UASTC (ASTC/BC6H/BC7 compatible)";
            use_uastc = true;
        }
        else
        {
            // Default to ETC1S for backward compatibility
            target_fmt_name = "BASIS_ETC1S (BC1-5/6H/7 compatible)";
            use_uastc = false;
        }

        // Build log message
        AnsiString msg = "Compress Image to BasisU: ";
        msg += AnsiString::numberOf(image->width());
        msg += "x";
        msg += AnsiString::numberOf(image->height());
        msg += " ";
        msg += target_fmt_name.c_str();
        msg += " format.";

        LogInfo(msg.c_str());

        if (type == IL_UNSIGNED_BYTE) { pixel_bytes = 1; }
        else if (type == IL_UNSIGNED_SHORT) { pixel_bytes = 2; }
        else if (type == IL_UNSIGNED_INT) { pixel_bytes = 4; }
        else if (type == IL_HALF) { pixel_bytes = 2; }
        else if (type == IL_FLOAT) { pixel_bytes = 4; }
        else
        {
            LogError(OS_TEXT("Unsupported type: %d"), type);
            return false;
        }

        // Convert image to RGBA for basisu (it expects RGBA input)
        if (channels == 1)
        {
            image->ConvertToLum(type);
        }
        else if (channels == 2)
        {
            image->ConvertToRG(type);
        }
        else if (channels == 3 || channels == 4)
        {
            image->ConvertToRGBA(type);
        }
        else
        {
            LogError(OS_TEXT("Unsupported channel count: %d"), channels);
            return false;
        }

        return true;
    }

    uint32 Write() override
    {
        const int width = image->width();
        const int height = image->height();

        // Get source data (always RGBA after conversion)
        void* source_data = image->GetRGBA(type);
        if (!source_data)
        {
            LogError(OS_TEXT("Failed to get image data for BasisU compression"));

            return 0;
        }

        // BasisU expects RGBA data
        const uint8_t* rgba_data = reinterpret_cast<const uint8_t*>(source_data);

        // Initialize basisu encoder
        basis_compressor_params params;
        params.m_source_images.push_back(basisu::image(rgba_data, width, height, 4));  // 4 channels

        if (use_uastc)
        {
            params.m_uastc = true;
        }
        else
        {
            params.m_uastc = false;
            params.m_etc1s_quality_level = BASISU_QUALITY_MAX;
        }
        
        params.m_compression_level = BASISU_DEFAULT_COMPRESSION_LEVEL;
        params.m_perceptual = true;  // Use perceptual color space
        params.m_mip_gen = false;    // We'll handle mipmaps ourselves

        // Create compressor and compress
        job_pool jpool(1);  // Single threaded for simplicity
        basis_compressor compressor;
        if (!compressor.init(params))
        {
            LogError(OS_TEXT("Failed to initialize BasisU compressor"));

            return 0;
        }

        int ec = compressor.process();
        if (ec != 0)
        {
            LogError(OS_TEXT("BasisU compression failed with error code: %d"), ec);

            return 0;
        }

        // Get compressed data
        const uint8_vec& compressed_data = compressor.get_output_basis_file();
        if (compressed_data.empty())
        {
            LogError(OS_TEXT("BasisU compression produced no output"));

            return 0;
        }

        // Build log message
        AnsiString msg = "Compressed Image To BasisU: ";
        msg += AnsiString::numberOf(width);
        msg += "x";
        msg += AnsiString::numberOf(height);
        msg += " ";
        msg += AnsiString::numberOf((uint)compressed_data.size());
        msg += " bytes.";

        LogInfo(msg.c_str());

        // Write the compressed data
        uint32 result = TextureFileCreater::Write(const_cast<void*>(static_cast<const void*>(compressed_data.data())), (uint)compressed_data.size());

        return result;
    }
}; // class TextureFileCreaterBasisU

TextureFileCreater* CreateTextureFileCreaterBasisU(const PixelFormat* pf)
{
    return new TextureFileCreaterBasisU(pf);
}