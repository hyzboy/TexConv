#pragma once

#include <Magick++.h>
#include <hgl/log/Log.h>
#include <hgl/type/String.h>

using namespace hgl;

// Define type constants compatible with DevIL
#define IL_UNSIGNED_BYTE    0x1401
#define IL_UNSIGNED_SHORT   0x1403
#define IL_FLOAT            0x1406
#define IL_HALF             0x140B

// Define format constants compatible with DevIL
#define IL_RGB              0x1907
#define IL_RGBA             0x1908
#define IL_LUMINANCE        0x1909
#define IL_LUMINANCE_ALPHA  0x190A
#define IL_ALPHA            0x1906

// Type alias for compatibility
typedef unsigned int ILuint;

class MagickImage
{
    OBJECT_LOGGER

private:
    Magick::Image m_image;
    uint m_width;
    uint m_height;
    uint m_depth;
    uint m_channels;
    ILuint m_format;
    ILuint m_type;

    void Refresh();
    bool Convert(ILuint format, ILuint type);
    void *GetData(ILuint format, ILuint type);

public:
    const ILuint width()  const { return m_width; }
    const ILuint height() const { return m_height; }
    const ILuint depth()  const { return m_depth; }
    const ILuint bit()    const;
    const ILuint format() const { return m_format; }
    const ILuint type()   const { return m_type; }

    const ILuint pixel_total() const { return width() * height() * depth(); }

public:
    const uint channels() const { return m_channels; }

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
    void *ToRGB(ILuint type = IL_UNSIGNED_BYTE);
    void *ToGray(ILuint type = IL_UNSIGNED_BYTE);
    
    void *GetR(ILuint type);
    void *GetRG(ILuint type) { return GetData(IL_LUMINANCE_ALPHA, type); }
    void *GetRGB(ILuint type) { return GetData(IL_RGB, type); }
    void *GetRGBA(ILuint type);
    void *GetLum(ILuint type) { return GetData(IL_LUMINANCE, type); }
    void *GetAlpha(ILuint type);

    bool ConvertToR(ILuint type) { return (m_format == IL_LUMINANCE ? Convert(IL_LUMINANCE, type) : Convert(IL_ALPHA, type)); }
    bool ConvertToRG(ILuint type) { return Convert(IL_LUMINANCE_ALPHA, type); }
    bool ConvertToRGB(ILuint type) { return Convert(IL_RGB, type); }
    bool ConvertToRGBA(ILuint type) { return Convert(IL_RGBA, type); }
    bool ConvertToLum(ILuint type) { return Convert(IL_LUMINANCE, type); }
};

bool SaveImageToFile(const OSString &filename, ILuint w, ILuint h, const float scale, ILuint c, ILuint t, void *data);

inline bool SaveImageToFile(const OSString &filename, ILuint w, ILuint h, ILuint c, ILuint t, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0, c, t, data);
}

inline bool SaveImageToFile(const OSString &filename, ILuint w, ILuint h, ILuint c, void *data)
{
    return SaveImageToFile(filename, w, h, 1.0, c, IL_UNSIGNED_BYTE, data);
}
