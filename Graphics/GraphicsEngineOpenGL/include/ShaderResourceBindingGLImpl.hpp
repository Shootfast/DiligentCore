/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#pragma once

/// \file
/// Declaration of Diligent::ShaderResourceBindingGLImpl class

#include "ShaderResourceBindingGL.h"
#include "RenderDeviceGL.h"
#include "ShaderResourceBindingBase.hpp"
#include "GLProgramResources.hpp"
#include "GLProgramResourceCache.hpp"
#include "GLPipelineResourceLayout.hpp"
#include "PipelineResourceSignatureGLImpl.hpp"

namespace Diligent
{

/// Shader resource binding object implementation in OpenGL backend.
class ShaderResourceBindingGLImpl final : public ShaderResourceBindingBase<IShaderResourceBindingGL, PipelineResourceSignatureGLImpl>
{
public:
    using TBase = ShaderResourceBindingBase<IShaderResourceBindingGL, PipelineResourceSignatureGLImpl>;

    ShaderResourceBindingGLImpl(IReferenceCounters*              pRefCounters,
                                PipelineResourceSignatureGLImpl* pPRS,
                                bool                             IsDeviceInternal = false);
    ~ShaderResourceBindingGLImpl();

    virtual void DILIGENT_CALL_TYPE QueryInterface(const INTERFACE_ID& IID, IObject** ppInterface) override final;

    /// Implementation of IShaderResourceBinding::BindResources() in OpenGL backend.
    virtual void DILIGENT_CALL_TYPE BindResources(Uint32            ShaderFlags,
                                                  IResourceMapping* pResMapping,
                                                  Uint32            Flags) override final;

    /// Implementation of IShaderResourceBinding::GetVariableByName() in OpenGL backend.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetVariableByName(SHADER_TYPE ShaderType, const char* Name) override final;

    /// Implementation of IShaderResourceBinding::GetVariableCount() in OpenGL backend.
    virtual Uint32 DILIGENT_CALL_TYPE GetVariableCount(SHADER_TYPE ShaderType) const override final;

    /// Implementation of IShaderResourceBinding::GetVariableByIndex() in OpenGL backend.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    /// Implementation of IShaderResourceBinding::InitializeStaticResources() in OpenGL backend.
    virtual void DILIGENT_CALL_TYPE InitializeStaticResources(const IPipelineState* pPipelineState) override final;

    /// Implementation of IShaderResourceBinding::InitializeStaticResourcesWithSignature() in OpenGL backend.
    virtual void DILIGENT_CALL_TYPE InitializeStaticResourcesWithSignature(const IPipelineResourceSignature* pResourceSignature) override final;

    const GLProgramResourceCache& GetResourceCache() const { return m_ResourceCache; }

private:
    void Destruct();

    std::array<Int8, MAX_SHADERS_IN_PIPELINE> m_ShaderVarIndex = {-1, -1, -1, -1, -1, -1};
    static_assert(MAX_SHADERS_IN_PIPELINE == 6, "Please update the initializer list above");

    bool m_bStaticResourcesInitialized = false;

    const Uint8 m_NumShaders = 0;

    // The resource cache holds resource bindings for all variables
    GLProgramResourceCache m_ResourceCache;

    // The resource layout only references mutable and dynamic variables
    GLPipelineResourceLayout* m_ResourceLayouts = nullptr; // [m_NumShaders]
};

} // namespace Diligent
