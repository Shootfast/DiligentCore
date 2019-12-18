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

#include "pch.h"

#include "TextureCubeArray_OGL.h"
#include "RenderDeviceGLImpl.h"
#include "DeviceContextGLImpl.h"
#include "GLTypeConversions.h"
#include "GraphicsAccessories.h"
#include "BufferGLImpl.h"

namespace Diligent
{

TextureCubeArray_OGL::TextureCubeArray_OGL(IReferenceCounters*        pRefCounters,
                                           FixedBlockMemoryAllocator& TexViewObjAllocator,
                                           RenderDeviceGLImpl*        pDeviceGL,
                                           GLContextState&            GLState,
                                           const TextureDesc&         TexDesc,
                                           const TextureData*         pInitData /*= nullptr*/,
                                           bool                       bIsDeviceInternal /*= false*/) :
    // clang-format off
    TextureBaseGL
    {
        pRefCounters,
        TexViewObjAllocator,
        pDeviceGL,
        TexDesc,
        GL_TEXTURE_CUBE_MAP_ARRAY,
        pInitData,
        bIsDeviceInternal
    }
// clang-format on
{
    if (TexDesc.Usage == USAGE_STAGING)
    {
        // We will use PBO initialized by TextureBaseGL
        return;
    }

    VERIFY(m_Desc.SampleCount == 1, "Multisampled texture cube arrays are not supported");

    GLState.BindTexture(-1, m_BindTarget, m_GlTexture);

    // Every OpenGL API call that operates on cubemap array textures takes layer-faces, not array layers.
    // For example, when you allocate storage for the texture, you would use glTexStorage3D? or glTexImage3D? or similar.
    // The depth? parameter will be the number of layer-faces, not layers. So it must be divisible by 6.
    VERIFY((m_Desc.ArraySize % 6) == 0, "Array size must be multiple of 6");
    //                             levels             format          width         height          depth
    glTexStorage3D(m_BindTarget, m_Desc.MipLevels, m_GLTexFormat, m_Desc.Width, m_Desc.Height, m_Desc.ArraySize);
    CHECK_GL_ERROR_AND_THROW("Failed to allocate storage for the Cubemap texture array");
    //When target is GL_TEXTURE_CUBE_MAP_ARRAY glTexStorage3D is equivalent to:
    //
    //for (i = 0; i < levels; i++) {
    //    glTexImage3D(target, i, internalformat, width, height, depth, 0, format, type, NULL);
    //    width = max(1, (width / 2));
    //    height = max(1, (height / 2));
    //}

    SetDefaultGLParameters();

    if (pInitData != nullptr && pInitData->pSubResources != nullptr)
    {
        VERIFY((m_Desc.ArraySize % 6) == 0, "Array size must be multiple of 6");
        if (m_Desc.MipLevels * m_Desc.ArraySize == pInitData->NumSubresources)
        {
            for (Uint32 Slice = 0; Slice < m_Desc.ArraySize; ++Slice)
            {
                for (Uint32 Mip = 0; Mip < m_Desc.MipLevels; ++Mip)
                {
                    Box DstBox{0, std::max(m_Desc.Width >> Mip, 1U),
                               0, std::max(m_Desc.Height >> Mip, 1U)};
                    // UpdateData() is a virtual function. If we try to call it through vtbl from here,
                    // we will get into TextureBaseGL::UpdateData(), because instance of TextureCubeArray_OGL
                    // is not fully constructed yet.
                    // To call the required function, we need to explicitly specify the class:
                    TextureCubeArray_OGL::UpdateData(GLState, Mip, Slice, DstBox, pInitData->pSubResources[Slice * m_Desc.MipLevels + Mip]);
                }
            }
        }
        else
        {
            UNEXPECTED("Incorrect number of subresources");
        }
    }

    GLState.BindTexture(-1, m_BindTarget, GLObjectWrappers::GLTextureObj::Null());
}

TextureCubeArray_OGL::TextureCubeArray_OGL(IReferenceCounters*        pRefCounters,
                                           FixedBlockMemoryAllocator& TexViewObjAllocator,
                                           RenderDeviceGLImpl*        pDeviceGL,
                                           GLContextState&            GLState,
                                           const TextureDesc&         TexDesc,
                                           GLuint                     GLTextureHandle,
                                           bool                       bIsDeviceInternal) :
    // clang-format off
    TextureBaseGL
    {
        pRefCounters,
        TexViewObjAllocator,
        pDeviceGL,
        GLState,
        TexDesc,
        GLTextureHandle,
        GL_TEXTURE_CUBE_MAP_ARRAY,
        bIsDeviceInternal
    }
// clang-format on
{
}

TextureCubeArray_OGL::~TextureCubeArray_OGL()
{
}

void TextureCubeArray_OGL::UpdateData(GLContextState&          ContextState,
                                      Uint32                   MipLevel,
                                      Uint32                   Slice,
                                      const Box&               DstBox,
                                      const TextureSubResData& SubresData)
{
    TextureBaseGL::UpdateData(ContextState, MipLevel, Slice, DstBox, SubresData);

    ContextState.BindTexture(-1, m_BindTarget, m_GlTexture);

    // Bind buffer if it is provided; copy from CPU memory otherwise
    GLuint UnpackBuffer = 0;
    if (SubresData.pSrcBuffer != nullptr)
    {
        auto* pBufferGL = ValidatedCast<BufferGLImpl>(SubresData.pSrcBuffer);
        UnpackBuffer    = pBufferGL->GetGLHandle();
    }

    // Transfers to OpenGL memory are called unpack operations
    // If there is a buffer bound to GL_PIXEL_UNPACK_BUFFER target, then all the pixel transfer
    // operations will be performed from this buffer.
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, UnpackBuffer);

