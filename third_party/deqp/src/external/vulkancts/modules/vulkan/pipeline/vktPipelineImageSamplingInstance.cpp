/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 The Khronos Group Inc.
 * Copyright (c) 2015 Imagination Technologies Ltd.
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
 * \brief Image sampling case
 *//*--------------------------------------------------------------------*/

#include "vktPipelineImageSamplingInstance.hpp"
#include "vktPipelineClearUtil.hpp"
#include "vktPipelineReferenceRenderer.hpp"
#include "vkBuilderUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkPrograms.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "tcuTexLookupVerifier.hpp"
#include "tcuTextureUtil.hpp"
#include "deSTLUtil.hpp"

namespace vkt
{
namespace pipeline
{

using namespace vk;
using de::MovePtr;
using de::UniquePtr;

namespace
{

static VkImageType getCompatibleImageType (VkImageViewType viewType)
{
	switch (viewType)
	{
		case VK_IMAGE_VIEW_TYPE_1D:				return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_1D_ARRAY:		return VK_IMAGE_TYPE_1D;
		case VK_IMAGE_VIEW_TYPE_2D:				return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_2D_ARRAY:		return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_3D:				return VK_IMAGE_TYPE_3D;
		case VK_IMAGE_VIEW_TYPE_CUBE:			return VK_IMAGE_TYPE_2D;
		case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:		return VK_IMAGE_TYPE_2D;
		default:
			break;
	}

	DE_ASSERT(false);
	return VK_IMAGE_TYPE_1D;
}

template<typename TcuFormatType>
static MovePtr<TestTexture> createTestTexture (const TcuFormatType format, VkImageViewType viewType, const tcu::IVec3& size, int layerCount)
{
	MovePtr<TestTexture>	texture;
	const VkImageType		imageType = getCompatibleImageType(viewType);

	switch (imageType)
	{
		case VK_IMAGE_TYPE_1D:
			if (layerCount == 1)
				texture = MovePtr<TestTexture>(new TestTexture1D(format, size.x()));
			else
				texture = MovePtr<TestTexture>(new TestTexture1DArray(format, size.x(), layerCount));

			break;

		case VK_IMAGE_TYPE_2D:
			if (layerCount == 1)
			{
				texture = MovePtr<TestTexture>(new TestTexture2D(format, size.x(), size.y()));
			}
			else
			{
				if (viewType == VK_IMAGE_VIEW_TYPE_CUBE || viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
				{
					if (layerCount == tcu::CUBEFACE_LAST && viewType == VK_IMAGE_VIEW_TYPE_CUBE)
					{
						texture = MovePtr<TestTexture>(new TestTextureCube(format, size.x()));
					}
					else
					{
						DE_ASSERT(layerCount % tcu::CUBEFACE_LAST == 0);

						texture = MovePtr<TestTexture>(new TestTextureCubeArray(format, size.x(), layerCount));
					}
				}
				else
				{
					texture = MovePtr<TestTexture>(new TestTexture2DArray(format, size.x(), size.y(), layerCount));
				}
			}

			break;

		case VK_IMAGE_TYPE_3D:
			texture = MovePtr<TestTexture>(new TestTexture3D(format, size.x(), size.y(), size.z()));
			break;

		default:
			DE_ASSERT(false);
	}

	return texture;
}

} // anonymous

ImageSamplingInstance::ImageSamplingInstance (Context&							context,
											  const tcu::UVec2&					renderSize,
											  VkImageViewType					imageViewType,
											  VkFormat							imageFormat,
											  const tcu::IVec3&					imageSize,
											  int								layerCount,
											  const VkComponentMapping&			componentMapping,
											  const VkImageSubresourceRange&	subresourceRange,
											  const VkSamplerCreateInfo&		samplerParams,
											  float								samplerLod,
											  const std::vector<Vertex4Tex4>&	vertices,
											  VkDescriptorType					samplingType,
											  int								imageCount)
	: vkt::TestInstance		(context)
	, m_samplingType		(samplingType)
	, m_imageViewType		(imageViewType)
	, m_imageFormat			(imageFormat)
	, m_imageSize			(imageSize)
	, m_layerCount			(layerCount)
	, m_imageCount			(imageCount)
	, m_componentMapping	(componentMapping)
	, m_subresourceRange	(subresourceRange)
	, m_samplerParams		(samplerParams)
	, m_samplerLod			(samplerLod)
	, m_renderSize			(renderSize)
	, m_colorFormat			(VK_FORMAT_R8G8B8A8_UNORM)
	, m_vertices			(vertices)
{
	const DeviceInterface&		vk						= context.getDeviceInterface();
	const VkDevice				vkDevice				= context.getDevice();
	const VkQueue				queue					= context.getUniversalQueue();
	const deUint32				queueFamilyIndex		= context.getUniversalQueueFamilyIndex();
	SimpleAllocator				memAlloc				(vk, vkDevice, getPhysicalDeviceMemoryProperties(context.getInstanceInterface(), context.getPhysicalDevice()));
	const VkComponentMapping	componentMappingRGBA	= { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };

	if (!isSupportedSamplableFormat(context.getInstanceInterface(), context.getPhysicalDevice(), imageFormat))
		throw tcu::NotSupportedError(std::string("Unsupported format for sampling: ") + getFormatName(imageFormat));

	if ((samplerParams.minFilter == VK_FILTER_LINEAR ||
		 samplerParams.magFilter == VK_FILTER_LINEAR ||
		 samplerParams.mipmapMode == VK_SAMPLER_MIPMAP_MODE_LINEAR) &&
		!isLinearFilteringSupported(context.getInstanceInterface(), context.getPhysicalDevice(), imageFormat, VK_IMAGE_TILING_OPTIMAL))
		throw tcu::NotSupportedError(std::string("Unsupported format for linear filtering: ") + getFormatName(imageFormat));

	if ((samplerParams.addressModeU == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
		 samplerParams.addressModeV == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE ||
		 samplerParams.addressModeW == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE) &&
		!de::contains(context.getDeviceExtensions().begin(), context.getDeviceExtensions().end(), "VK_KHR_sampler_mirror_clamp_to_edge"))
		TCU_THROW(NotSupportedError, "VK_KHR_sampler_mirror_clamp_to_edge not supported");

	if (isCompressedFormat(imageFormat) && imageViewType == VK_IMAGE_VIEW_TYPE_3D)
	{
		// \todo [2016-01-22 pyry] Mandate VK_ERROR_FORMAT_NOT_SUPPORTED
		try
		{
			const VkImageFormatProperties	formatProperties	= getPhysicalDeviceImageFormatProperties(context.getInstanceInterface(),
																										 context.getPhysicalDevice(),
																										 imageFormat,
																										 VK_IMAGE_TYPE_3D,
																										 VK_IMAGE_TILING_OPTIMAL,
																										 VK_IMAGE_USAGE_SAMPLED_BIT,
																										 (VkImageCreateFlags)0);

			if (formatProperties.maxExtent.width == 0 &&
				formatProperties.maxExtent.height == 0 &&
				formatProperties.maxExtent.depth == 0)
				TCU_THROW(NotSupportedError, "3D compressed format not supported");
		}
		catch (const Error&)
		{
			TCU_THROW(NotSupportedError, "3D compressed format not supported");
		}
	}

	if (imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY && !context.getDeviceFeatures().imageCubeArray)
		TCU_THROW(NotSupportedError, "imageCubeArray feature is not supported");

	// Create texture images, views and samplers
	{
		VkImageCreateFlags			imageFlags			= 0u;

		if (m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE || m_imageViewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
			imageFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		// Initialize texture data
		if (isCompressedFormat(imageFormat))
			m_texture = createTestTexture(mapVkCompressedFormat(imageFormat), imageViewType, imageSize, layerCount);
		else
			m_texture = createTestTexture(mapVkFormat(imageFormat), imageViewType, imageSize, layerCount);

		const VkImageCreateInfo	imageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,							// VkStructureType			sType;
			DE_NULL,														// const void*				pNext;
			imageFlags,														// VkImageCreateFlags		flags;
			getCompatibleImageType(m_imageViewType),						// VkImageType				imageType;
			imageFormat,													// VkFormat					format;
			{																// VkExtent3D				extent;
				(deUint32)m_imageSize.x(),
				(deUint32)m_imageSize.y(),
				(deUint32)m_imageSize.z()
			},
			(deUint32)m_texture->getNumLevels(),							// deUint32					mipLevels;
			(deUint32)m_layerCount,											// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,											// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,										// VkImageTiling			tiling;
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,	// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,										// VkSharingMode			sharingMode;
			1u,																// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,												// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED										// VkImageLayout			initialLayout;
		};

		m_images.resize(m_imageCount);
		m_imageAllocs.resize(m_imageCount);
		m_imageViews.resize(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_images[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &imageParams)));
			m_imageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, **m_images[imgNdx]), MemoryRequirement::Any)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_images[imgNdx], (*m_imageAllocs[imgNdx])->getMemory(), (*m_imageAllocs[imgNdx])->getOffset()));

			// Upload texture data
			uploadTestTexture(vk, vkDevice, queue, queueFamilyIndex, memAlloc, *m_texture, **m_images[imgNdx]);

			// Create image view and sampler
			const VkImageViewCreateInfo imageViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,	// VkStructureType			sType;
				DE_NULL,									// const void*				pNext;
				0u,											// VkImageViewCreateFlags	flags;
				**m_images[imgNdx],							// VkImage					image;
				m_imageViewType,							// VkImageViewType			viewType;
				imageFormat,								// VkFormat					format;
				m_componentMapping,							// VkComponentMapping		components;
				m_subresourceRange,							// VkImageSubresourceRange	subresourceRange;
			};

			m_imageViews[imgNdx] = SharedImageViewPtr(new UniqueImageView(createImageView(vk, vkDevice, &imageViewParams)));
		}

		m_sampler	= createSampler(vk, vkDevice, &m_samplerParams);
	}

	// Create descriptor set for image and sampler
	{
		DescriptorPoolBuilder descriptorPoolBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			descriptorPoolBuilder.addType(VK_DESCRIPTOR_TYPE_SAMPLER, 1u);
		descriptorPoolBuilder.addType(m_samplingType, m_imageCount);
		m_descriptorPool = descriptorPoolBuilder.build(vk, vkDevice, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? m_imageCount + 1u : m_imageCount);

		DescriptorSetLayoutBuilder setLayoutBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			setLayoutBuilder.addSingleBinding(VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		setLayoutBuilder.addArrayBinding(m_samplingType, m_imageCount, VK_SHADER_STAGE_FRAGMENT_BIT);
		m_descriptorSetLayout = setLayoutBuilder.build(vk, vkDevice);

		const VkDescriptorSetAllocateInfo descriptorSetAllocateInfo =
		{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			*m_descriptorPool,									// VkDescriptorPool				descriptorPool;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get()						// const VkDescriptorSetLayout*	pSetLayouts;
		};

		m_descriptorSet = allocateDescriptorSet(vk, vkDevice, &descriptorSetAllocateInfo);

		const VkSampler sampler = m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? DE_NULL : *m_sampler;
		std::vector<VkDescriptorImageInfo> descriptorImageInfo(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			descriptorImageInfo[imgNdx].sampler		= sampler;									// VkSampler		sampler;
			descriptorImageInfo[imgNdx].imageView	= **m_imageViews[imgNdx];					// VkImageView		imageView;
			descriptorImageInfo[imgNdx].imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;	// VkImageLayout	imageLayout;
		}

		DescriptorSetUpdateBuilder setUpdateBuilder;
		if (m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
		{
			const VkDescriptorImageInfo descriptorSamplerInfo =
			{
				*m_sampler,									// VkSampler		sampler;
				DE_NULL,									// VkImageView		imageView;
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL	// VkImageLayout	imageLayout;
			};
			setUpdateBuilder.writeSingle(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(0), VK_DESCRIPTOR_TYPE_SAMPLER, &descriptorSamplerInfo);
		}

		const deUint32 binding = m_samplingType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ? 1u : 0u;
		setUpdateBuilder.writeArray(*m_descriptorSet, DescriptorSetUpdateBuilder::Location::binding(binding), m_samplingType, m_imageCount, descriptorImageInfo.data());
		setUpdateBuilder.update(vk, vkDevice);
	}

	// Create color images and views
	{
		const VkImageCreateInfo colorImageParams =
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,										// VkStructureType			sType;
			DE_NULL,																	// const void*				pNext;
			0u,																			// VkImageCreateFlags		flags;
			VK_IMAGE_TYPE_2D,															// VkImageType				imageType;
			m_colorFormat,																// VkFormat					format;
			{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y(), 1u },				// VkExtent3D				extent;
			1u,																			// deUint32					mipLevels;
			1u,																			// deUint32					arrayLayers;
			VK_SAMPLE_COUNT_1_BIT,														// VkSampleCountFlagBits	samples;
			VK_IMAGE_TILING_OPTIMAL,													// VkImageTiling			tiling;
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,		// VkImageUsageFlags		usage;
			VK_SHARING_MODE_EXCLUSIVE,													// VkSharingMode			sharingMode;
			1u,																			// deUint32					queueFamilyIndexCount;
			&queueFamilyIndex,															// const deUint32*			pQueueFamilyIndices;
			VK_IMAGE_LAYOUT_UNDEFINED													// VkImageLayout			initialLayout;
		};

		m_colorImages.resize(m_imageCount);
		m_colorImageAllocs.resize(m_imageCount);
		m_colorAttachmentViews.resize(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			m_colorImages[imgNdx] = SharedImagePtr(new UniqueImage(createImage(vk, vkDevice, &colorImageParams)));
			m_colorImageAllocs[imgNdx] = SharedAllocPtr(new UniqueAlloc(memAlloc.allocate(getImageMemoryRequirements(vk, vkDevice, **m_colorImages[imgNdx]), MemoryRequirement::Any)));
			VK_CHECK(vk.bindImageMemory(vkDevice, **m_colorImages[imgNdx], (*m_colorImageAllocs[imgNdx])->getMemory(), (*m_colorImageAllocs[imgNdx])->getOffset()));

			const VkImageViewCreateInfo colorAttachmentViewParams =
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,			// VkStructureType			sType;
				DE_NULL,											// const void*				pNext;
				0u,													// VkImageViewCreateFlags	flags;
				**m_colorImages[imgNdx],							// VkImage					image;
				VK_IMAGE_VIEW_TYPE_2D,								// VkImageViewType			viewType;
				m_colorFormat,										// VkFormat					format;
				componentMappingRGBA,								// VkComponentMapping		components;
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }		// VkImageSubresourceRange	subresourceRange;
			};

			m_colorAttachmentViews[imgNdx] = SharedImageViewPtr(new UniqueImageView(createImageView(vk, vkDevice, &colorAttachmentViewParams)));
		}
	}

	// Create render pass
	{
		std::vector<VkAttachmentDescription>	colorAttachmentDescriptions(m_imageCount);
		std::vector<VkAttachmentReference>		colorAttachmentReferences(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			colorAttachmentDescriptions[imgNdx].flags			= 0u;										// VkAttachmentDescriptionFlags		flags;
			colorAttachmentDescriptions[imgNdx].format			= m_colorFormat;							// VkFormat							format;
			colorAttachmentDescriptions[imgNdx].samples			= VK_SAMPLE_COUNT_1_BIT;					// VkSampleCountFlagBits			samples;
			colorAttachmentDescriptions[imgNdx].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;				// VkAttachmentLoadOp				loadOp;
			colorAttachmentDescriptions[imgNdx].storeOp			= VK_ATTACHMENT_STORE_OP_STORE;				// VkAttachmentStoreOp				storeOp;
			colorAttachmentDescriptions[imgNdx].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;			// VkAttachmentLoadOp				stencilLoadOp;
			colorAttachmentDescriptions[imgNdx].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;			// VkAttachmentStoreOp				stencilStoreOp;
			colorAttachmentDescriptions[imgNdx].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					initialLayout;
			colorAttachmentDescriptions[imgNdx].finalLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					finalLayout;

			colorAttachmentReferences[imgNdx].attachment		= (deUint32)imgNdx;							// deUint32							attachment;
			colorAttachmentReferences[imgNdx].layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout					layout;
		}

		const VkSubpassDescription subpassDescription =
		{
			0u,													// VkSubpassDescriptionFlags	flags;
			VK_PIPELINE_BIND_POINT_GRAPHICS,					// VkPipelineBindPoint			pipelineBindPoint;
			0u,													// deUint32						inputAttachmentCount;
			DE_NULL,											// const VkAttachmentReference*	pInputAttachments;
			(deUint32)m_imageCount,								// deUint32						colorAttachmentCount;
			&colorAttachmentReferences[0],						// const VkAttachmentReference*	pColorAttachments;
			DE_NULL,											// const VkAttachmentReference*	pResolveAttachments;
			DE_NULL,											// const VkAttachmentReference*	pDepthStencilAttachment;
			0u,													// deUint32						preserveAttachmentCount;
			DE_NULL												// const VkAttachmentReference*	pPreserveAttachments;
		};

		const VkRenderPassCreateInfo renderPassParams =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,			// VkStructureType					sType;
			DE_NULL,											// const void*						pNext;
			0u,													// VkRenderPassCreateFlags			flags;
			(deUint32)m_imageCount,								// deUint32							attachmentCount;
			&colorAttachmentDescriptions[0],					// const VkAttachmentDescription*	pAttachments;
			1u,													// deUint32							subpassCount;
			&subpassDescription,								// const VkSubpassDescription*		pSubpasses;
			0u,													// deUint32							dependencyCount;
			DE_NULL												// const VkSubpassDependency*		pDependencies;
		};

		m_renderPass = createRenderPass(vk, vkDevice, &renderPassParams);
	}

	// Create framebuffer
	{
		std::vector<VkImageView> pAttachments(m_imageCount);
		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
			pAttachments[imgNdx] = m_colorAttachmentViews[imgNdx]->get();

		const VkFramebufferCreateInfo framebufferParams =
		{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,			// VkStructureType			sType;
			DE_NULL,											// const void*				pNext;
			0u,													// VkFramebufferCreateFlags	flags;
			*m_renderPass,										// VkRenderPass				renderPass;
			(deUint32)m_imageCount,								// deUint32					attachmentCount;
			&pAttachments[0],									// const VkImageView*		pAttachments;
			(deUint32)m_renderSize.x(),							// deUint32					width;
			(deUint32)m_renderSize.y(),							// deUint32					height;
			1u													// deUint32					layers;
		};

		m_framebuffer = createFramebuffer(vk, vkDevice, &framebufferParams);
	}

	// Create pipeline layout
	{
		const VkPipelineLayoutCreateInfo pipelineLayoutParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,											// const void*					pNext;
			0u,													// VkPipelineLayoutCreateFlags	flags;
			1u,													// deUint32						setLayoutCount;
			&m_descriptorSetLayout.get(),						// const VkDescriptorSetLayout*	pSetLayouts;
			0u,													// deUint32						pushConstantRangeCount;
			DE_NULL												// const VkPushConstantRange*	pPushConstantRanges;
		};

		m_pipelineLayout = createPipelineLayout(vk, vkDevice, &pipelineLayoutParams);
	}

	m_vertexShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_vert"), 0);
	m_fragmentShaderModule	= createShaderModule(vk, vkDevice, m_context.getBinaryCollection().get("tex_frag"), 0);

	// Create pipeline
	{
		const VkPipelineShaderStageCreateInfo shaderStages[2] =
		{
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_VERTEX_BIT,									// VkShaderStageFlagBits				stage;
				*m_vertexShaderModule,										// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			},
			{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,		// VkStructureType						sType;
				DE_NULL,													// const void*							pNext;
				0u,															// VkPipelineShaderStageCreateFlags		flags;
				VK_SHADER_STAGE_FRAGMENT_BIT,								// VkShaderStageFlagBits				stage;
				*m_fragmentShaderModule,									// VkShaderModule						module;
				"main",														// const char*							pName;
				DE_NULL														// const VkSpecializationInfo*			pSpecializationInfo;
			}
		};

		const VkVertexInputBindingDescription vertexInputBindingDescription =
		{
			0u,									// deUint32					binding;
			sizeof(Vertex4Tex4),				// deUint32					strideInBytes;
			VK_VERTEX_INPUT_RATE_VERTEX			// VkVertexInputStepRate	inputRate;
		};

		const VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2] =
		{
			{
				0u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				0u										// deUint32	offset;
			},
			{
				1u,										// deUint32	location;
				0u,										// deUint32	binding;
				VK_FORMAT_R32G32B32A32_SFLOAT,			// VkFormat	format;
				DE_OFFSET_OF(Vertex4Tex4, texCoord),	// deUint32	offset;
			}
		};

		const VkPipelineVertexInputStateCreateInfo vertexInputStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineVertexInputStateCreateFlags	flags;
			1u,																// deUint32									vertexBindingDescriptionCount;
			&vertexInputBindingDescription,									// const VkVertexInputBindingDescription*	pVertexBindingDescriptions;
			2u,																// deUint32									vertexAttributeDescriptionCount;
			vertexInputAttributeDescriptions								// const VkVertexInputAttributeDescription*	pVertexAttributeDescriptions;
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineInputAssemblyStateCreateFlags	flags;
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,							// VkPrimitiveTopology						topology;
			false															// VkBool32									primitiveRestartEnable;
		};

		const VkViewport viewport =
		{
			0.0f,						// float	x;
			0.0f,						// float	y;
			(float)m_renderSize.x(),	// float	width;
			(float)m_renderSize.y(),	// float	height;
			0.0f,						// float	minDepth;
			1.0f						// float	maxDepth;
		};

		const VkRect2D scissor = { { 0, 0 }, { (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() } };

		const VkPipelineViewportStateCreateInfo viewportStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,			// VkStructureType						sType;
			DE_NULL,														// const void*							pNext;
			0u,																// VkPipelineViewportStateCreateFlags	flags;
			1u,																// deUint32								viewportCount;
			&viewport,														// const VkViewport*					pViewports;
			1u,																// deUint32								scissorCount;
			&scissor														// const VkRect2D*						pScissors;
		};

		const VkPipelineRasterizationStateCreateInfo rasterStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,		// VkStructureType							sType;
			DE_NULL,														// const void*								pNext;
			0u,																// VkPipelineRasterizationStateCreateFlags	flags;
			false,															// VkBool32									depthClampEnable;
			false,															// VkBool32									rasterizerDiscardEnable;
			VK_POLYGON_MODE_FILL,											// VkPolygonMode							polygonMode;
			VK_CULL_MODE_NONE,												// VkCullModeFlags							cullMode;
			VK_FRONT_FACE_COUNTER_CLOCKWISE,								// VkFrontFace								frontFace;
			false,															// VkBool32									depthBiasEnable;
			0.0f,															// float									depthBiasConstantFactor;
			0.0f,															// float									depthBiasClamp;
			0.0f,															// float									depthBiasSlopeFactor;
			1.0f															// float									lineWidth;
		};

		std::vector<VkPipelineColorBlendAttachmentState>	colorBlendAttachmentStates(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			colorBlendAttachmentStates[imgNdx].blendEnable			= false;												// VkBool32					blendEnable;
			colorBlendAttachmentStates[imgNdx].srcColorBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstColorBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstColorBlendFactor;
			colorBlendAttachmentStates[imgNdx].colorBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				colorBlendOp;
			colorBlendAttachmentStates[imgNdx].srcAlphaBlendFactor	= VK_BLEND_FACTOR_ONE;									// VkBlendFactor			srcAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;									// VkBlendFactor			dstAlphaBlendFactor;
			colorBlendAttachmentStates[imgNdx].alphaBlendOp			= VK_BLEND_OP_ADD;										// VkBlendOp				alphaBlendOp;
			colorBlendAttachmentStates[imgNdx].colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |	// VkColorComponentFlags	colorWriteMask;
																		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		}

		const VkPipelineColorBlendStateCreateInfo colorBlendStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,	// VkStructureType								sType;
			DE_NULL,													// const void*									pNext;
			0u,															// VkPipelineColorBlendStateCreateFlags			flags;
			false,														// VkBool32										logicOpEnable;
			VK_LOGIC_OP_COPY,											// VkLogicOp									logicOp;
			(deUint32)m_imageCount,										// deUint32										attachmentCount;
			&colorBlendAttachmentStates[0],								// const VkPipelineColorBlendAttachmentState*	pAttachments;
			{ 0.0f, 0.0f, 0.0f, 0.0f }									// float										blendConstants[4];
		};

		const VkPipelineMultisampleStateCreateInfo multisampleStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineMultisampleStateCreateFlags	flags;
			VK_SAMPLE_COUNT_1_BIT,										// VkSampleCountFlagBits					rasterizationSamples;
			false,														// VkBool32									sampleShadingEnable;
			0.0f,														// float									minSampleShading;
			DE_NULL,													// const VkSampleMask*						pSampleMask;
			false,														// VkBool32									alphaToCoverageEnable;
			false														// VkBool32									alphaToOneEnable;
		};

		VkPipelineDepthStencilStateCreateInfo depthStencilStateParams =
		{
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,	// VkStructureType							sType;
			DE_NULL,													// const void*								pNext;
			0u,															// VkPipelineDepthStencilStateCreateFlags	flags;
			false,														// VkBool32									depthTestEnable;
			false,														// VkBool32									depthWriteEnable;
			VK_COMPARE_OP_LESS,											// VkCompareOp								depthCompareOp;
			false,														// VkBool32									depthBoundsTestEnable;
			false,														// VkBool32									stencilTestEnable;
			{															// VkStencilOpState							front;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	failOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	passOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u						// deUint32		reference;
			},
			{															// VkStencilOpState	back;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	failOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	passOp;
				VK_STENCIL_OP_ZERO,		// VkStencilOp	depthFailOp;
				VK_COMPARE_OP_NEVER,	// VkCompareOp	compareOp;
				0u,						// deUint32		compareMask;
				0u,						// deUint32		writeMask;
				0u						// deUint32		reference;
			},
			0.0f,														// float			minDepthBounds;
			1.0f														// float			maxDepthBounds;
		};

		const VkGraphicsPipelineCreateInfo graphicsPipelineParams =
		{
			VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,	// VkStructureType									sType;
			DE_NULL,											// const void*										pNext;
			0u,													// VkPipelineCreateFlags							flags;
			2u,													// deUint32											stageCount;
			shaderStages,										// const VkPipelineShaderStageCreateInfo*			pStages;
			&vertexInputStateParams,							// const VkPipelineVertexInputStateCreateInfo*		pVertexInputState;
			&inputAssemblyStateParams,							// const VkPipelineInputAssemblyStateCreateInfo*	pInputAssemblyState;
			DE_NULL,											// const VkPipelineTessellationStateCreateInfo*		pTessellationState;
			&viewportStateParams,								// const VkPipelineViewportStateCreateInfo*			pViewportState;
			&rasterStateParams,									// const VkPipelineRasterizationStateCreateInfo*	pRasterizationState;
			&multisampleStateParams,							// const VkPipelineMultisampleStateCreateInfo*		pMultisampleState;
			&depthStencilStateParams,							// const VkPipelineDepthStencilStateCreateInfo*		pDepthStencilState;
			&colorBlendStateParams,								// const VkPipelineColorBlendStateCreateInfo*		pColorBlendState;
			(const VkPipelineDynamicStateCreateInfo*)DE_NULL,	// const VkPipelineDynamicStateCreateInfo*			pDynamicState;
			*m_pipelineLayout,									// VkPipelineLayout									layout;
			*m_renderPass,										// VkRenderPass										renderPass;
			0u,													// deUint32											subpass;
			0u,													// VkPipeline										basePipelineHandle;
			0u													// deInt32											basePipelineIndex;
		};

		m_graphicsPipeline	= createGraphicsPipeline(vk, vkDevice, DE_NULL, &graphicsPipelineParams);
	}

	// Create vertex buffer
	{
		const VkDeviceSize			vertexBufferSize	= (VkDeviceSize)(m_vertices.size() * sizeof(Vertex4Tex4));
		const VkBufferCreateInfo	vertexBufferParams	=
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,		// VkStructureType		sType;
			DE_NULL,									// const void*			pNext;
			0u,											// VkBufferCreateFlags	flags;
			vertexBufferSize,							// VkDeviceSize			size;
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// VkBufferUsageFlags	usage;
			VK_SHARING_MODE_EXCLUSIVE,					// VkSharingMode		sharingMode;
			1u,											// deUint32				queueFamilyIndexCount;
			&queueFamilyIndex							// const deUint32*		pQueueFamilyIndices;
		};

		DE_ASSERT(vertexBufferSize > 0);

		m_vertexBuffer		= createBuffer(vk, vkDevice, &vertexBufferParams);
		m_vertexBufferAlloc	= memAlloc.allocate(getBufferMemoryRequirements(vk, vkDevice, *m_vertexBuffer), MemoryRequirement::HostVisible);

		VK_CHECK(vk.bindBufferMemory(vkDevice, *m_vertexBuffer, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset()));

		// Load vertices into vertex buffer
		deMemcpy(m_vertexBufferAlloc->getHostPtr(), &m_vertices[0], (size_t)vertexBufferSize);
		flushMappedMemoryRange(vk, vkDevice, m_vertexBufferAlloc->getMemory(), m_vertexBufferAlloc->getOffset(), vertexBufferParams.size);
	}

	// Create command pool
	{
		const VkCommandPoolCreateInfo cmdPoolParams =
		{
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,		// VkStructureType				sType;
			DE_NULL,										// const void*					pNext;
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,			// VkCommandPoolCreateFlags	flags;
			queueFamilyIndex								// deUint32					queueFamilyIndex;
		};

		m_cmdPool = createCommandPool(vk, vkDevice, &cmdPoolParams);
	}

	// Create command buffer
	{
		const VkCommandBufferAllocateInfo cmdBufferAllocateInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,	// VkStructureType			sType;
			DE_NULL,										// const void*				pNext;
			*m_cmdPool,										// VkCommandPool			commandPool;
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,				// VkCommandBufferLevel		level;
			1u,												// deUint32					bufferCount;
		};

		const VkCommandBufferBeginInfo cmdBufferBeginInfo =
		{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,	// VkStructureType					sType;
			DE_NULL,										// const void*						pNext;
			0u,												// VkCommandBufferUsageFlags		flags;
			(const VkCommandBufferInheritanceInfo*)DE_NULL,
		};

		const VkClearValue attachmentClearValue = defaultClearValue(m_colorFormat);

		const VkRenderPassBeginInfo renderPassBeginInfo =
		{
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,				// VkStructureType		sType;
			DE_NULL,												// const void*			pNext;
			*m_renderPass,											// VkRenderPass			renderPass;
			*m_framebuffer,											// VkFramebuffer		framebuffer;
			{
				{ 0, 0 },
				{ (deUint32)m_renderSize.x(), (deUint32)m_renderSize.y() }
			},														// VkRect2D				renderArea;
			1,														// deUint32				clearValueCount;
			&attachmentClearValue									// const VkClearValue*	pClearValues;
		};

		std::vector<VkImageMemoryBarrier> preAttachmentBarriers(m_imageCount);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			preAttachmentBarriers[imgNdx].sType								= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;	// VkStructureType			sType;
			preAttachmentBarriers[imgNdx].pNext								= DE_NULL;									// const void*				pNext;
			preAttachmentBarriers[imgNdx].srcAccessMask						= 0u;										// VkAccessFlags			srcAccessMask;
			preAttachmentBarriers[imgNdx].dstAccessMask						= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;		// VkAccessFlags			dstAccessMask;
			preAttachmentBarriers[imgNdx].oldLayout							= VK_IMAGE_LAYOUT_UNDEFINED;				// VkImageLayout			oldLayout;
			preAttachmentBarriers[imgNdx].newLayout							= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// VkImageLayout			newLayout;
			preAttachmentBarriers[imgNdx].srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;					// deUint32					srcQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;					// deUint32					dstQueueFamilyIndex;
			preAttachmentBarriers[imgNdx].image								= **m_colorImages[imgNdx];					// VkImage					image;
			preAttachmentBarriers[imgNdx].subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;				// VkImageSubresourceRange	subresourceRange;
			preAttachmentBarriers[imgNdx].subresourceRange.baseMipLevel		= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.levelCount		= 1u;
			preAttachmentBarriers[imgNdx].subresourceRange.baseArrayLayer	= 0u;
			preAttachmentBarriers[imgNdx].subresourceRange.layerCount		= 1u;
		}

		m_cmdBuffer = allocateCommandBuffer(vk, vkDevice, &cmdBufferAllocateInfo);

		VK_CHECK(vk.beginCommandBuffer(*m_cmdBuffer, &cmdBufferBeginInfo));

		vk.cmdPipelineBarrier(*m_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, (VkDependencyFlags)0,
			0u, DE_NULL, 0u, DE_NULL, (deUint32)m_imageCount, &preAttachmentBarriers[0]);

		vk.cmdBeginRenderPass(*m_cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vk.cmdBindPipeline(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_graphicsPipeline);

		vk.cmdBindDescriptorSets(*m_cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *m_pipelineLayout, 0, 1, &m_descriptorSet.get(), 0, DE_NULL);

		const VkDeviceSize vertexBufferOffset = 0;
		vk.cmdBindVertexBuffers(*m_cmdBuffer, 0, 1, &m_vertexBuffer.get(), &vertexBufferOffset);
		vk.cmdDraw(*m_cmdBuffer, (deUint32)m_vertices.size(), 1, 0, 0);

		vk.cmdEndRenderPass(*m_cmdBuffer);
		VK_CHECK(vk.endCommandBuffer(*m_cmdBuffer));
	}

	// Create fence
	{
		const VkFenceCreateInfo fenceParams =
		{
			VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,	// VkStructureType		sType;
			DE_NULL,								// const void*			pNext;
			0u										// VkFenceCreateFlags	flags;
		};

		m_fence = createFence(vk, vkDevice, &fenceParams);
	}
}

