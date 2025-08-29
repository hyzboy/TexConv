# Lanczos3 High-Quality MIPMAP Generation

This implementation replaces DevIL's default scaling algorithm with a high-quality Lanczos3 resampling algorithm for MIPMAP generation.

## Improvements

- **Better Quality**: Lanczos3 provides superior image quality compared to DevIL's default Mitchell filter
- **Multi-format Support**: Supports all pixel formats by converting to float, scaling, then converting back
- **Minimal Changes**: Only adds new functionality without breaking existing code

## Supported Pixel Formats

- `IL_UNSIGNED_BYTE` (8-bit)
- `IL_UNSIGNED_SHORT` (16-bit) 
- `IL_HALF` (16-bit half-precision float)
- `IL_FLOAT` (32-bit float)

## Implementation Details

### Files Added:
- `Lanczos3Scaler.h` - Header for Lanczos3 scaling implementation
- `Lanczos3Scaler.cpp` - Complete Lanczos3 algorithm implementation

### Files Modified:
- `ILImage.h` - Added ResizeLanczos3() method declaration
- `ILImageSupport.cpp` - Implemented ResizeLanczos3() method
- `ConvertImage.cpp` - Changed to use ResizeLanczos3() for MIPMAP generation
- `CMakeLists.txt` - Added new source files to build

## How It Works

1. **Format Conversion**: Source data is converted to normalized float (0.0-1.0 range)
2. **Lanczos3 Scaling**: High-quality resampling using 3-tap Lanczos kernel
3. **Format Restoration**: Scaled data is converted back to original pixel format

## Quality Comparison

- **Before**: DevIL Mitchell filter (basic bilinear-like interpolation)
- **After**: Lanczos3 (sinc-based reconstruction with 3-pixel radius)

The Lanczos3 algorithm preserves fine details and reduces aliasing artifacts during downsampling, resulting in much sharper and cleaner MIPMAPs.

## Usage

The change is transparent - when generating MIPMAPs with `/mip` parameter, the program automatically uses Lanczos3 scaling:

```bash
TexConv /mip /RGBA:BC7 image.png
```

The existing API remains unchanged, ensuring backward compatibility.