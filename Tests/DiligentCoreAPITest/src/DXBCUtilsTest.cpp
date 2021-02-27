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

#include <atlcomcli.h>
#include <d3dcompiler.h>

#include "DXBCUtils.hpp"
#include "TestingEnvironment.hpp"

#include "gtest/gtest.h"

using namespace Diligent;
using namespace Diligent::Testing;

namespace
{

static void TestDXBCRemapping(const char* Source, const char* Entry, const char* Profile, const DXBCUtils::TResourceBindingMap& ResMap)
{
    CComPtr<ID3DBlob> Blob;
    CComPtr<ID3DBlob> CompilerOutput;

    auto hr = D3DCompile(Source, strlen(Source), nullptr, nullptr, nullptr, Entry, Profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &Blob, &CompilerOutput);
    if (FAILED(hr))
    {
        const char* Msg = CompilerOutput ? static_cast<char*>(CompilerOutput->GetBufferPointer()) : "";
        LOG_ERROR_MESSAGE("D3DCompile failed: ", Msg);
    }
    ASSERT_HRESULT_SUCCEEDED(hr);

    ASSERT_TRUE(DXBCUtils::RemapResourceBindings(ResMap, Blob->GetBufferPointer(), Blob->GetBufferSize()));

    CComPtr<ID3D12ShaderReflection> ShaderReflection;

    hr = D3DReflect(Blob->GetBufferPointer(), Blob->GetBufferSize(), __uuidof(ShaderReflection), reinterpret_cast<void**>(&ShaderReflection));
    ASSERT_HRESULT_SUCCEEDED(hr);

    D3D12_SHADER_DESC ShaderDesc = {};
    ShaderReflection->GetDesc(&ShaderDesc);

    for (Uint32 ResInd = 0; ResInd < ShaderDesc.BoundResources; ++ResInd)
    {
        D3D12_SHADER_INPUT_BIND_DESC BindDesc = {};

        hr = ShaderReflection->GetResourceBindingDesc(ResInd, &BindDesc);
        ASSERT_HRESULT_SUCCEEDED(hr);

        String ResName{BindDesc.Name};

        auto Iter = ResMap.find(ResName.c_str());
        if (Iter != ResMap.end())
        {
            EXPECT_EQ(BindDesc.BindPoint, Iter->second.BindPoint);
            EXPECT_EQ(BindDesc.Space, Iter->second.Space);
            EXPECT_EQ(BindDesc.BindCount, Iter->second.ArraySize);
            continue;
        }

        if (ResName.back() != ']')
        {
            ADD_FAILURE() << "Can't find shader resource '" << BindDesc.Name << "'";
            continue;
        }

        Uint32 ArrayInd = 0;
        {
            size_t ArrEnd   = ResName.length() - 1;
            size_t ArrStart = ArrEnd - 1;

            for (; ArrStart > 1; --ArrStart)
            {
                if (ResName[ArrStart] == '[')
                    break;
                VERIFY_EXPR(ResName[ArrStart] >= '0' && ResName[ArrStart] <= '9');
            }

            ArrayInd = std::stoi(ResName.substr(ArrStart + 1, ArrEnd - ArrStart - 1));
            ResName.resize(ArrStart);
        }

        Iter = ResMap.find(ResName.c_str());
        if (Iter != ResMap.end())
        {
            EXPECT_LT(ArrayInd, Iter->second.ArraySize);
            EXPECT_EQ(BindDesc.BindPoint, Iter->second.BindPoint + ArrayInd);
            EXPECT_EQ(BindDesc.Space, Iter->second.Space);
            EXPECT_EQ(BindDesc.BindCount, 1u);
            continue;
        }

        ADD_FAILURE() << "Can't find shader resource '" << BindDesc.Name << "[" << ArrayInd << "]";
    }
}

using BindInfo = ResourceBinding::BindInfo;
using ResType  = ResourceBinding::ResType;

TEST(DXBCUtils, PatchSM50)
{
    static constexpr char Source[] = R"hlsl(
Texture2D g_Tex2D_1 : register(t4);
Texture2D g_Tex2D_2 : register(t3);
Texture2D g_Tex2D_3 : register(t0);
Texture2D g_Tex2D_4 : register(t1);

StructuredBuffer<float4>  g_InColorArray     : register(t2);
RWTexture2D<float4>       g_OutColorBuffer_1 : register(u1);
RWTexture2D<float4>       g_OutColorBuffer_2 : register(u2);

SamplerState g_Sampler_1 : register(s4);
SamplerState g_Sampler_2[4] : register(s0);

cbuffer Constants1 : register(b1)
{
    float4 g_ColorScale;
    float4 g_ColorBias;
};

cbuffer Constants2 : register(b0)
{
    float4 g_ColorMask;
};

float4 PSMain(in float4 f4Position : SV_Position) : SV_Target
{
    uint2  Coord = uint2(f4Position.xy);
    float2 UV    = f4Position.xy;
    g_OutColorBuffer_1[Coord] = g_Tex2D_1.SampleLevel(g_Sampler_1, UV.xy, 0.0) * g_ColorScale + g_ColorBias;
    g_OutColorBuffer_2[Coord] = g_Tex2D_2.SampleLevel(g_Sampler_1, UV.xy, 0.0) * g_ColorMask;

    float4 f4Color = float4(0.0, 0.0, 0.0, 0.0);
    f4Color += g_InColorArray[Coord.x];
    f4Color += g_Tex2D_3.SampleLevel(g_Sampler_2[1], UV.xy, 0.0);
    f4Color += g_Tex2D_4.SampleLevel(g_Sampler_2[3], UV.xy, 0.0);
    return f4Color;
}
)hlsl";