ImageSamplingInstance::~ImageSamplingInstance (void)
{
}

tcu::TestStatus ImageSamplingInstance::iterate (void)
{
	const DeviceInterface&		vk			= m_context.getDeviceInterface();
	const VkDevice				vkDevice	= m_context.getDevice();
	const VkQueue				queue		= m_context.getUniversalQueue();
	const VkSubmitInfo			submitInfo	=
	{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,	// VkStructureType			sType;
		DE_NULL,						// const void*				pNext;
		0u,								// deUint32					waitSemaphoreCount;
		DE_NULL,						// const VkSemaphore*		pWaitSemaphores;
		DE_NULL,
		1u,								// deUint32					commandBufferCount;
		&m_cmdBuffer.get(),				// const VkCommandBuffer*	pCommandBuffers;
		0u,								// deUint32					signalSemaphoreCount;
		DE_NULL							// const VkSemaphore*		pSignalSemaphores;
	};

	VK_CHECK(vk.resetFences(vkDevice, 1, &m_fence.get()));
	VK_CHECK(vk.queueSubmit(queue, 1, &submitInfo, *m_fence));
	VK_CHECK(vk.waitForFences(vkDevice, 1, &m_fence.get(), true, ~(0ull) /* infinity */));

	return verifyImage();
}