    const auto& TransferAttribs = GetNativePixelTransferAttribs(m_Desc.Format);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (TransferAttribs.IsCompressed)
    {
        auto MipWidth  = std::max(m_Desc.Width >> MipLevel, 1U);
        auto MipHeight = std::max(m_Desc.Height >> MipLevel, 1U);
        // clang-format off
        VERIFY((DstBox.MinX % 4) == 0 && (DstBox.MinY % 4) == 0 &&
               ((DstBox.MaxX % 4) == 0 || DstBox.MaxX == MipWidth) && 
               ((DstBox.MaxY % 4) == 0 || DstBox.MaxY == MipHeight), 
               "Compressed texture update region must be 4 pixel-aligned");
        // clang-format on
#ifdef _DEBUG
        {
            const auto& FmtAttribs      = GetTextureFormatAttribs(m_Desc.Format);
            auto        BlockBytesInRow = ((DstBox.MaxX - DstBox.MinX + 3) / 4) * Uint32{FmtAttribs.ComponentSize};
            VERIFY(SubresData.Stride == BlockBytesInRow,
                   "Compressed data stride (", SubresData.Stride, " must match the size of a row of compressed blocks (", BlockBytesInRow, ")");
        }
#endif

        // Every OpenGL API call that operates on cubemap array textures takes layer-faces, not array layers.

        //glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        //glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
        auto UpdateRegionWidth  = DstBox.MaxX - DstBox.MinX;
        auto UpdateRegionHeight = DstBox.MaxY - DstBox.MinY;
        UpdateRegionWidth       = std::min(UpdateRegionWidth, MipWidth - DstBox.MinX);
        UpdateRegionHeight      = std::min(UpdateRegionHeight, MipHeight - DstBox.MinY);
        glCompressedTexSubImage3D(m_BindTarget, MipLevel,
                                  DstBox.MinX,
                                  DstBox.MinY,
                                  Slice,
                                  UpdateRegionWidth,
                                  UpdateRegionHeight,
                                  1,
                                  // The format must be the same compressed-texture format previously
                                  // specified by glTexStorage2D() (thank you OpenGL for another useless
                                  // parameter that is nothing but the source of confusion), otherwise
                                  // INVALID_OPERATION error is generated.
                                  m_GLTexFormat,
                                  // An INVALID_VALUE error is generated if imageSize is not consistent with
                                  // the format, dimensions, and contents of the compressed image( too little or
                                  // too much data ),
                                  ((DstBox.MaxY - DstBox.MinY + 3) / 4) * SubresData.Stride,
                                  // If a non-zero named buffer object is bound to the GL_PIXEL_UNPACK_BUFFER target, 'data' is treated
                                  // as a byte offset into the buffer object's data store.
                                  // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glCompressedTexSubImage3D.xhtml
                                  SubresData.pSrcBuffer != nullptr ? reinterpret_cast<void*>(static_cast<size_t>(SubresData.SrcOffset)) : SubresData.pData);
    }
    else
    {
        const auto TexFmtInfo = GetTextureFormatAttribs(m_Desc.Format);
        const auto PixelSize  = Uint32{TexFmtInfo.NumComponents} * Uint32{TexFmtInfo.ComponentSize};
        VERIFY((SubresData.Stride % PixelSize) == 0, "Data stride is not multiple of pixel size");
        glPixelStorei(GL_UNPACK_ROW_LENGTH, SubresData.Stride / PixelSize);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        // Every OpenGL API call that operates on cubemap array textures takes layer-faces, not array layers.
        // When uploading texel data to the cubemap array, the parameters that represent the Z component
        // are layer-faces. So if you want to upload to just the positive Z face of the second layer in the array,
        // you would use call glTexSubImage3D?, with the zoffset?? parameter set to 10 (layer 1 * 6 faces per layer + face index 4),
        // and the depth? set to 1 (because you're only uploading one layer-face).

        // Target must be GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY, or GL_TEXTURE_CUBE_MAP_ARRAY.
        // (NO individual cubemap faces GL_TEXTURE_CUBE_MAP_POSITIVE_X .. GL_TEXTURE_CUBE_MAP_NEGATIVE_Z!!!)
        glTexSubImage3D(m_BindTarget, MipLevel,
                        DstBox.MinX,
                        DstBox.MinY,
                        Slice,
                        DstBox.MaxX - DstBox.MinX,
                        DstBox.MaxY - DstBox.MinY,
                        1,
                        TransferAttribs.PixelFormat, TransferAttribs.DataType,
                        // If a non-zero named buffer object is bound to the GL_PIXEL_UNPACK_BUFFER target, 'data' is treated
                        // as a byte offset into the buffer object's data store.
                        // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexSubImage3D.xhtml
                        SubresData.pSrcBuffer != nullptr ? reinterpret_cast<void*>(static_cast<size_t>(SubresData.SrcOffset)) : SubresData.pData);
    }
    CHECK_GL_ERROR("Failed to update subimage data");

