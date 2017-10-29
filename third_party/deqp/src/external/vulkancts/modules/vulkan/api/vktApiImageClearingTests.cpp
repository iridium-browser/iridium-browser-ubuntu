/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan Image Clearing Tests
 *//*--------------------------------------------------------------------*/

#include "vktApiImageClearingTests.hpp"

#include "deRandom.hpp"
#include "deMath.h"
#include "deStringUtil.hpp"
#include "deUniquePtr.hpp"
#include "deArrayUtil.hpp"
#include "deInt32.h"
#include "vkImageUtil.hpp"
#include "vkMemUtil.hpp"
#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"
#include "tcuImageCompare.hpp"
#include "tcuTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuVectorType.hpp"
#include "tcuTexture.hpp"
#include "tcuFloat.hpp"
#include "tcuTestLog.hpp"
#include "tcuVectorUtil.hpp"
#include <sstream>
#include <numeric>

namespace vkt
{

namespace api
{

using namespace vk;
using namespace tcu;

namespace
{

VkExtent3D getMipLevelExtent (VkExtent3D baseExtent, const deUint32 mipLevel)
{
	baseExtent.width	= std::max(baseExtent.width  >> mipLevel, 1u);
	baseExtent.height	= std::max(baseExtent.height >> mipLevel, 1u);
	baseExtent.depth	= std::max(baseExtent.depth  >> mipLevel, 1u);
	return baseExtent;
}

deUint32 getNumMipLevels (const VkExtent3D& baseExtent, const deUint32 maxMipLevels)
{
	const deUint32 widestEdge = std::max(std::max(baseExtent.width, baseExtent.height), baseExtent.depth);
	return std::min(static_cast<deUint32>(deFloatLog2(static_cast<float>(widestEdge))) + 1u, maxMipLevels);
}

std::vector<deUint32> getImageMipLevelSizes (const deUint32 pixelSize, const VkExtent3D& baseExtent, const deUint32 numMipLevels, const deUint32 perLevelAlignment = 1u)
{
	std::vector<deUint32> results(numMipLevels);

	for (deUint32 mipLevel = 0; mipLevel < numMipLevels; ++mipLevel)
	{
		const VkExtent3D extent = getMipLevelExtent(baseExtent, mipLevel);
		results[mipLevel] = static_cast<deUint32>(deAlignSize(extent.width * extent.height * extent.depth * pixelSize, perLevelAlignment));
	}

	return results;
}

//! Check if a point lies in a cross-like area.
inline bool isInClearRange (const UVec4& clearCoords, const deUint32 x, const deUint32 y)
{
	return !((x <  clearCoords[0] && y <  clearCoords[1]) ||
			 (x <  clearCoords[0] && y >= clearCoords[3]) ||
			 (x >= clearCoords[2] && y <  clearCoords[1]) ||
			 (x >= clearCoords[2] && y >= clearCoords[3]));
}

// This method is copied from the vktRenderPassTests.cpp. It should be moved to a common place.
int calcFloatDiff (float a, float b)
{
	const int			asign	= Float32(a).sign();
	const int			bsign	= Float32(a).sign();

	const deUint32		avalue	= (Float32(a).bits() & ((0x1u << 31u) - 1u));
	const deUint32		bvalue	= (Float32(b).bits() & ((0x1u << 31u) - 1u));

	if (asign != bsign)
		return avalue + bvalue + 1u;
	else if (avalue < bvalue)
		return bvalue - avalue;
	else
		return avalue - bvalue;
}

// This method is copied from the vktRenderPassTests.cpp and extended with the stringResult parameter.
bool comparePixelToDepthClearValue (const ConstPixelBufferAccess&	access,
									int								x,
									int								y,
									float							ref,
									std::string&					stringResult)
{
	const TextureFormat			format			= getEffectiveDepthStencilTextureFormat(access.getFormat(), Sampler::MODE_DEPTH);
	const TextureChannelClass	channelClass	= getTextureChannelClass(format.type);

	switch (channelClass)
	{
		case TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		{
			const int	bitDepth	= getTextureFormatBitDepth(format).x();
			const float	depth		= access.getPixDepth(x, y);
			const float	threshold	= 2.0f / (float)((1 << bitDepth) - 1);
			const bool	result		= deFloatAbs(depth - ref) <= threshold;

			if (!result)
			{
				std::stringstream s;
				s << "Ref:" << ref << " Threshold:" << threshold << " Depth:" << depth;
				stringResult	= s.str();
			}

			return result;
		}

		case TEXTURECHANNELCLASS_FLOATING_POINT:
		{
			const float	depth			= access.getPixDepth(x, y);
			const int	mantissaBits	= getTextureFormatMantissaBitDepth(format).x();
			const int	threshold		= 10 * 1 << (23 - mantissaBits);

			DE_ASSERT(mantissaBits <= 23);

			const bool	result			= calcFloatDiff(depth, ref) <= threshold;

			if (!result)
			{
				float				floatThreshold	= Float32((deUint32)threshold).asFloat();
				std::stringstream	s;

				s << "Ref:" << ref << " Threshold:" << floatThreshold << " Depth:" << depth;
				stringResult	= s.str();
			}

			return result;
		}

		default:
			DE_FATAL("Invalid channel class");
			return false;
	}
}

// This method is copied from the vktRenderPassTests.cpp and extended with the stringResult parameter.
bool comparePixelToStencilClearValue (const ConstPixelBufferAccess&	access,
									  int							x,
									  int							y,
									  deUint32						ref,
									  std::string&					stringResult)
{
	const deUint32	stencil	= access.getPixStencil(x, y);
	const bool		result	= stencil == ref;

	if (!result)
	{
		std::stringstream s;
		s << "Ref:" << ref << " Threshold:0" << " Stencil:" << stencil;
		stringResult	= s.str();
	}

	return result;
}

// This method is copied from the vktRenderPassTests.cpp and extended with the stringResult parameter.
bool comparePixelToColorClearValue (const ConstPixelBufferAccess&	access,
									int								x,
									int								y,
									int								z,
									const VkClearColorValue&		ref,
									std::string&					stringResult)
{
	const TextureFormat				format			= access.getFormat();
	const TextureChannelClass		channelClass	= getTextureChannelClass(format.type);
	const BVec4						channelMask		= getTextureFormatChannelMask(format);

	switch (channelClass)
	{
		case TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
		case TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
		{
			const IVec4	bitDepth	(getTextureFormatBitDepth(format));
			const Vec4	resColor	(access.getPixel(x, y, z));
			Vec4		refColor	(ref.float32[0],
									 ref.float32[1],
									 ref.float32[2],
									 ref.float32[3]);
			const int	modifier	= (channelClass == TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT) ? 0 : 1;
			const Vec4	threshold	(bitDepth[0] > 0 ? 1.0f / ((float)(1 << (bitDepth[0] - modifier)) - 1.0f) : 1.0f,
									 bitDepth[1] > 0 ? 1.0f / ((float)(1 << (bitDepth[1] - modifier)) - 1.0f) : 1.0f,
									 bitDepth[2] > 0 ? 1.0f / ((float)(1 << (bitDepth[2] - modifier)) - 1.0f) : 1.0f,
									 bitDepth[3] > 0 ? 1.0f / ((float)(1 << (bitDepth[3] - modifier)) - 1.0f) : 1.0f);

			if (isSRGB(access.getFormat()))
				refColor	= linearToSRGB(refColor);

			const bool	result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

			if (!result)
			{
				std::stringstream s;
				s << "Ref:" << refColor << " Mask:" << channelMask << " Threshold:" << threshold << " Color:" << resColor;
				stringResult	= s.str();
			}

			return result;
		}

		case TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
		{
			const UVec4	resColor	(access.getPixelUint(x, y, z));
			const UVec4	refColor	(ref.uint32[0],
									 ref.uint32[1],
									 ref.uint32[2],
									 ref.uint32[3]);
			const UVec4	threshold	(1);

			const bool	result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

			if (!result)
			{
				std::stringstream s;
				s << "Ref:" << refColor << " Mask:" << channelMask << " Threshold:" << threshold << " Color:" << resColor;
				stringResult	= s.str();
			}

			return result;
		}

		case TEXTURECHANNELCLASS_SIGNED_INTEGER:
		{
			const IVec4	resColor	(access.getPixelInt(x, y, z));
			const IVec4	refColor	(ref.int32[0],
									 ref.int32[1],
									 ref.int32[2],
									 ref.int32[3]);
			const IVec4	threshold	(1);

			const bool	result		= !(anyNotEqual(logicalAnd(lessThanEqual(absDiff(resColor, refColor), threshold), channelMask), channelMask));

			if (!result)
			{
				std::stringstream s;
				s << "Ref:" << refColor << " Mask:" << channelMask << " Threshold:" << threshold << " Color:" << resColor;
				stringResult	= s.str();
			}

			return result;
		}

		case TEXTURECHANNELCLASS_FLOATING_POINT:
		{
			const Vec4	resColor		(access.getPixel(x, y, z));
			const Vec4	refColor		(ref.float32[0],
										 ref.float32[1],
										 ref.float32[2],
										 ref.float32[3]);
			const IVec4	mantissaBits	(getTextureFormatMantissaBitDepth(format));
			const IVec4	threshold		(10 * IVec4(1) << (23 - mantissaBits));

			DE_ASSERT(allEqual(greaterThanEqual(threshold, IVec4(0)), BVec4(true)));

			for (int ndx = 0; ndx < 4; ndx++)
			{
				const bool result	= !(calcFloatDiff(resColor[ndx], refColor[ndx]) > threshold[ndx] && channelMask[ndx]);

				if (!result)
				{
					float				floatThreshold	= Float32((deUint32)(threshold)[0]).asFloat();
					Vec4				thresholdVec4	(floatThreshold,
														 floatThreshold,
														 floatThreshold,
														 floatThreshold);
					std::stringstream	s;
					s << "Ref:" << refColor << " Mask:" << channelMask << " Threshold:" << thresholdVec4 << " Color:" << resColor;
					stringResult	= s.str();

					return false;
				}
			}

			return true;
		}

		default:
			DE_FATAL("Invalid channel class");
			return false;
	}
}

struct TestParams
{
	bool			useSingleMipLevel;	//!< only mip level 0, otherwise up to maxMipLevels
	VkImageType		imageType;
	VkFormat		imageFormat;
	VkExtent3D		imageExtent;
	VkClearValue	initValue;
	VkClearValue	clearValue[2];		//!< the second value is used with more than one mip map
};

class ImageClearingTestInstance : public vkt::TestInstance
{
public:
										ImageClearingTestInstance		(Context&			context,
																		 const TestParams&	testParams);