namespace
{

bool isLookupResultValid (const tcu::Texture1DView&		texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.x(), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture1DArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture2DView&		texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1), lodBounds, result);
}

bool isLookupResultValid (const tcu::Texture2DArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

bool isLookupResultValid (const tcu::TextureCubeView&	texture,
						  const tcu::Sampler&			sampler,
						  const tcu::LookupPrecision&	precision,
						  const tcu::Vec4&				coords,
						  const tcu::Vec2&				lodBounds,
						  const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

bool isLookupResultValid (const tcu::TextureCubeArrayView&	texture,
						  const tcu::Sampler&				sampler,
						  const tcu::LookupPrecision&		precision,
						  const tcu::Vec4&					coords,
						  const tcu::Vec2&					lodBounds,
						  const tcu::Vec4&					result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, tcu::IVec4(precision.coordBits.x()), coords, lodBounds, result);
}

bool isLookupResultValid(const tcu::Texture3DView&		texture,
						 const tcu::Sampler&			sampler,
						 const tcu::LookupPrecision&	precision,
						 const tcu::Vec4&				coords,
						 const tcu::Vec2&				lodBounds,
						 const tcu::Vec4&				result)
{
	return tcu::isLookupResultValid(texture, sampler, precision, coords.swizzle(0,1,2), lodBounds, result);
}

template<typename TextureViewType>
bool validateResultImage (const TextureViewType&				texture,
						  const tcu::Sampler&					sampler,
						  const tcu::ConstPixelBufferAccess&	texCoords,
						  const tcu::Vec2&						lodBounds,
						  const tcu::LookupPrecision&			lookupPrecision,
						  const tcu::Vec4&						lookupScale,
						  const tcu::Vec4&						lookupBias,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::PixelBufferAccess&			errorMask)
{
	const int	w		= result.getWidth();
	const int	h		= result.getHeight();
	bool		allOk	= true;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const tcu::Vec4		resultPixel	= result.getPixel(x, y);
			const tcu::Vec4		resultColor	= (resultPixel - lookupBias) / lookupScale;
			const tcu::Vec4		texCoord	= texCoords.getPixel(x, y);
			const bool			pixelOk		= isLookupResultValid(texture, sampler, lookupPrecision, texCoord, lodBounds, resultColor);

			errorMask.setPixel(tcu::Vec4(pixelOk?0.0f:1.0f, pixelOk?1.0f:0.0f, 0.0f, 1.0f), x, y);

			if (!pixelOk)
				allOk = false;
		}
	}

	return allOk;
}