    Uint32       Tex   = 0;
    Uint32       UAV   = 1; // in because render targets acquire first UAV bindings
    Uint32       Samp  = 0;
    Uint32       Buff  = 0;
    const Uint32 Space = 0;

    DXBCUtils::TResourceBindingMap ResMap;

    const auto GetBindInfo = [&ResMap](Uint32 Register, Uint32 RegisterSpace, Uint32 ArraySize, ResType Type) {
        return BindInfo{Register, RegisterSpace, ArraySize, Type, ResMap.size()};
    };

    // clang-format off
    ResMap.emplace("g_Tex2D_1",          GetBindInfo(Tex++,  Space, 1, ResType::SRV    ));
    ResMap.emplace("g_Tex2D_2",          GetBindInfo(Tex++,  Space, 1, ResType::SRV    ));
    ResMap.emplace("g_Tex2D_3",          GetBindInfo(Tex++,  Space, 1, ResType::SRV    ));
    ResMap.emplace("g_Tex2D_4",          GetBindInfo(Tex++,  Space, 1, ResType::SRV    ));
    ResMap.emplace("g_InColorArray",     GetBindInfo(Tex++,  Space, 1, ResType::SRV    ));
    ResMap.emplace("g_OutColorBuffer_1", GetBindInfo(UAV++,  Space, 1, ResType::UAV    ));
    ResMap.emplace("g_OutColorBuffer_2", GetBindInfo(UAV++,  Space, 1, ResType::UAV    ));
    ResMap.emplace("g_Sampler_1",        GetBindInfo(Samp++, Space, 1, ResType::Sampler));
    ResMap.emplace("g_Sampler_2",        GetBindInfo(Samp++, Space, 4, ResType::Sampler));
    ResMap.emplace("Constants1",         GetBindInfo(Buff++, Space, 1, ResType::CBV    ));
    ResMap.emplace("Constants2",         GetBindInfo(Buff++, Space, 1, ResType::CBV    ));
    // clang-format on

    TestDXBCRemapping(Source, "PSMain", "ps_5_0", ResMap);
}