	Move<VkCommandPool>					createCommandPool				(VkCommandPoolCreateFlags commandPoolCreateFlags) const;
	Move<VkCommandBuffer>				allocatePrimaryCommandBuffer	(VkCommandPool commandPool) const;
	Move<VkImage>						createImage						(VkImageType imageType, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage) const;
	Move<VkImageView>					createImageView					(VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectMask) const;
	Move<VkRenderPass>					createRenderPass				(VkFormat format) const;
	Move<VkFramebuffer>					createFrameBuffer				(VkImageView imageView, VkRenderPass renderPass, deUint32 imageWidth, deUint32 imageHeight) const;

	void								beginCommandBuffer				(VkCommandBufferUsageFlags usageFlags) const;
	void								endCommandBuffer				(void) const;
	void								submitCommandBuffer				(void) const;
	void								beginRenderPass					(VkSubpassContents content, VkClearValue clearValue) const;

	void								pipelineImageBarrier			(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout) const;
	de::MovePtr<TextureLevelPyramid>	readImage						(VkImageAspectFlags aspectMask) const;
	tcu::TestStatus						verifyResultImage				(const std::string& successMessage, const UVec4& clearCoords = UVec4()) const;

protected:
	VkImageViewType						getCorrespondingImageViewType	(VkImageType imageType) const;
	VkImageUsageFlags					getImageUsageFlags				(VkFormat format) const;
	VkImageAspectFlags					getImageAspectFlags				(VkFormat format) const;
	bool								getIsAttachmentFormat			(VkFormat format) const;
	bool								getIsStencilFormat				(VkFormat format) const;
	bool								getIsDepthFormat				(VkFormat format) const;
	VkImageFormatProperties				getImageFormatProperties		(void) const;
	de::MovePtr<Allocation>				allocateAndBindImageMemory		(VkImage image) const;

	const TestParams&					m_params;
	const VkDevice						m_device;
	const InstanceInterface&			m_vki;
	const DeviceInterface&				m_vkd;
	const VkQueue						m_queue;
	const deUint32						m_queueFamilyIndex;
	Allocator&							m_allocator;

	const bool							m_isAttachmentFormat;
	const VkImageUsageFlags				m_imageUsageFlags;
	const VkImageAspectFlags			m_imageAspectFlags;
	const VkImageFormatProperties		m_imageFormatProperties;
	const deUint32						m_imageMipLevels;
	const deUint32						m_thresholdMipLevel;

	Unique<VkCommandPool>				m_commandPool;
	Unique<VkCommandBuffer>				m_commandBuffer;

	Unique<VkImage>						m_image;
	de::MovePtr<Allocation>				m_imageMemory;
	Unique<VkImageView>					m_imageView;
	Unique<VkRenderPass>				m_renderPass;
	Unique<VkFramebuffer>				m_frameBuffer;
};

ImageClearingTestInstance::ImageClearingTestInstance (Context& context, const TestParams& params)
	: TestInstance				(context)
	, m_params					(params)
	, m_device					(context.getDevice())
	, m_vki						(context.getInstanceInterface())
	, m_vkd						(context.getDeviceInterface())
	, m_queue					(context.getUniversalQueue())
	, m_queueFamilyIndex		(context.getUniversalQueueFamilyIndex())
	, m_allocator				(context.getDefaultAllocator())
	, m_isAttachmentFormat		(getIsAttachmentFormat(params.imageFormat))
	, m_imageUsageFlags			(getImageUsageFlags(params.imageFormat))
	, m_imageAspectFlags		(getImageAspectFlags(params.imageFormat))
	, m_imageFormatProperties	(getImageFormatProperties())
	, m_imageMipLevels			(params.useSingleMipLevel ? 1u : getNumMipLevels(params.imageExtent, m_imageFormatProperties.maxMipLevels))
	, m_thresholdMipLevel		(std::max(m_imageMipLevels / 2u, 1u))
	, m_commandPool				(createCommandPool(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT))
	, m_commandBuffer			(allocatePrimaryCommandBuffer(*m_commandPool))

	, m_image					(createImage(params.imageType,
											 params.imageFormat,
											 params.imageExtent,
											 m_imageUsageFlags))

	, m_imageMemory				(allocateAndBindImageMemory(*m_image))
	, m_imageView				(m_isAttachmentFormat ? createImageView(*m_image,
												 getCorrespondingImageViewType(params.imageType),
												 params.imageFormat,
												 m_imageAspectFlags) : vk::Move<VkImageView>())

	, m_renderPass				(m_isAttachmentFormat ? createRenderPass(params.imageFormat) : vk::Move<vk::VkRenderPass>())
	, m_frameBuffer				(m_isAttachmentFormat ? createFrameBuffer(*m_imageView, *m_renderPass, params.imageExtent.width, params.imageExtent.height) : vk::Move<vk::VkFramebuffer>())
{
}

VkImageViewType ImageClearingTestInstance::getCorrespondingImageViewType (VkImageType imageType) const
{
	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			return VK_IMAGE_VIEW_TYPE_1D;
		case VK_IMAGE_TYPE_2D:
			return VK_IMAGE_VIEW_TYPE_2D;
		case VK_IMAGE_TYPE_3D:
			return VK_IMAGE_VIEW_TYPE_3D;
		default:
			DE_FATAL("Unknown image type!");
	}

