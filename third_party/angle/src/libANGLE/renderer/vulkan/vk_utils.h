//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vk_utils:
//    Helper functions for the Vulkan Renderer.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
#define LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_

#include <limits>

#include <vulkan/vulkan.h>

#include "common/Optional.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/renderer_utils.h"

#define ANGLE_GL_OBJECTS_X(PROC) \
    PROC(Buffer)                 \
    PROC(Context)                \
    PROC(Framebuffer)            \
    PROC(Program)                \
    PROC(Texture)                \
    PROC(VertexArray)

#define ANGLE_PRE_DECLARE_OBJECT(OBJ) class OBJ;

namespace egl
{
class Display;
}

namespace gl
{
struct Box;
struct Extents;
struct RasterizerState;
struct Rectangle;
struct VertexAttribute;
class VertexBinding;

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_OBJECT);
}

#define ANGLE_PRE_DECLARE_VK_OBJECT(OBJ) class OBJ##Vk;

namespace rx
{
class DisplayVk;
class RenderTargetVk;
class RendererVk;
class ResourceVk;
class RenderPassCache;
class StreamingBuffer;

enum class DrawType
{
    Arrays,
    Elements,
};

ANGLE_GL_OBJECTS_X(ANGLE_PRE_DECLARE_VK_OBJECT);

const char *VulkanResultString(VkResult result);
// Verify that validation layers are available.
bool GetAvailableValidationLayers(const std::vector<VkLayerProperties> &layerProps,
                                  bool mustHaveLayers,
                                  const char *const **enabledLayerNames,
                                  uint32_t *enabledLayerCount);

extern const char *g_VkLoaderLayersPathEnv;

enum class TextureDimension
{
    TEX_2D,
    TEX_CUBE,
    TEX_3D,
    TEX_2D_ARRAY,
};

namespace vk
{
class CommandGraphNode;
struct Format;

template <typename T>
struct ImplTypeHelper;

// clang-format off
#define ANGLE_IMPL_TYPE_HELPER_GL(OBJ) \
template<>                             \
struct ImplTypeHelper<gl::OBJ>         \
{                                      \
    using ImplType = OBJ##Vk;          \
};
// clang-format on

ANGLE_GL_OBJECTS_X(ANGLE_IMPL_TYPE_HELPER_GL)

template <>
struct ImplTypeHelper<egl::Display>
{
    using ImplType = DisplayVk;
};

template <typename T>
using GetImplType = typename ImplTypeHelper<T>::ImplType;

template <typename T>
GetImplType<T> *GetImpl(const T *glObject)
{
    return GetImplAs<GetImplType<T>>(glObject);
}

class Error final
{
  public:
    Error(VkResult result);
    Error(VkResult result, const char *file, unsigned int line);
    ~Error();

    Error(const Error &other);
    Error &operator=(const Error &other);

    gl::Error toGL(GLenum glErrorCode) const;
    egl::Error toEGL(EGLint eglErrorCode) const;

    operator gl::Error() const;
    operator egl::Error() const;

    template <typename T>
    operator gl::ErrorOrResult<T>() const
    {
        return operator gl::Error();
    }

    bool isError() const;

    std::string toString() const;

