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

#include "pch.h"
#include "PipelineStateGLImpl.hpp"
#include "RenderDeviceGLImpl.hpp"
#include "ShaderGLImpl.hpp"
#include "ShaderResourceBindingGLImpl.hpp"
#include "EngineMemory.h"
#include "DeviceContextGLImpl.hpp"

namespace Diligent
{

void PipelineStateGLImpl::CreateDefaultSignature(const PipelineStateCreateInfo& CreateInfo,
                                                 std::vector<ShaderGLImpl*>&    Shaders,
                                                 SHADER_TYPE                    ActiveStages,
                                                 IPipelineResourceSignature**   ppSignature)
{
    std::vector<PipelineResourceDesc> Resources;

    const auto&        LayoutDesc     = CreateInfo.PSODesc.ResourceLayout;
    const auto         DefaultVarType = LayoutDesc.DefaultVariableType;
    GLProgramResources ProgramResources;

    const auto HandleResource = [&](const GLProgramResources::GLResourceAttribs& Attribs, PIPELINE_RESOURCE_FLAGS Flags) //
    {
        PipelineResourceDesc ResDesc = {};
        ResDesc.Name                 = Attribs.Name;
        ResDesc.ShaderStages         = Attribs.ShaderStages;
        ResDesc.ArraySize            = Attribs.ArraySize;
        ResDesc.ResourceType         = Attribs.ResourceType;
        ResDesc.VarType              = DefaultVarType;
        ResDesc.Flags                = Flags;

        for (Uint32 i = 0; i < LayoutDesc.NumVariables; ++i)
        {
            const auto& Var = LayoutDesc.Variables[i];
            if ((Var.ShaderStages & Attribs.ShaderStages) != 0 &&
                std::strcmp(Attribs.Name, Var.Name) == 0)
            {
                ResDesc.VarType = Var.Type;
                break;
            }
        }
        Resources.push_back(ResDesc);
    };
    const auto HandleUB = [&](const GLProgramResources::GLResourceAttribs& Attribs) {
        HandleResource(Attribs, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };
    const auto HandleTexture = [&](const GLProgramResources::GLResourceAttribs& Attribs) {
        HandleResource(Attribs, Attribs.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_SRV ? PIPELINE_RESOURCE_FLAG_COMBINED_SAMPLER : PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER);
    };
    const auto HandleImage = [&](const GLProgramResources::GLResourceAttribs& Attribs) {
        HandleResource(Attribs, Attribs.ResourceType == SHADER_RESOURCE_TYPE_TEXTURE_UAV ? PIPELINE_RESOURCE_FLAG_UNKNOWN : PIPELINE_RESOURCE_FLAG_FORMATTED_BUFFER);
    };
    const auto HandleSB = [&](const GLProgramResources::GLResourceAttribs& Attribs) {
        HandleResource(Attribs, PIPELINE_RESOURCE_FLAG_UNKNOWN);
    };

    if (m_IsProgramPipelineSupported)
    {
        for (size_t i = 0; i < Shaders.size(); ++i)
        {
            auto* pShaderGL = Shaders[i];
            pShaderGL->GetShaderResources().ProcessResources(HandleUB, HandleTexture, HandleImage, HandleSB);
        }
    }
    else
    {
        auto pImmediateCtx = m_pDevice->GetImmediateContext();
        VERIFY_EXPR(pImmediateCtx);
        VERIFY_EXPR(m_GLPrograms[0] != 0);

        Uint32 UniformBufferBinding = 0;
        Uint32 SamplerBinding       = 0;
        Uint32 ImageBinding         = 0;
        Uint32 StorageBufferBinding = 0;
        ProgramResources.LoadUniforms(ActiveStages,
                                      m_GLPrograms[0],
                                      pImmediateCtx.RawPtr<DeviceContextGLImpl>()->GetContextState(),
                                      UniformBufferBinding,
                                      SamplerBinding,
                                      ImageBinding,
                                      StorageBufferBinding);
        ProgramResources.ProcessResources(HandleUB, HandleTexture, HandleImage, HandleSB);
    }

    if (Resources.size())
    {
        String SignName = String{"Implicit signature for PSO '"} + m_Desc.Name + '\'';

        PipelineResourceSignatureCreateInfo ResSignCI = {};

        ResSignCI.Desc.Name                       = SignName.c_str();
        ResSignCI.Desc.Resources                  = Resources.data();
        ResSignCI.Desc.NumResources               = static_cast<Uint32>(Resources.size());
        ResSignCI.Desc.ImmutableSamplers          = LayoutDesc.ImmutableSamplers;
        ResSignCI.Desc.NumImmutableSamplers       = LayoutDesc.NumImmutableSamplers;
        ResSignCI.Desc.BindingIndex               = 0;
        ResSignCI.Desc.SRBAllocationGranularity   = CreateInfo.PSODesc.SRBAllocationGranularity;
        ResSignCI.Desc.UseCombinedTextureSamplers = true;

        GetDevice()->CreatePipelineResourceSignature(ResSignCI, ppSignature, true);

        if (*ppSignature == nullptr)
            LOG_ERROR_AND_THROW("Failed to create resource signature for pipeline state");
    }
}

void PipelineStateGLImpl::InitResourceLayouts(const PipelineStateCreateInfo& CreateInfo,
                                              std::vector<ShaderGLImpl*>&    Shaders,
                                              SHADER_TYPE                    ActiveStages)
{
    const Uint32                              SignatureCount = CreateInfo.ResourceSignaturesCount;
    RefCntAutoPtr<IPipelineResourceSignature> pImplicitSignature;

    if (SignatureCount == 0 || CreateInfo.ppResourceSignatures == nullptr)
    {
        CreateDefaultSignature(CreateInfo, Shaders, ActiveStages, &pImplicitSignature);
        if (pImplicitSignature != nullptr)
        {
            VERIFY_EXPR(pImplicitSignature->GetDesc().BindingIndex == 0);
            m_Signatures[0]  = ValidatedCast<PipelineResourceSignatureGLImpl>(pImplicitSignature.RawPtr());
            m_SignatureCount = 1;
        }
    }
    else
    {
        PipelineResourceSignatureGLImpl::CopyResourceSignatures(CreateInfo.PSODesc.PipelineType, SignatureCount, CreateInfo.ppResourceSignatures, m_Signatures, m_SignatureCount);
    }

    // Apply resource bindings to programs.
    auto& CtxState = m_pDevice->GetImmediateContext().RawPtr<DeviceContextGLImpl>()->GetContextState();

    for (Uint32 s = 0; s < m_SignatureCount; ++s)
    {
        const auto& pSignature = m_Signatures[s];
        if (pSignature == nullptr)
            continue;

        if (m_IsProgramPipelineSupported)
        {
            for (Uint32 p = 0; p < m_NumPrograms; ++p)
                pSignature->ApplyBindings(m_GLPrograms[p], CtxState, GetShaderStageType(p));
        }
        else
        {
            pSignature->ApplyBindings(m_GLPrograms[0], CtxState, ActiveStages);
        }
    }

    // AZ TODO: check resource bindings
}

template <typename PSOCreateInfoType>
void PipelineStateGLImpl::InitInternalObjects(const PSOCreateInfoType& CreateInfo, std::vector<ShaderGLImpl*>& Shaders)
{
    const auto& deviceCaps = GetDevice()->GetDeviceCaps();
    VERIFY(deviceCaps.DevType != RENDER_DEVICE_TYPE_UNDEFINED, "Device caps are not initialized");

    m_IsProgramPipelineSupported = deviceCaps.Features.SeparablePrograms != DEVICE_FEATURE_STATE_DISABLED;

    FixedLinearAllocator MemPool{GetRawAllocator()};

    ReserveSpaceForPipelineDesc(CreateInfo, MemPool);
    MemPool.AddSpace<GLProgramObj>(m_IsProgramPipelineSupported ? Shaders.size() : 1);

    MemPool.Reserve();

    InitializePipelineDesc(CreateInfo, MemPool);

    // Get active shader stages.
    SHADER_TYPE ActiveStages = SHADER_TYPE_UNKNOWN;
    for (auto* pShader : Shaders)
    {
        const auto ShaderType = pShader->GetDesc().ShaderType;
        VERIFY((ActiveStages & ShaderType) == 0, "Shader stage ", GetShaderTypeLiteralName(ShaderType), " is already active");
        ActiveStages |= ShaderType;
    }

    // Create programs.
    if (m_IsProgramPipelineSupported)
    {
        m_GLPrograms = MemPool.ConstructArray<GLProgramObj>(Shaders.size(), false);
        for (size_t i = 0; i < Shaders.size(); ++i)
        {
            auto* pShaderGL  = Shaders[i];
            m_GLPrograms[i]  = GLProgramObj{ShaderGLImpl::LinkProgram(&pShaderGL, 1, true)};
            m_ShaderTypes[i] = pShaderGL->GetDesc().ShaderType;
        }
        m_NumPrograms = static_cast<Uint8>(Shaders.size());
    }
    else
    {
        m_GLPrograms     = MemPool.ConstructArray<GLProgramObj>(1, false);
        m_GLPrograms[0]  = ShaderGLImpl::LinkProgram(Shaders.data(), static_cast<Uint32>(Shaders.size()), false);
        m_ShaderTypes[0] = ActiveStages;
        m_NumPrograms    = 1;
    }

    InitResourceLayouts(CreateInfo, Shaders, ActiveStages);
}

PipelineStateGLImpl::PipelineStateGLImpl(IReferenceCounters*                    pRefCounters,
                                         RenderDeviceGLImpl*                    pDeviceGL,
                                         const GraphicsPipelineStateCreateInfo& CreateInfo,
                                         bool                                   bIsDeviceInternal) :
    // clang-format off
    TPipelineStateBase
    {
        pRefCounters,
        pDeviceGL,
        CreateInfo,
        bIsDeviceInternal
    }
// clang-format on
{
    try
    {
        std::vector<ShaderGLImpl*> Shaders;
        ExtractShaders<ShaderGLImpl>(CreateInfo, Shaders);

        RefCntAutoPtr<ShaderGLImpl> pTempPS;
        if (CreateInfo.pPS == nullptr)
        {
            // Some OpenGL implementations fail if fragment shader is not present, so
            // create a dummy one.
            ShaderCreateInfo ShaderCI;
            ShaderCI.SourceLanguage  = SHADER_SOURCE_LANGUAGE_GLSL;
            ShaderCI.Source          = "void main(){}";
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.Desc.Name       = "Dummy fragment shader";
            pDeviceGL->CreateShader(ShaderCI, reinterpret_cast<IShader**>(static_cast<ShaderGLImpl**>(&pTempPS)));

            Shaders.emplace_back(pTempPS);
        }

        InitInternalObjects(CreateInfo, Shaders);
    }
    catch (...)
    {
        Destruct();
    }
}

PipelineStateGLImpl::PipelineStateGLImpl(IReferenceCounters*                   pRefCounters,
                                         RenderDeviceGLImpl*                   pDeviceGL,
                                         const ComputePipelineStateCreateInfo& CreateInfo,
                                         bool                                  bIsDeviceInternal) :
    // clang-format off
    TPipelineStateBase
    {
        pRefCounters,
        pDeviceGL,
        CreateInfo,
        bIsDeviceInternal
    }
// clang-format on
{
    try
    {
        std::vector<ShaderGLImpl*> Shaders;
        ExtractShaders<ShaderGLImpl>(CreateInfo, Shaders);

        InitInternalObjects(CreateInfo, Shaders);
    }
    catch (...)
    {
        Destruct();
    }
}

PipelineStateGLImpl::~PipelineStateGLImpl()
{
    Destruct();
}

void PipelineStateGLImpl::Destruct()
{
    GetDevice()->OnDestroyPSO(this);

    if (m_GLPrograms)
    {
        for (Uint32 i = 0; i < m_NumPrograms; ++i)
        {
            m_GLPrograms[i].~GLProgramObj();
        }
        m_GLPrograms = nullptr;
    }

    m_Signatures.fill({});

    m_SignatureCount = 0;
    m_NumPrograms    = 0;

    TPipelineStateBase::Destruct();
}

IMPLEMENT_QUERY_INTERFACE(PipelineStateGLImpl, IID_PipelineStateGL, TPipelineStateBase)

SHADER_TYPE PipelineStateGLImpl::GetShaderStageType(Uint32 Index) const
{
    VERIFY(Index < m_NumPrograms, "Index is out of range");
    return m_ShaderTypes[Index];
}

bool PipelineStateGLImpl::IsCompatibleWith(const IPipelineState* pPSO) const
{
    VERIFY_EXPR(pPSO != nullptr);

    if (pPSO == this)
        return true;

    const auto& lhs = *this;
    const auto& rhs = *ValidatedCast<const PipelineStateGLImpl>(pPSO);

    if (lhs.GetSignatureCount() != rhs.GetSignatureCount())
        return false;

    for (Uint32 s = 0, SigCount = lhs.GetSignatureCount(); s < SigCount; ++s)
    {
        if (!lhs.GetSignature(s)->IsCompatibleWith(*rhs.GetSignature(s)))
            return false;
    }
    return true;
}

void PipelineStateGLImpl::CommitProgram(GLContextState& State)
{
    if (m_IsProgramPipelineSupported)
    {
        // WARNING: glUseProgram() overrides glBindProgramPipeline(). That is, if you have a program in use and
        // a program pipeline bound, all rendering will use the program that is in use, not the pipeline programs!
        // So make sure that glUseProgram(0) has been called if pipeline is in use
        State.SetProgram(GLObjectWrappers::GLProgramObj::Null());
        auto& Pipeline = GetGLProgramPipeline(State.GetCurrentGLContext());
        VERIFY(Pipeline != 0, "Program pipeline must not be null");
        State.SetPipeline(Pipeline);
    }
    else
    {
        VERIFY_EXPR(m_GLPrograms != nullptr);
        State.SetProgram(m_GLPrograms[0]);
    }
}

GLObjectWrappers::GLPipelineObj& PipelineStateGLImpl::GetGLProgramPipeline(GLContext::NativeGLContextType Context)
{
    ThreadingTools::LockHelper Lock(m_ProgPipelineLockFlag);
    for (auto& ctx_pipeline : m_GLProgPipelines)
    {
        if (ctx_pipeline.first == Context)
            return ctx_pipeline.second;
    }

    // Create new progam pipeline
    m_GLProgPipelines.emplace_back(Context, true);
    auto&  ctx_pipeline = m_GLProgPipelines.back();
    GLuint Pipeline     = ctx_pipeline.second;
    for (Uint32 i = 0; i < GetNumShaderStages(); ++i)
    {
        auto GLShaderBit = ShaderTypeToGLShaderBit(GetShaderStageType(i));
        // If the program has an active code for each stage mentioned in set flags,
        // then that code will be used by the pipeline. If program is 0, then the given
        // stages are cleared from the pipeline.
        glUseProgramStages(Pipeline, GLShaderBit, m_GLPrograms[i]);
        CHECK_GL_ERROR("glUseProgramStages() failed");
    }
    return ctx_pipeline.second;
}

} // namespace Diligent