	return VK_IMAGE_VIEW_TYPE_2D;
}

VkImageUsageFlags ImageClearingTestInstance::getImageUsageFlags (VkFormat format) const
{
	VkImageUsageFlags	commonFlags	= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (m_isAttachmentFormat)
	{
		if (isDepthStencilFormat(format))
			return commonFlags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		return commonFlags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	return commonFlags;
}

VkImageAspectFlags ImageClearingTestInstance::getImageAspectFlags (VkFormat format) const
{
	VkImageAspectFlags	imageAspectFlags	= 0;

	if (getIsDepthFormat(format))
		imageAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;

	if (getIsStencilFormat(format))
		imageAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	if (imageAspectFlags == 0)
		imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

	return imageAspectFlags;
}

bool ImageClearingTestInstance::getIsAttachmentFormat (VkFormat format) const
{
	const VkFormatProperties props	= vk::getPhysicalDeviceFormatProperties(m_vki, m_context.getPhysicalDevice(), format);

	return (props.optimalTilingFeatures & (vk::VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | vk::VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
}

bool ImageClearingTestInstance::getIsStencilFormat (VkFormat format) const
{
	const TextureFormat tcuFormat	= mapVkFormat(format);

	if (tcuFormat.order == TextureFormat::S || tcuFormat.order == TextureFormat::DS)
		return true;

	return false;
}

bool ImageClearingTestInstance::getIsDepthFormat (VkFormat format) const
{
	const TextureFormat	tcuFormat	= mapVkFormat(format);

	if (tcuFormat.order == TextureFormat::D || tcuFormat.order == TextureFormat::DS)
		return true;

	return false;
}

VkImageFormatProperties ImageClearingTestInstance::getImageFormatProperties (void) const
{
	VkImageFormatProperties properties;
	const VkResult result = m_vki.getPhysicalDeviceImageFormatProperties(m_context.getPhysicalDevice(), m_params.imageFormat, m_params.imageType,
																		 VK_IMAGE_TILING_OPTIMAL, m_imageUsageFlags, (VkImageCreateFlags)0, &properties);

	if (result == VK_ERROR_FORMAT_NOT_SUPPORTED)
		TCU_THROW(NotSupportedError, "Format not supported");
	else
		return properties;
}

de::MovePtr<Allocation> ImageClearingTestInstance::allocateAndBindImageMemory (VkImage image) const
{
	de::MovePtr<Allocation>	imageMemory	(m_allocator.allocate(getImageMemoryRequirements(m_vkd, m_device, image), MemoryRequirement::Any));
	VK_CHECK(m_vkd.bindImageMemory(m_device, image, imageMemory->getMemory(), imageMemory->getOffset()));
	return imageMemory;
}

Move<VkCommandPool> ImageClearingTestInstance::createCommandPool (VkCommandPoolCreateFlags commandPoolCreateFlags) const
{
	const VkCommandPoolCreateInfo			cmdPoolCreateInfo		=
	{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,					// VkStructureType             sType;
		DE_NULL,													// const void*                 pNext;
		commandPoolCreateFlags,										// VkCommandPoolCreateFlags    flags;
		m_queueFamilyIndex											// deUint32                    queueFamilyIndex;
	};

	return vk::createCommandPool(m_vkd, m_device, &cmdPoolCreateInfo, DE_NULL);
}

Move<VkCommandBuffer> ImageClearingTestInstance::allocatePrimaryCommandBuffer (VkCommandPool commandPool) const
{
	const VkCommandBufferAllocateInfo		cmdBufferAllocateInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,				// VkStructureType             sType;
		DE_NULL,													// const void*                 pNext;
		commandPool,												// VkCommandPool               commandPool;
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,							// VkCommandBufferLevel        level;
		1															// deUint32                    commandBufferCount;
	};

	return vk::allocateCommandBuffer(m_vkd, m_device, &cmdBufferAllocateInfo);
}

Move<VkImage> ImageClearingTestInstance::createImage (VkImageType imageType, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage) const
{
	const VkImageCreateInfo					imageCreateInfo			=
	{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkImageCreateFlags		flags;
		imageType,									// VkImageType				imageType;
		format,										// VkFormat					format;
		extent,										// VkExtent3D				extent;
		m_imageMipLevels,							// deUint32					mipLevels;
		1u,											// deUint32					arrayLayers;
		VK_SAMPLE_COUNT_1_BIT,						// VkSampleCountFlagBits	samples;
		VK_IMAGE_TILING_OPTIMAL,					// VkImageTiling			tiling;
		usage,										// VkImageUsageFlags		usage;
		VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode			sharingMode;
		1u,											// deUint32					queueFamilyIndexCount;
		&m_queueFamilyIndex,						// const deUint32*			pQueueFamilyIndices;
		VK_IMAGE_LAYOUT_UNDEFINED					// VkImageLayout			initialLayout;
	};

	return vk::createImage(m_vkd, m_device, &imageCreateInfo, DE_NULL);
}

Move<VkImageView> ImageClearingTestInstance::createImageView (VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspectMask) const
{
	const VkImageViewCreateInfo				imageViewCreateInfo		=
	{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType				sType;
		DE_NULL,									// const void*					pNext;
		0u,											// VkImageViewCreateFlags		flags;
		image,										// VkImage						image;
		viewType,									// VkImageViewType				viewType;
		format,										// VkFormat						format;
		{
			VK_COMPONENT_SWIZZLE_IDENTITY,				// VkComponentSwizzle			r;
			VK_COMPONENT_SWIZZLE_IDENTITY,				// VkComponentSwizzle			g;
			VK_COMPONENT_SWIZZLE_IDENTITY,				// VkComponentSwizzle			b;
			VK_COMPONENT_SWIZZLE_IDENTITY,				// VkComponentSwizzle			a;
		},											// VkComponentMapping			components;
		{
			aspectMask,									// VkImageAspectFlags			aspectMask;
			0u,											// deUint32						baseMipLevel;
			1u,											// deUint32						mipLevels;
			0u,											// deUint32						baseArrayLayer;
			1u,											// deUint32						arraySize;
		},											// VkImageSubresourceRange		subresourceRange;
	};

	return vk::createImageView(m_vkd, m_device, &imageViewCreateInfo, DE_NULL);
}

Move<VkRenderPass> ImageClearingTestInstance::createRenderPass (VkFormat format) const
{
	VkImageLayout							imageLayout;

	if (isDepthStencilFormat(format))
		imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	else
		imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	const VkAttachmentDescription			attachmentDesc			=
	{
		0u,													// VkAttachmentDescriptionFlags		flags;
		format,												// VkFormat							format;
		VK_SAMPLE_COUNT_1_BIT,								// VkSampleCountFlagBits			samples;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				loadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				storeOp;
		VK_ATTACHMENT_LOAD_OP_CLEAR,						// VkAttachmentLoadOp				stencilLoadOp;
		VK_ATTACHMENT_STORE_OP_STORE,						// VkAttachmentStoreOp				stencilStoreOp;
		imageLayout,										// VkImageLayout					initialLayout;
		imageLayout,										// VkImageLayout					finalLayout;
	};

	const VkAttachmentDescription			attachments[1]			=
	{
		attachmentDesc
	};

	const VkAttachmentReference				attachmentRef			=
	{
		0u,													// deUint32							attachment;
		imageLayout,										// VkImageLayout					layout;
	};

	const VkAttachmentReference*			pColorAttachments		= DE_NULL;
	const VkAttachmentReference*			pDepthStencilAttachment	= DE_NULL;
	deUint32								colorAttachmentCount	= 1;

	if (isDepthStencilFormat(format))
	{
		colorAttachmentCount	= 0;
		pDepthStencilAttachment	= &attachmentRef;
	}
	else
	{
		colorAttachmentCount	= 1;
		pColorAttachments		= &attachmentRef;
	}

	const VkSubpassDescription				subpassDesc[1]			=
	{
		{
			0u,												// VkSubpassDescriptionFlags		flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,				// VkPipelineBindPoint				pipelineBindPoint;
			0u,												// deUint32							inputAttachmentCount;
			DE_NULL,										// const VkAttachmentReference*		pInputAttachments;
			colorAttachmentCount,							// deUint32							colorAttachmentCount;
			pColorAttachments,								// const VkAttachmentReference*		pColorAttachments;
			DE_NULL,										// const VkAttachmentReference*		pResolveAttachments;
			pDepthStencilAttachment,						// const VkAttachmentReference*		pDepthStencilAttachment;
			0u,												// deUint32							preserveAttachmentCount;
			DE_NULL,										// const VkAttachmentReference*		pPreserveAttachments;
		}
	};

	const VkRenderPassCreateInfo			renderPassCreateInfo	=
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
		DE_NULL,											// const void*						pNext;
		0u,													// VkRenderPassCreateFlags			flags;
		1u,													// deUint32							attachmentCount;
		attachments,										// const VkAttachmentDescription*	pAttachments;
		1u,													// deUint32							subpassCount;
		subpassDesc,										// const VkSubpassDescription*		pSubpasses;
		0u,													// deUint32							dependencyCount;
		DE_NULL,											// const VkSubpassDependency*		pDependencies;
	};

	return vk::createRenderPass(m_vkd, m_device, &renderPassCreateInfo, DE_NULL);
}

Move<VkFramebuffer> ImageClearingTestInstance::createFrameBuffer (VkImageView imageView, VkRenderPass renderPass, deUint32 imageWidth, deUint32 imageHeight) const
{
	const VkImageView						attachmentViews[1]		=
	{
		imageView
	};

	const VkFramebufferCreateInfo			framebufferCreateInfo	=
	{
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,	// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		0u,											// VkFramebufferCreateFlags	flags;
		renderPass,									// VkRenderPass				renderPass;
		1,											// deUint32					attachmentCount;
		attachmentViews,							// const VkImageView*		pAttachments;
		imageWidth,									// deUint32					width;
		imageHeight,								// deUint32					height;
		1u,											// deUint32					layers;
	};

	return createFramebuffer(m_vkd, m_device, &framebufferCreateInfo, DE_NULL);
}

void ImageClearingTestInstance::beginCommandBuffer (VkCommandBufferUsageFlags usageFlags) const
{
	const VkCommandBufferBeginInfo			commandBufferBeginInfo	=
	{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,			// VkStructureType                          sType;
		DE_NULL,												// const void*                              pNext;
		usageFlags,												// VkCommandBufferUsageFlags                flags;
		DE_NULL													// const VkCommandBufferInheritanceInfo*    pInheritanceInfo;
	};

	VK_CHECK(m_vkd.beginCommandBuffer(*m_commandBuffer, &commandBufferBeginInfo));
}

void ImageClearingTestInstance::endCommandBuffer (void) const
{
	VK_CHECK(m_vkd.endCommandBuffer(*m_commandBuffer));
}

void ImageClearingTestInstance::submitCommandBuffer (void) const
{
	const VkFenceCreateInfo fenceCreateInfo							=
	{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,					// VkStructureType       sType;
		DE_NULL,												// const void*           pNext;
		0u														// VkFenceCreateFlags    flags;
	};

	const Unique<VkFence>					fence					(createFence(m_vkd, m_device, &fenceCreateInfo));

	const VkSubmitInfo						submitInfo				=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,							// VkStructureType                sType;
		DE_NULL,												// const void*                    pNext;
		0u,														// deUint32                       waitSemaphoreCount;
		DE_NULL,												// const VkSemaphore*             pWaitSemaphores;
		DE_NULL,												// const VkPipelineStageFlags*    pWaitDstStageMask;
		1u,														// deUint32                       commandBufferCount;
		&(*m_commandBuffer),									// const VkCommandBuffer*         pCommandBuffers;
		0u,														// deUint32                       signalSemaphoreCount;
		DE_NULL													// const VkSemaphore*             pSignalSemaphores;
	};

	VK_CHECK(m_vkd.queueSubmit(m_queue, 1, &submitInfo, *fence));

	VK_CHECK(m_vkd.waitForFences(m_device, 1, &fence.get(), VK_TRUE, ~0ull));
}

void ImageClearingTestInstance::pipelineImageBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout) const
{
	const VkImageMemoryBarrier		imageBarrier	=
	{
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,		// VkStructureType			sType;
		DE_NULL,									// const void*				pNext;
		srcAccessMask,								// VkAccessFlags			srcAccessMask;
		dstAccessMask,								// VkAccessFlags			dstAccessMask;
		oldLayout,									// VkImageLayout			oldLayout;
		newLayout,									// VkImageLayout			newLayout;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32					destQueueFamilyIndex;
		*m_image,									// VkImage					image;
		{
			m_imageAspectFlags,							// VkImageAspectFlags	aspectMask;
			0u,											// deUint32				baseMipLevel;
			VK_REMAINING_MIP_LEVELS,					// deUint32				levelCount;
			0u,											// deUint32				baseArrayLayer;
			1u,											// deUint32				layerCount;
		},											// VkImageSubresourceRange	subresourceRange;
	};

	m_vkd.cmdPipelineBarrier(*m_commandBuffer, srcStageMask, dstStageMask, 0, 0, DE_NULL, 0, DE_NULL, 1, &imageBarrier);
}

de::MovePtr<TextureLevelPyramid> ImageClearingTestInstance::readImage (VkImageAspectFlags aspectMask) const
{
	const TextureFormat					tcuFormat		= aspectMask == VK_IMAGE_ASPECT_COLOR_BIT ? mapVkFormat(m_params.imageFormat) :
														  aspectMask == VK_IMAGE_ASPECT_DEPTH_BIT ? getDepthCopyFormat(m_params.imageFormat) :
														  aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT ? getStencilCopyFormat(m_params.imageFormat) :
														  TextureFormat();
	const deUint32						pixelSize		= getPixelSize(tcuFormat);
	const deUint32						alignment		= 4;	// subsequent mip levels aligned to 4 bytes
	const std::vector<deUint32>			mipLevelSizes	= getImageMipLevelSizes(pixelSize, m_params.imageExtent, m_imageMipLevels, alignment);
	const VkDeviceSize					imageTotalSize	= std::accumulate(mipLevelSizes.begin(), mipLevelSizes.end(), 0u);

	de::MovePtr<TextureLevelPyramid>	result			(new TextureLevelPyramid(tcuFormat, m_imageMipLevels));
	Move<VkBuffer>						buffer;
	de::MovePtr<Allocation>				bufferAlloc;

	// Create destination buffer
	{
		const VkBufferCreateInfo	bufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			imageTotalSize,								// VkDeviceSize			size;
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			0u,											// deUint32				queueFamilyIndexCount;
			DE_NULL										// const deUint32*		pQueueFamilyIndices;
		};

		buffer		= createBuffer(m_vkd, m_device, &bufferParams);
		bufferAlloc	= m_allocator.allocate(getBufferMemoryRequirements(m_vkd, m_device, *buffer), MemoryRequirement::HostVisible);
		VK_CHECK(m_vkd.bindBufferMemory(m_device, *buffer, bufferAlloc->getMemory(), bufferAlloc->getOffset()));
	}

