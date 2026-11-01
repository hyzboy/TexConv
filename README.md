# TexConv - 纹理转换工具

TexConv 是一个高性能的图像和纹理转换工具，支持将各种图像格式转换为优化的纹理文件格式。

## 功能特性

- 支持多种图像格式输入（PNG, JPG, TGA, HDR, EXR 等）
- 支持 BC1–BC7 块压缩（含 BC6H / BC6H_SF），以及 R8–RGBA32F 等未压缩格式
- 支持 Mipmap 生成
- 支持 Cubemap 转换、Octahedral Mapping 与 Lambert 等面积天空半球输出
- 支持根据 2D 颜色纹理亮度生成切线空间法线贴图(BC5)
- 支持根据单通道高度图生成切线空间法线贴图(BC5)
- 支持根据 2D 法线贴图的局部方向变化估算粗糙度纹理
- 支持距离场生成

> ⚠️ 平台限制：当前 `core/` 与 `cli/` 实现依赖 Win32 API
> （`__ImageBase` 模块基址、`WIN32_FIND_DATAW` 目录枚举、`_wfopen_s`/`_fseeki64`、
> 宽字符路径与 `wmain`），**只能在 Windows + MSVC 下构建**。
> 仓库中的 `download_linux_ispc.sh` 仅用于在 Linux 上生成 ISPC kernel 目标文件，
> 并不能让 TexConv 本身在 Linux 上编译。

## 图像处理库

本项目使用 ImageMagick 作为唯一图像处理后端。

### ImageMagick

ImageMagick 是一个活跃维护的图像处理库，提供：
- ✅ 更活跃的库维护
- ✅ 更好的 HDR/EXR 支持
- ✅ 更先进的图像处理算法
- ✅ 更多的图像格式支持
- ✅ 高质量的 Lanczos 缩放算法

## 构建说明

仅支持 **Windows + MSVC**，需要 **C++20**（`core/`、`cli/`、`common/` 使用 `std::filesystem` 等 C++17/20 特性）。

两种配置方式：

1. **从引擎根工程配置**（推荐日常开发）：根 `CMakeLists.txt` 已包含本目录，
   hgl 目标（`CMCore` 等）已存在时自动链接，无需额外参数。
2. **独立配置**（发布工具时不构建整个 ULRE）：`if(NOT TARGET CMCore)` 分支会从
   `${ULRE_ROOT}/build/out/Windows_64_{Debug,Release}` 导入**预编译**的
   `CMCoreType` / `CMPlatform` / `CMUtil` / `CMCore` 静态库，absl 默认取自
   `E:/vcpkg/installed/x64-windows`。
   **前提**：根工程至少构建过一次，且 hgl 静态库的运行时库设置（`/MT` 或 `/MD`）
   与本工程一致，否则会 `LNK2038`。

其余细节见 **DEPLOY.md**（ImageMagick 运行时部署、插件发现、`TexImage.dll` 恒为
Release 的约束、Qt GUI 的独立部署步骤等）。

## 模块化架构(2026-09 重构)

自 2026-09 起 TexConv 拆分为模块化架构,CLI 仅是薄外壳,转换能力全部来自 DLL:

| 模块 | 职责 |
|------|------|
| TexCommon(静态库) | 公共 C 接口头 + 像素格式表 |
| TexImage.dll | ImageMagick++ 图像加载/预处理 |
| TexEncAMD.dll / TexEncIntel.dll | 块压缩编码器插件(运行时加载) |
| TexConvCore.dll | 转换流水线 / .Tex2D 容器 / 插件管理 |
| TexConv.exe | CLI 外壳 |

Qt GUI 等未来外壳只需链接 `common/include/texconv/tex_core.h` 暴露的 C API
(逐文件格式配置、进度回调、取消、产物读回等)。
构建、部署与回归验证见 **DEPLOY.md**;命令行用法保持不变(见下文)。

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

