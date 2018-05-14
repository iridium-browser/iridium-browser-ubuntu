//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RenderbufferVk.cpp:
//    Implements the class methods for RenderbufferVk.
//

#include "libANGLE/renderer/vulkan/RenderbufferVk.h"

#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"

namespace rx
{

RenderbufferVk::RenderbufferVk(const gl::RenderbufferState &state)
    : RenderbufferImpl(state), mRequiredSize(0)
{
}

RenderbufferVk::~RenderbufferVk()
{
}

gl::Error RenderbufferVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    renderer->releaseResource(*this, &mImage);
    renderer->releaseResource(*this, &mDeviceMemory);
    renderer->releaseResource(*this, &mImageView);
    return gl::NoError();
}

gl::Error RenderbufferVk::setStorage(const gl::Context *context,
                                     GLenum internalformat,
                                     size_t width,
                                     size_t height)
{
    ContextVk *contextVk       = vk::GetImpl(context);
    RendererVk *renderer       = contextVk->getRenderer();
    const vk::Format &vkFormat = renderer->getFormat(internalformat);
    VkDevice device            = renderer->getDevice();

    VkImageUsageFlags usage =
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
         VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

    VkImageCreateInfo imageInfo;
    imageInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext                 = nullptr;
    imageInfo.flags                 = 0;
    imageInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageInfo.format                = vkFormat.vkTextureFormat;
    imageInfo.extent.width          = static_cast<uint32_t>(width);
    imageInfo.extent.height         = static_cast<uint32_t>(height);
    imageInfo.extent.depth          = 1;
    imageInfo.mipLevels             = 1;
    imageInfo.arrayLayers           = 1;
    imageInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage                 = usage;
    imageInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 0;
    imageInfo.pQueueFamilyIndices   = nullptr;
    imageInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    ANGLE_TRY(mImage.init(device, imageInfo));

    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ANGLE_TRY(vk::AllocateImageMemory(renderer, flags, &mImage, &mDeviceMemory, &mRequiredSize));

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    // Allocate ImageView.
    VkImageViewCreateInfo viewInfo;
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext                           = nullptr;
    viewInfo.flags                           = 0;
    viewInfo.image                           = mImage.getHandle();
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = vkFormat.vkTextureFormat;
    viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    viewInfo.subresourceRange.aspectMask     = aspect;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    ANGLE_TRY(mImageView.init(device, viewInfo));

    // Init RenderTarget.
    mRenderTarget.extents.width  = static_cast<int>(width);
    mRenderTarget.extents.height = static_cast<int>(height);
    mRenderTarget.extents.depth  = 1;
    mRenderTarget.format         = &vkFormat;
    mRenderTarget.image          = &mImage;
    mRenderTarget.imageView      = &mImageView;
    mRenderTarget.resource       = this;
    mRenderTarget.samples        = VK_SAMPLE_COUNT_1_BIT;  // TODO(jmadill): Multisample bits.

    return gl::NoError();
}

gl::Error RenderbufferVk::setStorageMultisample(const gl::Context *context,
                                                size_t samples,
                                                GLenum internalformat,
                                                size_t width,
                                                size_t height)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::setStorageEGLImageTarget(const gl::Context *context, egl::Image *image)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

gl::Error RenderbufferVk::getAttachmentRenderTarget(const gl::Context * /*context*/,
                                                    GLenum /*binding*/,
                                                    const gl::ImageIndex & /*imageIndex*/,
                                                    FramebufferAttachmentRenderTarget **rtOut)
{
    ASSERT(mImage.valid());
    *rtOut = &mRenderTarget;
    return gl::NoError();
}

gl::Error RenderbufferVk::initializeContents(const gl::Context *context,
                                             const gl::ImageIndex &imageIndex)
{
    UNIMPLEMENTED();
    return gl::NoError();
}

}  // namespace rx