template<typename ScalarType>
ScalarType getSwizzledComp (const tcu::Vector<ScalarType, 4>& vec, vk::VkComponentSwizzle comp, int identityNdx)
{
	if (comp == vk::VK_COMPONENT_SWIZZLE_IDENTITY)
		return vec[identityNdx];
	else if (comp == vk::VK_COMPONENT_SWIZZLE_ZERO)
		return ScalarType(0);
	else if (comp == vk::VK_COMPONENT_SWIZZLE_ONE)
		return ScalarType(1);
	else
		return vec[comp - vk::VK_COMPONENT_SWIZZLE_R];
}

template<typename ScalarType>
tcu::Vector<ScalarType, 4> swizzle (const tcu::Vector<ScalarType, 4>& vec, const vk::VkComponentMapping& swz)
{
	return tcu::Vector<ScalarType, 4>(getSwizzledComp(vec, swz.r, 0),
									  getSwizzledComp(vec, swz.g, 1),
									  getSwizzledComp(vec, swz.b, 2),
									  getSwizzledComp(vec, swz.a, 3));
}

tcu::Vec4 swizzleScaleBias (const tcu::Vec4& vec, const vk::VkComponentMapping& swz)
{
	const float channelValues[] =
	{
		1.0f, // -1
		1.0f, // 0
		1.0f,
		vec.x(),
		vec.y(),
		vec.z(),
		vec.w()
	};

	return tcu::Vec4(channelValues[swz.r], channelValues[swz.g], channelValues[swz.b], channelValues[swz.a]);
}

