// AMD AMDUtils code
//
// Copyright(c) 2017 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#pragma once

#include "cmp_gltfcommon.h"
#include "cmp_gltftechnique.h"

#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

#include <vulkan/vulkan.h>

// Forward Declaration
class CMP_DeviceVK;
class CMP_ResourceViewHeapsVK;
class CMP_DynamicBufferRingVK;
class CMP_StaticBufferPoolVK;
class CMP_UploadHeapVK;

// This class takes a GltfCommon class (that holds all the non-GPU specific data) as an input and loads all the GPU specific data
//
class GltfBBoxPassVK : public CMP_GltfTechnique
{
  public:
    struct per_object {
        glm::mat4x4 mWorldViewProj;
        glm::vec4 vCenter;
        glm::vec4 vRadius;
        glm::vec4 vColor;
    };

    void OnCreate(CMP_DeviceVK*        pDevice,
        VkRenderPass renderPass,
                  CMP_UploadHeapVK*        pUploadHeap,
                  CMP_ResourceViewHeapsVK* pHeaps,
                  CMP_DynamicBufferRingVK* pDynamicBufferRing,
                  CMP_StaticBufferPoolVK*  pStaticBufferPool,
                  CMP_GLTFCommon*      pGLTFData);

    void OnDestroy();
    glm::mat4x4 *SetPerBatchConstants() {
        return &m_Camera;
    };
    void DrawMesh(VkCommandBuffer cmd_buf, int meshIndex, const glm::mat4x4& worldMatrix);
  private:

    CMP_DeviceVK*        m_pDevice;
      CMP_ResourceViewHeapsVK* m_pResourceViewHeaps;

    // all bounding boxes of all the meshes use the same geometry, shaders and pipelines.
    VkIndexType m_indexType;
    UINT m_NumIndices;
    VkDescriptorBufferInfo m_IBV;
    VkDescriptorBufferInfo m_VBV;

    VkPipeline m_pipeline;
    VkPipelineCache m_pipelineCache;
    VkPipelineLayout m_pipelineLayout;
    VkDescriptorPool m_descriptorPool;
    VkDescriptorSet m_descriptorSet;
    VkDescriptorSetLayout m_descriptorSetLayout;

    glm::mat4x4 m_Camera;
};


