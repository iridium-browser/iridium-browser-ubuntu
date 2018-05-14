//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.cpp:
//    Implements the class methods for VertexArrayVk.
//

#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

#include "common/debug.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"

namespace rx
{

VertexArrayVk::VertexArrayVk(const gl::VertexArrayState &state)
    : VertexArrayImpl(state),
      mCurrentArrayBufferHandles{},
      mCurrentArrayBufferOffsets{},
      mCurrentArrayBufferResources{},
      mCurrentElementArrayBufferResource(nullptr)
{
    mCurrentArrayBufferHandles.fill(VK_NULL_HANDLE);
    mCurrentArrayBufferOffsets.fill(0);
    mCurrentArrayBufferResources.fill(nullptr);

    mPackedInputBindings.fill({0, 0});
    mPackedInputAttributes.fill({0, 0, 0});
}

VertexArrayVk::~VertexArrayVk()
{
}

void VertexArrayVk::destroy(const gl::Context *context)
{
}

gl::Error VertexArrayVk::streamVertexData(ContextVk *context,
                                          StreamingBuffer *stream,
                                          int firstVertex,
                                          int lastVertex)
{
    const auto &attribs          = mState.getVertexAttributes();
    const auto &bindings         = mState.getVertexBindings();
    const gl::Program *programGL = context->getGLState().getProgram();

    // TODO(fjhenigman): When we have a bunch of interleaved attributes, they end up
    // un-interleaved, wasting space and copying time.  Consider improving on that.
    for (auto attribIndex : programGL->getActiveAttribLocationsMask())
    {
        const auto &attrib   = attribs[attribIndex];
        const auto &binding  = bindings[attrib.bindingIndex];
        gl::Buffer *bufferGL = binding.getBuffer().get();

        if (attrib.enabled && !bufferGL)
        {
            // TODO(fjhenigman): Work with more formats than just GL_FLOAT.
            if (attrib.type != GL_FLOAT)
            {
                UNIMPLEMENTED();
                return gl::InternalError();
            }

            // Only [firstVertex, lastVertex] is needed by the upcoming draw so that
            // is all we copy, but we allocate space for [0, lastVertex] so indexing
            // will work.  If we don't start at zero all the indices will be off.
            // TODO(fjhenigman): See if we can account for indices being off by adjusting
            // the offset, thus avoiding wasted memory.
            const size_t firstByte = firstVertex * binding.getStride();
            const size_t lastByte =
                lastVertex * binding.getStride() + gl::ComputeVertexAttributeTypeSize(attrib);
            uint8_t *dst = nullptr;
            ANGLE_TRY(stream->allocate(context, lastByte, &dst,
                                       &mCurrentArrayBufferHandles[attribIndex],
                                       &mCurrentArrayBufferOffsets[attribIndex]));
            memcpy(dst + firstByte, static_cast<const uint8_t *>(attrib.pointer) + firstByte,
                   lastByte - firstByte);
        }
    }

    ANGLE_TRY(stream->flush(context));
    return gl::NoError();
}

void VertexArrayVk::syncState(const gl::Context *context,
                              const gl::VertexArray::DirtyBits &dirtyBits)
{
    ASSERT(dirtyBits.any());

    // Invalidate current pipeline.
    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->onVertexArrayChange();

    // Rebuild current attribute buffers cache. This will fail horribly if the buffer changes.
    // TODO(jmadill): Handle buffer storage changes.
    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (auto dirtyBit : dirtyBits)
    {
        if (dirtyBit == gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER)
        {
            gl::Buffer *bufferGL = mState.getElementArrayBuffer().get();
            if (bufferGL)
            {
                mCurrentElementArrayBufferResource = vk::GetImpl(bufferGL);
            }
            else
            {
                mCurrentElementArrayBufferResource = nullptr;
            }
            continue;
        }

        size_t attribIndex = gl::VertexArray::GetVertexIndexFromDirtyBit(dirtyBit);

        // Invalidate the input description for pipelines.
        mDirtyPackedInputs.set(attribIndex);

        const auto &attrib  = attribs[attribIndex];
        const auto &binding = bindings[attrib.bindingIndex];

        if (attrib.enabled)
        {
            gl::Buffer *bufferGL = binding.getBuffer().get();

            if (bufferGL)
            {
                BufferVk *bufferVk                        = vk::GetImpl(bufferGL);
                mCurrentArrayBufferResources[attribIndex] = bufferVk;
                mCurrentArrayBufferHandles[attribIndex]   = bufferVk->getVkBuffer().getHandle();
            }
            else
            {
                mCurrentArrayBufferResources[attribIndex] = nullptr;
                mCurrentArrayBufferHandles[attribIndex]   = VK_NULL_HANDLE;
            }
            // TODO(jmadill): Offset handling.  Assume zero for now.
            mCurrentArrayBufferOffsets[attribIndex] = 0;
        }
        else
        {
            UNIMPLEMENTED();
        }
    }
}

const gl::AttribArray<VkBuffer> &VertexArrayVk::getCurrentArrayBufferHandles() const
{
    return mCurrentArrayBufferHandles;
}

const gl::AttribArray<VkDeviceSize> &VertexArrayVk::getCurrentArrayBufferOffsets() const
{
    return mCurrentArrayBufferOffsets;
}

void VertexArrayVk::updateDrawDependencies(vk::CommandGraphNode *readNode,
                                           const gl::AttributesMask &activeAttribsMask,
                                           Serial serial,
                                           DrawType drawType)
{
    // Handle the bound array buffers.
    for (auto attribIndex : activeAttribsMask)
    {
        if (mCurrentArrayBufferResources[attribIndex])
            mCurrentArrayBufferResources[attribIndex]->onReadResource(readNode, serial);
    }

    // Handle the bound element array buffer.
    if (drawType == DrawType::Elements)
    {
        ASSERT(mCurrentElementArrayBufferResource);
        mCurrentElementArrayBufferResource->onReadResource(readNode, serial);
    }
}

void VertexArrayVk::getPackedInputDescriptions(vk::PipelineDesc *pipelineDesc)
{
    updatePackedInputDescriptions();
    pipelineDesc->updateVertexInputInfo(mPackedInputBindings, mPackedInputAttributes);
}

void VertexArrayVk::updatePackedInputDescriptions()
{
    if (!mDirtyPackedInputs.any())
    {
        return;
    }

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (auto attribIndex : mDirtyPackedInputs)
    {
        const auto &attrib  = attribs[attribIndex];
        const auto &binding = bindings[attrib.bindingIndex];
        if (attrib.enabled)
        {
            updatePackedInputInfo(static_cast<uint32_t>(attribIndex), binding, attrib);
        }
        else
        {
            UNIMPLEMENTED();
        }
    }

    mDirtyPackedInputs.reset();
}

void VertexArrayVk::updatePackedInputInfo(uint32_t attribIndex,
                                          const gl::VertexBinding &binding,
                                          const gl::VertexAttribute &attrib)
{
    vk::PackedVertexInputBindingDesc &bindingDesc = mPackedInputBindings[attribIndex];

    size_t attribSize = gl::ComputeVertexAttributeTypeSize(attrib);
    ASSERT(attribSize <= std::numeric_limits<uint16_t>::max());

    bindingDesc.stride    = static_cast<uint16_t>(binding.getStride());
    bindingDesc.inputRate = static_cast<uint16_t>(
        binding.getDivisor() > 0 ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX);

    gl::VertexFormatType vertexFormatType = gl::GetVertexFormatType(attrib);
    VkFormat vkFormat                     = vk::GetNativeVertexFormat(vertexFormatType);
    ASSERT(vkFormat <= std::numeric_limits<uint16_t>::max());

    vk::PackedVertexInputAttributeDesc &attribDesc = mPackedInputAttributes[attribIndex];
    attribDesc.format                              = static_cast<uint16_t>(vkFormat);
    attribDesc.location                            = static_cast<uint16_t>(attribIndex);
    attribDesc.offset = static_cast<uint32_t>(ComputeVertexAttributeOffset(attrib, binding));
}

}  // namespace rx
