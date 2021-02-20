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
/// Declaration of Diligent::PipelineResourceSignatureGLImpl class

#include <array>

#include "PipelineResourceSignatureBase.hpp"
#include "GLProgramResourceCache.hpp"

namespace Diligent
{
class RenderDeviceGLImpl;
class GLPipelineResourceLayout;

/// Implementation of the Diligent::PipelineResourceSignatureGLImpl class
class PipelineResourceSignatureGLImpl final : public PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceGLImpl>
{
public:
    using TPipelineResourceSignatureBase = PipelineResourceSignatureBase<IPipelineResourceSignature, RenderDeviceGLImpl>;

    PipelineResourceSignatureGLImpl(IReferenceCounters*                        pRefCounters,
                                    RenderDeviceGLImpl*                        pDevice,
                                    const PipelineResourceSignatureCreateInfo& CreateInfo,
                                    bool                                       bIsDeviceInternal = false);
    ~PipelineResourceSignatureGLImpl();

    // sizeof(ResourceAttribs) == 8, x64
    struct ResourceAttribs
    {
    private:
        static constexpr Uint32 _SamplerIndBits      = 31;
        static constexpr Uint32 _SamplerAssignedBits = 1;

    public:
        static constexpr Uint32 InvalidCacheOffset = ~0u;
        static constexpr Uint32 InvalidSamplerInd  = (1u << _SamplerIndBits) - 1;

        // clang-format off
        const Uint32  CacheOffset;                                 // SRB and Signature has the same cache offsets for static resources.
                                                                   // Binding = m_FirstBinding[Range] + CacheOffset
        const Uint32  SamplerInd           : _SamplerIndBits;      // ImtblSamplerAssigned == true:  index of the immutable sampler in m_ImmutableSamplers.
                                                                   // ImtblSamplerAssigned == false: index of the assigned sampler in m_Desc.Resources.
        const Uint32  ImtblSamplerAssigned : _SamplerAssignedBits; // Immutable sampler flag
        // clang-format on

        ResourceAttribs(Uint32 _CacheOffset,
                        Uint32 _SamplerInd,
                        bool   _ImtblSamplerAssigned) noexcept :
            // clang-format off
            CacheOffset         {_CacheOffset                   },
            SamplerInd          {_SamplerInd                    },
            ImtblSamplerAssigned{_ImtblSamplerAssigned ? 1u : 0u}
        // clang-format on
        {
            VERIFY(SamplerInd == _SamplerInd, "Sampler index (", _SamplerInd, ") exceeds maximum representable value");
            VERIFY(!_ImtblSamplerAssigned || SamplerInd != InvalidSamplerInd, "Immutable sampler assigned, but sampler index is not valid");
        }

        bool IsSamplerAssigned() const { return SamplerInd != InvalidSamplerInd; }
        bool IsImmutableSamplerAssigned() const { return ImtblSamplerAssigned != 0; }
    };

    const ResourceAttribs& GetResourceAttribs(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_pResourceAttribs[ResIndex];
    }

    const PipelineResourceDesc& GetResourceDesc(Uint32 ResIndex) const
    {
        VERIFY_EXPR(ResIndex < m_Desc.NumResources);
        return m_Desc.Resources[ResIndex];
    }

    Uint32 GetFirstUBBinding() const { return m_FirstBinding[SHADER_RESOURCE_RANGE_CONSTANT_BUFFER]; }
    Uint32 GetFirstTextureBinding() const { return m_FirstBinding[SHADER_RESOURCE_RANGE_TEXTURE_SRV]; }
    Uint32 GetFirstImageBinding() const { return m_FirstBinding[SHADER_RESOURCE_RANGE_TEXTURE_UAV]; }
    Uint32 GetFirstSSBBinding() const { return m_FirstBinding[SHADER_RESOURCE_RANGE_BUFFER_UAV]; }

    void ApplyBindings(GLObjectWrappers::GLProgramObj& GLProgram,
                       class GLContextState&           State,
                       SHADER_TYPE                     Stages) const;

    /// Implementation of IPipelineResourceSignature::CreateShaderResourceBinding.
    virtual void DILIGENT_CALL_TYPE CreateShaderResourceBinding(IShaderResourceBinding** ppShaderResourceBinding,
                                                                bool                     InitStaticResources) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableByName.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByName(SHADER_TYPE ShaderType, const Char* Name) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableByIndex.
    virtual IShaderResourceVariable* DILIGENT_CALL_TYPE GetStaticVariableByIndex(SHADER_TYPE ShaderType, Uint32 Index) override final;

    /// Implementation of IPipelineResourceSignature::GetStaticVariableCount.
    virtual Uint32 DILIGENT_CALL_TYPE GetStaticVariableCount(SHADER_TYPE ShaderType) const override final;

    /// Implementation of IPipelineResourceSignature::BindStaticResources.
    virtual void DILIGENT_CALL_TYPE BindStaticResources(Uint32            ShaderFlags,
                                                        IResourceMapping* pResourceMapping,
                                                        Uint32            Flags) override final;

    /// Implementation of IPipelineResourceSignature::IsCompatibleWith.
    virtual bool DILIGENT_CALL_TYPE IsCompatibleWith(const IPipelineResourceSignature* pPRS) const override final
    {
        VERIFY_EXPR(pPRS != nullptr);
        return IsCompatibleWith(*ValidatedCast<const PipelineResourceSignatureGLImpl>(pPRS));
    }

    bool IsCompatibleWith(const PipelineResourceSignatureGLImpl& Other) const;

    void InitSRBResourceCache(GLProgramResourceCache& ResourceCache) const;

    // Copies static resources from the static resource cache to the destination cache
    void InitializeStaticSRBResources(GLProgramResourceCache& ResourceCache) const;

private:
    void CreateLayouts(const PipelineResourceSignatureCreateInfo& CreateInfo);

    void Destruct();

    size_t CalculateHash() const;

private:
    ResourceAttribs* m_pResourceAttribs = nullptr; // [m_Desc.NumResources]

    // Resource cache for static resource variables only
    GLProgramResourceCache* m_StaticResourceCache = nullptr;

    GLPipelineResourceLayout* m_StaticResourceLayouts = nullptr; // [m_NumShaderStages]

    using SamplerPtr                = RefCntAutoPtr<ISampler>;
    SamplerPtr* m_ImmutableSamplers = nullptr; // [m_Desc.NumImmutableSamplers]

    std::array<Uint32, SHADER_RESOURCE_RANGE_LAST + 1> m_FirstBinding = {};
    std::array<Uint32, SHADER_RESOURCE_RANGE_LAST + 1> m_BindingCount = {};
};

SHADER_RESOURCE_RANGE PipelineResourceToShaderResourceRange(const PipelineResourceDesc& Desc);

} // namespace Diligent
