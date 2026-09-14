# TexConv 模块化部署说明

## 模块结构

```
TexCommon.lib     公共 C 接口头(common/include/texconv/*.h)+ 像素格式表(静态链接进各消费者)
TexImage.dll      图像核心:ImageMagick++ 加载/预处理/通道转换(hgl 仅链入此 DLL)
TexEncAMD.dll     AMD Compressonator 编码器插件(CMP_ConvertTexture)
TexEncIntel.dll   Intel ISPC 编码器插件(ispc_texcomp;kernel*.obj 缺失时不构建)
TexConvCore.dll   编排核心:转换流水线 / .Tex2D 容器 / 运行时加载 texenc*.dll 插件
TexConv.exe       CLI 外壳(参数解析 + 调用 TexCore,零 hgl)
```

数据流:外壳 → TexCore_RunJob → TexImage(加载/预处理/归一)→ 编码器插件(BC 压缩)→ .Tex2D 容器写出。
`.Tex2D` 文件格式与旧版完全一致(32 字节头),引擎零改动。

## 发布包清单

```
bin/
  TexConv.exe
  TexConvCore.dll
  TexImage.dll
  TexEncAMD.dll              # 无此文件 = 无 AMD 后端
  TexEncIntel.dll            # 可选;缺失时 Intel 后端不可用(CLI 报错退出 1,GUI 应置灰)
  ImageMagick 运行时:        # 来自系统安装目录(如 E:\ImageMagick-7.1.2-Q16-HDRI)
    CORE_RL_Magick++_.dll
    CORE_RL_MagickWand_.dll
    CORE_RL_MagickCore_.dll
    CORE_RL_*.dll( delegates 依赖项)
    modules/coders/IM_MOD_RL_*.dll   # 解码委托,缺 PNG/TGA 等会加载失败
    modules/filters/
```

## DLL 搜索路径

`TexCore_Init` 会自动把"自身 exe 所在目录"加入 DLL 搜索(`SetDllDirectory`),
因此随包分发时无需修改 PATH。若系统 PATH 中另装有不同版本的 ImageMagick,
随包版本优先(建议发布时始终随包,避免委托版本不匹配导致 "no decode delegate")。

## 编码器插件机制

- TexConvCore 启动时扫描自身目录下 `texenc*.dll`,取 `TexGetEncoderProvider` 导出并校验 ABI 版本。
- 插件接口:`common/include/texconv/texenc_plugin.h`(C ABI,稳定)。
- 新增后端(如 NVTT/ASTC):实现 Provider 并命名为 `TexEnc<Name>.dll` 放入 bin 即可,核心零改动。

## 后续:Qt GUI 外壳

TexConvQt 只依赖 `common/include/texconv/tex_core.h`(C API):
`TexCore_Init / ProbeImage / EnumDirectory / RunJob(同步+进度回调+取消) / ReadInfo / EnumProviders`。
在工作线程调用 `RunJob`,进度回调刷新 UI,回调返回非 0 即取消并删除半成品。
逐文件配置通过 `TexJobParams.target_format`(显式指定)实现,槽位模式(`slot_format`)仅为 CLI 兼容保留。

## Qt GUI 外壳(TexConvQt)

`gui/` 子目录,Qt6 Widgets(经 vcpkg;找不到 Qt 时自动跳过、不影响 CLI 构建)。
只依赖 TexCommon + TexConvCore 的 C API。平台插件 `platforms/qwindows.dll`
由 POST_BUILD 自动拷到产物目录。拖拽文件/文件夹 → 检测 → 逐行选目标格式 → 转换;
产物与源图同目录(可在界面指定统一输出目录)。

- **逐文件配置**:法线/距离场(含阈值)是每文件属性,选中一批后右键菜单批量设置
  (设置目标格式/法线BC5/距离场/距离场阈值/移除)。
- **法线自动识别**:添加文件时按文件名自动标记(含 normal/nmap,或以 _n/-n/ n 结尾),
  识别错误的可手动取消。
- **无头冒烟**:`TexConvQt.exe --auto [--mip|--gray|--discard|--normal|--df|
  --df-threshold:N|--provider:X|--outdir:DIR] 文件...` 自动 检测→转换→退出,
  退出码 0 = 全部成功(其中 --normal/--df 作用于全部文件)。

## 重新构建(两种生命周期)

六个工程(TexCommon/TexImage/TexEncAMD/TexEncIntel/TexConvCore/TexConv)在两种
VC 解决方案生命周期里均可整体构建与调试,均按 `CM/Tools/Texture` 分组:

**1. ULRE 根解决方案**(`E:\ULRE\build\ULRE.slnx`):

```
cmake --build E:\ULRE\build --config Release ^
  --target TexConv TexImage TexConvCore TexEncAMD TexEncIntel TexCommon
```

产物输出到 `E:\ULRE\build\out\Windows_64_Release\`。
注意:`--clean-first` 会清空整个 out 目录(含全部示例程序),慎用。

**2. 独立解决方案**(`src/Tools/TexConv/build/TexConv.slnx`,推荐日常开发):
hgl 静态库仅链入 TexImage.dll,独立配置只需引用根工程预编译库,无需构建整个 ULRE。

```
cd src/Tools/TexConv
cmake -B build -S . -G "Visual Studio 18 2026" -A x64 ^
      -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

- 产物输出到 `src/Tools/TexConv/build/out/Windows_64_{Debug,Release}\`。
- 前提:根工程至少构建过一次(存在 `build/out` 下的 hgl 静态库);
  运行时库(MT)与 GLM 布局宏已由 CMakeLists 自动对齐。
- TexConv 设为启动工程,可直接 F5 调试;命令行参数在
  项目属性 -> Debugging -> Command Arguments 配置。
- 两种构建的产物均通过 43/43 回归。

## 回归验证

```
cd src/Tools/TexConv/tests
python regression.py gen-inputs                                  # 生成测试图(一次性)
python regression.py run    --exe <旧exe> --out baseline/old     # 生成基线(已生成)
python regression.py compare --exe <新exe> --against baseline/old/manifest.json
```

判定标准:产物 .Tex2D 字节级一致(sha256)。当前状态:43/43 通过。
`tests/unit_core.cpp` / `unit_encoders.cpp` / `unit_image.cpp` 为进程内 C API 对拍驱动。

## 已知行为说明(与旧版保持一致)

- 3 通道源配 RGBA8/R8 等不兼容目标:转换失败并残留 32 字节头文件(旧 InitFormat 行为)。
- 旧版 `/gray`+3 通道源会崩溃(AMD 编码器路径),新版已不崩溃且产物正确。
- `/ColorKey:` 仅解析不生效(旧版即如此,为 GUI 预留)。
- 距离场(`/DF`):算法迁自旧 DFGen(双网格两遍扫描 SDF,输出 = (到暗部距离-到亮部距离)*3+128);
  修正了旧实现 uint32 偏移的回绕缺陷(改有符号)。单通道源对灰度生成,
  RGBA/GrayAlpha 源对 Alpha 生成,RGB 无 Alpha 报错;生成后按 1 通道继续,
  默认槽位 R8(AMD 的 BC4 灰度源路径有已知 0 字节问题,可用 /Intel+/R:BC4 规避)。
  GUI 侧通过 `TexJobParams.df_mode/df_threshold` 配置。
- 退出码:任一转换失败返回 1(旧版恒 0,此为唯一有意的行为改进)。
- `res/image/TexConv.exe` 为人工维护副本:更新时构建后手动复制,并在 res 子模块单独提交。
