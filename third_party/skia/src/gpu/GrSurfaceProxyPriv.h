/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrSurfaceProxyPriv_DEFINED
#define GrSurfaceProxyPriv_DEFINED

#include "GrSurfaceProxy.h"

/** Class that adds methods to GrSurfaceProxy that are only intended for use internal to Skia.
    This class is purely a privileged window into GrSurfaceProxy. It should never have additional
    data members or virtual methods. */
class GrSurfaceProxyPriv {
public:
    // Beware! This call is only guaranteed to tell you if the proxy in question has
    // any pending IO in its current state. It won't tell you about the IO state in the
    // future when the proxy is actually used/instantiated.
    bool hasPendingIO() const { return fProxy->hasPendingIO(); }

    // Don't abuse this!!!!!!!
    bool isExact() const { return SkBackingFit::kExact == fProxy->fFit; }

private:
    explicit GrSurfaceProxyPriv(GrSurfaceProxy* proxy) : fProxy(proxy) {}
    GrSurfaceProxyPriv(const GrSurfaceProxyPriv&) {} // unimpl
    GrSurfaceProxyPriv& operator=(const GrSurfaceProxyPriv&); // unimpl

    // No taking addresses of this type.
    const GrSurfaceProxyPriv* operator&() const;
    GrSurfaceProxyPriv* operator&();

    GrSurfaceProxy* fProxy;

    friend class GrSurfaceProxy; // to construct/copy this type.
};

inline GrSurfaceProxyPriv GrSurfaceProxy::priv() { return GrSurfaceProxyPriv(this); }

inline const GrSurfaceProxyPriv GrSurfaceProxy::priv () const {
    return GrSurfaceProxyPriv(const_cast<GrSurfaceProxy*>(this));
}

#endif
