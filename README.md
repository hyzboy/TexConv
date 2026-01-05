# TexConv - 纹理转换工具

TexConv 是一个高性能的图像和纹理转换工具，支持将各种图像格式转换为优化的纹理文件格式。

## 功能特性

- 支持多种图像格式输入（PNG, JPG, TGA, HDR, EXR 等）
- 支持多种纹理压缩格式（BC1-BC7, ASTC, ETC, 等）
- 支持 Mipmap 生成
- 支持 Cubemap 转换
- 支持距离场生成
- 支持 HDR/EXR 到 PNG 转换

## 图像处理库支持

本项目支持两种图像处理库：

### ImageMagick (推荐)

ImageMagick 是一个活跃维护的图像处理库，提供：
- ✅ 更活跃的库维护
- ✅ 更好的 HDR/EXR 支持
- ✅ 更先进的图像处理算法
- ✅ 更多的图像格式支持
- ✅ 高质量的 Lanczos 缩放算法

### DevIL (遗留支持)

DevIL 1.8.0 是传统的图像处理库，保留用于向后兼容。

## 构建说明

### 使用 ImageMagick（推荐）

```bash
# 1. 安装 ImageMagick 开发库

# Windows:
# 下载并安装 ImageMagick from https://imagemagick.org/script/download.php
# 确保选择安装开发头文件和库（Magick++）

# Ubuntu/Debian:
sudo apt-get install libmagick++-dev

# CentOS/RHEL:
sudo yum install ImageMagick-c++-devel

# macOS:
brew install imagemagick

# 2. 构建项目
mkdir build
cd build
cmake -DUSE_IMAGEMAGICK=ON ..
cmake --build .
```

### 使用 DevIL（遗留）

```bash
mkdir build
cd build
cmake -DUSE_IMAGEMAGICK=OFF ..
cmake --build .
```

## 使用示例

### TexConv - 纹理转换

```bash
# 转换单个图像文件
TexConv image.png

# 转换目录下的所有图像（包含子目录）
TexConv /s image_directory

# 生成 mipmaps
TexConv /mip image.png

# 指定输出文件名
TexConv /out:output_name image.png

# 使用 AMD Compressonator
TexConv /AMD image.png

# 使用 Intel ISPC
TexConv /Intel image.png

# 指定格式
TexConv /RGB:BC1 /RGBA:BC3 image.png
```

### HDR2PNG - HDR/EXR 转 PNG

```bash
# 将 HDR 或 EXR 文件转换为 PNG
HDR2PNG input.hdr

# 输出: input.png
```

### ComboTexture - 组合纹理

```bash
# 材质贴图打包（颜色+法线+金属度+粗糙度）
ComboTexture output MPBR Color.png Normal.png Metallic.png Roughness.png

# 多通道 RGB 打包
ComboTexture output 4RGB Color1.png Color2.png Color3.png Color4.png

# 双通道颜色+法线打包
ComboTexture output 2CN Color1.png Normal1.png Color2.png Normal2.png
```

### CubeMapConv - Cubemap 转换

```bash
# 将 6 个面转换为 cubemap
CubeMapConv output.tex PosX.png NegX.png PosY.png NegY.png PosZ.png NegZ.png
```

### DFGen - 距离场生成

```bash
# 从灰度图生成距离场
DFGen input.png

# 从 Alpha 通道生成距离场
DFGen input.png alpha
```

## CMake 选项

- `USE_IMAGEMAGICK` - 使用 ImageMagick 而不是 DevIL（默认：ON）
- `CMAKE_BUILD_TYPE` - 构建类型（Debug/Release）
- `HGL_BITS` - 目标架构位数（32/64）

## 依赖项

### 必需依赖

- CMake 3.28+
- C++17 或更高版本编译器
- CMCore, CMPlatform, CMUtil 库
- 图像库（ImageMagick 或 DevIL）

### 可选依赖

- AMD Compressonator（用于 AMD 压缩）
- Intel ISPC Texture Compressor（用于 Intel 压缩）
- Qt5/Qt6（用于 GUI 工具）
- GLM（向量数学库）

## Windows 特定说明

### ImageMagick 安装

1. 从 [ImageMagick 官网](https://imagemagick.org/script/download.php) 下载 Windows 安装程序
2. 在安装过程中，确保选择：
   - "Install development headers and libraries for C and C++"
   - "Add application directory to your system path"
3. 重启命令提示符或 PowerShell 以使环境变量生效

### DevIL SDK

DevIL Windows SDK 应放置在项目根目录的 `DevIL Windows SDK` 文件夹中。

## Linux/macOS 说明

确保已安装 ImageMagick 开发包：

```bash
# 验证安装
Magick++-config --version
Magick++-config --cxxflags
Magick++-config --libs
```

## 技术细节

### 图像缩放算法

使用高质量的 Lanczos3 滤波器进行图像缩放，提供优于双线性和双三次插值的结果。

### 支持的图像格式

#### 输入格式
- 标准格式：PNG, JPEG, BMP, TGA, GIF
- 高动态范围：HDR (Radiance), EXR (OpenEXR)
- 其他：TIFF, PSD, WebP, 等

#### 输出格式
- 未压缩：R8/R16/R32, RG8/RG16/RG32, RGB8/RGB16/RGB32, RGBA8/RGBA16/RGBA32
- 压缩格式：BC1-BC7, ASTC, ETC, 等

### 像素数据类型支持

- `IMAGE_UNSIGNED_BYTE` - 8位无符号整数
- `IMAGE_UNSIGNED_SHORT` - 16位无符号整数
- `IMAGE_FLOAT` - 32位浮点数
- `IMAGE_HALF` - 16位半精度浮点数

## 故障排除

### ImageMagick 未找到

```
CMake Error: ImageMagick not found
```

**解决方案：**
- Windows: 确保安装了 ImageMagick 并包含开发库
- Linux: 安装 `libmagick++-dev` 或 `ImageMagick-c++-devel`
- macOS: 使用 `brew install imagemagick`

### 链接错误

如果遇到链接错误，尝试：
1. 清理构建目录：`rm -rf build && mkdir build`
2. 确保所有依赖库都已正确安装
3. 检查 CMake 配置输出中的库路径

### 向后兼容

如果需要使用 DevIL：
```bash
cmake -DUSE_IMAGEMAGICK=OFF ..
```

## 许可证

请查看项目许可证文件。

## 贡献

欢迎提交问题报告和拉取请求。

## 联系方式

- GitHub: https://github.com/hyzboy/TexConv
