/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Request.h"

#include "SkPictureRecorder.h"
#include "SkPixelSerializer.h"
#include "SkPM4fPriv.h"
#include "picture_utils.h"

using namespace sk_gpu_test;

static int kDefaultWidth = 1920;
static int kDefaultHeight = 1080;
static int kMaxWidth = 8192;
static int kMaxHeight = 8192;


Request::Request(SkString rootUrl)
    : fUploadContext(nullptr)
    , fUrlDataManager(rootUrl)
    , fGPUEnabled(false)
    , fColorMode(0) {
    // create surface
#if SK_SUPPORT_GPU
    GrContextOptions grContextOpts;
    fContextFactory = new GrContextFactory(grContextOpts);
#else
    fContextFactory = nullptr;
#endif
}

Request::~Request() {
#if SK_SUPPORT_GPU
    if (fContextFactory) {
        delete fContextFactory;
    }
#endif
}

SkBitmap* Request::getBitmapFromCanvas(SkCanvas* canvas) {
    SkBitmap* bmp = new SkBitmap();
    bmp->setInfo(canvas->imageInfo());
    if (!canvas->readPixels(bmp, 0, 0)) {
        fprintf(stderr, "Can't read pixels\n");
        return nullptr;
    }
    return bmp;
}

sk_sp<SkData> Request::writeCanvasToPng(SkCanvas* canvas) {
    // capture pixels
    SkAutoTDelete<SkBitmap> bmp(this->getBitmapFromCanvas(canvas));
    SkASSERT(bmp);

    // Convert to format suitable for PNG output
    sk_sp<SkData> encodedBitmap = sk_tools::encode_bitmap_for_png(*bmp);
    SkASSERT(encodedBitmap.get());

    // write to an opaque png (black background)
    SkDynamicMemoryWStream buffer;
    SkDrawCommand::WritePNG((const png_bytep) encodedBitmap->bytes(), bmp->width(), bmp->height(),
                            buffer, true);
    return sk_sp<SkData>(buffer.copyToData());
}

SkCanvas* Request::getCanvas() {
#if SK_SUPPORT_GPU
    GrContextFactory* factory = fContextFactory;
    GLTestContext* gl = factory->getContextInfo(GrContextFactory::kNativeGL_ContextType,
                                                GrContextFactory::kNone_ContextOptions).glContext();
    if (!gl) {
        gl = factory->getContextInfo(GrContextFactory::kMESA_ContextType,
                                     GrContextFactory::kNone_ContextOptions).glContext();
    }
    if (gl) {
        gl->makeCurrent();
    }
#endif
    SkASSERT(fDebugCanvas);

    // create the appropriate surface if necessary
    if (!fSurface) {
        this->enableGPU(fGPUEnabled);
    }
    SkCanvas* target = fSurface->getCanvas();
    return target;
}

void Request::drawToCanvas(int n, int m) {
    SkCanvas* target = this->getCanvas();
    fDebugCanvas->drawTo(target, n, m);
}

sk_sp<SkData> Request::drawToPng(int n, int m) {
    this->drawToCanvas(n, m);
    return writeCanvasToPng(this->getCanvas());
}

sk_sp<SkData> Request::writeOutSkp() {
    // Playback into picture recorder
    SkIRect bounds = this->getBounds();
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(SkIntToScalar(bounds.width()),
                                               SkIntToScalar(bounds.height()));

    fDebugCanvas->draw(canvas);

    sk_sp<SkPicture> picture(recorder.finishRecordingAsPicture());

    SkDynamicMemoryWStream outStream;

    SkAutoTUnref<SkPixelSerializer> serializer(SkImageEncoder::CreatePixelSerializer());
    picture->serialize(&outStream, serializer);

    return sk_sp<SkData>(outStream.copyToData());
}

GrContext* Request::getContext() {
#if SK_SUPPORT_GPU
    GrContext* result = fContextFactory->get(GrContextFactory::kNativeGL_ContextType,
                                             GrContextFactory::kNone_ContextOptions);
    if (!result) {
        result = fContextFactory->get(GrContextFactory::kMESA_ContextType,
                                      GrContextFactory::kNone_ContextOptions);
    }
    return result;
#else
    return nullptr;
#endif
}

SkIRect Request::getBounds() {
    SkIRect bounds;
    if (fPicture) {
        bounds = fPicture->cullRect().roundOut();
        if (fGPUEnabled) {
#if SK_SUPPORT_GPU
            int maxRTSize = this->getContext()->caps()->maxRenderTargetSize();
            bounds = SkIRect::MakeWH(SkTMin(bounds.width(), maxRTSize),
                                     SkTMin(bounds.height(), maxRTSize));
#endif
        }
    } else {
        bounds = SkIRect::MakeWH(kDefaultWidth, kDefaultHeight);
    }

    // We clip to kMaxWidth / kMaxHeight for performance reasons.
    // TODO make this configurable
    bounds = SkIRect::MakeWH(SkTMin(bounds.width(), kMaxWidth),
                             SkTMin(bounds.height(), kMaxHeight));
    return bounds;
}

namespace {

struct ColorAndProfile {
    SkColorType fColorType;
    bool fSRGB;
};

ColorAndProfile ColorModes[] = {
    { kN32_SkColorType,      false },
    { kN32_SkColorType,       true },
    { kRGBA_F16_SkColorType,  true },
};

}