  private:
    VkResult mResult;
    const char *mFile;
    unsigned int mLine;
};

template <typename ResultT>
using ErrorOrResult = angle::ErrorOrResultBase<Error, ResultT, VkResult, VK_SUCCESS>;

// Avoid conflicting with X headers which define "Success".
inline Error NoError()
{
    return Error(VK_SUCCESS);
}

// Unimplemented handle types:
// Instance
// PhysicalDevice
// Device
// Queue
// Event
// QueryPool
// BufferView
// DescriptorSet
// PipelineCache

#define ANGLE_HANDLE_TYPES_X(FUNC) \
    FUNC(Semaphore)                \
    FUNC(CommandBuffer)            \
    FUNC(Fence)                    \
    FUNC(DeviceMemory)             \
    FUNC(Buffer)                   \
    FUNC(Image)                    \
    FUNC(ImageView)                \
    FUNC(ShaderModule)             \
    FUNC(PipelineLayout)           \
    FUNC(RenderPass)               \
    FUNC(Pipeline)                 \
    FUNC(DescriptorSetLayout)      \
    FUNC(Sampler)                  \
    FUNC(DescriptorPool)           \
    FUNC(Framebuffer)              \
    FUNC(CommandPool)

#define ANGLE_COMMA_SEP_FUNC(TYPE) TYPE,

enum class HandleType
{
    Invalid,
    ANGLE_HANDLE_TYPES_X(ANGLE_COMMA_SEP_FUNC)
};

#undef ANGLE_COMMA_SEP_FUNC

#define ANGLE_PRE_DECLARE_CLASS_FUNC(TYPE) class TYPE;
ANGLE_HANDLE_TYPES_X(ANGLE_PRE_DECLARE_CLASS_FUNC)
#undef ANGLE_PRE_DECLARE_CLASS_FUNC

// Returns the HandleType of a Vk Handle.
template <typename T>
struct HandleTypeHelper;

// clang-format off
#define ANGLE_HANDLE_TYPE_HELPER_FUNC(TYPE)                     \
template<> struct HandleTypeHelper<TYPE>                        \
{                                                               \
    constexpr static HandleType kHandleType = HandleType::TYPE; \
};
// clang-format on

ANGLE_HANDLE_TYPES_X(ANGLE_HANDLE_TYPE_HELPER_FUNC)

#undef ANGLE_HANDLE_TYPE_HELPER_FUNC

class GarbageObject final
{
  public:
    template <typename ObjectT>
    GarbageObject(Serial serial, const ObjectT &object)
        : mSerial(serial),
          mHandleType(HandleTypeHelper<ObjectT>::kHandleType),
          mHandle(reinterpret_cast<VkDevice>(object.getHandle()))
    {
    }

    GarbageObject();
    GarbageObject(const GarbageObject &other);
    GarbageObject &operator=(const GarbageObject &other);

    bool destroyIfComplete(VkDevice device, Serial completedSerial);
    void destroy(VkDevice device);

  private:
    // TODO(jmadill): Since many objects will have the same serial, it might be more efficient to
    // store the serial outside of the garbage object itself. We could index ranges of garbage
    // objects in the Renderer, using a circular buffer.
    Serial mSerial;
    HandleType mHandleType;
    VkDevice mHandle;
};

template <typename DerivedT, typename HandleT>
class WrappedObject : angle::NonCopyable
{
  public:
    HandleT getHandle() const { return mHandle; }
    bool valid() const { return (mHandle != VK_NULL_HANDLE); }

    const HandleT *ptr() const { return &mHandle; }

    void dumpResources(Serial serial, std::vector<vk::GarbageObject> *garbageQueue)
    {
        if (valid())
        {
            garbageQueue->emplace_back(serial, *static_cast<DerivedT *>(this));
            mHandle = VK_NULL_HANDLE;
        }
    }

  protected:
    WrappedObject() : mHandle(VK_NULL_HANDLE) {}
    ~WrappedObject() { ASSERT(!valid()); }

    WrappedObject(WrappedObject &&other) : mHandle(other.mHandle)
    {
        other.mHandle = VK_NULL_HANDLE;
    }

    // Only works to initialize empty objects, since we don't have the device handle.
    WrappedObject &operator=(WrappedObject &&other)
    {
        ASSERT(!valid());
        std::swap(mHandle, other.mHandle);
        return *this;
    }

    HandleT mHandle;
};

class MemoryProperties final : angle::NonCopyable
{
  public:
    MemoryProperties();

