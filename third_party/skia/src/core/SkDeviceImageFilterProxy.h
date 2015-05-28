/*
 * Copyright 2012 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkDeviceImageFilterProxy_DEFINED
#define SkDeviceImageFilterProxy_DEFINED

#include "SkDevice.h"
#include "SkImageFilter.h"
#include "SkSurfaceProps.h"

class SkDeviceImageFilterProxy : public SkImageFilter::Proxy {
public:
    SkDeviceImageFilterProxy(SkBaseDevice* device, const SkSurfaceProps& props)
        : fDevice(device)
        , fProps(props.flags(), kUnknown_SkPixelGeometry)
    {}

    SkBaseDevice* createDevice(int w, int h) override {
        SkBaseDevice::CreateInfo cinfo(SkImageInfo::MakeN32Premul(w, h),
                                       SkBaseDevice::kPossible_TileUsage,
                                       kUnknown_SkPixelGeometry);
        return fDevice->onCreateDevice(cinfo, NULL);
    }
    bool canHandleImageFilter(const SkImageFilter* filter) override {
        return fDevice->canHandleImageFilter(filter);
    }
    virtual bool filterImage(const SkImageFilter* filter, const SkBitmap& src,
                             const SkImageFilter::Context& ctx,
                             SkBitmap* result, SkIPoint* offset) override {
        return fDevice->filterImage(filter, src, ctx, result, offset);
    }

    const SkSurfaceProps* surfaceProps() const override {
        return &fProps;
    }

private:
    SkBaseDevice*  fDevice;
    const SkSurfaceProps fProps;
};

#endif
