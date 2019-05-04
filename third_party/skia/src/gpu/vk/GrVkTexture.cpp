/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrVkTexture.h"

#include "GrTexturePriv.h"
#include "GrVkGpu.h"
#include "GrVkImageView.h"
#include "GrVkTextureRenderTarget.h"
#include "GrVkUtil.h"

#include "vk/GrVkTypes.h"

#define VK_CALL(GPU, X) GR_VK_CALL(GPU->vkInterface(), X)

// Because this class is virtually derived from GrSurface we must explicitly call its constructor.
GrVkTexture::GrVkTexture(GrVkGpu* gpu,
                         SkBudgeted budgeted,
                         const GrSurfaceDesc& desc,
                         const GrVkImageInfo& info,
                         sk_sp<GrVkImageLayout> layout,
                         const GrVkImageView* view,
                         GrMipMapsStatus mipMapsStatus)
        : GrSurface(gpu, desc)
        , GrVkImage(info, std::move(layout), GrBackendObjectOwnership::kOwned)
        , INHERITED(gpu, desc, GrTextureType::k2D, mipMapsStatus)
        , fTextureView(view) {
    SkASSERT((GrMipMapsStatus::kNotAllocated == mipMapsStatus) == (1 == info.fLevelCount));
    this->registerWithCache(budgeted);
    if (GrPixelConfigIsCompressed(desc.fConfig)) {
        this->setReadOnly();
    }
}

GrVkTexture::GrVkTexture(GrVkGpu* gpu,
                         Wrapped,
                         const GrSurfaceDesc& desc,
                         const GrVkImageInfo& info,
                         sk_sp<GrVkImageLayout> layout,
                         const GrVkImageView* view,
                         GrMipMapsStatus mipMapsStatus,
                         GrBackendObjectOwnership ownership,
                         GrIOType ioType,
                         bool purgeImmediately)
        : GrSurface(gpu, desc)
        , GrVkImage(info, std::move(layout), ownership)
        , INHERITED(gpu, desc, GrTextureType::k2D, mipMapsStatus)
        , fTextureView(view) {
    SkASSERT((GrMipMapsStatus::kNotAllocated == mipMapsStatus) == (1 == info.fLevelCount));
    if (ioType == kRead_GrIOType) {
        this->setReadOnly();
    }
    this->registerWithCacheWrapped(purgeImmediately);
}

// Because this class is virtually derived from GrSurface we must explicitly call its constructor.
GrVkTexture::GrVkTexture(GrVkGpu* gpu,
                         const GrSurfaceDesc& desc,
                         const GrVkImageInfo& info,
                         sk_sp<GrVkImageLayout> layout,
                         const GrVkImageView* view,
                         GrMipMapsStatus mipMapsStatus,
                         GrBackendObjectOwnership ownership)
        : GrSurface(gpu, desc)
        , GrVkImage(info, layout, ownership)
        , INHERITED(gpu, desc, GrTextureType::k2D, mipMapsStatus)
        , fTextureView(view) {
    SkASSERT((GrMipMapsStatus::kNotAllocated == mipMapsStatus) == (1 == info.fLevelCount));
}

sk_sp<GrVkTexture> GrVkTexture::MakeNewTexture(GrVkGpu* gpu, SkBudgeted budgeted,
                                               const GrSurfaceDesc& desc,
                                               const GrVkImage::ImageDesc& imageDesc,
                                               GrMipMapsStatus mipMapsStatus) {
    SkASSERT(imageDesc.fUsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT);

    GrVkImageInfo info;
    if (!GrVkImage::InitImageInfo(gpu, imageDesc, &info)) {
        return nullptr;
    }

    const GrVkImageView* imageView = GrVkImageView::Create(
            gpu, info.fImage, info.fFormat, GrVkImageView::kColor_Type, info.fLevelCount,
            info.fYcbcrConversionInfo);
    if (!imageView) {
        GrVkImage::DestroyImageInfo(gpu, &info);
        return nullptr;
    }
    sk_sp<GrVkImageLayout> layout(new GrVkImageLayout(info.fImageLayout));

    return sk_sp<GrVkTexture>(new GrVkTexture(gpu, budgeted, desc, info, std::move(layout),
                                              imageView, mipMapsStatus));
}

