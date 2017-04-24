/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrTextureProvider.h"

#include "GrCaps.h"
#include "GrTexturePriv.h"
#include "GrResourceCache.h"
#include "GrGpu.h"
#include "../private/GrSingleOwner.h"
#include "SkMathPriv.h"
#include "SkTArray.h"
#include "SkTLazy.h"

const int GrTextureProvider::kMinScratchTextureSize = 16;

#define ASSERT_SINGLE_OWNER \
    SkDEBUGCODE(GrSingleOwner::AutoEnforce debug_SingleOwner(fSingleOwner);)

GrTextureProvider::GrTextureProvider(GrGpu* gpu, GrResourceCache* cache, GrSingleOwner* singleOwner)
    : fCache(cache)
    , fGpu(gpu)
#ifdef SK_DEBUG
    , fSingleOwner(singleOwner)
#endif
    {
}

GrTexture* GrTextureProvider::createMipMappedTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                                     const GrMipLevel* texels, int mipLevelCount,
                                                     uint32_t flags) {
    ASSERT_SINGLE_OWNER

    if (this->isAbandoned()) {
        return nullptr;
    }
    if (mipLevelCount && !texels) {
        return nullptr;
    }
    for (int i = 0; i < mipLevelCount; ++i) {
        if (!texels[i].fPixels) {
            return nullptr;
        }
    }
    if (mipLevelCount > 1 && GrPixelConfigIsSint(desc.fConfig)) {
        return nullptr;
    }
    if ((desc.fFlags & kRenderTarget_GrSurfaceFlag) &&
        !fGpu->caps()->isConfigRenderable(desc.fConfig, desc.fSampleCnt > 0)) {
        return nullptr;
    }
    if (!GrPixelConfigIsCompressed(desc.fConfig)) {
        if (mipLevelCount < 2) {
            flags |= kExact_ScratchTextureFlag | kNoCreate_ScratchTextureFlag;
            if (GrTexture* texture = this->refScratchTexture(desc, flags)) {
                if (!mipLevelCount ||
                    texture->writePixels(0, 0, desc.fWidth, desc.fHeight, desc.fConfig,
                                         texels[0].fPixels, texels[0].fRowBytes)) {
                    if (SkBudgeted::kNo == budgeted) {
                        texture->resourcePriv().makeUnbudgeted();
                    }
                    return texture;
                }
                texture->unref();
            }
        }
    }

    SkTArray<GrMipLevel> texelsShallowCopy(mipLevelCount);
    for (int i = 0; i < mipLevelCount; ++i) {
        texelsShallowCopy.push_back(texels[i]);
    }
    return fGpu->createTexture(desc, budgeted, texelsShallowCopy);
}

GrTexture* GrTextureProvider::createTexture(const GrSurfaceDesc& desc, SkBudgeted budgeted,
                                            const void* srcData, size_t rowBytes, uint32_t flags) {
    GrMipLevel tempTexels;
    GrMipLevel* texels = nullptr;
    int levelCount = 0;
    if (srcData) {
        tempTexels.fPixels = srcData;
        tempTexels.fRowBytes = rowBytes;
        texels = &tempTexels;
        levelCount = 1;
    }
    return this->createMipMappedTexture(desc, budgeted, texels, levelCount, flags);
}

GrTexture* GrTextureProvider::createApproxTexture(const GrSurfaceDesc& desc, uint32_t flags) {
    ASSERT_SINGLE_OWNER
    return this->internalCreateApproxTexture(desc, flags);
}

GrTexture* GrTextureProvider::internalCreateApproxTexture(const GrSurfaceDesc& desc,
                                                          uint32_t scratchFlags) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned()) {
        return nullptr;
    }
    // Currently we don't recycle compressed textures as scratch.
    if (GrPixelConfigIsCompressed(desc.fConfig)) {
        return nullptr;
    } else {
        return this->refScratchTexture(desc, scratchFlags);
    }
}