    void init(VkPhysicalDevice physicalDevice);
    Error findCompatibleMemoryIndex(const VkMemoryRequirements &memoryRequirements,
                                    VkMemoryPropertyFlags memoryPropertyFlags,
                                    uint32_t *indexOut) const;

  private:
    VkPhysicalDeviceMemoryProperties mMemoryProperties;
};

class CommandPool final : public WrappedObject<CommandPool, VkCommandPool>
{
  public:
    CommandPool();

    void destroy(VkDevice device);

    Error init(VkDevice device, const VkCommandPoolCreateInfo &createInfo);
};

// Helper class that wraps a Vulkan command buffer.
class CommandBuffer : public WrappedObject<CommandBuffer, VkCommandBuffer>
{
  public:
    CommandBuffer();

    VkCommandBuffer releaseHandle();
    void destroy(VkDevice device, const vk::CommandPool &commandPool);
    Error init(VkDevice device, const VkCommandBufferAllocateInfo &createInfo);
    using WrappedObject::operator=;

    Error begin(const VkCommandBufferBeginInfo &info);

    Error end();
    Error reset();

    void singleImageBarrier(VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask,
                            VkDependencyFlags dependencyFlags,
                            const VkImageMemoryBarrier &imageMemoryBarrier);

    void singleBufferBarrier(VkPipelineStageFlags srcStageMask,
                             VkPipelineStageFlags dstStageMask,
                             VkDependencyFlags dependencyFlags,
                             const VkBufferMemoryBarrier &bufferBarrier);

    void clearSingleColorImage(const vk::Image &image, const VkClearColorValue &color);
    void clearSingleDepthStencilImage(const vk::Image &image,
                                      VkImageAspectFlags aspectFlags,
                                      const VkClearDepthStencilValue &depthStencil);

    void clearDepthStencilImage(const vk::Image &image,
                                const VkClearDepthStencilValue &depthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *ranges);

    void copyBuffer(const vk::Buffer &srcBuffer,
                    const vk::Buffer &destBuffer,
                    uint32_t regionCount,
                    const VkBufferCopy *regions);

    void copySingleImage(const vk::Image &srcImage,
                         const vk::Image &destImage,
                         const gl::Box &copyRegion,
                         VkImageAspectFlags aspectMask);

    void copyImage(const vk::Image &srcImage,
                   const vk::Image &dstImage,
                   uint32_t regionCount,
                   const VkImageCopy *regions);

    void beginRenderPass(const VkRenderPassBeginInfo &beginInfo, VkSubpassContents subpassContents);
    void endRenderPass();

    void draw(uint32_t vertexCount,
              uint32_t instanceCount,
              uint32_t firstVertex,
              uint32_t firstInstance);

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount,
                     uint32_t firstIndex,
                     int32_t vertexOffset,
                     uint32_t firstInstance);

    void bindPipeline(VkPipelineBindPoint pipelineBindPoint, const vk::Pipeline &pipeline);
    void bindVertexBuffers(uint32_t firstBinding,
                           uint32_t bindingCount,
                           const VkBuffer *buffers,
                           const VkDeviceSize *offsets);
    void bindIndexBuffer(const vk::Buffer &buffer, VkDeviceSize offset, VkIndexType indexType);
    void bindIndexBuffer(const VkBuffer &buffer, VkDeviceSize offset, VkIndexType indexType);
    void bindDescriptorSets(VkPipelineBindPoint bindPoint,
                            const vk::PipelineLayout &layout,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *descriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *dynamicOffsets);

    void executeCommands(uint32_t commandBufferCount, const vk::CommandBuffer *commandBuffers);
};

class Image final : public WrappedObject<Image, VkImage>
{
  public:
    Image();

    // Use this method if the lifetime of the image is not controlled by ANGLE. (SwapChain)
    void setHandle(VkImage handle);

    // Called on shutdown when the helper class *doesn't* own the handle to the image resource.
    void reset();

