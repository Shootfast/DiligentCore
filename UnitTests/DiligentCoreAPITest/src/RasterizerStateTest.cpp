/*     Copyright 2019 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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

#include "TestingEnvironment.h"
#include "PSOTestBase.h"

#include "gtest/gtest.h"

using namespace Diligent;

namespace
{

class RasterizerStateTest : public PSOTestBase, public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        InitResources();
    }

    static void TearDownTestSuite()
    {
        ReleaseResources();
        TestingEnvironment::GetInstance()->ReleaseResources();
    }
};

TEST_F(RasterizerStateTest, CreatePSO)
{
    auto PSODesc = GetPSODesc();

    RasterizerStateDesc& RSDesc = PSODesc.GraphicsPipeline.RasterizerDesc;

    ASSERT_TRUE(CreateTestPSO(PSODesc, true));

    for (auto FillMode = FILL_MODE_UNDEFINED + 1; FillMode < FILL_MODE_NUM_MODES; ++FillMode)
    {
        RSDesc.FillMode = static_cast<FILL_MODE>(FillMode);
        auto pPSO       = CreateTestPSO(PSODesc, true);
        ASSERT_TRUE(pPSO);
        EXPECT_EQ(pPSO->GetDesc().GraphicsPipeline.RasterizerDesc.FillMode, RSDesc.FillMode);
    }

    for (auto CullMode = CULL_MODE_UNDEFINED + 1; CullMode < CULL_MODE_NUM_MODES; ++CullMode)
    {
        RSDesc.CullMode = static_cast<CULL_MODE>(CullMode);
        auto pPSO       = CreateTestPSO(PSODesc, true);
        ASSERT_TRUE(pPSO);
        EXPECT_EQ(pPSO->GetDesc().GraphicsPipeline.RasterizerDesc.CullMode, RSDesc.CullMode);
    }

    {
        RSDesc.FrontCounterClockwise = !RSDesc.FrontCounterClockwise;
        auto pPSO                    = CreateTestPSO(PSODesc, true);
        ASSERT_TRUE(pPSO);
        EXPECT_EQ(pPSO->GetDesc().GraphicsPipeline.RasterizerDesc.FrontCounterClockwise, RSDesc.FrontCounterClockwise);
    }

    RSDesc.DepthBias = 100;
    ASSERT_TRUE(CreateTestPSO(PSODesc, true));

    RSDesc.DepthBiasClamp = 1.f;
    ASSERT_TRUE(CreateTestPSO(PSODesc, true));

    RSDesc.SlopeScaledDepthBias = 2.f;
    ASSERT_TRUE(CreateTestPSO(PSODesc, true));

    {
        RSDesc.DepthClipEnable = !RSDesc.DepthClipEnable;
        auto pPSO              = CreateTestPSO(PSODesc, true);
        ASSERT_TRUE(pPSO);
        EXPECT_EQ(pPSO->GetDesc().GraphicsPipeline.RasterizerDesc.DepthClipEnable, RSDesc.DepthClipEnable);
    }

    {
        RSDesc.ScissorEnable = !RSDesc.ScissorEnable;
        auto pPSO            = CreateTestPSO(PSODesc, true);
        ASSERT_TRUE(pPSO);
        EXPECT_EQ(pPSO->GetDesc().GraphicsPipeline.RasterizerDesc.ScissorEnable, RSDesc.ScissorEnable);
    }

    RSDesc.AntialiasedLineEnable = !RSDesc.AntialiasedLineEnable;
    ASSERT_TRUE(CreateTestPSO(PSODesc, true));
}

} // namespace