    if (UnpackBuffer != 0)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    ContextState.BindTexture(-1, m_BindTarget, GLObjectWrappers::GLTextureObj::Null());
}

void TextureCubeArray_OGL::AttachToFramebuffer(const TextureViewDesc& ViewDesc, GLenum AttachmentPoint)
{
    // Same as for 2D array textures

    // Every OpenGL API call that operates on cubemap array textures takes layer-faces, not array layers.
    // So the parameters that represent the Z component are layer-faces.
    if (ViewDesc.NumArraySlices == m_Desc.ArraySize)
    {
        // glFramebufferTexture() attaches the given mipmap levelas a layered image with the number of layers that the given texture has.
        glFramebufferTexture(GL_DRAW_FRAMEBUFFER, AttachmentPoint, m_GlTexture, ViewDesc.MostDetailedMip);
        CHECK_GL_ERROR("Failed to attach texture cubemap array to draw framebuffer");
        glFramebufferTexture(GL_READ_FRAMEBUFFER, AttachmentPoint, m_GlTexture, ViewDesc.MostDetailedMip);
        CHECK_GL_ERROR("Failed to attach texture cubemap array to read framebuffer");
    }
    else if (ViewDesc.NumArraySlices == 1)
    {
        // Texture name must either be zero or the name of an existing 3D texture, 1D or 2D array texture,
        // cube map array texture, or multisample array texture.
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, AttachmentPoint, m_GlTexture, ViewDesc.MostDetailedMip, ViewDesc.FirstArraySlice);
        CHECK_GL_ERROR("Failed to attach texture cubemap array to draw framebuffer");
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, AttachmentPoint, m_GlTexture, ViewDesc.MostDetailedMip, ViewDesc.FirstArraySlice);
        CHECK_GL_ERROR("Failed to attach texture cubemap array to read framebuffer");
    }
    else
    {
        UNEXPECTED("Only one slice or the entire cubemap array can be attached to a framebuffer");
    }
}

} // namespace Diligent
