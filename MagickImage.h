#pragma once

#include <Magick++.h>
#include <hgl/log/Log.h>
#include <hgl/type/String.h>

using namespace hgl;

enum class ImagePixelType : uint8
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

enum class ImageChannelLayout : uint8
{
    Alpha,
    Gray,
    GrayAlpha,
    RGB,
    RGBA,
};

class MagickImage
{
    OBJECT_LOGGER

private:
    Magick::Image m_image;
    uint m_width;
    uint m_height;
    uint m_depth;
    uint m_channels;
    ImageChannelLayout m_layout;
    ImagePixelType m_pixel_type;

    void Refresh();
    bool Convert(ImageChannelLayout layout, ImagePixelType pixel_type);
    void *GetData(ImageChannelLayout layout, ImagePixelType pixel_type);

public:
    const uint width()  const { return m_width; }
    const uint height() const { return m_height; }
    const uint depth()  const { return m_depth; }
    const uint bit()    const;
    const ImageChannelLayout layout() const { return m_layout; }
    const ImagePixelType pixelType()   const { return m_pixel_type; }

    const uint pixel_total() const { return width() * height() * depth(); }

public:

    const uint channels() const { return m_channels; }

    bool isGray() const { return m_layout==ImageChannelLayout::Gray; }
    bool isGrayAlpha() const { return m_layout==ImageChannelLayout::GrayAlpha; }
    bool isRGBA() const { return m_layout == ImageChannelLayout::RGBA; }
    bool hasAlpha() const { return m_image.alpha(); }

public:
    MagickImage();
    MagickImage(MagickImage *);
    ~MagickImage();

    bool LoadFile(const OSString &);

    void Bind();
    void Copy(MagickImage *);

    bool Resize(uint, uint);

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

bool SaveImageToFile(const OSString &filename, uint w, uint h, const float scale, uint c, ImagePixelType pixel_type, void *data);

inline bool SaveImageToFile(const OSString &filename, uint w, uint h, uint c, ImagePixelType pixel_type, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0f, c, pixel_type, data);
}

inline bool SaveImageToFile(const OSString &filename, uint w, uint h, uint c, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0f, c, ImagePixelType::UInt8, data);
}