SkSurface* Request::createCPUSurface() {
    SkIRect bounds = this->getBounds();
    ColorAndProfile cap = ColorModes[fColorMode];
    auto srgbColorSpace = SkColorSpace::NewNamed(SkColorSpace::kSRGB_Named);
    SkImageInfo info = SkImageInfo::Make(bounds.width(), bounds.height(), cap.fColorType,
                                         kPremul_SkAlphaType, cap.fSRGB ? srgbColorSpace : nullptr);
    return SkSurface::MakeRaster(info).release();
}

SkSurface* Request::createGPUSurface() {
    GrContext* context = this->getContext();
    SkIRect bounds = this->getBounds();
    ColorAndProfile cap = ColorModes[fColorMode];
    auto srgbColorSpace = SkColorSpace::NewNamed(SkColorSpace::kSRGB_Named);
    SkImageInfo info = SkImageInfo::Make(bounds.width(), bounds.height(), cap.fColorType,
                                         kPremul_SkAlphaType, cap.fSRGB ? srgbColorSpace : nullptr);
    SkSurface* surface = SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, info).release();
    return surface;
}

bool Request::setColorMode(int mode) {
    fColorMode = mode;
    return enableGPU(fGPUEnabled);
}

bool Request::enableGPU(bool enable) {
    if (enable) {
        SkSurface* surface = this->createGPUSurface();
        if (surface) {
            fSurface.reset(surface);
            fGPUEnabled = true;

            // When we switch to GPU, there seems to be some mystery draws in the canvas.  So we
            // draw once to flush the pipe
            // TODO understand what is actually happening here
            if (fDebugCanvas) {
                fDebugCanvas->drawTo(this->getCanvas(), this->getLastOp());
                this->getCanvas()->flush();
            }

            return true;
        }
        return false;
    }
    fSurface.reset(this->createCPUSurface());
    fGPUEnabled = false;
    return true;
}

bool Request::initPictureFromStream(SkStream* stream) {
    // parse picture from stream
    fPicture = SkPicture::MakeFromStream(stream);
    if (!fPicture) {
        fprintf(stderr, "Could not create picture from stream.\n");
        return false;
    }

    // reinitialize canvas with the new picture dimensions
    this->enableGPU(fGPUEnabled);

    // pour picture into debug canvas
    SkIRect bounds = this->getBounds();
    fDebugCanvas.reset(new SkDebugCanvas(bounds.width(), bounds.height()));
    fDebugCanvas->drawPicture(fPicture);

    // for some reason we need to 'flush' the debug canvas by drawing all of the ops
    fDebugCanvas->drawTo(this->getCanvas(), this->getLastOp());
    this->getCanvas()->flush();
    return true;
}

sk_sp<SkData> Request::getJsonOps(int n) {
    SkCanvas* canvas = this->getCanvas();
    Json::Value root = fDebugCanvas->toJSON(fUrlDataManager, n, canvas);
    root["mode"] = Json::Value(fGPUEnabled ? "gpu" : "cpu");
    root["drawGpuBatchBounds"] = Json::Value(fDebugCanvas->getDrawGpuBatchBounds());
    root["colorMode"] = Json::Value(fColorMode);
    SkDynamicMemoryWStream stream;
    stream.writeText(Json::FastWriter().write(root).c_str());

    return sk_sp<SkData>(stream.copyToData());
}

sk_sp<SkData> Request::getJsonBatchList(int n) {
    SkCanvas* canvas = this->getCanvas();
    SkASSERT(fGPUEnabled);

    Json::Value result = fDebugCanvas->toJSONBatchList(n, canvas);

    SkDynamicMemoryWStream stream;
    stream.writeText(Json::FastWriter().write(result).c_str());

    return sk_sp<SkData>(stream.copyToData());
}

sk_sp<SkData> Request::getJsonInfo(int n) {
    // drawTo
    SkAutoTUnref<SkSurface> surface(this->createCPUSurface());
    SkCanvas* canvas = surface->getCanvas();

    // TODO this is really slow and we should cache the matrix and clip
    fDebugCanvas->drawTo(canvas, n);

    // make some json
    SkMatrix vm = fDebugCanvas->getCurrentMatrix();
    SkIRect clip = fDebugCanvas->getCurrentClip();
    Json::Value info(Json::objectValue);
    info["ViewMatrix"] = SkDrawCommand::MakeJsonMatrix(vm);
    info["ClipRect"] = SkDrawCommand::MakeJsonIRect(clip);

    std::string json = Json::FastWriter().write(info);

    // We don't want the null terminator so strlen is correct
    return SkData::MakeWithCopy(json.c_str(), strlen(json.c_str()));
}

SkColor Request::getPixel(int x, int y) {
    SkCanvas* canvas = this->getCanvas();
    canvas->flush();
    SkAutoTDelete<SkBitmap> bitmap(this->getBitmapFromCanvas(canvas));
    SkASSERT(bitmap);

    // Convert to format suitable for inspection
    sk_sp<SkData> encodedBitmap = sk_tools::encode_bitmap_for_png(*bitmap);
    SkASSERT(encodedBitmap.get());

    const uint8_t* start = encodedBitmap->bytes() + ((y * bitmap->width() + x) * 4);
    SkColor result = SkColorSetARGB(start[3], start[0], start[1], start[2]);
    return result;
}