# 距离场:单通道纹理按灰度生成,RGBA 纹理按 Alpha 生成(结果为单通道纹理)
TexConv /DF image.png
TexConv /DF:160 image.png          # 自定义内外判定阈值(默认 128)
TexConv /DF /R:R8 /mip image.png   # 未压缩 R8 + mipmap

# 将颜色亮度视为高度,通过 Sobel 梯度生成 XY 法线并输出 BC5
TexConv /height2normal image.png

# 高度图为单通道输入;会拒绝 RGB/RGBA 颜色图
TexConv /heightmap2normal height.png

# 由法线方向的局部变化估算单通道粗糙度(默认 BC4)
TexConv /normal2roughness normal.png

# 使用 Intel ISPC
TexConv /Intel image.png

# 指定格式
TexConv /RGB:BC1 /RGBA:BC3 image.png
```

### Cubemap / Octahedral / IBL（TexConv 内建）

```bash
# 将 6 个面转换为 cubemap(.TexCube)
TexConv /cube /mip /out:output PosX.png NegX.png PosY.png NegY.png PosZ.png NegZ.png

# 转换为正方形 Octahedral Mapping 2D 纹理
TexConv /cube /octa /mip /out:output PosX.png NegX.png PosY.png NegY.png PosZ.png NegZ.png

# 只取 +Z 天空半球,输出 Lambert Azimuthal Equal-Area 圆盘投影
TexConv /cube /laea /mip /out:sky PosX.png NegX.png PosY.png NegY.png PosZ.png NegZ.png

# Octahedral 主输出及 Irradiance/Prefilter 两个 IBL 附加输出均使用 Octahedral Mapping
TexConv /cube /octa /ibl /mip /out:sky_octa PosX.png NegX.png PosY.png NegY.png PosZ.png NegZ.png
```

`/laea` 输出边长为 Cubemap 面宽两倍的正方形 `.Tex2D`；圆盘内映射 +Z 半球，
圆盘外像素为零。该模式与 `/ibl`、`/octa` 互斥；`/octa /ibl` 会额外生成采用相同
Octahedral Mapping 的 Irradiance 与 Prefilter `.Tex2D` 文件。

`/cube` 也可只给一个目录：目录会被递归扫描，按面文件名分组，凑齐 6 面的组才会转换
（输出写在输入同目录，`/out:` 在目录模式下无效）。

## 未参与构建的附加工具

以下工具的历史源码仍保留在工程根目录，但**未加入任何 CMake 目标**，
因此构建产物中不存在对应可执行文件（见根 `CMakeLists.txt` 的说明注释）：

| 源码 | 原用途 | 现状 |
|---|---|---|
| `HDR2PNG.cpp` | HDR/EXR 转 PNG | 未构建；TexConv 只输出 `.Tex2D` / `.TexCube` |
| `ComboTexture.cpp` | 材质贴图通道打包 | 未构建 |
| `CubemapConvert.cpp` | 独立 6 面转 cubemap 工具 | 未构建；请改用 `TexConv /cube` |
| `DistanceFieldGenerater.cpp` | 独立距离场工具 | 未构建；请改用 `TexConv /DF` |
| `NormalTest.cpp` / `SpheremapNormal.cpp` | 法线/球面贴图试验 | 未构建 |

这些源码需要在迁移到 `TexImage.dll` 之上后才能重新启用，目前保留仅为参考。

## CMake 选项

- `CMAKE_BUILD_TYPE` - 构建类型（Debug/Release）
- `HGL_BITS` - 目标架构位数（32/64），独立配置时用于定位预建 hgl 库
- `TEXCONV_ISPC_EXE` - ISPC 编译器路径（`enc_intel` 构建期用它编译 kernel；
  默认查环境变量 `ISPC_EXE`，再退回 `ISPCTextureCompressor/ISPC/win/ispc.exe`）

## 回归测试

`tests/regression.py` 是一套纯 Python（仅标准库）的 CLI 回归套件，分两个阶段：
先跑一遍记录产物哈希，之后再跑一遍与基线比对。

```bash
# 1) 首次生成输入图（需要 ImageMagick）
python tests/regression.py gen-inputs