	// Barriers for copying image to buffer

	const VkBufferMemoryBarrier		bufferBarrier	=
	{
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,	// VkStructureType	sType;
		DE_NULL,									// const void*		pNext;
		VK_ACCESS_TRANSFER_WRITE_BIT,				// VkAccessFlags	srcAccessMask;
		VK_ACCESS_HOST_READ_BIT,					// VkAccessFlags	dstAccessMask;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			srcQueueFamilyIndex;
		VK_QUEUE_FAMILY_IGNORED,					// deUint32			dstQueueFamilyIndex;
		*buffer,									// VkBuffer			buffer;
		0u,											// VkDeviceSize		offset;
		imageTotalSize,								// VkDeviceSize		size;
	};

	// Copy image to buffer
	std::vector<VkBufferImageCopy> copyRegions;
	{
		deUint32 offset = 0u;
		for (deUint32 mipLevel = 0; mipLevel < m_imageMipLevels; ++mipLevel)
		{
			const VkExtent3D		extent	= getMipLevelExtent(m_params.imageExtent, mipLevel);
			const VkBufferImageCopy	region	=
			{
				offset,									// VkDeviceSize				bufferOffset;
				0u,										// deUint32					bufferRowLength;
				0u,										// deUint32					bufferImageHeight;
				{ aspectMask, mipLevel, 0u, 1u },		// VkImageSubresourceLayers	imageSubresource;
				{ 0, 0, 0 },							// VkOffset3D				imageOffset;
				extent									// VkExtent3D				imageExtent;
			};
			copyRegions.push_back(region);
			offset += mipLevelSizes[mipLevel];
		}
	}