sk_sp<GrVkTexture> GrVkTexture::MakeWrappedTexture(GrVkGpu* gpu,
                                                   const GrSurfaceDesc& desc,
                                                   GrWrapOwnership wrapOwnership,
                                                   GrIOType ioType,
                                                   bool purgeImmediately,
                                                   const GrVkImageInfo& info,
                                                   sk_sp<GrVkImageLayout> layout) {
    // Wrapped textures require both image and allocation (because they can be mapped)
    SkASSERT(VK_NULL_HANDLE != info.fImage && VK_NULL_HANDLE != info.fAlloc.fMemory);

    const GrVkImageView* imageView = GrVkImageView::Create(
            gpu, info.fImage, info.fFormat, GrVkImageView::kColor_Type, info.fLevelCount,
            info.fYcbcrConversionInfo);
    if (!imageView) {
        return nullptr;
    }

    GrMipMapsStatus mipMapsStatus = info.fLevelCount > 1 ? GrMipMapsStatus::kValid
                                                         : GrMipMapsStatus::kNotAllocated;

    GrBackendObjectOwnership ownership = kBorrow_GrWrapOwnership == wrapOwnership
            ? GrBackendObjectOwnership::kBorrowed : GrBackendObjectOwnership::kOwned;
    return sk_sp<GrVkTexture>(new GrVkTexture(gpu, kWrapped, desc, info, std::move(layout),
                                              imageView, mipMapsStatus, ownership, ioType,
                                              purgeImmediately));
}

GrVkTexture::~GrVkTexture() {
    // either release or abandon should have been called by the owner of this object.
    SkASSERT(!fTextureView);
}

void GrVkTexture::onRelease() {
    // When there is an idle proc, the Resource will call the proc in releaseImage() so
    // we clear it here.
    fIdleProc = nullptr;
    fIdleProcContext = nullptr;

    // we create this and don't hand it off, so we should always destroy it
    if (fTextureView) {
        fTextureView->unref(this->getVkGpu());
        fTextureView = nullptr;
    }

    this->releaseImage(this->getVkGpu());

    INHERITED::onRelease();
}

void GrVkTexture::onAbandon() {
    // When there is an idle proc, the Resource will call the proc in abandonImage() so
    // we clear it here.
    fIdleProc = nullptr;
    fIdleProcContext = nullptr;
    // we create this and don't hand it off, so we should always destroy it
    if (fTextureView) {
        fTextureView->unrefAndAbandon();
        fTextureView = nullptr;
    }

    this->abandonImage();
    INHERITED::onAbandon();
}

GrBackendTexture GrVkTexture::getBackendTexture() const {
    return GrBackendTexture(this->width(), this->height(), fInfo, this->grVkImageLayout());
}

GrVkGpu* GrVkTexture::getVkGpu() const {
    SkASSERT(!this->wasDestroyed());
    return static_cast<GrVkGpu*>(this->getGpu());
}

const GrVkImageView* GrVkTexture::textureView() {
    return fTextureView;
}

void GrVkTexture::setIdleProc(IdleProc proc, void* context) {
    fIdleProc = proc;
    fIdleProcContext = context;
    if (auto* resource = this->resource()) {
        resource->setIdleProc(proc ? this : nullptr, proc, context);
    }
}

void GrVkTexture::becamePurgeable() {
    if (!fIdleProc) {
        return;
    }
    // This is called when the GrTexture is purgeable. However, we need to check whether the
    // Resource is still owned by any command buffers. If it is then it will call the proc.
    auto* resource = this->resource();
    SkASSERT(resource);
    if (resource->isOwnedByCommandBuffer()) {
        return;
    }
    fIdleProc(fIdleProcContext);
    fIdleProc = nullptr;
    fIdleProcContext = nullptr;
    resource->setIdleProc(nullptr, nullptr, nullptr);
}