TEST(DXBCUtils, PatchSM51)
{
    static constexpr char Source[] = R"hlsl(
// space 0
SamplerState g_Sampler_1 : register(s0, space0);
SamplerState g_Sampler_2[4] : register(s5, space0);

cbuffer Constants1 : register(b0, space0)
{
    float4 g_Color1;
};

cbuffer Constants2 : register(b1, space0)
{
    float4 g_Color2;
};

// space 1
Texture2D            g_Tex2D_1          : register(t0, space1);
Texture2D            g_Tex2D_2          : register(t1, space1);
RWTexture2D<float4>  g_OutColorBuffer_2 : register(u0, space1);

// space 2
Texture2D                 g_Tex2D_3          : register(t0, space2);
Texture2D                 g_Tex2D_4          : register(t1, space2);
StructuredBuffer<float4>  g_InColorArray     : register(t2, space2);
RWTexture2D<float4>       g_OutColorBuffer_1 : register(u0, space2);


float4 PSMain(in float4 f4Position : SV_Position) : SV_Target
{
    uint2  Coord = uint2(f4Position.xy);
    float2 UV    = f4Position.xy;
    g_OutColorBuffer_1[Coord] = g_Tex2D_1.SampleLevel(g_Sampler_1, UV.xy, 0.0) * g_Color1;
    g_OutColorBuffer_2[Coord] = g_Tex2D_2.SampleLevel(g_Sampler_1, UV.xy, 0.0) * g_Color2;

    float4 f4Color = float4(0.0, 0.0, 0.0, 0.0);
    f4Color += g_InColorArray[Coord.x];
    f4Color += g_Tex2D_3.SampleLevel(g_Sampler_2[1], UV.xy, 0.0);
    f4Color += g_Tex2D_4.SampleLevel(g_Sampler_2[2], UV.xy, 0.0);
    return f4Color;
}
)hlsl";

    DXBCUtils::TResourceBindingMap ResMap;

    const auto GetBindInfo = [&ResMap](Uint32 Register, Uint32 RegisterSpace, Uint32 ArraySize, ResType Type) {
        return BindInfo{Register, RegisterSpace, ArraySize, Type, ResMap.size()};
    };

    // clang-format off
    // space 0
    {
        const Uint32 Space = 0;
        Uint32       Tex   = 0;
        Uint32       Buff  = 0;
        ResMap.emplace("g_Tex2D_2",  GetBindInfo(Tex++,  Space, 1, ResType::SRV));
        ResMap.emplace("g_Tex2D_3",  GetBindInfo(Tex++,  Space, 1, ResType::SRV));
        ResMap.emplace("Constants1", GetBindInfo(Buff++, Space, 1, ResType::CBV));
        ResMap.emplace("Constants2", GetBindInfo(Buff++, Space, 1, ResType::CBV));
    }
    // space 1
    {
        const Uint32 Space = 1;
        Uint32       Samp  = 0;
        Uint32       UAV   = 0;
        ResMap.emplace("g_OutColorBuffer_1", GetBindInfo(UAV++,  Space, 1, ResType::UAV    ));
        ResMap.emplace("g_OutColorBuffer_2", GetBindInfo(UAV++,  Space, 1, ResType::UAV    ));
        ResMap.emplace("g_Sampler_1",        GetBindInfo(Samp++, Space, 1, ResType::Sampler));
        ResMap.emplace("g_Sampler_2",        GetBindInfo(Samp++, Space, 4, ResType::Sampler));
    }
    // space 2
    {
        const Uint32 Space = 2;
        Uint32       Tex   = 0;
        ResMap.emplace("g_Tex2D_1",      GetBindInfo(Tex++, Space, 1, ResType::SRV));
        ResMap.emplace("g_Tex2D_4",      GetBindInfo(Tex++, Space, 1, ResType::SRV));
        ResMap.emplace("g_InColorArray", GetBindInfo(Tex++, Space, 1, ResType::SRV));
    }
    // clang-format on

    TestDXBCRemapping(Source, "PSMain", "ps_5_1", ResMap);
}

TEST(DXBCUtils, PatchSM51_DynamicIndices)
{
    static constexpr char Source[] = R"hlsl(
SamplerState g_Sampler             : register(s0, space0);
Texture2D    g_Tex2D_StatArray[8]  : register(t0, space0);
Texture2D    g_Tex2D_DynArray[100] : register(t0, space1);

cbuffer Constants : register(b0, space0)
{
    uint2 Range1;
    uint2 Range2;
};

float4 PSMain(in float4 f4Position : SV_Position) : SV_Target
{
    uint2  Coord   = uint2(f4Position.xy);
    float2 UV      = f4Position.xy;
    float4 f4Color = float4(0.0, 0.0, 0.0, 0.0);

    for (uint i = Range1.x; i < Range1.y; ++i)
    {
        f4Color += g_Tex2D_StatArray[i].SampleLevel(g_Sampler, UV, 0.0);
    }
    for (uint j = Range2.x; j < Range2.y; ++j)
    {
        f4Color += g_Tex2D_DynArray[i].SampleLevel(g_Sampler, UV, 0.0);
    }
    return f4Color;
}
)hlsl";

    DXBCUtils::TResourceBindingMap ResMap;

    const auto GetBindInfo = [&ResMap](Uint32 Register, Uint32 RegisterSpace, Uint32 ArraySize, ResType Type) {
        return BindInfo{Register, RegisterSpace, ArraySize, Type, ResMap.size()};
    };

    // clang-format off
    ResMap.emplace("g_Sampler",         GetBindInfo(11, 3,   1, ResType::Sampler));
    ResMap.emplace("g_Tex2D_StatArray", GetBindInfo(22, 3,   8, ResType::SRV    ));
    ResMap.emplace("g_Tex2D_DynArray",  GetBindInfo( 0, 2, 100, ResType::SRV    ));
    ResMap.emplace("Constants",         GetBindInfo(44, 1,   1, ResType::CBV    ));
    // clang-format on

    TestDXBCRemapping(Source, "PSMain", "ps_5_1", ResMap);
}
} // namespace