	beginCommandBuffer(0);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
						 VK_ACCESS_TRANSFER_READ_BIT,
						 VK_IMAGE_LAYOUT_GENERAL,
						 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	m_vkd.cmdCopyImageToBuffer(*m_commandBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, *buffer, static_cast<deUint32>(copyRegions.size()), &copyRegions[0]);
	m_vkd.cmdPipelineBarrier(*m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, (VkDependencyFlags)0, 0, (const VkMemoryBarrier*)DE_NULL, 1, &bufferBarrier, 0, (const VkImageMemoryBarrier*)DE_NULL);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_ACCESS_TRANSFER_READ_BIT,
						 VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
						 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						 VK_IMAGE_LAYOUT_GENERAL);

	endCommandBuffer();
	submitCommandBuffer();

	invalidateMappedMemoryRange(m_vkd, m_device, bufferAlloc->getMemory(), bufferAlloc->getOffset(), imageTotalSize);

	{
		deUint32 offset = 0u;
		for (deUint32 mipLevel = 0; mipLevel < m_imageMipLevels; ++mipLevel)
		{
			const VkExtent3D	extent		= getMipLevelExtent(m_params.imageExtent, mipLevel);
			const void*			pLevelData	= static_cast<const void*>(reinterpret_cast<deUint8*>(bufferAlloc->getHostPtr()) + offset);

			result->allocLevel(mipLevel, extent.width, extent.height, extent.depth);
			copy(result->getLevel(mipLevel), ConstPixelBufferAccess(result->getFormat(), result->getLevel(mipLevel).getSize(), pLevelData));

			offset += mipLevelSizes[mipLevel];
		}
	}

	return result;
}

tcu::TestStatus ImageClearingTestInstance::verifyResultImage (const std::string& successMessage, const UVec4& clearCoords) const
{
	const bool useClearRange = clearCoords != UVec4();
	DE_ASSERT(!useClearRange || m_params.imageExtent.depth == 1u);

	if (getIsDepthFormat(m_params.imageFormat))
	{
		DE_ASSERT(m_imageMipLevels == 1u);

		de::MovePtr<TextureLevelPyramid>	image			= readImage(VK_IMAGE_ASPECT_DEPTH_BIT);
		std::string							message;
		float								depthValue;

		for (deUint32 y = 0; y < m_params.imageExtent.height; ++y)
		for (deUint32 x = 0; x < m_params.imageExtent.width; ++x)
		{
			if (!useClearRange || isInClearRange(clearCoords, x, y))
				depthValue = m_params.clearValue[0].depthStencil.depth;
			else
				depthValue = m_params.initValue.depthStencil.depth;

			if (!comparePixelToDepthClearValue(image->getLevel(0), x, y, depthValue, message))
				return TestStatus::fail("Depth value mismatch! " + message);
		}
	}

	if (getIsStencilFormat(m_params.imageFormat))
	{
		DE_ASSERT(m_imageMipLevels == 1u);

		de::MovePtr<TextureLevelPyramid>	image			= readImage(VK_IMAGE_ASPECT_STENCIL_BIT);
		std::string							message;
		deUint32							stencilValue;

		for (deUint32 y = 0; y < m_params.imageExtent.height; ++y)
		for (deUint32 x = 0; x < m_params.imageExtent.width; ++x)
		{
			if (!useClearRange || isInClearRange(clearCoords, x, y))
				stencilValue = m_params.clearValue[0].depthStencil.stencil;
			else
				stencilValue = m_params.initValue.depthStencil.stencil;

			if (!comparePixelToStencilClearValue(image->getLevel(0), x, y, stencilValue, message))
				return TestStatus::fail("Stencil value mismatch! " + message);
		}
	}

	if (!isDepthStencilFormat(m_params.imageFormat))
	{
		de::MovePtr<TextureLevelPyramid>	image			= readImage(VK_IMAGE_ASPECT_COLOR_BIT);
		std::string							message;
		const VkClearColorValue*			pColorValue;

		for (deUint32 mipLevel = 0; mipLevel < m_imageMipLevels; ++mipLevel)
		{
			const int			clearColorNdx	= (mipLevel < m_thresholdMipLevel ? 0 : 1);
			const VkExtent3D	extent			= getMipLevelExtent(m_params.imageExtent, mipLevel);

			for (deUint32 z = 0; z < extent.depth;  ++z)
			for (deUint32 y = 0; y < extent.height; ++y)
			for (deUint32 x = 0; x < extent.width;  ++x)
			{
				if (!useClearRange || isInClearRange(clearCoords, x, y))
					pColorValue = &m_params.clearValue[clearColorNdx].color;
				else
					pColorValue = &m_params.initValue.color;

				if (!comparePixelToColorClearValue(image->getLevel(mipLevel), x, y, z, *pColorValue, message))
					return TestStatus::fail("Color value mismatch! " + message);
			}
		}
	}

	return TestStatus::pass(successMessage);
}

void ImageClearingTestInstance::beginRenderPass (VkSubpassContents content, VkClearValue clearValue) const
{
	const VkRenderPassBeginInfo renderPassBeginInfo =
	{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
		DE_NULL,												// const void*			pNext;
		*m_renderPass,											// VkRenderPass			renderPass;
		*m_frameBuffer,											// VkFramebuffer		framebuffer;
		{
			{ 0, 0 },												// VkOffset2D			offset;
			{
				m_params.imageExtent.width,								// deUint32				width;
				m_params.imageExtent.height								// deUint32				height;
			}														// VkExtent2D			extent;
		},														// VkRect2D				renderArea;
		1u,														// deUint32				clearValueCount;
		&clearValue												// const VkClearValue*	pClearValues;
	};

	m_vkd.cmdBeginRenderPass(*m_commandBuffer, &renderPassBeginInfo, content);
}

class ClearColorImageTestInstance : public ImageClearingTestInstance
{
public:
				ClearColorImageTestInstance	(Context& context, const TestParams& testParams) : ImageClearingTestInstance (context, testParams) {}
	TestStatus	iterate						(void);
};

TestStatus ClearColorImageTestInstance::iterate (void)
{
	std::vector<VkImageSubresourceRange> subresourceRanges;

	if (m_imageMipLevels == 1)
		subresourceRanges.push_back(makeImageSubresourceRange(m_imageAspectFlags, 0u, 1u, 0u, 1u));
	else
	{
		subresourceRanges.push_back(makeImageSubresourceRange(m_imageAspectFlags,	0u,						m_thresholdMipLevel,		0u, 1u));
		subresourceRanges.push_back(makeImageSubresourceRange(m_imageAspectFlags,	m_thresholdMipLevel,	VK_REMAINING_MIP_LEVELS,	0u, 1u));
	}

	beginCommandBuffer(0);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags		srcStageMask
						 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,			// VkPipelineStageFlags		dstStageMask
						 0,												// VkAccessFlags			srcAccessMask
						 (m_isAttachmentFormat
							? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
							: VK_ACCESS_TRANSFER_WRITE_BIT),			// VkAccessFlags			dstAccessMask
						 VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
						 (m_isAttachmentFormat
							? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
							: VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));	// VkImageLayout			newLayout;

	if (m_isAttachmentFormat)
	{
		beginRenderPass(VK_SUBPASS_CONTENTS_INLINE, m_params.initValue);
		m_vkd.cmdEndRenderPass(*m_commandBuffer);

		pipelineImageBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,		// VkPipelineStageFlags		srcStageMask
			VK_PIPELINE_STAGE_TRANSFER_BIT,								// VkPipelineStageFlags		dstStageMask
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,						// VkAccessFlags			srcAccessMask
			VK_ACCESS_TRANSFER_WRITE_BIT,								// VkAccessFlags			dstAccessMask
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,					// VkImageLayout			oldLayout;
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);						// VkImageLayout			newLayout;
	}

	// Different clear color per range
	for (std::size_t i = 0u; i < subresourceRanges.size(); ++i)
		m_vkd.cmdClearColorImage(*m_commandBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_params.clearValue[i].color, 1, &subresourceRanges[i]);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags		srcStageMask
						 VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags		dstStageMask
						 VK_ACCESS_TRANSFER_WRITE_BIT,					// VkAccessFlags			srcAccessMask
						 VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags			dstAccessMask
						 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,			// VkImageLayout			oldLayout;
						 VK_IMAGE_LAYOUT_GENERAL);						// VkImageLayout			newLayout;

	endCommandBuffer();
	submitCommandBuffer();

	return verifyResultImage("cmdClearColorImage passed");
}

class ClearDepthStencilImageTestInstance : public ImageClearingTestInstance
{
public:
				ClearDepthStencilImageTestInstance	(Context& context, const TestParams& testParams) : ImageClearingTestInstance (context, testParams) {}
	TestStatus	iterate								(void);
};

