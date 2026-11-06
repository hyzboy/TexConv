#include "MagickImage.h"
#include <windows.h>
#include "ImageLog.h"
#include <cstring>

using namespace teximage;

namespace
{
    bool FileExist(const std::wstring &filename)
    {
        return GetFileAttributesW(filename.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    Magick::StorageType GetMagickStorageType(ImagePixelType pixel_type)
    {
        switch(pixel_type)
        {
            case ImagePixelType::Int8:
            case ImagePixelType::UInt8:   return Magick::CharPixel;
            case ImagePixelType::Int16:
            case ImagePixelType::UInt16:
            case ImagePixelType::Float16: return Magick::ShortPixel;
            case ImagePixelType::Int32:
            case ImagePixelType::UInt32:  return Magick::LongPixel;
            case ImagePixelType::Float32: return Magick::FloatPixel;
            case ImagePixelType::Float64: return Magick::DoublePixel;
            default:                return Magick::CharPixel;
        }
    }

    const char* GetChannelMap(ImageChannelLayout layout)
    {
        switch(layout)
        {
            case ImageChannelLayout::RGB:       return "RGB";
            case ImageChannelLayout::RGBA:      return "RGBA";
            case ImageChannelLayout::Gray:      return "I";
            case ImageChannelLayout::GrayAlpha: return "IA";
            case ImageChannelLayout::Alpha:     return "A";
            default:                return "RGB";
        }
    }

    uint32_t GetChannelCount(ImageChannelLayout layout)
    {
        switch(layout)
        {
            case ImageChannelLayout::RGB:       return 3;
            case ImageChannelLayout::RGBA:      return 4;
            case ImageChannelLayout::Gray:      return 1;
            case ImageChannelLayout::Alpha:     return 1;
            case ImageChannelLayout::GrayAlpha: return 2;
            default:                return 3;
        }
    }

    size_t GetBytesPerComponent(ImagePixelType pixel_type)
    {
        switch(pixel_type)
        {
            case ImagePixelType::Int8:
            case ImagePixelType::UInt8:   return 1;
            case ImagePixelType::Int16:
            case ImagePixelType::UInt16:
            case ImagePixelType::Float16: return 2;
            case ImagePixelType::Int32:
            case ImagePixelType::UInt32:
            case ImagePixelType::Float32: return 4;
            case ImagePixelType::Float64: return 8;
            default:                return 1;
        }
    }

    // 宽字符 → UTF-8(ImageMagick 只接受 UTF-8 路径)
    std::string ToUTF8String(const std::wstring &str)
    {
        if(str.empty())
            return std::string();

        const int size_needed = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(),
                                                   nullptr, 0, nullptr, nullptr);

        if(size_needed <= 0)
            return std::string();

        std::string result((size_t)size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(),
                            &result[0], size_needed, nullptr, nullptr);
        return result;
    }

    // UTF-8 → 宽字符(日志用)
    std::wstring ToWide(const std::string &str)
    {
        if(str.empty())
            return std::wstring();

        const int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(),
                                                   nullptr, 0);

        if(size_needed <= 0)
            return std::wstring();

        std::wstring result((size_t)size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(),
                            &result[0], size_needed);
        return result;
    }

}

MagickImage::MagickImage()
    : m_width(0)
    , m_height(0)
    , m_depth(1)
    , m_channels(0)
    , m_layout(ImageChannelLayout::RGB)
    , m_pixel_type(ImagePixelType::UInt8)
{
}

MagickImage::MagickImage(MagickImage *img)
    : MagickImage()
{
    m_image = img->m_image;
    Refresh();
}

MagickImage::~MagickImage()
{
}

void MagickImage::Copy(MagickImage *img)
{
    m_image = img->m_image;
    Refresh();
}