template<typename ScalarType>
void swizzleT (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	for (int z = 0; z < dst.getDepth(); ++z)
	for (int y = 0; y < dst.getHeight(); ++y)
	for (int x = 0; x < dst.getWidth(); ++x)
		dst.setPixel(swizzle(src.getPixelT<ScalarType>(x, y, z), swz), x, y, z);
}

void swizzleFromSRGB (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	for (int z = 0; z < dst.getDepth(); ++z)
	for (int y = 0; y < dst.getHeight(); ++y)
	for (int x = 0; x < dst.getWidth(); ++x)
		dst.setPixel(swizzle(tcu::sRGBToLinear(src.getPixelT<float>(x, y, z)), swz), x, y, z);
}

void swizzle (const tcu::ConstPixelBufferAccess& src, const tcu::PixelBufferAccess& dst, const vk::VkComponentMapping& swz)
{
	const tcu::TextureChannelClass	chnClass	= tcu::getTextureChannelClass(dst.getFormat().type);

	DE_ASSERT(src.getWidth() == dst.getWidth() &&
			  src.getHeight() == dst.getHeight() &&
			  src.getDepth() == dst.getDepth());

	if (chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		swizzleT<deInt32>(src, dst, swz);
	else if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		swizzleT<deUint32>(src, dst, swz);
	else if (tcu::isSRGB(src.getFormat()) && !tcu::isSRGB(dst.getFormat()))
		swizzleFromSRGB(src, dst, swz);
	else
		swizzleT<float>(src, dst, swz);
}

bool isIdentitySwizzle (const vk::VkComponentMapping& swz)
{
	return (swz.r == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.r == vk::VK_COMPONENT_SWIZZLE_R) &&
		   (swz.g == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.g == vk::VK_COMPONENT_SWIZZLE_G) &&
		   (swz.b == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.b == vk::VK_COMPONENT_SWIZZLE_B) &&
		   (swz.a == vk::VK_COMPONENT_SWIZZLE_IDENTITY || swz.a == vk::VK_COMPONENT_SWIZZLE_A);
}

template<typename TextureViewType> struct TexViewTraits;

template<> struct TexViewTraits<tcu::Texture1DView>			{ typedef tcu::Texture1D		TextureType; };
template<> struct TexViewTraits<tcu::Texture1DArrayView>	{ typedef tcu::Texture1DArray	TextureType; };
template<> struct TexViewTraits<tcu::Texture2DView>			{ typedef tcu::Texture2D		TextureType; };
template<> struct TexViewTraits<tcu::Texture2DArrayView>	{ typedef tcu::Texture2DArray	TextureType; };
template<> struct TexViewTraits<tcu::TextureCubeView>		{ typedef tcu::TextureCube		TextureType; };
template<> struct TexViewTraits<tcu::TextureCubeArrayView>	{ typedef tcu::TextureCubeArray	TextureType; };
template<> struct TexViewTraits<tcu::Texture3DView>			{ typedef tcu::Texture3D		TextureType; };

template<typename TextureViewType>
typename TexViewTraits<TextureViewType>::TextureType* createSkeletonClone (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0);

tcu::TextureFormat getSwizzleTargetFormat (tcu::TextureFormat format)
{
	// Swizzled texture needs to hold all four channels
	// \todo [2016-09-21 pyry] We could save some memory by using smaller formats
	//						   when possible (for example U8).

	const tcu::TextureChannelClass	chnClass	= tcu::getTextureChannelClass(format.type);

	if (chnClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER)
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::SIGNED_INT32);
	else if (chnClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER)
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNSIGNED_INT32);
	else
		return tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);
}