# 2) 记录基线
python tests/regression.py run --exe TexConv.exe --out tests/baseline/new

# 3) 与已有基线比对
python tests/regression.py compare --exe TexConv.exe --against tests/baseline/old/manifest.json
```

覆盖范围：

- **单图转换**：灰度 / 灰度+alpha / RGB / RGBA / TGA / 16bit / EXR / HDR，
  未压缩格式与 BC 系列压缩、mipmap、AMD 后端、距离场、法线图、Intel 后端缺失负例
- **目录模式**：`/s` 递归枚举（含跳过既有 `.Tex2D`）
- **Cubemap / 投影 / IBL**：`/cube`、`/octa`、`/laea`、`/ibl` 及其组合，
  以及 `/octa`+`/laea`、`/laea`+`/ibl` 互斥负例

Cubemap 用例**不依赖基线**，改为校验与实现无关的不变量，因此换机器/换压缩库也能用：

- 容器几何：`TexCube` 必为 `view=3` 且 6 面 32×32，2D 投影必为 `view=1` 且 64×64
- 白炉（6 面全白）：漫反射辐照度与 GGX 预滤波必须恒为 255（能量守恒）
- 单面点亮：辐照度/预滤波的 4 个相邻面必须旋转对称（偏差 ≤2%），对侧面最暗，
  且预滤波峰值高于辐照度峰值（波瓣更收束）——切线基或采样权重写错时这里会失败

ImageMagick 与输入图源目录可通过参数或环境变量覆盖，便于在其他机器上运行：

```bash
python tests/regression.py --magick /usr/bin/magick --res /path/to/res/image \
    compare --exe TexConv.exe --against tests/baseline/old/manifest.json
