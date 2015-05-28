/*
 * Copyright 2010 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkDevice_DEFINED
#define SkDevice_DEFINED

#include "SkRefCnt.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColor.h"
#include "SkImageFilter.h"

class SkClipStack;
class SkDraw;
class SkDrawFilter;
struct SkIRect;
class SkMatrix;
class SkMetaData;
class SkRegion;
struct SkDeviceProperties;
class GrRenderTarget;

class SK_API SkBaseDevice : public SkRefCnt {
public:
    SK_DECLARE_INST_COUNT(SkBaseDevice)

    /**
     *  Construct a new device.
    */
    SkBaseDevice();
    explicit SkBaseDevice(const SkDeviceProperties&);
    virtual ~SkBaseDevice();

    SkMetaData& getMetaData();

    /**
     *  Return ImageInfo for this device. If the canvas is not backed by pixels
     *  (cpu or gpu), then the info's ColorType will be kUnknown_SkColorType.
     */
    virtual SkImageInfo imageInfo() const;

    /**
     *  Return the bounds of the device in the coordinate space of the root
     *  canvas. The root device will have its top-left at 0,0, but other devices
     *  such as those associated with saveLayer may have a non-zero origin.
     */
    void getGlobalBounds(SkIRect* bounds) const {
        SkASSERT(bounds);
        const SkIPoint& origin = this->getOrigin();
        bounds->setXYWH(origin.x(), origin.y(), this->width(), this->height());
    }

    SkIRect getGlobalBounds() const {
        SkIRect bounds;
        this->getGlobalBounds(&bounds);
        return bounds;
    }

    int width() const {
        return this->imageInfo().width();
    }

    int height() const {
        return this->imageInfo().height();
    }

    bool isOpaque() const {
        return this->imageInfo().isOpaque();
    }

    /** Return the bitmap associated with this device. Call this each time you need
        to access the bitmap, as it notifies the subclass to perform any flushing
        etc. before you examine the pixels.
        @param changePixels set to true if the caller plans to change the pixels
        @return the device's bitmap
    */
    const SkBitmap& accessBitmap(bool changePixels);

    bool writePixels(const SkImageInfo&, const void*, size_t rowBytes, int x, int y);

    void* accessPixels(SkImageInfo* info, size_t* rowBytes);

    /**
     * Return the device's associated gpu render target, or NULL.
     */
    virtual GrRenderTarget* accessRenderTarget() { return NULL; }


    /**
     *  Return the device's origin: its offset in device coordinates from
     *  the default origin in its canvas' matrix/clip
     */
    const SkIPoint& getOrigin() const { return fOrigin; }

    /**
     * onAttachToCanvas is invoked whenever a device is installed in a canvas
     * (i.e., setDevice, saveLayer (for the new device created by the save),
     * and SkCanvas' SkBaseDevice & SkBitmap -taking ctors). It allows the
     * devices to prepare for drawing (e.g., locking their pixels, etc.)
     */
    virtual void onAttachToCanvas(SkCanvas*) {
        SkASSERT(!fAttachedToCanvas);
        this->lockPixels();
#ifdef SK_DEBUG
        fAttachedToCanvas = true;
#endif
    };

    /**
     * onDetachFromCanvas notifies a device that it will no longer be drawn to.
     * It gives the device a chance to clean up (e.g., unlock its pixels). It
     * is invoked from setDevice (for the displaced device), restore and
     * possibly from SkCanvas' dtor.
     */
    virtual void onDetachFromCanvas() {
        SkASSERT(fAttachedToCanvas);
        this->unlockPixels();
#ifdef SK_DEBUG
        fAttachedToCanvas = false;
#endif
    };