template<>
tcu::Texture1D* createSkeletonClone<tcu::Texture1DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture1D(format, level0.getWidth());
}

template<>
tcu::Texture1DArray* createSkeletonClone<tcu::Texture1DArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture1DArray(format, level0.getWidth(), level0.getHeight());
}

template<>
tcu::Texture2D* createSkeletonClone<tcu::Texture2DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture2D(format, level0.getWidth(), level0.getHeight());
}

template<>
tcu::Texture2DArray* createSkeletonClone<tcu::Texture2DArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture2DArray(format, level0.getWidth(), level0.getHeight(), level0.getDepth());
}

template<>
tcu::Texture3D* createSkeletonClone<tcu::Texture3DView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::Texture3D(format, level0.getWidth(), level0.getHeight(), level0.getDepth());
}

template<>
tcu::TextureCubeArray* createSkeletonClone<tcu::TextureCubeArrayView> (tcu::TextureFormat format, const tcu::ConstPixelBufferAccess& level0)
{
	return new tcu::TextureCubeArray(format, level0.getWidth(), level0.getDepth());
}

template<typename TextureViewType>
MovePtr<typename TexViewTraits<TextureViewType>::TextureType> createSwizzledCopy (const TextureViewType& texture, const vk::VkComponentMapping& swz)
{
	MovePtr<typename TexViewTraits<TextureViewType>::TextureType>	copy	(createSkeletonClone<TextureViewType>(getSwizzleTargetFormat(texture.getLevel(0).getFormat()), texture.getLevel(0)));

	for (int levelNdx = 0; levelNdx < texture.getNumLevels(); ++levelNdx)
	{
		copy->allocLevel(levelNdx);
		swizzle(texture.getLevel(levelNdx), copy->getLevel(levelNdx), swz);
	}

	return copy;
}

