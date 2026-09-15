# SkyCube 材质(天空球 Cubemap)实施笔记

## 调查结论(全部已核实)

1. 材质定义解析器已支持 `SamplerCube`(`MaterialDefinitionFile.cpp:385`,GLSLSamplerType 枚举在 `MaterialRecipe.h:43`)。
2. ShaderGen 发射端对纹理槽发出的引用行是 `uvec2 tex_<名字>`(`MaterialShaderEmitter.cpp:211`),**与采样类型无关** → 发射端零改动。
3. Bindless 系统(`VKBindlessTextureManager.cpp` + `ShaderLibrary/common/bindless_textures.glsl`)只有 `texture2DArray[]`(binding=0)与 `sampler[]`(binding=1)——**需要新增 binding=2 `textureCube[]`**。
4. `Texture::GetBindlessArrayView()` 基类返回主 view(TextureCube 的主 view 即 CUBE 类型);`Texture2D` 覆写为 companion 2D_ARRAY view。→ 给 Texture 加 `virtual VkImageView GetBindlessCubeView(){return VK_NULL_HANDLE;}`,TextureCube 覆写返回主 view。
5. 注册路径:`RenderSceneUBOSystem::RegisterTextureResource(resource_id, tex, bindless_mgr)` → `BindlessTextureManager::RegisterTexture(tex)`(写 `tex->GetBindlessArrayView()` 到 binding=0)。→ RegisterTexture 分支:`GetBindlessCubeView()` 非空写 binding=2,否则 binding=0。
6. ECS 校验(`PrimitiveComponent.cpp:301/420`):只限制 2DArray 的 layer 匹配;`SamplerCube` 声明 + 默认 kind Texture2D 能通过现有校验(非 array sampler),**枚举可不加 Cube**;但运行时 `RegisterTexture` 必须按 view 类型分流。
7. `RenderSceneUBOSystem` 还有一个 path-policy 无关的确认:材质化把 handle 按 resource_id 写入行(`GetBindlessHandle` → materialization)。handle 流转与采样类型无关。

## 实施状态(已按用户决定调整设计)

**最终设计:与 2D 完全同构——所有 Cubemap 统一注册为 6 层 CUBE_ARRAY companion
view(单张 = DEPTH 1),走 bindless binding=2 `textureCubeArray[]`。**

- VKBindlessTextureManager:binding=2 SAMPLED_IMAGE kMax + pool 第三项
- TextureCube::GetBindlessArrayView:惰性 CreateImageViewCubeArray(镜像 Texture2D 模式)
- bindless_textures.glsl:textureCubeArray 声明 + SampleCubeArray(handle,samp,vec4(dir,layer))
- SkyCube 材质(surface 形态,走 Sky 模板 fragDirection→si.worldPos)
- 示例 SkyCubeSphere

## 待实现清单(历史)

- [ ] `VKBindlessTextureManager.h/.cpp`:binding=2 `textureCube[]`(SAMPLED_IMAGE,kMax,同 flags);pool sizes 加第三项;RegisterTexture 按 `GetBindlessCubeView()` 分流到 binding=2。
- [ ] `VKTexture.h`:Texture 加 `virtual VkImageView GetBindlessCubeView(){return VK_NULL_HANDLE;}`;TextureCube 覆写 `return GetVulkanImageView();`(主 view 即 CUBE)。
- [ ] `ShaderLibrary/common/bindless_textures.glsl`:binding=2 `uniform textureCube bindless_cube[]` + `vec4 SampleCube(uint tex_handle, uint samp_idx, vec3 dir)`。
- [ ] `ShaderLibrary/material/sky_cube.material.toml`:id="SkyCube",provider_policy="GeometryOnly",transform 与 sky_minimal 相同(Vec3Position/Passthrough3D/World/World/WorldCameraVP),[fragment] material_source_module="material/sky_cube_source.glsl",[vertex] requirements=["Position"] varyings=["emit_frag_direction","emit_data_index_id"],[resources] ubos=["CameraInfo"] textures=[{name="sky_cube",sampler="SamplerCube",required=true}] samplers=["Trilinear"]。
  - 注意:dataIndex 链(emit_data_index_id / MTL_TEX(i))是否对"无 SSBO 行"的 primitive 可用 **未核实**;若不可用,退化方案:照 unlit_texture 建 material_source,要求 geometry 带 UV0;或给 sky recipe 分配一行材质数据。
- [ ] `ShaderLibrary/material/sky_cube_source.glsl`:@ulre texture_reference sky_cube Fragment required;SampleCube(MTL_TEX(dataIndex).sky_cube.x, TrilinearSampler, normalize(si.worldPos));输出 baseColor/alpha。
- [ ] 示例 `example/Environment/SkyCubeSphere.cpp`:LoadTextureCube(.TexCube) → HexSphere(半径 256,subdiv 3) 居中 → recipe mtl_def_id="SkyCube" + MakeSkyConfig() → SetMaterialTextureResource("sky_cube", tex, sampler) → 相机 target 原点。
- [ ] 运行时材质化路径(PrimitiveComponent.cpp:301/420 校验 + materialization)对 SamplerCube 声明 + TextureCube* 指针放行(补校验用例)。
- [ ] 验证:Debug 用 Release(见 IM 已知限制);渲染出天空球 = 成功。

## 已踩坑

- **glsl 模块文件禁止 UTF-8 BOM**:`ShaderCodeModuleFile.cpp` 的解析器按"行首"匹配
  `// @ulre`,BOM 会使 begin 行失配并报 MissingBegin(导致整个模块被拒注册)。
  用普通 UTF-8(无 BOM)写 glsl;python 改写时勿用 utf-8-sig 编码回写。
- 宽字符日志:`GLogError("%s", full_name.c_str())` 在 UNICODE 构建下打印宽串
  只会显示首字符(如 "file=E"),是日志显示问题,不代表文件名真的是 E。

## CMake/工程注意

- example 的 CMakeLists 参考 example/Environment/CMakeLists.txt(如无则新建,挂在 example 构建树)。
