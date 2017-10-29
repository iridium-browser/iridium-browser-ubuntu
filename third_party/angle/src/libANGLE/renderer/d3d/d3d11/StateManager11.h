//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManager11.h: Defines a class for caching D3D11 state

#ifndef LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
#define LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_

#include <array>

#include "libANGLE/angletypes.h"
#include "libANGLE/ContextState.h"
#include "libANGLE/State.h"
#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/renderer/d3d/d3d11/Query11.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"

namespace rx
{

struct RenderTargetDesc;
struct Renderer11DeviceCaps;

struct dx_VertexConstants11
{
    float depthRange[4];
    float viewAdjust[4];
    float viewCoords[4];
    float viewScale[4];
};

struct dx_PixelConstants11
{
    float depthRange[4];
    float viewCoords[4];
    float depthFront[4];
    float viewScale[4];
};

struct dx_ComputeConstants11
{
    unsigned int numWorkGroups[3];
    unsigned int padding;  // This just pads the struct to 16 bytes
};

class StateManager11 final : angle::NonCopyable
{
  public:
    StateManager11(Renderer11 *renderer);
    ~StateManager11();

    void initialize(const gl::Caps &caps);
    void deinitialize();

    void syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits);

    const dx_VertexConstants11 &getVertexConstants() const { return mVertexConstants; }
    const dx_PixelConstants11 &getPixelConstants() const { return mPixelConstants; }
    const dx_ComputeConstants11 &getComputeConstants() const { return mComputeConstants; }

    void setComputeConstants(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ);

    void updateStencilSizeIfChanged(bool depthStencilInitialized, unsigned int stencilSize);