template<>
MovePtr<tcu::TextureCube> createSwizzledCopy (const tcu::TextureCubeView& texture, const vk::VkComponentMapping& swz)
{
	MovePtr<tcu::TextureCube>	copy	(new tcu::TextureCube(getSwizzleTargetFormat(texture.getLevelFace(0, tcu::CUBEFACE_NEGATIVE_X).getFormat()), texture.getSize()));

	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
	{
		for (int levelNdx = 0; levelNdx < texture.getNumLevels(); ++levelNdx)
		{
			copy->allocLevel((tcu::CubeFace)faceNdx, levelNdx);
			swizzle(texture.getLevelFace(levelNdx, (tcu::CubeFace)faceNdx), copy->getLevelFace(levelNdx, (tcu::CubeFace)faceNdx), swz);
		}
	}

	return copy;
}

template<typename TextureViewType>
bool validateResultImage (const TextureViewType&				texture,
						  const tcu::Sampler&					sampler,
						  const vk::VkComponentMapping&			swz,
						  const tcu::ConstPixelBufferAccess&	texCoords,
						  const tcu::Vec2&						lodBounds,
						  const tcu::LookupPrecision&			lookupPrecision,
						  const tcu::Vec4&						lookupScale,
						  const tcu::Vec4&						lookupBias,
						  const tcu::ConstPixelBufferAccess&	result,
						  const tcu::PixelBufferAccess&			errorMask)
{
	if (isIdentitySwizzle(swz))
		return validateResultImage(texture, sampler, texCoords, lodBounds, lookupPrecision, lookupScale, lookupBias, result, errorMask);
	else
	{
		// There is (currently) no way to handle swizzling inside validation loop
		// and thus we need to pre-swizzle the texture.
		UniquePtr<typename TexViewTraits<TextureViewType>::TextureType>	swizzledTex	(createSwizzledCopy(texture, swz));

		return validateResultImage(*swizzledTex, sampler, texCoords, lodBounds, lookupPrecision, swizzleScaleBias(lookupScale, swz), swizzleScaleBias(lookupBias, swz), result, errorMask);
	}
}

vk::VkImageSubresourceRange resolveSubresourceRange (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource)
{
	vk::VkImageSubresourceRange	resolved					= subresource;

	if (subresource.levelCount == VK_REMAINING_MIP_LEVELS)
		resolved.levelCount = testTexture.getNumLevels()-subresource.baseMipLevel;

	if (subresource.layerCount == VK_REMAINING_ARRAY_LAYERS)
		resolved.layerCount = testTexture.getArraySize()-subresource.baseArrayLayer;

	return resolved;
}

MovePtr<tcu::Texture1DView> getTexture1DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	DE_ASSERT(subresource.layerCount == 1);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = testTexture.getLevel((int)subresource.baseMipLevel+levelNdx, subresource.baseArrayLayer);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, 0, srcLevel.getWidth(), 1, 1);
	}

	return MovePtr<tcu::Texture1DView>(new tcu::Texture1DView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture1DArrayView> getTexture1DArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture1D*		tex1D		= dynamic_cast<const TestTexture1D*>(&testTexture);
	const TestTexture1DArray*	tex1DArray	= dynamic_cast<const TestTexture1DArray*>(&testTexture);

	DE_ASSERT(!!tex1D != !!tex1DArray);
	DE_ASSERT(tex1DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex1D ? tex1D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex1DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, (int)subresource.baseArrayLayer, 0, srcLevel.getWidth(), (int)subresource.layerCount, 1);
	}

	return MovePtr<tcu::Texture1DArrayView>(new tcu::Texture1DArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture2DView> getTexture2DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture2D*		tex2D		= dynamic_cast<const TestTexture2D*>(&testTexture);
	const TestTexture2DArray*	tex2DArray	= dynamic_cast<const TestTexture2DArray*>(&testTexture);

	DE_ASSERT(subresource.layerCount == 1);
	DE_ASSERT(!!tex2D != !!tex2DArray);
	DE_ASSERT(tex2DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex2D ? tex2D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex2DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), 1);
	}

	return MovePtr<tcu::Texture2DView>(new tcu::Texture2DView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture2DArrayView> getTexture2DArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTexture2D*		tex2D		= dynamic_cast<const TestTexture2D*>(&testTexture);
	const TestTexture2DArray*	tex2DArray	= dynamic_cast<const TestTexture2DArray*>(&testTexture);

	DE_ASSERT(!!tex2D != !!tex2DArray);
	DE_ASSERT(tex2DArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = tex2D ? tex2D->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
															: tex2DArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), (int)subresource.layerCount);
	}

	return MovePtr<tcu::Texture2DArrayView>(new tcu::Texture2DArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::TextureCubeView> getTextureCubeView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const static tcu::CubeFace s_faceMap[tcu::CUBEFACE_LAST] =
	{
		tcu::CUBEFACE_POSITIVE_X,
		tcu::CUBEFACE_NEGATIVE_X,
		tcu::CUBEFACE_POSITIVE_Y,
		tcu::CUBEFACE_NEGATIVE_Y,
		tcu::CUBEFACE_POSITIVE_Z,
		tcu::CUBEFACE_NEGATIVE_Z
	};

	const TestTextureCube*		texCube			= dynamic_cast<const TestTextureCube*>(&testTexture);
	const TestTextureCubeArray*	texCubeArray	= dynamic_cast<const TestTextureCubeArray*>(&testTexture);

	DE_ASSERT(!!texCube != !!texCubeArray);
	DE_ASSERT(subresource.layerCount == 6);
	DE_ASSERT(texCubeArray || subresource.baseArrayLayer == 0);

	levels.resize(subresource.levelCount*tcu::CUBEFACE_LAST);

	for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
	{
		for (int levelNdx = 0; levelNdx < (int)subresource.levelCount; ++levelNdx)
		{
			const tcu::ConstPixelBufferAccess& srcLevel = texCubeArray ? texCubeArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx)
																	   : texCube->getTexture().getLevelFace(levelNdx, s_faceMap[faceNdx]);

			levels[faceNdx*subresource.levelCount + levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer + (texCubeArray ? faceNdx : 0), srcLevel.getWidth(), srcLevel.getHeight(), 1);
		}
	}

	{
		const tcu::ConstPixelBufferAccess*	reordered[tcu::CUBEFACE_LAST];

		for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
			reordered[s_faceMap[faceNdx]] = &levels[faceNdx*subresource.levelCount];

		return MovePtr<tcu::TextureCubeView>(new tcu::TextureCubeView((int)subresource.levelCount, reordered));
	}
}