void MagickImage::Refresh()
{
    try
    {
        m_width = static_cast<uint32_t>(m_image.columns());
        m_height = static_cast<uint32_t>(m_image.rows());
        m_depth = 1;

        // Determine format based on image type
        Magick::ImageType imgType = m_image.type();

        if(imgType == Magick::GrayscaleType || imgType == Magick::GrayscaleAlphaType
         ||imgType == Magick::BilevelType)
        {
            if(m_image.alpha())
            {
                m_layout = ImageChannelLayout::GrayAlpha;
                m_channels = 2;
            }
            else
            {
                m_layout = ImageChannelLayout::Gray;
                m_channels = 1;
            }
        }
        else
        {
            if(m_image.alpha())
            {
                m_layout = ImageChannelLayout::RGBA;
                m_channels = 4;
            }
            else
            {
                m_layout = ImageChannelLayout::RGB;
                m_channels = 3;
            }
        }

        // Determine type based on depth
        size_t depth = m_image.depth();
        if(depth <= 8)
            m_pixel_type = ImagePixelType::UInt8;
        else if(depth <= 16)
            m_pixel_type = ImagePixelType::UInt16;
        else
            m_pixel_type = ImagePixelType::Float32;

        LogInfo("Image info:");
        LogInfo("\t width: " + std::to_string(m_width));
        LogInfo("\t height: " + std::to_string(m_height));
        LogInfo("\t channels: " + std::to_string(m_channels));
    }
    catch(Magick::Exception &error)
    {
        LogError(L"Refresh failed: " + ToWide(error.what()));
    }
}

const uint32_t MagickImage::bit() const
{
    return m_channels * static_cast<uint32_t>(GetBytesPerComponent(m_pixel_type)) * 8;
}

void MagickImage::Bind()
{
    // No-op for ImageMagick, kept for compatibility
}

bool MagickImage::LoadFile(const std::wstring &filename)
{
    try
    {
        if(!FileExist(filename))
        {
            LogError(L"Can't find filename: " + filename);
            return false;
        }

        // Convert to UTF-8 for ImageMagick
        std::string utf8Path = ToUTF8String(filename);
        m_image.read(utf8Path);

        // Ensure consistent orientation (top-left origin)
        if(m_image.orientation() == Magick::BottomLeftOrientation ||
           m_image.orientation() == Magick::BottomRightOrientation)
        {
            m_image.flip();
            LogInfo("Image flipped to upper-left origin.");
        }

        LogInfo(L"\nFile: " + filename);
        Refresh();

        return true;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"Failed to load: " + ToWide(error.what()));
        return false;
    }
}

bool MagickImage::CreateFromData(uint32_t w, uint32_t h, uint32_t channels, ImagePixelType pixel_type, const void *data)
{
    if(w == 0 || h == 0 || channels < 1 || channels > 4 || !data)
        return false;

    try
    {
        constexpr ImageChannelLayout layout_by_channel[] =
        {
            ImageChannelLayout::Gray,
            ImageChannelLayout::GrayAlpha,
            ImageChannelLayout::RGB,
            ImageChannelLayout::RGBA,
        };

        const char *map            = GetChannelMap(layout_by_channel[channels - 1]);
        Magick::StorageType storage = GetMagickStorageType(pixel_type);

        m_image.read(w, h, map, storage, const_cast<void *>(data));

        // 按请求通道数强制图像类型:1 通道数据经 Magick 分类可能变成
        // BilevelType(如 0/255 图案),不做会导致 Refresh 误判为 RGB
        switch(channels)
        {
            case 1: m_image.type(Magick::GrayscaleType);        break;
            case 2: m_image.type(Magick::GrayscaleAlphaType);   break;
            case 3: m_image.type(Magick::TrueColorType);        break;
            case 4: m_image.type(Magick::TrueColorAlphaType);   break;
        }

        Refresh();
        return true;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"CreateFromData failed: " + ToWide(error.what()));
        return false;
    }
}

bool MagickImage::Resize(uint32_t nw, uint32_t nh)
{
    if(nw == m_width && nh == m_height) return true;
    if(nw == 0 || nh == 0) return false;

    try
    {
        // Use Lanczos filter for high-quality resizing
        m_image.filterType(Magick::LanczosFilter);
        m_image.resize(Magick::Geometry(nw, nh));

        m_width = nw;
        m_height = nh;

        LogInfo("Resized to: " + std::to_string(nw) + "x" + std::to_string(nh));
        return true;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"Resize failed: " + ToWide(error.what()));
        return false;
    }
}

bool MagickImage::Convert(ImageChannelLayout layout, ImagePixelType pixel_type)
{
    if(m_layout == layout && m_pixel_type == pixel_type) return true;

    try
    {
        switch(layout)
        {
            case ImageChannelLayout::RGB:
                m_image.type(Magick::TrueColorType);
                m_image.alpha(false);
                break;
            case ImageChannelLayout::RGBA:
                m_image.type(Magick::TrueColorAlphaType);
                m_image.alpha(true);
                break;
            case ImageChannelLayout::Gray:
                m_image.type(Magick::GrayscaleType);
                m_image.alpha(false);
                break;
            case ImageChannelLayout::GrayAlpha:
                m_image.type(Magick::GrayscaleAlphaType);
                m_image.alpha(true);
                break;
            case ImageChannelLayout::Alpha:
                m_image.alpha(true);
                break;
        }

        m_layout = layout;
        m_pixel_type = pixel_type;
        m_channels = GetChannelCount(layout);

        return true;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"Convert failed: " + ToWide(error.what()));
        return false;
    }
}

