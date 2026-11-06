#pragma once

#include <Magick++.h>

#include <stdint.h>
#include <string>

enum class ImagePixelType : uint8_t
{
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float16,
    Float32,
    Float64,
};

enum class ImageChannelLayout : uint8_t
{
    Alpha,
    Gray,
    GrayAlpha,
    RGB,
    RGBA,
};

class MagickImage
{
private:
    Magick::Image m_image;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depth;
    uint32_t m_channels;
    ImageChannelLayout m_layout;
    ImagePixelType m_pixel_type;

    void Refresh();
    bool Convert(ImageChannelLayout layout, ImagePixelType pixel_type);
    void *GetData(ImageChannelLayout layout, ImagePixelType pixel_type);

public:
    const uint32_t width()  const { return m_width; }
    const uint32_t height() const { return m_height; }
    const uint32_t depth()  const { return m_depth; }
    const uint32_t bit()    const;
    const ImageChannelLayout layout() const { return m_layout; }
    const ImagePixelType pixelType()   const { return m_pixel_type; }

    const uint32_t pixel_total() const { return width() * height() * depth(); }

public:

    const uint32_t channels() const { return m_channels; }

    bool isGray() const { return m_layout==ImageChannelLayout::Gray; }
    bool isGrayAlpha() const { return m_layout==ImageChannelLayout::GrayAlpha; }
    bool isRGBA() const { return m_layout == ImageChannelLayout::RGBA; }
    bool hasAlpha() const { return m_image.alpha(); }

public:
    MagickImage();
    MagickImage(MagickImage *);
    ~MagickImage();

    bool LoadFile(const std::wstring &);

    /// 由原始像素数据创建图像(数据布局与 Get* 输出一致,首行为顶部)
    bool CreateFromData(uint32_t w, uint32_t h, uint32_t channels, ImagePixelType pixel_type, const void *data);

    void Bind();
    void Copy(MagickImage *);

    bool Resize(uint32_t, uint32_t);

    // Pixel data accessors
    // Note: All Get* and To* methods allocate memory with 'new[]' that must be freed by caller with 'delete[]'
    void *ToRGB(ImagePixelType pixel_type = ImagePixelType::UInt8);
    void *ToGray(ImagePixelType pixel_type = ImagePixelType::UInt8);

    void *GetR(ImagePixelType pixel_type);
    void *GetRG(ImagePixelType pixel_type) { return GetData(ImageChannelLayout::GrayAlpha, pixel_type); }
    void *GetRGB(ImagePixelType pixel_type) { return GetData(ImageChannelLayout::RGB, pixel_type); }
    void *GetRGBA(ImagePixelType pixel_type);
    void *GetGray(ImagePixelType pixel_type) { return GetData(ImageChannelLayout::Gray, pixel_type); }
    void *GetAlpha(ImagePixelType pixel_type);

    bool ConvertToR(ImagePixelType pixel_type) { return (m_layout == ImageChannelLayout::Gray ? Convert(ImageChannelLayout::Gray, pixel_type) : Convert(ImageChannelLayout::Alpha, pixel_type)); }
    bool ConvertToRG(ImagePixelType pixel_type) { return Convert(ImageChannelLayout::GrayAlpha, pixel_type); }
    bool ConvertToRGB(ImagePixelType pixel_type) { return Convert(ImageChannelLayout::RGB, pixel_type); }
    bool ConvertToRGBA(ImagePixelType pixel_type) { return Convert(ImageChannelLayout::RGBA, pixel_type); }
    bool ConvertToGray(ImagePixelType pixel_type) { return Convert(ImageChannelLayout::Gray, pixel_type); }
};

bool SaveImageToFile(const std::wstring &filename, uint32_t w, uint32_t h, const float scale, uint32_t c, ImagePixelType pixel_type, void *data);

inline bool SaveImageToFile(const std::wstring &filename, uint32_t w, uint32_t h, uint32_t c, ImagePixelType pixel_type, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0f, c, pixel_type, data);
}

inline bool SaveImageToFile(const std::wstring &filename, uint32_t w, uint32_t h, uint32_t c, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0f, c, ImagePixelType::UInt8, data);
}