    // Called on shutdown when the helper class *does* own the handle to the image resource.
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkImageCreateInfo &createInfo);

    void changeLayoutTop(VkImageAspectFlags aspectMask,
                         VkImageLayout newLayout,
                         CommandBuffer *commandBuffer);

    void changeLayoutWithStages(VkImageAspectFlags aspectMask,
                                VkImageLayout newLayout,
                                VkPipelineStageFlags srcStageMask,
                                VkPipelineStageFlags dstStageMask,
                                CommandBuffer *commandBuffer);

    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *requirementsOut) const;
    Error bindMemory(VkDevice device, const vk::DeviceMemory &deviceMemory);

    VkImageLayout getCurrentLayout() const { return mCurrentLayout; }
    void updateLayout(VkImageLayout layout) { mCurrentLayout = layout; }

  private:
    VkImageLayout mCurrentLayout;
};

class ImageView final : public WrappedObject<ImageView, VkImageView>
{
  public:
    ImageView();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkImageViewCreateInfo &createInfo);
};

class Semaphore final : public WrappedObject<Semaphore, VkSemaphore>
{
  public:
    Semaphore();
    void destroy(VkDevice device);

    Error init(VkDevice device);
};

class Framebuffer final : public WrappedObject<Framebuffer, VkFramebuffer>
{
  public:
    Framebuffer();
    void destroy(VkDevice device);

    // Use this method only in necessary cases. (RenderPass)
    void setHandle(VkFramebuffer handle);

    Error init(VkDevice device, const VkFramebufferCreateInfo &createInfo);
};

class DeviceMemory final : public WrappedObject<DeviceMemory, VkDeviceMemory>
{
  public:
    DeviceMemory();
    void destroy(VkDevice device);

    Error allocate(VkDevice device, const VkMemoryAllocateInfo &allocInfo);
    Error map(VkDevice device,
              VkDeviceSize offset,
              VkDeviceSize size,
              VkMemoryMapFlags flags,
              uint8_t **mapPointer);
    void unmap(VkDevice device);
};

class RenderPass final : public WrappedObject<RenderPass, VkRenderPass>
{
  public:
    RenderPass();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkRenderPassCreateInfo &createInfo);
};

enum class StagingUsage
{
    Read,
    Write,
    Both,
};

class Buffer final : public WrappedObject<Buffer, VkBuffer>
{
  public:
    Buffer();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkBufferCreateInfo &createInfo);
    Error bindMemory(VkDevice device, const DeviceMemory &deviceMemory);
    void getMemoryRequirements(VkDevice device, VkMemoryRequirements *memoryRequirementsOut);
};

class ShaderModule final : public WrappedObject<ShaderModule, VkShaderModule>
{
  public:
    ShaderModule();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkShaderModuleCreateInfo &createInfo);
};

class Pipeline final : public WrappedObject<Pipeline, VkPipeline>
{
  public:
    Pipeline();
    void destroy(VkDevice device);

    Error initGraphics(VkDevice device, const VkGraphicsPipelineCreateInfo &createInfo);
};

class PipelineLayout final : public WrappedObject<PipelineLayout, VkPipelineLayout>
{
  public:
    PipelineLayout();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkPipelineLayoutCreateInfo &createInfo);
};

class DescriptorSetLayout final : public WrappedObject<DescriptorSetLayout, VkDescriptorSetLayout>
{
  public:
    DescriptorSetLayout();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkDescriptorSetLayoutCreateInfo &createInfo);
};

class DescriptorPool final : public WrappedObject<DescriptorPool, VkDescriptorPool>
{
  public:
    DescriptorPool();
    void destroy(VkDevice device);

    Error init(VkDevice device, const VkDescriptorPoolCreateInfo &createInfo);

    Error allocateDescriptorSets(VkDevice device,
                                 const VkDescriptorSetAllocateInfo &allocInfo,
                                 VkDescriptorSet *descriptorSetsOut);
};