void *MagickImage::GetData(ImageChannelLayout layout, ImagePixelType pixel_type)
{
    if(m_layout != layout || m_pixel_type != pixel_type)
    {
        if(!Convert(layout, pixel_type))
            return nullptr;
    }

    try
    {
        const char* map = GetChannelMap(layout);
        Magick::StorageType storage = GetMagickStorageType(pixel_type);
        uint32_t channels = GetChannelCount(layout);

        size_t size = m_width * m_height * channels * GetBytesPerComponent(pixel_type);
        void *data = new uint8_t[size];

        m_image.write(0, 0, m_width, m_height, map, storage, data);

        return data;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"GetData failed: " + ToWide(error.what()));
        return nullptr;
    }
}

void *MagickImage::ToRGB(ImagePixelType pixel_type)
{
    if(m_layout != ImageChannelLayout::RGB)
        Convert(ImageChannelLayout::RGB, pixel_type);

    return GetData(ImageChannelLayout::RGB, pixel_type);
}

void *MagickImage::ToGray(ImagePixelType pixel_type)
{
    if(m_layout != ImageChannelLayout::Gray)
        Convert(ImageChannelLayout::Gray, pixel_type);

    return GetData(ImageChannelLayout::Gray, pixel_type);
}

void *MagickImage::GetR(ImagePixelType pixel_type)
{
    if(m_layout == ImageChannelLayout::Alpha)
        return GetAlpha(pixel_type);

    if(m_layout == ImageChannelLayout::Gray)
    {
        if(m_pixel_type != pixel_type)
        {
            if(!Convert(m_layout, pixel_type))
                return nullptr;
        }
        return GetData(ImageChannelLayout::Gray, pixel_type);
    }

    return nullptr;
}

void *MagickImage::GetAlpha(ImagePixelType pixel_type)
{
    try
    {
        if(!m_image.alpha())
            return nullptr;

        Magick::StorageType storage = GetMagickStorageType(pixel_type);
        size_t size = m_width * m_height * GetBytesPerComponent(pixel_type);
        void *data = new uint8_t[size];

        m_image.write(0, 0, m_width, m_height, "A", storage, data);

        return data;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"GetAlpha failed: " + ToWide(error.what()));
        return nullptr;
    }
}

template<typename T>
void MixRGBA(T *rgba, T *alpha, int size)
{
    for(int i = 0; i < size; i++)
    {
        rgba += 3;
        *rgba++ = *alpha++;
    }
}

void *MagickImage::GetRGBA(ImagePixelType pixel_type)
{
    void *data = GetData(ImageChannelLayout::RGBA, pixel_type);

    if(!data)
        return nullptr;

    // Alpha is already included in RGBA data from ImageMagick
    return data;
}

constexpr ImageChannelLayout layout_by_channel[] =
{
    ImageChannelLayout::Gray,
    ImageChannelLayout::GrayAlpha,
    ImageChannelLayout::RGB,
    ImageChannelLayout::RGBA,
};

bool SaveImageToFile(const std::wstring &filename, uint32_t w, uint32_t h, const float scale, uint32_t c, ImagePixelType pixel_type, void *data)
{
    if(filename.empty()) return false;
    if(w <= 0 || h <= 0) return false;
    if(c < 1 || c > 4) return false;
    if(!data) return false;

    try
    {
        const char* map = GetChannelMap(layout_by_channel[c - 1]);
        Magick::StorageType storage = GetMagickStorageType(pixel_type);

        Magick::Image image;
        image.read(w, h, map, storage, data);

        // Apply scaling if needed
        if(scale != 1.0f && scale > 0.0f)
        {
            uint32_t new_w = static_cast<uint32_t>(w * scale);
            uint32_t new_h = static_cast<uint32_t>(h * scale);
            image.filterType(Magick::LanczosFilter);
            image.resize(Magick::Geometry(new_w, new_h));
        }

        // Flip for correct orientation
        image.flip();

        // Convert to UTF-8 for ImageMagick
        std::string utf8Path = ToUTF8String(filename);
        image.write(utf8Path);

        return true;
    }
    catch(Magick::Exception &error)
    {
        LogError(L"SaveImageToFile failed: " + ToWide(error.what()));
        return false;
    }
}
