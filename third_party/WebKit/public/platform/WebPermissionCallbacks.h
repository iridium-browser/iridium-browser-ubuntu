// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPermissionCallbacks_h
#define WebPermissionCallbacks_h

#include "WebPrivatePtr.h"

namespace WTF { template <typename T> class PassOwnPtr; }

namespace blink {

class ContentSettingCallbacks;
class WebContentSettingCallbacksPrivate;

class WebPermissionCallbacks {
public:
    ~WebPermissionCallbacks() { reset(); }
    WebPermissionCallbacks() { }
    WebPermissionCallbacks(const WebPermissionCallbacks& c) { assign(c); }
    WebPermissionCallbacks& operator=(const WebPermissionCallbacks& c)
    {
        assign(c);
        return *this;
    }

    BLINK_PLATFORM_EXPORT void reset();
    BLINK_PLATFORM_EXPORT void assign(const WebPermissionCallbacks&);

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebPermissionCallbacks(const WTF::PassOwnPtr<ContentSettingCallbacks>&);
#endif

    BLINK_PLATFORM_EXPORT void doAllow();
    BLINK_PLATFORM_EXPORT void doDeny();

private:
    WebPrivatePtr<WebContentSettingCallbacksPrivate> m_private;
};

} // namespace blink

#endif