class Sampler final : public WrappedObject<Sampler, VkSampler>
{
  public:
    Sampler();
    void destroy(VkDevice device);
    Error init(VkDevice device, const VkSamplerCreateInfo &createInfo);
};

class Fence final : public WrappedObject<Fence, VkFence>
{
  public:
    Fence();
    void destroy(VkDevice fence);
    using WrappedObject::operator=;

    Error init(VkDevice device, const VkFenceCreateInfo &createInfo);
    VkResult getStatus(VkDevice device) const;
};

// Helper class for managing a CPU/GPU transfer Image.
class StagingImage final : angle::NonCopyable
{
  public:
    StagingImage();
    StagingImage(StagingImage &&other);
    void destroy(VkDevice device);

    vk::Error init(ContextVk *contextVk,
                   TextureDimension dimension,
                   const Format &format,
                   const gl::Extents &extent,
                   StagingUsage usage);

    Image &getImage() { return mImage; }
    const Image &getImage() const { return mImage; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    VkDeviceSize getSize() const { return mSize; }

    void dumpResources(Serial serial, std::vector<vk::GarbageObject> *garbageQueue);

  private:
    Image mImage;
    DeviceMemory mDeviceMemory;
    size_t mSize;
};

// Similar to StagingImage, for Buffers.
class StagingBuffer final : angle::NonCopyable
{
  public:
    StagingBuffer();
    void destroy(VkDevice device);

    vk::Error init(ContextVk *contextVk, VkDeviceSize size, StagingUsage usage);

    Buffer &getBuffer() { return mBuffer; }
    const Buffer &getBuffer() const { return mBuffer; }
    DeviceMemory &getDeviceMemory() { return mDeviceMemory; }
    const DeviceMemory &getDeviceMemory() const { return mDeviceMemory; }
    size_t getSize() const { return mSize; }

    void dumpResources(Serial serial, std::vector<vk::GarbageObject> *garbageQueue);

  private:
    Buffer mBuffer;
    DeviceMemory mDeviceMemory;
    size_t mSize;
};

template <typename ObjT>
class ObjectAndSerial final : angle::NonCopyable
{
  public:
    ObjectAndSerial(ObjT &&object, Serial queueSerial)
        : mObject(std::move(object)), mQueueSerial(queueSerial)
    {
    }

    ObjectAndSerial(ObjectAndSerial &&other)
        : mObject(std::move(other.mObject)), mQueueSerial(std::move(other.mQueueSerial))
    {
    }
    ObjectAndSerial &operator=(ObjectAndSerial &&other)
    {
        mObject      = std::move(other.mObject);
        mQueueSerial = std::move(other.mQueueSerial);
        return *this;
    }

    Serial queueSerial() const { return mQueueSerial; }
    void updateSerial(Serial newSerial)
    {
        ASSERT(newSerial >= mQueueSerial);
        mQueueSerial = newSerial;
    }

    const ObjT &get() const { return mObject; }
    ObjT &get() { return mObject; }

    bool valid() const { return mObject.valid(); }

  private:
    ObjT mObject;
    Serial mQueueSerial;
};

Error AllocateBufferMemory(RendererVk *renderer,
                           VkMemoryPropertyFlags memoryPropertyFlags,
                           Buffer *buffer,
                           DeviceMemory *deviceMemoryOut,
                           size_t *requiredSizeOut);

struct BufferAndMemory final : private angle::NonCopyable
{
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

Error AllocateImageMemory(RendererVk *renderer,
                          VkMemoryPropertyFlags memoryPropertyFlags,
                          Image *image,
                          DeviceMemory *deviceMemoryOut,
                          size_t *requiredSizeOut);

// This class responsibility is to bind an indexed buffer needed to support line loops in Vulkan.
// In the setup phase of drawing, the bindLineLoopIndexBuffer method should be called with the
// first/last vertex and the current commandBuffer. If the user wants to draw a loop between [v1,
// v2, v3], we will create an indexed buffer with these indexes: [0, 1, 2, 3, 0] to emulate the
// loop.
class LineLoopHandler final : angle::NonCopyable
{
  public:
    LineLoopHandler();
    ~LineLoopHandler();