TestStatus ClearDepthStencilImageTestInstance::iterate (void)
{
	const VkImageSubresourceRange subresourceRange = makeImageSubresourceRange(m_imageAspectFlags, 0u, 1u, 0u, 1u);

	beginCommandBuffer(0);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,					// VkPipelineStageFlags		srcStageMask
						 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,				// VkPipelineStageFlags		dstStageMask
						 0,													// VkAccessFlags			srcAccessMask
						 (m_isAttachmentFormat
							?	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
							:	VK_ACCESS_TRANSFER_WRITE_BIT),				// VkAccessFlags			dstAccessMask
						 VK_IMAGE_LAYOUT_UNDEFINED,							// VkImageLayout			oldLayout;
						 (m_isAttachmentFormat
							?	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
							:	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));		// VkImageLayout			newLayout;

	if (m_isAttachmentFormat)
	{
		beginRenderPass(VK_SUBPASS_CONTENTS_INLINE, m_params.initValue);
		m_vkd.cmdEndRenderPass(*m_commandBuffer);

		pipelineImageBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,					// VkPipelineStageFlags		srcStageMask
							 VK_PIPELINE_STAGE_TRANSFER_BIT,						// VkPipelineStageFlags		dstStageMask
							 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,			// VkAccessFlags			srcAccessMask
							 VK_ACCESS_TRANSFER_WRITE_BIT,							// VkAccessFlags			dstAccessMask
							 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,		// VkImageLayout			oldLayout;
							 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);					// VkImageLayout			newLayout;
	}

	m_vkd.cmdClearDepthStencilImage(*m_commandBuffer, *m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_params.clearValue[0].depthStencil, 1, &subresourceRange);

	pipelineImageBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags		srcStageMask
						 VK_PIPELINE_STAGE_TRANSFER_BIT,					// VkPipelineStageFlags		dstStageMask
						 VK_ACCESS_TRANSFER_WRITE_BIT,						// VkAccessFlags			srcAccessMask
						 VK_ACCESS_TRANSFER_READ_BIT,						// VkAccessFlags			dstAccessMask
						 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,				// VkImageLayout			oldLayout;
						 VK_IMAGE_LAYOUT_GENERAL);							// VkImageLayout			newLayout;

	endCommandBuffer();
	submitCommandBuffer();

	return verifyResultImage("cmdClearDepthStencilImage passed");
}

class ClearAttachmentTestInstance : public ImageClearingTestInstance
{
public:
	enum ClearType
	{
		FULL_CLEAR,
		PARTIAL_CLEAR,
	};

	ClearAttachmentTestInstance (Context& context, const TestParams& testParams, const ClearType clearType = FULL_CLEAR)
		: ImageClearingTestInstance	(context, testParams)
		, m_clearType				(clearType)
	{
		if (!m_isAttachmentFormat)
			TCU_THROW(NotSupportedError, "Format not renderable");
	}

	TestStatus iterate (void)
	{
		const VkClearAttachment clearAttachment =
		{
			m_imageAspectFlags,					// VkImageAspectFlags	aspectMask;
			0u,									// deUint32				colorAttachment;
			m_params.clearValue[0]				// VkClearValue			clearValue;
		};

		UVec4						clearCoords;
		std::vector<VkClearRect>	clearRects;

		if (m_clearType == FULL_CLEAR)
		{
			const VkClearRect rect =
			{
				{
					{ 0, 0 },																	// VkOffset2D    offset;
					{ m_params.imageExtent.width, m_params.imageExtent.height }					// VkExtent2D    extent;
				},																			// VkRect2D	rect;
				0u,																			// deUint32	baseArrayLayer;
				1u																			// deUint32	layerCount;
			};

			clearRects.push_back(rect);
		}
		else
		{
			const deUint32	clearX		= m_params.imageExtent.width  / 4u;
			const deUint32	clearY		= m_params.imageExtent.height / 4u;
			const deUint32	clearWidth	= m_params.imageExtent.width  / 2u;
			const deUint32	clearHeight	= m_params.imageExtent.height / 2u;

			clearCoords	= UVec4(clearX,					clearY,
								clearX + clearWidth,	clearY + clearHeight);

			const VkClearRect rects[2] =
			{
				{
					{
						{ 0,							static_cast<deInt32>(clearY)	},		// VkOffset2D    offset;
						{ m_params.imageExtent.width,	clearHeight						}		// VkExtent2D    extent;
					},																		// VkRect2D	rect;
					0u,																		// deUint32	baseArrayLayer;
					1u																		// deUint32	layerCount;
				},
				{
					{
						{ static_cast<deInt32>(clearX),	0							},			// VkOffset2D    offset;
						{ clearWidth,					m_params.imageExtent.height	}			// VkExtent2D    extent;
					},																		// VkRect2D	rect;
					0u,																		// deUint32	baseArrayLayer;
					1u																		// deUint32	layerCount;
				}
			};

			clearRects.push_back(rects[0]);
			clearRects.push_back(rects[1]);
		}

		const bool			isDepthStencil		= isDepthStencilFormat(m_params.imageFormat);
		const VkAccessFlags	accessMask			= (isDepthStencil ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT     : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
		const VkImageLayout	attachmentLayout	= (isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		beginCommandBuffer(0);

		pipelineImageBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,				// VkPipelineStageFlags		srcStageMask
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,			// VkPipelineStageFlags		dstStageMask
							 0,												// VkAccessFlags			srcAccessMask
							 accessMask,									// VkAccessFlags			dstAccessMask
							 VK_IMAGE_LAYOUT_UNDEFINED,						// VkImageLayout			oldLayout;
							 attachmentLayout);								// VkImageLayout			newLayout;

		beginRenderPass(VK_SUBPASS_CONTENTS_INLINE, m_params.initValue);
		m_vkd.cmdClearAttachments(*m_commandBuffer, 1, &clearAttachment, static_cast<deUint32>(clearRects.size()), &clearRects[0]);
		m_vkd.cmdEndRenderPass(*m_commandBuffer);

		pipelineImageBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,			// VkPipelineStageFlags		srcStageMask
							 VK_PIPELINE_STAGE_TRANSFER_BIT,				// VkPipelineStageFlags		dstStageMask
							 accessMask,									// VkAccessFlags			srcAccessMask
							 VK_ACCESS_TRANSFER_READ_BIT,					// VkAccessFlags			dstAccessMask
							 attachmentLayout,								// VkImageLayout			oldLayout;
							 VK_IMAGE_LAYOUT_GENERAL);						// VkImageLayout			newLayout;

		endCommandBuffer();
		submitCommandBuffer();

		return verifyResultImage("cmdClearAttachments passed", clearCoords);
	}

private:
	const ClearType	m_clearType;
};

class PartialClearAttachmentTestInstance : public ClearAttachmentTestInstance
{
public:
	PartialClearAttachmentTestInstance (Context& context, const TestParams& testParams) : ClearAttachmentTestInstance (context, testParams, PARTIAL_CLEAR) {}
};