GrTexture* GrTextureProvider::refScratchTexture(const GrSurfaceDesc& inDesc,
                                                uint32_t flags) {
    ASSERT_SINGLE_OWNER
    SkASSERT(!this->isAbandoned());
    SkASSERT(!GrPixelConfigIsCompressed(inDesc.fConfig));

    SkTCopyOnFirstWrite<GrSurfaceDesc> desc(inDesc);

    if (fGpu->caps()->reuseScratchTextures() || (desc->fFlags & kRenderTarget_GrSurfaceFlag)) {
        if (!(kExact_ScratchTextureFlag & flags)) {
            // bin by pow2 with a reasonable min
            GrSurfaceDesc* wdesc = desc.writable();
            wdesc->fWidth  = SkTMax(kMinScratchTextureSize, GrNextPow2(desc->fWidth));
            wdesc->fHeight = SkTMax(kMinScratchTextureSize, GrNextPow2(desc->fHeight));
        }

        GrScratchKey key;
        GrTexturePriv::ComputeScratchKey(*desc, &key);
        uint32_t scratchFlags = 0;
        if (kNoPendingIO_ScratchTextureFlag & flags) {
            scratchFlags = GrResourceCache::kRequireNoPendingIO_ScratchFlag;
        } else  if (!(desc->fFlags & kRenderTarget_GrSurfaceFlag)) {
            // If it is not a render target then it will most likely be populated by
            // writePixels() which will trigger a flush if the texture has pending IO.
            scratchFlags = GrResourceCache::kPreferNoPendingIO_ScratchFlag;
        }
        GrGpuResource* resource = fCache->findAndRefScratchResource(key,
                                                                   GrSurface::WorstCaseSize(*desc),
                                                                   scratchFlags);
        if (resource) {
            GrSurface* surface = static_cast<GrSurface*>(resource);
            GrRenderTarget* rt = surface->asRenderTarget();
            if (rt && fGpu->caps()->discardRenderTargetSupport()) {
                rt->discard();
            }
            return surface->asTexture();
        }
    }

    if (!(kNoCreate_ScratchTextureFlag & flags)) {
        return fGpu->createTexture(*desc, SkBudgeted::kYes);
    }

    return nullptr;
}

sk_sp<GrTexture> GrTextureProvider::wrapBackendTexture(const GrBackendTextureDesc& desc,
                                                       GrWrapOwnership ownership) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned()) {
        return nullptr;
    }
    return fGpu->wrapBackendTexture(desc, ownership);
}

sk_sp<GrRenderTarget> GrTextureProvider::wrapBackendRenderTarget(
    const GrBackendRenderTargetDesc& desc)
{
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? nullptr
                               : fGpu->wrapBackendRenderTarget(desc, kBorrow_GrWrapOwnership);
}

void GrTextureProvider::assignUniqueKeyToResource(const GrUniqueKey& key, GrGpuResource* resource) {
    ASSERT_SINGLE_OWNER
    if (this->isAbandoned() || !resource) {
        return;
    }
    resource->resourcePriv().setUniqueKey(key);
}

bool GrTextureProvider::existsResourceWithUniqueKey(const GrUniqueKey& key) const {
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? false : fCache->hasUniqueKey(key);
}

GrGpuResource* GrTextureProvider::findAndRefResourceByUniqueKey(const GrUniqueKey& key) {
    ASSERT_SINGLE_OWNER
    return this->isAbandoned() ? nullptr : fCache->findAndRefUniqueResource(key);
}

GrTexture* GrTextureProvider::findAndRefTextureByUniqueKey(const GrUniqueKey& key) {
    ASSERT_SINGLE_OWNER
    GrGpuResource* resource = this->findAndRefResourceByUniqueKey(key);
    if (resource) {
        GrTexture* texture = static_cast<GrSurface*>(resource)->asTexture();
        SkASSERT(texture);
        return texture;
    }
    return NULL;
}