MovePtr<tcu::TextureCubeArrayView> getTextureCubeArrayView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	const TestTextureCubeArray*	texCubeArray	= dynamic_cast<const TestTextureCubeArray*>(&testTexture);

	DE_ASSERT(texCubeArray);
	DE_ASSERT(subresource.layerCount%6 == 0);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)subresource.levelCount; ++levelNdx)
	{
		const tcu::ConstPixelBufferAccess& srcLevel = texCubeArray->getTexture().getLevel((int)subresource.baseMipLevel+levelNdx);

		levels[levelNdx] = tcu::getSubregion(srcLevel, 0, 0, (int)subresource.baseArrayLayer, srcLevel.getWidth(), srcLevel.getHeight(), (int)subresource.layerCount);
	}

	return MovePtr<tcu::TextureCubeArrayView>(new tcu::TextureCubeArrayView((int)levels.size(), &levels[0]));
}

MovePtr<tcu::Texture3DView> getTexture3DView (const TestTexture& testTexture, const vk::VkImageSubresourceRange& subresource, std::vector<tcu::ConstPixelBufferAccess>& levels)
{
	DE_ASSERT(subresource.baseArrayLayer == 0 && subresource.layerCount == 1);

	levels.resize(subresource.levelCount);

	for (int levelNdx = 0; levelNdx < (int)levels.size(); ++levelNdx)
		levels[levelNdx] = testTexture.getLevel((int)subresource.baseMipLevel+levelNdx, subresource.baseArrayLayer);

	return MovePtr<tcu::Texture3DView>(new tcu::Texture3DView((int)levels.size(), &levels[0]));
}

} // anonymous

tcu::TestStatus ImageSamplingInstance::verifyImage (void)
{
	// \note Color buffer is used to capture coordinates - not sampled texture values
	const tcu::TextureFormat			colorFormat				(tcu::TextureFormat::RGBA, tcu::TextureFormat::FLOAT);
	const tcu::TextureFormat			depthStencilFormat;		// Undefined depth/stencil format.
	const CoordinateCaptureProgram		coordCaptureProgram;
	const rr::Program					rrProgram				= coordCaptureProgram.getReferenceProgram();
	ReferenceRenderer					refRenderer				(m_renderSize.x(), m_renderSize.y(), 1, colorFormat, depthStencilFormat, &rrProgram);

	bool								compareOkAll			= true;

	tcu::Vec4							lookupScale				(1.0f);
	tcu::Vec4							lookupBias				(0.0f);

	getLookupScaleBias(m_imageFormat, lookupScale, lookupBias);

	// Render out coordinates
	{
		const rr::RenderState renderState(refRenderer.getViewportState());
		refRenderer.draw(renderState, rr::PRIMITIVETYPE_TRIANGLES, m_vertices);
	}

	// Verify results
	{
		const tcu::Sampler					sampler			= mapVkSampler(m_samplerParams);
		const float							referenceLod	= de::clamp(m_samplerParams.mipLodBias + m_samplerLod, m_samplerParams.minLod, m_samplerParams.maxLod);
		const float							lodError		= 1.0f / 255.f;
		const tcu::Vec2						lodBounds		(referenceLod - lodError, referenceLod + lodError);
		const vk::VkImageSubresourceRange	subresource		= resolveSubresourceRange(*m_texture, m_subresourceRange);

		const tcu::ConstPixelBufferAccess	coordAccess		= refRenderer.getAccess();
		tcu::TextureLevel					errorMask		(tcu::TextureFormat(tcu::TextureFormat::RGBA, tcu::TextureFormat::UNORM_INT8), (int)m_renderSize.x(), (int)m_renderSize.y());
		const tcu::PixelBufferAccess		errorAccess		= errorMask.getAccess();

		tcu::LookupPrecision				lookupPrecision;

		// Set precision requirements - very low for these tests as
		// the point of the test is not to validate accuracy.
		lookupPrecision.coordBits		= tcu::IVec3(17, 17, 17);
		lookupPrecision.uvwBits			= tcu::IVec3(5, 5, 5);
		lookupPrecision.colorMask		= tcu::BVec4(true);
		lookupPrecision.colorThreshold	= tcu::computeFixedPointThreshold(tcu::IVec4(8, 8, 8, 8)) / swizzleScaleBias(lookupScale, m_componentMapping);

		if (tcu::isSRGB(m_texture->getTextureFormat()))
			lookupPrecision.colorThreshold += tcu::Vec4(4.f / 255.f);

		for (int imgNdx = 0; imgNdx < m_imageCount; ++imgNdx)
		{
			// Read back result image
			UniquePtr<tcu::TextureLevel>		result(readColorAttachment(m_context.getDeviceInterface(),
				m_context.getDevice(),
				m_context.getUniversalQueue(),
				m_context.getUniversalQueueFamilyIndex(),
				m_context.getDefaultAllocator(),
				**m_colorImages[imgNdx],
				m_colorFormat,
				m_renderSize));
			const tcu::ConstPixelBufferAccess	resultAccess = result->getAccess();

			bool								compareOk	 = true;

			switch (m_imageViewType)
			{
				case VK_IMAGE_VIEW_TYPE_1D:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::Texture1DView>				texView(getTexture1DView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::Texture1DArrayView>			texView(getTexture1DArrayView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_2D:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::Texture2DView>				texView(getTexture2DView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::Texture2DArrayView>			texView(getTexture2DArrayView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_CUBE:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::TextureCubeView>				texView(getTextureCubeView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::TextureCubeArrayView>		texView(getTextureCubeArrayView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				case VK_IMAGE_VIEW_TYPE_3D:
				{
					std::vector<tcu::ConstPixelBufferAccess>	levels;
					UniquePtr<tcu::Texture3DView>				texView(getTexture3DView(*m_texture, subresource, levels));

					compareOk = validateResultImage(*texView, sampler, m_componentMapping, coordAccess, lodBounds, lookupPrecision, lookupScale, lookupBias, resultAccess, errorAccess);
					break;
				}

				default:
					DE_ASSERT(false);
			}

			if (!compareOk)
				m_context.getTestContext().getLog()
				<< tcu::TestLog::Image("Result", "Result Image", resultAccess)
				<< tcu::TestLog::Image("ErrorMask", "Error Mask", errorAccess);

			compareOkAll = compareOkAll && compareOk;
		}
	}

	if (compareOkAll)
		return tcu::TestStatus::pass("Result image matches reference");
	else
		return tcu::TestStatus::fail("Image mismatch");
}

} // pipeline
} // vkt