VkClearValue makeClearColorValue (VkFormat format, float r, float g, float b, float a)
{
	const	TextureFormat tcuFormat	= mapVkFormat(format);
	VkClearValue clearValue;

	if (getTextureChannelClass(tcuFormat.type) == TEXTURECHANNELCLASS_FLOATING_POINT
		|| getTextureChannelClass(tcuFormat.type) == TEXTURECHANNELCLASS_SIGNED_FIXED_POINT
		|| getTextureChannelClass(tcuFormat.type) == TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
	{
		clearValue.color.float32[0] = r;
		clearValue.color.float32[1] = g;
		clearValue.color.float32[2] = b;
		clearValue.color.float32[3] = a;
	}
	else if (getTextureChannelClass(tcuFormat.type) == TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
	{
		UVec4 maxValues = getFormatMaxUintValue(tcuFormat);

		clearValue.color.uint32[0] = (deUint32)((float)maxValues[0] * r);
		clearValue.color.uint32[1] = (deUint32)((float)maxValues[1] * g);
		clearValue.color.uint32[2] = (deUint32)((float)maxValues[2] * b);
		clearValue.color.uint32[3] = (deUint32)((float)maxValues[3] * a);
	}
	else if (getTextureChannelClass(tcuFormat.type) == TEXTURECHANNELCLASS_SIGNED_INTEGER)
	{
		IVec4 maxValues = getFormatMaxIntValue(tcuFormat);

		clearValue.color.int32[0] = (deUint32)((float)maxValues[0] * r);
		clearValue.color.int32[1] = (deUint32)((float)maxValues[1] * g);
		clearValue.color.int32[2] = (deUint32)((float)maxValues[2] * b);
		clearValue.color.int32[3] = (deUint32)((float)maxValues[3] * a);
	}
	else
		DE_FATAL("Unknown channel class");

	return clearValue;
}

std::string getFormatCaseName (VkFormat format)
{
	return de::toLower(de::toString(getFormatStr(format)).substr(10));
}

const char* getImageTypeCaseName (VkImageType type)
{
	const char* s_names[] =
	{
		"1d",
		"2d",
		"3d"
	};
	return de::getSizedArrayElement<VK_IMAGE_TYPE_LAST>(s_names, type);
}

} // anonymous

TestCaseGroup* createImageClearingTests (TestContext& testCtx)
{
	// Main testgroup.
	de::MovePtr<TestCaseGroup>	imageClearingTests						(new TestCaseGroup(testCtx, "image_clearing", "Image Clearing Tests"));

	de::MovePtr<TestCaseGroup>	colorImageClearTests					(new TestCaseGroup(testCtx, "clear_color_image", "Color Image Clear Tests"));
	de::MovePtr<TestCaseGroup>	depthStencilImageClearTests				(new TestCaseGroup(testCtx, "clear_depth_stencil_image", "Color Depth/Stencil Image Tests"));
	de::MovePtr<TestCaseGroup>	colorAttachmentClearTests				(new TestCaseGroup(testCtx, "clear_color_attachment", "Color Color Attachment Tests"));
	de::MovePtr<TestCaseGroup>	depthStencilAttachmentClearTests		(new TestCaseGroup(testCtx, "clear_depth_stencil_attachment", "Color Depth/Stencil Attachment Tests"));
	de::MovePtr<TestCaseGroup>	partialColorAttachmentClearTests		(new TestCaseGroup(testCtx, "partial_clear_color_attachment", "Clear Partial Color Attachment Tests"));
	de::MovePtr<TestCaseGroup>	partialDepthStencilAttachmentClearTests	(new TestCaseGroup(testCtx, "partial_clear_depth_stencil_attachment", "Clear Partial Depth/Stencil Attachment Tests"));

	// Some formats are commented out due to the tcu::TextureFormat does not support them yet.
	const VkFormat		colorImageFormatsToTest[]	=
	{
		VK_FORMAT_R4G4_UNORM_PACK8,
		VK_FORMAT_R4G4B4A4_UNORM_PACK16,
		VK_FORMAT_B4G4R4A4_UNORM_PACK16,
		VK_FORMAT_R5G6B5_UNORM_PACK16,
		VK_FORMAT_B5G6R5_UNORM_PACK16,
		VK_FORMAT_R5G5B5A1_UNORM_PACK16,
		VK_FORMAT_B5G5R5A1_UNORM_PACK16,
		VK_FORMAT_A1R5G5B5_UNORM_PACK16,
		VK_FORMAT_R8_UNORM,
		VK_FORMAT_R8_SNORM,
		VK_FORMAT_R8_USCALED,
		VK_FORMAT_R8_SSCALED,
		VK_FORMAT_R8_UINT,
		VK_FORMAT_R8_SINT,
		VK_FORMAT_R8_SRGB,
		VK_FORMAT_R8G8_UNORM,
		VK_FORMAT_R8G8_SNORM,
		VK_FORMAT_R8G8_USCALED,
		VK_FORMAT_R8G8_SSCALED,
		VK_FORMAT_R8G8_UINT,
		VK_FORMAT_R8G8_SINT,
		VK_FORMAT_R8G8_SRGB,
		VK_FORMAT_R8G8B8_UNORM,
		VK_FORMAT_R8G8B8_SNORM,
		VK_FORMAT_R8G8B8_USCALED,
		VK_FORMAT_R8G8B8_SSCALED,
		VK_FORMAT_R8G8B8_UINT,
		VK_FORMAT_R8G8B8_SINT,
		VK_FORMAT_R8G8B8_SRGB,
		VK_FORMAT_B8G8R8_UNORM,
		VK_FORMAT_B8G8R8_SNORM,
		VK_FORMAT_B8G8R8_USCALED,
		VK_FORMAT_B8G8R8_SSCALED,
		VK_FORMAT_B8G8R8_UINT,
		VK_FORMAT_B8G8R8_SINT,
		VK_FORMAT_B8G8R8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SNORM,
		VK_FORMAT_R8G8B8A8_USCALED,
		VK_FORMAT_R8G8B8A8_SSCALED,
		VK_FORMAT_R8G8B8A8_UINT,
		VK_FORMAT_R8G8B8A8_SINT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_B8G8R8A8_SNORM,
		VK_FORMAT_B8G8R8A8_USCALED,
		VK_FORMAT_B8G8R8A8_SSCALED,
		VK_FORMAT_B8G8R8A8_UINT,
		VK_FORMAT_B8G8R8A8_SINT,
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_A8B8G8R8_UNORM_PACK32,
		VK_FORMAT_A8B8G8R8_SNORM_PACK32,
		VK_FORMAT_A8B8G8R8_USCALED_PACK32,
		VK_FORMAT_A8B8G8R8_SSCALED_PACK32,
		VK_FORMAT_A8B8G8R8_UINT_PACK32,
		VK_FORMAT_A8B8G8R8_SINT_PACK32,
		VK_FORMAT_A8B8G8R8_SRGB_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_SNORM_PACK32,
		VK_FORMAT_A2R10G10B10_USCALED_PACK32,
		VK_FORMAT_A2R10G10B10_SSCALED_PACK32,
		VK_FORMAT_A2R10G10B10_UINT_PACK32,
		VK_FORMAT_A2R10G10B10_SINT_PACK32,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2B10G10R10_SNORM_PACK32,
		VK_FORMAT_A2B10G10R10_USCALED_PACK32,
		VK_FORMAT_A2B10G10R10_SSCALED_PACK32,
		VK_FORMAT_A2B10G10R10_UINT_PACK32,
		VK_FORMAT_A2B10G10R10_SINT_PACK32,
		VK_FORMAT_R16_UNORM,
		VK_FORMAT_R16_SNORM,
		VK_FORMAT_R16_USCALED,
		VK_FORMAT_R16_SSCALED,
		VK_FORMAT_R16_UINT,
		VK_FORMAT_R16_SINT,
		VK_FORMAT_R16_SFLOAT,
		VK_FORMAT_R16G16_UNORM,
		VK_FORMAT_R16G16_SNORM,
		VK_FORMAT_R16G16_USCALED,
		VK_FORMAT_R16G16_SSCALED,
		VK_FORMAT_R16G16_UINT,
		VK_FORMAT_R16G16_SINT,
		VK_FORMAT_R16G16_SFLOAT,
		VK_FORMAT_R16G16B16_UNORM,
		VK_FORMAT_R16G16B16_SNORM,
		VK_FORMAT_R16G16B16_USCALED,
		VK_FORMAT_R16G16B16_SSCALED,
		VK_FORMAT_R16G16B16_UINT,
		VK_FORMAT_R16G16B16_SINT,
		VK_FORMAT_R16G16B16_SFLOAT,
		VK_FORMAT_R16G16B16A16_UNORM,
		VK_FORMAT_R16G16B16A16_SNORM,
		VK_FORMAT_R16G16B16A16_USCALED,
		VK_FORMAT_R16G16B16A16_SSCALED,
		VK_FORMAT_R16G16B16A16_UINT,
		VK_FORMAT_R16G16B16A16_SINT,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_FORMAT_R32_UINT,
		VK_FORMAT_R32_SINT,
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_UINT,
		VK_FORMAT_R32G32_SINT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_UINT,
		VK_FORMAT_R32G32B32_SINT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_UINT,
		VK_FORMAT_R32G32B32A32_SINT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
//		VK_FORMAT_R64_UINT,
//		VK_FORMAT_R64_SINT,
//		VK_FORMAT_R64_SFLOAT,
//		VK_FORMAT_R64G64_UINT,
//		VK_FORMAT_R64G64_SINT,
//		VK_FORMAT_R64G64_SFLOAT,
//		VK_FORMAT_R64G64B64_UINT,
//		VK_FORMAT_R64G64B64_SINT,
//		VK_FORMAT_R64G64B64_SFLOAT,
//		VK_FORMAT_R64G64B64A64_UINT,
//		VK_FORMAT_R64G64B64A64_SINT,
//		VK_FORMAT_R64G64B64A64_SFLOAT,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32,
		VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
//		VK_FORMAT_BC1_RGB_UNORM_BLOCK,
//		VK_FORMAT_BC1_RGB_SRGB_BLOCK,
//		VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
//		VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
//		VK_FORMAT_BC2_UNORM_BLOCK,
//		VK_FORMAT_BC2_SRGB_BLOCK,
//		VK_FORMAT_BC3_UNORM_BLOCK,
//		VK_FORMAT_BC3_SRGB_BLOCK,
//		VK_FORMAT_BC4_UNORM_BLOCK,
//		VK_FORMAT_BC4_SNORM_BLOCK,
//		VK_FORMAT_BC5_UNORM_BLOCK,
//		VK_FORMAT_BC5_SNORM_BLOCK,
//		VK_FORMAT_BC6H_UFLOAT_BLOCK,
//		VK_FORMAT_BC6H_SFLOAT_BLOCK,
//		VK_FORMAT_BC7_UNORM_BLOCK,
//		VK_FORMAT_BC7_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
//		VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,
//		VK_FORMAT_EAC_R11_UNORM_BLOCK,
//		VK_FORMAT_EAC_R11_SNORM_BLOCK,
//		VK_FORMAT_EAC_R11G11_UNORM_BLOCK,
//		VK_FORMAT_EAC_R11G11_SNORM_BLOCK,
//		VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
//		VK_FORMAT_ASTC_4x4_SRGB_BLOCK,
//		VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
//		VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
//		VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_5x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_6x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
//		VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x5_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x8_SRGB_BLOCK,
//		VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
//		VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
//		VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
//		VK_FORMAT_ASTC_12x10_SRGB_BLOCK,
//		VK_FORMAT_ASTC_12x12_UNORM_BLOCK,
//		VK_FORMAT_ASTC_12x12_SRGB_BLOCK
	};
	const size_t	numOfColorImageFormatsToTest			= DE_LENGTH_OF_ARRAY(colorImageFormatsToTest);

	const VkFormat	depthStencilImageFormatsToTest[]		=
	{
		VK_FORMAT_D16_UNORM,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D32_SFLOAT_S8_UINT
	};
	const size_t	numOfDepthStencilImageFormatsToTest		= DE_LENGTH_OF_ARRAY(depthStencilImageFormatsToTest);

	// Clear color image
	{
		const VkImageType			imageTypesToTest[]		=
		{
			VK_IMAGE_TYPE_1D,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_TYPE_3D
		};
		const size_t				numOfImageTypesToTest	= DE_LENGTH_OF_ARRAY(imageTypesToTest);

		const VkExtent3D			imageDimensionsByType[]	=
		{
			{ 256, 1, 1},
			{ 256, 256, 1},
			{ 256, 256, 16}
		};

		for (size_t	imageTypeIndex = 0; imageTypeIndex < numOfImageTypesToTest; ++imageTypeIndex)
		for (size_t imageFormatIndex = 0; imageFormatIndex < numOfColorImageFormatsToTest; ++imageFormatIndex)
		{
			const VkFormat		format		= colorImageFormatsToTest[imageFormatIndex];
			const TestParams	testParams	=
			{
				false,														// bool				useSingleMipLevel;
				imageTypesToTest[imageTypeIndex],							// VkImageType		imageType;
				format,														// VkFormat			imageFormat;
				imageDimensionsByType[imageTypeIndex],						// VkExtent3D		imageExtent;
				makeClearColorValue(format, 0.2f, 0.1f, 0.7f, 0.8f),		// VkClearValue		initValue;
				{
					makeClearColorValue(format, 0.1f, 0.5f, 0.3f, 0.9f),	// VkClearValue		clearValue[0];
					makeClearColorValue(format, 0.3f, 0.6f, 0.2f, 0.7f),	// VkClearValue		clearValue[1];
				}
			};

			std::ostringstream	testCaseName;
			testCaseName << getImageTypeCaseName(testParams.imageType) << "_" << getFormatCaseName(format);

			colorImageClearTests->addChild(new InstanceFactory1<ClearColorImageTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Clear Color Image", testParams));
		}

		imageClearingTests->addChild(colorImageClearTests.release());
	}

	// Clear depth/stencil image
	{
		TestParams testParams =
		{
			true,											// bool				useSingleMipLevel;
			VK_IMAGE_TYPE_2D,								// VkImageType		imageType;
			VK_FORMAT_UNDEFINED,							// VkFormat			format;
			{ 256, 256, 1 },								// VkExtent3D		extent;
			makeClearValueDepthStencil(0.5f, 0x03),			// VkClearValue		initValue
			{
				makeClearValueDepthStencil(0.1f, 0x06),		// VkClearValue		clearValue[0];
				makeClearValueDepthStencil(0.3f, 0x04),		// VkClearValue		clearValue[1];
			}
		};

		for (size_t imageFormatIndex = 0; imageFormatIndex < numOfDepthStencilImageFormatsToTest; ++imageFormatIndex)
		{
			testParams.imageFormat = depthStencilImageFormatsToTest[imageFormatIndex];

			std::ostringstream	testCaseName;
			testCaseName << getImageTypeCaseName(testParams.imageType) << "_" << getFormatCaseName(testParams.imageFormat);

			depthStencilImageClearTests->addChild(new InstanceFactory1<ClearDepthStencilImageTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Clear Depth/Stencil Image", testParams));
		}

		imageClearingTests->addChild(depthStencilImageClearTests.release());
	}

	// Clear color attachment
	{
		for (size_t imageFormatIndex = 0; imageFormatIndex < numOfColorImageFormatsToTest; ++imageFormatIndex)
		{
			const VkFormat		format		= colorImageFormatsToTest[imageFormatIndex];
			const TestParams	testParams	=
			{
				true,														// bool				useSingleMipLevel;
				VK_IMAGE_TYPE_2D,											// VkImageType		imageType;
				format,														// VkFormat			format;
				{ 256, 256, 1 },											// VkExtent3D		extent;
				makeClearColorValue(format, 0.2f, 0.1f, 0.7f, 0.8f),		// VkClearValue		initValue
				{
					makeClearColorValue(format, 0.1f, 0.5f, 0.3f, 0.9f),	// VkClearValue		clearValue[0];
					makeClearColorValue(format, 0.3f, 0.6f, 0.2f, 0.7f),	// VkClearValue		clearValue[1];
				}
			};

			std::ostringstream	testCaseName;
			testCaseName << getImageTypeCaseName(testParams.imageType) << "_" << getFormatCaseName(format);

			colorAttachmentClearTests->addChild(new InstanceFactory1<ClearAttachmentTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Clear Color Attachment", testParams));
			partialColorAttachmentClearTests->addChild(new InstanceFactory1<PartialClearAttachmentTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Partial Clear Color Attachment", testParams));
		}

		imageClearingTests->addChild(colorAttachmentClearTests.release());
		imageClearingTests->addChild(partialColorAttachmentClearTests.release());
	}

	// Clear depth/stencil attachment
	{
		TestParams testParams =
		{
			true,											// bool				useSingleMipLevel;
			VK_IMAGE_TYPE_2D,								// VkImageType		imageType;
			VK_FORMAT_UNDEFINED,							// VkFormat			format;
			{ 256, 256, 1 },								// VkExtent3D		extent;
			makeClearValueDepthStencil(0.5f, 0x03),			// VkClearValue		initValue
			{
				makeClearValueDepthStencil(0.1f, 0x06),		// VkClearValue		clearValue[0];
				makeClearValueDepthStencil(0.3f, 0x04),		// VkClearValue		clearValue[1];
			}
		};

		for (size_t imageFormatIndex = 0; imageFormatIndex < numOfDepthStencilImageFormatsToTest; ++imageFormatIndex)
		{
			testParams.imageFormat = depthStencilImageFormatsToTest[imageFormatIndex];

			std::ostringstream	testCaseName;
			testCaseName << getImageTypeCaseName(testParams.imageType) << "_" << getFormatCaseName(testParams.imageFormat);

			depthStencilAttachmentClearTests->addChild(new InstanceFactory1<ClearAttachmentTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Clear Depth/Stencil Attachment", testParams));
			partialDepthStencilAttachmentClearTests->addChild(new InstanceFactory1<PartialClearAttachmentTestInstance, TestParams>(testCtx, NODETYPE_SELF_VALIDATE, testCaseName.str(), "Parital Clear Depth/Stencil Attachment", testParams));
		}

		imageClearingTests->addChild(depthStencilAttachmentClearTests.release());
		imageClearingTests->addChild(partialDepthStencilAttachmentClearTests.release());
	}

	return imageClearingTests.release();
}

} // api
} // vkt