    void setShaderResource(gl::SamplerType shaderType,
                           UINT resourceSlot,
                           ID3D11ShaderResourceView *srv);
    gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd);

    // Checks are done on a framebuffer state change to trigger other state changes.
    // The Context is allowed to be nullptr for these methods, when called in EGL init code.
    void invalidateRenderTarget(const gl::Context *context);
    void invalidateBoundViews(const gl::Context *context);
    void invalidateVertexBuffer();
    void invalidateEverything(const gl::Context *context);

    void setOneTimeRenderTarget(const gl::Context *context,
                                ID3D11RenderTargetView *rtv,
                                ID3D11DepthStencilView *dsv);
    void setOneTimeRenderTargets(const gl::Context *context,
                                 ID3D11RenderTargetView **rtvs,
                                 UINT numRtvs,
                                 ID3D11DepthStencilView *dsv);

    void onBeginQuery(Query11 *query);
    void onDeleteQueryObject(Query11 *query);
    gl::Error onMakeCurrent(const gl::Context *context);

    gl::Error updateCurrentValueAttribs(const gl::State &state,
                                        VertexDataManager *vertexDataManager);

    const std::vector<TranslatedAttribute> &getCurrentValueAttribs() const;

    void setInputLayout(const d3d11::InputLayout *inputLayout);

    // TODO(jmadill): Migrate to d3d11::Buffer.
    bool queueVertexBufferChange(size_t bufferIndex,
                                 ID3D11Buffer *buffer,
                                 UINT stride,
                                 UINT offset);
    bool queueVertexOffsetChange(size_t bufferIndex, UINT offsetOnly);
    void applyVertexBufferChanges();

    void setSingleVertexBuffer(const d3d11::Buffer *buffer, UINT stride, UINT offset);

    gl::Error updateState(const gl::Context *context, GLenum drawMode);

    void setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY primitiveTopology);

    void setDrawShaders(const d3d11::VertexShader *vertexShader,
                        const d3d11::GeometryShader *geometryShader,
                        const d3d11::PixelShader *pixelShader);
    void setVertexShader(const d3d11::VertexShader *shader);
    void setGeometryShader(const d3d11::GeometryShader *shader);
    void setPixelShader(const d3d11::PixelShader *shader);
    void setComputeShader(const d3d11::ComputeShader *shader);

  private:
    void unsetConflictingSRVs(gl::SamplerType shaderType,
                              uintptr_t resource,
                              const gl::ImageIndex &index);
    void unsetConflictingAttachmentResources(const gl::FramebufferAttachment *attachment,
                                             ID3D11Resource *resource);

    gl::Error syncBlendState(const gl::Context *context,
                             const gl::Framebuffer *framebuffer,
                             const gl::BlendState &blendState,
                             const gl::ColorF &blendColor,
                             unsigned int sampleMask);

    gl::Error syncDepthStencilState(const gl::State &glState);

    gl::Error syncRasterizerState(const gl::Context *context, bool pointDrawMode);

    void syncScissorRectangle(const gl::Rectangle &scissor, bool enabled);

    void syncViewport(const gl::Caps *caps, const gl::Rectangle &viewport, float zNear, float zFar);

    void checkPresentPath(const gl::Context *context);

    gl::Error syncFramebuffer(const gl::Context *context, gl::Framebuffer *framebuffer);

    enum DirtyBitType
    {
        DIRTY_BIT_RENDER_TARGET,
        DIRTY_BIT_VIEWPORT_STATE,
        DIRTY_BIT_SCISSOR_STATE,
        DIRTY_BIT_RASTERIZER_STATE,
        DIRTY_BIT_BLEND_STATE,
        DIRTY_BIT_DEPTH_STENCIL_STATE,
        DIRTY_BIT_INVALID,
        DIRTY_BIT_MAX = DIRTY_BIT_INVALID,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;

    Renderer11 *mRenderer;

    // Internal dirty bits.
    DirtyBits mInternalDirtyBits;

    // Blend State
    gl::BlendState mCurBlendState;
    gl::ColorF mCurBlendColor;
    unsigned int mCurSampleMask;

    // Currently applied depth stencil state
    gl::DepthStencilState mCurDepthStencilState;
    int mCurStencilRef;
    int mCurStencilBackRef;
    unsigned int mCurStencilSize;
    Optional<bool> mCurDisableDepth;
    Optional<bool> mCurDisableStencil;

    // Currently applied rasterizer state
    gl::RasterizerState mCurRasterState;

    // Currently applied scissor rectangle state
    bool mCurScissorEnabled;
    gl::Rectangle mCurScissorRect;

    // Currently applied viewport state
    gl::Rectangle mCurViewport;
    float mCurNear;
    float mCurFar;

    // Things needed in viewport state
    dx_VertexConstants11 mVertexConstants;
    dx_PixelConstants11 mPixelConstants;

    dx_ComputeConstants11 mComputeConstants;

    // Render target variables
    gl::Extents mViewportBounds;

    // EGL_ANGLE_experimental_present_path variables
    bool mCurPresentPathFastEnabled;
    int mCurPresentPathFastColorBufferHeight;

    // Queries that are currently active in this state
    std::set<Query11 *> mCurrentQueries;

    // Currently applied textures
    struct SRVRecord
    {
        uintptr_t srv;
        uintptr_t resource;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    };

    // A cache of current SRVs that also tracks the highest 'used' (non-NULL) SRV
    // We might want to investigate a more robust approach that is also fast when there's
    // a large gap between used SRVs (e.g. if SRV 0 and 7 are non-NULL, this approach will
    // waste time on SRVs 1-6.)
    class SRVCache : angle::NonCopyable
    {
      public:
        SRVCache() : mHighestUsedSRV(0) {}

        void initialize(size_t size) { mCurrentSRVs.resize(size); }

        size_t size() const { return mCurrentSRVs.size(); }
        size_t highestUsed() const { return mHighestUsedSRV; }

        const SRVRecord &operator[](size_t index) const { return mCurrentSRVs[index]; }
        void clear();
        void update(size_t resourceIndex, ID3D11ShaderResourceView *srv);

      private:
        std::vector<SRVRecord> mCurrentSRVs;
        size_t mHighestUsedSRV;
    };

    SRVCache mCurVertexSRVs;
    SRVCache mCurPixelSRVs;

    // A block of NULL pointers, cached so we don't re-allocate every draw call
    std::vector<ID3D11ShaderResourceView *> mNullSRVs;

    // Current translations of "Current-Value" data - owned by Context, not VertexArray.
    gl::AttributesMask mDirtyCurrentValueAttribs;
    std::vector<TranslatedAttribute> mCurrentValueAttribs;

    // Current applied input layout.
    ResourceSerial mCurrentInputLayout;

    // Current applied vertex states.
    // TODO(jmadill): Figure out how to use ResourceSerial here.
    std::array<ID3D11Buffer *, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexBuffers;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexStrides;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexOffsets;
    gl::RangeUI mDirtyVertexBufferRange;

    // Currently applied primitive topology
    D3D11_PRIMITIVE_TOPOLOGY mCurrentPrimitiveTopology;

    // Currently applied shaders
    ResourceSerial mAppliedVertexShader;
    ResourceSerial mAppliedGeometryShader;
    ResourceSerial mAppliedPixelShader;
    ResourceSerial mAppliedComputeShader;
};

}  // namespace rx
#endif  // LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