protected:
    enum TileUsage {
        kPossible_TileUsage,    //!< the created device may be drawn tiled
        kNever_TileUsage,       //!< the created device will never be drawn tiled
    };

    struct TextFlags {
        uint32_t    fFlags;     // SkPaint::getFlags()
    };

    /**
     * Returns the text-related flags, possibly modified based on the state of the
     * device (e.g. support for LCD).
     */
    uint32_t filterTextFlags(const SkPaint&) const;

    virtual bool onShouldDisableLCD(const SkPaint&) const { return false; }

    /**
     *
     *  DEPRECATED: This will be removed in a future change. Device subclasses
     *  should use the matrix and clip from the SkDraw passed to draw functions.
     *
     *  Called with the correct matrix and clip before this device is drawn
     *  to using those settings. If your subclass overrides this, be sure to
     *  call through to the base class as well.
     *
     *  The clipstack is another view of the clip. It records the actual
     *  geometry that went into building the region. It is present for devices
     *  that want to parse it, but is not required: the region is a complete
     *  picture of the current clip. (i.e. if you regionize all of the geometry
     *  in the clipstack, you will arrive at an equivalent region to the one
     *  passed in).
     */
     virtual void setMatrixClip(const SkMatrix&, const SkRegion&,
                                const SkClipStack&) {};

    /** These are called inside the per-device-layer loop for each draw call.
     When these are called, we have already applied any saveLayer operations,
     and are handling any looping from the paint, and any effects from the
     DrawFilter.
     */
    virtual void drawPaint(const SkDraw&, const SkPaint& paint) = 0;
    virtual void drawPoints(const SkDraw&, SkCanvas::PointMode mode, size_t count,
                            const SkPoint[], const SkPaint& paint) = 0;
    virtual void drawRect(const SkDraw&, const SkRect& r,
                          const SkPaint& paint) = 0;
    virtual void drawOval(const SkDraw&, const SkRect& oval,
                          const SkPaint& paint) = 0;
    virtual void drawRRect(const SkDraw&, const SkRRect& rr,
                           const SkPaint& paint) = 0;

    // Default impl calls drawPath()
    virtual void drawDRRect(const SkDraw&, const SkRRect& outer,
                            const SkRRect& inner, const SkPaint&);

    /**
     *  If pathIsMutable, then the implementation is allowed to cast path to a
     *  non-const pointer and modify it in place (as an optimization). Canvas
     *  may do this to implement helpers such as drawOval, by placing a temp
     *  path on the stack to hold the representation of the oval.
     *
     *  If prePathMatrix is not null, it should logically be applied before any
     *  stroking or other effects. If there are no effects on the paint that
     *  affect the geometry/rasterization, then the pre matrix can just be
     *  pre-concated with the current matrix.
     */
    virtual void drawPath(const SkDraw&, const SkPath& path,
                          const SkPaint& paint,
                          const SkMatrix* prePathMatrix = NULL,
                          bool pathIsMutable = false) = 0;
    virtual void drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                            const SkMatrix& matrix, const SkPaint& paint) = 0;
    virtual void drawSprite(const SkDraw&, const SkBitmap& bitmap,
                            int x, int y, const SkPaint& paint) = 0;

    /**
     *  The default impl. will create a bitmap-shader from the bitmap,
     *  and call drawRect with it.
     */
    virtual void drawBitmapRect(const SkDraw&, const SkBitmap&,
                                const SkRect* srcOrNull, const SkRect& dst,
                                const SkPaint& paint,
                                SkCanvas::DrawBitmapRectFlags flags) = 0;

    /**
     *  Does not handle text decoration.
     *  Decorations (underline and stike-thru) will be handled by SkCanvas.
     */
    virtual void drawText(const SkDraw&, const void* text, size_t len,
                          SkScalar x, SkScalar y, const SkPaint& paint) = 0;
    virtual void drawPosText(const SkDraw&, const void* text, size_t len,
                             const SkScalar pos[], int scalarsPerPos,
                             const SkPoint& offset, const SkPaint& paint) = 0;
    virtual void drawVertices(const SkDraw&, SkCanvas::VertexMode, int vertexCount,
                              const SkPoint verts[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint) = 0;
    // default implementation unrolls the blob runs.
    virtual void drawTextBlob(const SkDraw&, const SkTextBlob*, SkScalar x, SkScalar y,
                              const SkPaint& paint, SkDrawFilter* drawFilter);
    // default implementation calls drawVertices
    virtual void drawPatch(const SkDraw&, const SkPoint cubics[12], const SkColor colors[4],
                           const SkPoint texCoords[4], SkXfermode* xmode, const SkPaint& paint);
    /** The SkDevice passed will be an SkDevice which was returned by a call to
        onCreateDevice on this device with kNeverTile_TileExpectation.
     */
    virtual void drawDevice(const SkDraw&, SkBaseDevice*, int x, int y,
                            const SkPaint&) = 0;

    virtual void drawTextOnPath(const SkDraw&, const void* text, size_t len, const SkPath&,
                                const SkMatrix*, const SkPaint&);
    bool readPixels(const SkImageInfo&, void* dst, size_t rowBytes, int x, int y);

    ///////////////////////////////////////////////////////////////////////////

    /** Update as needed the pixel value in the bitmap, so that the caller can
        access the pixels directly.
        @return The device contents as a bitmap
    */
    virtual const SkBitmap& onAccessBitmap() = 0;

    /** Called when this device is installed into a Canvas. Balanced by a call
        to unlockPixels() when the device is removed from a Canvas.
    */
    virtual void lockPixels() {}
    virtual void unlockPixels() {}

    /**
     *  Override and return true for filters that the device can handle
     *  intrinsically. Doing so means that SkCanvas will pass-through this
     *  filter to drawSprite and drawDevice (and potentially filterImage).
     *  Returning false means the SkCanvas will have apply the filter itself,
     *  and just pass the resulting image to the device.
     */
    virtual bool canHandleImageFilter(const SkImageFilter*) { return false; }

    /**
     *  Related (but not required) to canHandleImageFilter, this method returns
     *  true if the device could apply the filter to the src bitmap and return
     *  the result (and updates offset as needed).
     *  If the device does not recognize or support this filter,
     *  it just returns false and leaves result and offset unchanged.
     */
    virtual bool filterImage(const SkImageFilter*, const SkBitmap&,
                             const SkImageFilter::Context&,
                             SkBitmap* /*result*/, SkIPoint* /*offset*/) {
        return false;
    }

protected:
    // default impl returns NULL
    virtual SkSurface* newSurface(const SkImageInfo&, const SkSurfaceProps&);

    // default impl returns NULL
    virtual const void* peekPixels(SkImageInfo*, size_t* rowBytes);

    /**
     *  The caller is responsible for "pre-clipping" the dst. The impl can assume that the dst
     *  image at the specified x,y offset will fit within the device's bounds.
     *
     *  This is explicitly asserted in readPixels(), the public way to call this.
     */
    virtual bool onReadPixels(const SkImageInfo&, void*, size_t, int x, int y);

    /**
     *  The caller is responsible for "pre-clipping" the src. The impl can assume that the src
     *  image at the specified x,y offset will fit within the device's bounds.
     *
     *  This is explicitly asserted in writePixelsDirect(), the public way to call this.
     */
    virtual bool onWritePixels(const SkImageInfo&, const void*, size_t, int x, int y);

    /**
     *  Default impl returns NULL.
     */
    virtual void* onAccessPixels(SkImageInfo* info, size_t* rowBytes);

    /**
     *  Leaky properties are those which the device should be applying but it isn't.
     *  These properties will be applied by the draw, when and as it can.
     *  If the device does handle a property, that property should be set to the identity value
     *  for that property, effectively making it non-leaky.
     */
    const SkDeviceProperties& getLeakyProperties() const {
        return *fLeakyProperties;
    }

    /**
     *  PRIVATE / EXPERIMENTAL -- do not call
     *  This entry point gives the backend an opportunity to take over the rendering
     *  of 'picture'. If optimization data is available (due to an earlier
     *  'optimize' call) this entry point should make use of it and return true
     *  if all rendering has been done. If false is returned, SkCanvas will
     *  perform its own rendering pass. It is acceptable for the backend
     *  to perform some device-specific warm up tasks and then let SkCanvas
     *  perform the main rendering loop (by return false from here).
     */
    virtual bool EXPERIMENTAL_drawPicture(SkCanvas*, const SkPicture*, const SkMatrix*,
                                          const SkPaint*);

    struct CreateInfo {
        static SkPixelGeometry AdjustGeometry(const SkImageInfo&, TileUsage, SkPixelGeometry);

        // The constructor may change the pixel geometry based on other parameters.
        CreateInfo(const SkImageInfo& info, TileUsage tileUsage, SkPixelGeometry geo)
            : fInfo(info)
            , fTileUsage(tileUsage)
            , fPixelGeometry(AdjustGeometry(info, tileUsage, geo))
        {}

        const SkImageInfo       fInfo;
        const TileUsage         fTileUsage;
        const SkPixelGeometry   fPixelGeometry;
    };

    /**
     *  Create a new device based on CreateInfo. If the paint is not null, then it represents a
     *  preview of how the new device will be composed with its creator device (this).
     */
    virtual SkBaseDevice* onCreateDevice(const CreateInfo&, const SkPaint*) {
        return NULL;
    }

    virtual void initForRootLayer(SkPixelGeometry geo);

private:
    friend class SkCanvas;
    friend struct DeviceCM; //for setMatrixClip
    friend class SkDraw;
    friend class SkDrawIter;
    friend class SkDeviceFilteredPaint;
    friend class SkDeviceImageFilterProxy;
    friend class SkDeferredDevice;    // for newSurface
    friend class SkNoPixelsBitmapDevice;

    friend class SkSurface_Raster;

    // used to change the backend's pixels (and possibly config/rowbytes)
    // but cannot change the width/height, so there should be no change to
    // any clip information.
    // TODO: move to SkBitmapDevice
    virtual void replaceBitmapBackendForRasterSurface(const SkBitmap&) {}

    virtual bool forceConservativeRasterClip() const { return false; }

    // just called by SkCanvas when built as a layer
    void setOrigin(int x, int y) { fOrigin.set(x, y); }

    /** Causes any deferred drawing to the device to be completed.
     */
    virtual void flush() {}

    virtual SkImageFilter::Cache* getImageFilterCache() { return NULL; }

    SkIPoint    fOrigin;
    SkMetaData* fMetaData;
    SkDeviceProperties* fLeakyProperties;   // will always exist.

#ifdef SK_DEBUG
    bool        fAttachedToCanvas;
#endif

    typedef SkRefCnt INHERITED;
};

#endif
