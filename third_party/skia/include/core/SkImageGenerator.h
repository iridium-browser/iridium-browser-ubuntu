/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkImageGenerator_DEFINED
#define SkImageGenerator_DEFINED

#include "SkBitmap.h"
#include "SkColor.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkYUVSizeInfo.h"

class GrContext;
class GrContextThreadSafeProxy;
class GrTexture;
class GrSamplerParams;
class SkBitmap;
class SkData;
class SkMatrix;
class SkPaint;
class SkPicture;

class SK_API SkImageGenerator : public SkNoncopyable {
public:
    /**
     *  The PixelRef which takes ownership of this SkImageGenerator
     *  will call the image generator's destructor.
     */
    virtual ~SkImageGenerator() { }

    uint32_t uniqueID() const { return fUniqueID; }

    /**
     *  Return a ref to the encoded (i.e. compressed) representation,
     *  of this data. If the GrContext is non-null, then the caller is only interested in
     *  gpu-specific formats, so the impl may return null even if they have encoded data,
     *  assuming they know it is not suitable for the gpu.
     *
     *  If non-NULL is returned, the caller is responsible for calling
     *  unref() on the data when it is finished.
     */
    SkData* refEncodedData(GrContext* ctx = nullptr) {
        return this->onRefEncodedData(ctx);
    }

    /**
     *  Return the ImageInfo associated with this generator.
     */
    const SkImageInfo& getInfo() const { return fInfo; }

    /**
     *  Decode into the given pixels, a block of memory of size at
     *  least (info.fHeight - 1) * rowBytes + (info.fWidth *
     *  bytesPerPixel)
     *
     *  Repeated calls to this function should give the same results,
     *  allowing the PixelRef to be immutable.
     *
     *  @param info A description of the format (config, size)
     *         expected by the caller.  This can simply be identical
     *         to the info returned by getInfo().
     *
     *         This contract also allows the caller to specify
     *         different output-configs, which the implementation can
     *         decide to support or not.
     *
     *         A size that does not match getInfo() implies a request
     *         to scale. If the generator cannot perform this scale,
     *         it will return kInvalidScale.
     *
     *  If info is kIndex8_SkColorType, then the caller must provide storage for up to 256
     *  SkPMColor values in ctable. On success the generator must copy N colors into that storage,
     *  (where N is the logical number of table entries) and set ctableCount to N.
     *
     *  If info is not kIndex8_SkColorType, then the last two parameters may be NULL. If ctableCount
     *  is not null, it will be set to 0.
     *
     *  @return true on success.
     */
    bool getPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                   SkPMColor ctable[], int* ctableCount);

    /**
     *  Simplified version of getPixels() that asserts that info is NOT kIndex8_SkColorType and
     *  uses the default Options.
     */
    bool getPixels(const SkImageInfo& info, void* pixels, size_t rowBytes);

    /**
     *  If decoding to YUV is supported, this returns true.  Otherwise, this
     *  returns false and does not modify any of the parameters.
     *
     *  @param sizeInfo   Output parameter indicating the sizes and required
     *                    allocation widths of the Y, U, and V planes.
     *  @param colorSpace Output parameter.
     */
    bool queryYUV8(SkYUVSizeInfo* sizeInfo, SkYUVColorSpace* colorSpace) const;

    /**
     *  Returns true on success and false on failure.
     *  This always attempts to perform a full decode.  If the client only
     *  wants size, it should call queryYUV8().
     *
     *  @param sizeInfo   Needs to exactly match the values returned by the
     *                    query, except the WidthBytes may be larger than the
     *                    recommendation (but not smaller).
     *  @param planes     Memory for each of the Y, U, and V planes.
     */
    bool getYUV8Planes(const SkYUVSizeInfo& sizeInfo, void* planes[3]);

    /**
     *  If the generator can natively/efficiently return its pixels as a GPU image (backed by a
     *  texture) this will return that image. If not, this will return NULL.
     *
     *  This routine also supports retrieving only a subset of the pixels. That subset is specified
     *  by the following rectangle:
     *
     *      subset = SkIRect::MakeXYWH(origin.x(), origin.y(), info.width(), info.height())
     *
     *  If subset is not contained inside the generator's bounds, this returns false.
     *
     *      whole = SkIRect::MakeWH(getInfo().width(), getInfo().height())
     *      if (!whole.contains(subset)) {
     *          return false;
     *      }
     *
     *  Regarding the GrContext parameter:
     *
     *  The caller may pass NULL for the context. In that case the generator may assume that its
     *  internal context is current. If it has no internal context, then it should just return
     *  null.
     *
     *  If the caller passes a non-null context, then the generator should only succeed if:
     *  - it has no intrinsic context, and will use the caller's
     *  - its internal context is the same
     *  - it can somehow convert its texture into one that is valid for the provided context.
     */
    GrTexture* generateTexture(GrContext*, const SkImageInfo& info, const SkIPoint& origin);

    struct SupportedSizes {
        SkISize fSizes[2];
    };

    /**
     *  Some generators can efficiently scale their contents. If this is supported, the generator
     *  may only support certain scaled dimensions. Call this with the desired scale factor,
     *  and it will return true if scaling is supported, and in supportedSizes[] it will return
     *  the nearest supported dimensions.
     *
     *  If no native scaling is supported, or scale is invalid (e.g. scale <= 0 || scale > 1)
     *  this will return false, and the supportedsizes will be undefined.
     */
    bool computeScaledDimensions(SkScalar scale, SupportedSizes*);

    /**
     *  Copy the pixels from this generator into the provided pixmap, respecting
     *  all of the pixmap's attributes: dimensions, colortype, alphatype, colorspace.
     *  returns true on success.
     *
     *  Some generators can only scale to certain dimensions (e.g. powers-of-2 smaller).
     *  Thus a generator may fail (return false) for some sizes but succeed for other sizes.
     *  Call computeScaledDimensions() to know, for a given requested scale, what output size(s)
     *  the generator might support.
     *
     *  Note: this call does NOT allocate the memory for the pixmap; that must be done
     *  by the caller.
     */
    bool generateScaledPixels(const SkPixmap& scaledPixels);

    /**
     *  External generator API: provides efficient access to externally-managed image data.
     *
     *  Skia calls accessScaledPixels() during rasterization, to gain temporary access to
     *  the external pixel data.  When done, the provided callback is invoked to release the
     *  associated resources.
     *
     *  @param srcRect     the source rect in use for the current draw
     *  @param totalMatrix full matrix in effect (mapping srcRect -> device space)
     *  @param quality     the SkFilterQuality requested for rasterization.
     *  @param rec         out param, expected to be set when the call succeeds:
     *
     *                       - fPixmap      external pixel data
     *                       - fSrcRect     is an adjusted srcRect
     *                       - fQuality     is the adjusted filter quality
     *                       - fReleaseProc pixmap release callback, same signature as the
     *                                      SkBitmap::installPixels() callback
     *                       - fReleaseCtx  opaque release context argument
     *
     *  @return            true on success, false otherwise (error or if this API is not supported;
     *                     in this case Skia will fall back to its internal scaling and caching
     *                     heuristics)
     *
     *  Implementors can return pixmaps with a different size than requested, by adjusting the
     *  src rect.  The contract is that Skia will observe the adjusted src rect, and will map it
     *  to the same dest as the original draw (the impl doesn't get to control the destination).
     *
     */

    struct ScaledImageRec {
        SkPixmap        fPixmap;
        SkRect          fSrcRect;
        SkFilterQuality fQuality;

        using ReleaseProcT = void (*)(void* pixels, void* releaseCtx);

        ReleaseProcT    fReleaseProc;
        void*           fReleaseCtx;
    };

    bool accessScaledImage(const SkRect& srcRect, const SkMatrix& totalMatrix,
                           SkFilterQuality quality, ScaledImageRec* rec);

    /**
     *  If the default image decoder system can interpret the specified (encoded) data, then
     *  this returns a new ImageGenerator for it. Otherwise this returns NULL. Either way
     *  the caller is still responsible for managing their ownership of the data.
     */
    static SkImageGenerator* NewFromEncoded(SkData*);

    /** Return a new image generator backed by the specified picture.  If the size is empty or
     *  the picture is NULL, this returns NULL.
     *  The optional matrix and paint arguments are passed to drawPicture() at rasterization
     *  time.
     */
    static SkImageGenerator* NewFromPicture(const SkISize&, const SkPicture*, const SkMatrix*,
                                            const SkPaint*, SkImage::BitDepth, sk_sp<SkColorSpace>);

    bool tryGenerateBitmap(SkBitmap* bm, const SkImageInfo& info, SkBitmap::Allocator* allocator);