    void destroy(VkDevice device);

    gl::Error draw(ContextVk *contextVk, int firstVertex, int count, CommandBuffer *commandBuffer);

  private:
    gl::Error bindLineLoopIndexBuffer(ContextVk *contextVk,
                                      int firstVertex,
                                      int count,
                                      vk::CommandBuffer **commandBuffer);
    std::unique_ptr<StreamingBuffer> mStreamingLineLoopIndicesData;
    VkBuffer mLineLoopIndexBuffer;
    VkDeviceSize mLineLoopIndexBufferOffset;
    Optional<int> mLineLoopBufferFirstIndex;
    Optional<int> mLineLoopBufferLastIndex;
};

}  // namespace vk

namespace gl_vk
{
VkPrimitiveTopology GetPrimitiveTopology(GLenum mode);
VkCullModeFlags GetCullMode(const gl::RasterizerState &rasterState);
VkFrontFace GetFrontFace(GLenum frontFace);
}  // namespace gl_vk

// This is a helper class for back-end objects used in Vk command buffers. It records a serial
// at command recording times indicating an order in the queue. We use Fences to detect when
// commands finish, and then release any unreferenced and deleted resources based on the stored
// queue serial in a special 'garbage' queue. Resources also track current read and write
// dependencies. Only one command buffer node can be writing to the Resource at a time, but many
// can be reading from it. Together the dependencies will form a command graph at submission time.
class ResourceVk
{
  public:
    ResourceVk();
    virtual ~ResourceVk();

    void updateQueueSerial(Serial queueSerial);
    Serial getQueueSerial() const;

    // Returns true if any tracked read or write nodes match 'currentSerial'.
    bool hasCurrentWritingNode(Serial currentSerial) const;

    // Returns the active write node, and asserts 'currentSerial' matches the stored serial.
    vk::CommandGraphNode *getCurrentWritingNode(Serial currentSerial);

    // Allocates a new write node and calls onWriteResource internally.
    vk::CommandGraphNode *getNewWritingNode(RendererVk *renderer);

    // Allocates a write node via getNewWriteNode and returns a started command buffer.
    // The started command buffer will render outside of a RenderPass.
    vk::Error beginWriteResource(RendererVk *renderer, vk::CommandBuffer **commandBufferOut);

    // Sets up dependency relations. 'writingNode' will modify 'this' ResourceVk.
    void onWriteResource(vk::CommandGraphNode *writingNode, Serial serial);

    // Sets up dependency relations. 'readingNode' will read from 'this' ResourceVk.
    void onReadResource(vk::CommandGraphNode *readingNode, Serial serial);

  private:
    Serial mStoredQueueSerial;
    std::vector<vk::CommandGraphNode *> mCurrentReadingNodes;
    vk::CommandGraphNode *mCurrentWritingNode;
};

}  // namespace rx

#define ANGLE_VK_TRY(command)                                          \
    {                                                                  \
        auto ANGLE_LOCAL_VAR = command;                                \
        if (ANGLE_LOCAL_VAR != VK_SUCCESS)                             \
        {                                                              \
            return rx::vk::Error(ANGLE_LOCAL_VAR, __FILE__, __LINE__); \
        }                                                              \
    }                                                                  \
    ANGLE_EMPTY_STATEMENT

#define ANGLE_VK_CHECK(test, error) ANGLE_VK_TRY(test ? VK_SUCCESS : error)

std::ostream &operator<<(std::ostream &stream, const rx::vk::Error &error);

#endif  // LIBANGLE_RENDERER_VULKAN_VK_UTILS_H_