```

对应环境变量：`TEXCONV_MAGICK`、`TEXCONV_RES_IMAGE`。

已知环境差异：`intel_ispc` 用例需要 `TexEncIntel.dll` 与 `TexConv.exe` 同目录；ISPC 不可用时
该插件不参与构建，用例会失败（这是环境问题而非回归）。

另外 `tests/baseline/old/manifest.json` 记录于「默认后端为 AMD」的时期（早于改为默认 Intel
的提交），因此补齐 Intel 后端后，凡是**未显式指定后端**的压缩用例（`*_default`、`rgb8_bc7`、
`rgb8_gray`、`dir_mode_s` 等）产物哈希必然与旧基线不同——要核对后端一致性请用 `/AMD`、
`/Intel` 显式指定（`rgb8_amd`、`intel_ispc` 两个用例就是为此保留的）。

## 依赖项

### 必需依赖

- Windows + MSVC，CMake 3.28+
- C++20
- ImageMagick（需含 Magick++ 开发头文件）
- 独立配置额外需要：预编译的 `CMCore` / `CMPlatform` / `CMUtil` / `CMCoreType` 静态库、
  vcpkg 提供的 `absl`、GLM

### 可选依赖

- AMD Compressonator（用于 AMD 压缩后端，`TexEncAMD.dll`）
- Intel ISPC Texture Compressor（用于 Intel 压缩后端，`TexEncIntel.dll`；
  需先用 `download_win_ispc.bat` 装好 `ispc.exe`，构建期由 CMake 调用它把
  `kernel*.ispc` 编译到构建树，找不到编译器时该插件不参与构建）
- Qt6 Widgets（用于 `TexConvQt` GUI，找不到 Qt 时自动跳过）

## Windows 特定说明

### ImageMagick 安装

1. 从 [ImageMagick 官网](https://imagemagick.org/script/download.php) 下载 Windows 安装程序
2. 在安装过程中，确保选择：
   - "Install development headers and libraries for C and C++"
   - "Add application directory to your system path"
3. 重启命令提示符或 PowerShell 以使环境变量生效

## 跨平台说明

本工程当前**不支持** Linux/macOS 构建。`core` 的目录枚举、文件读写、模块自定位
均直接调用 Win32 API，且公共 API 使用 `wchar_t` 宽字符路径。

仓库中的 `download_linux_ispc.sh` 只用于在 Linux 上生成 ISPC kernel 目标文件
（供 CI 交叉取得 `.obj`），与 TexConv 自身的可编译性无关。

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
- 压缩：BC1RGB, BC1RGBA, BC2, BC3, BC4, BC5, BC6H, BC6H_SF, BC7
  （不支持 ASTC / ETC）

#### BC6H 源数据要求

BC6H/BC6H_SF 是 HDR 三通道格式，Alpha 不参与压缩：

- 源图必须是 **3 或 4 通道**；1/2 通道源直接报错（退出码 1），日志为：

  ```
  encoder Intel can not encode BC6H from 1 channel source.
  ```

- 4 通道源只取 R/G/B，Alpha 被丢弃。
- 非半精度源会自动转成半精度后再编码：Intel 后端向核心声明「需要 RGBA + Float32」，
  再由插件内部做 float32 → float16 位模式转换（`enc_intel/half_float.h`，就近舍入到偶数，
  含 Inf/NaN/上溢/非规格化处理），因此 8/16 位整数源与 float32/EXR/HDR 源都可直接使用。
  因为显式要求 Float32，EXR/HDR 中 >1.0 的亮度也完整保留（旧实现按图像标称像素类型取值，
  depth≤16 的 EXR 会被压到 1.0）。

### 像素数据类型支持

- `ImagePixelType::UInt8` - 8位无符号整数
- `ImagePixelType::UInt16` - 16位无符号整数
- `ImagePixelType::Float32` - 32位浮点数
- `ImagePixelType::Float16` - 16 位整数存储（映射 Magick `ShortPixel`），不是 IEEE 半精度位模式；
  需要真正的 half 位模式时须显式转换（BC6H 的转换在 `enc_intel/half_float.h`）

## 故障排除

### ImageMagick 未找到

```
CMake Error: ImageMagick not found
```

**解决方案：**
- Windows: 确保安装了 ImageMagick 并包含开发库（Magick++）
- 独立配置时还需保证 `absl_DIR`、`HGL_OUT_*` 指向的预建库存在

### 解码时提示 "no decode delegate"

ImageMagick 只拷贝了 `Magick++` DLL 而漏了运行时组件。构建脚本的
`texconv_deploy_im_runtime` 会把 `*.xml` 配置与 `modules/` 编解码器目录一起部署；
手工拷贝时也必须一并复制，否则 HDR/EXR/PSD 等格式无法读取。

### 提示 "Intel ISPC texture compressor is not available"

`enc_intel` 只有在能找到 `ispc.exe` 时才会被构建：`enc_intel/CMakeLists.txt` 在构建期用
`add_custom_command` 直接调用它把 `ISPCTextureCompressor/ispc_texcomp/kernel{,_astc}.ispc`
编译成 obj 与头文件（产物落在构建树 `enc_intel/ispc_kernels/`，不污染源码树），
**不需要先 msbuild 上游的 `ispc_texcomp.vcxproj`**。

编译器查找顺序：CMake 变量 `TEXCONV_ISPC_EXE` → 环境变量 `ISPC_EXE` →
`ISPCTextureCompressor/ISPC/win/ispc.exe`。都找不到时 configure 阶段会打印提示并跳过
`TexEncIntel.dll`，内核会回落到 AMD 后端
（AMD BC7 在部分 512×512 RGBA 内容上存在死锁问题，因此默认优先后端是 Intel）。
需要 Intel 后端时先运行 `download_win_ispc.bat`，或手工指定：

```bash
cmake -DTEXCONV_ISPC_EXE="D:/ispc/ispc.exe" -S . -B build
```

### 链接错误

如果遇到链接错误，尝试：
1. 清理构建目录：`rm -rf build && mkdir build`
2. 确保所有依赖库都已正确安装
3. 检查 CMake 配置输出中的库路径

## 许可证

请查看项目许可证文件。

## 贡献

欢迎提交问题报告和拉取请求。

## 联系方式

- GitHub: https://github.com/hyzboy/TexConv