protected:
    enum {
        kNeedNewImageUniqueID = 0
    };

    SkImageGenerator(const SkImageInfo& info, uint32_t uniqueId = kNeedNewImageUniqueID);

    virtual SkData* onRefEncodedData(GrContext* ctx);

    virtual bool onGetPixels(const SkImageInfo& info, void* pixels, size_t rowBytes,
                             SkPMColor ctable[], int* ctableCount);

    virtual bool onQueryYUV8(SkYUVSizeInfo*, SkYUVColorSpace*) const {
        return false;
    }
    virtual bool onGetYUV8Planes(const SkYUVSizeInfo&, void*[3] /*planes*/) {
        return false;
    }

    virtual GrTexture* onGenerateTexture(GrContext*, const SkImageInfo&, const SkIPoint&) {
        return nullptr;
    }

    virtual bool onComputeScaledDimensions(SkScalar, SupportedSizes*) {
        return false;
    }
    virtual bool onGenerateScaledPixels(const SkPixmap&) {
        return false;
    }

    virtual bool onAccessScaledImage(const SkRect&, const SkMatrix&, SkFilterQuality,
                                     ScaledImageRec*) {
        return false;
    }

private:
    const SkImageInfo fInfo;
    const uint32_t fUniqueID;

    // This is our default impl, which may be different on different platforms.
    // It is called from NewFromEncoded() after it has checked for any runtime factory.
    // The SkData will never be NULL, as that will have been checked by NewFromEncoded.
    static SkImageGenerator* NewFromEncodedImpl(SkData*);
};

#endif  // SkImageGenerator_DEFINED
