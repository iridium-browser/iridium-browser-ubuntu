// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionsCallback_h
#define PermissionsCallback_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/permissions/WebPermissionStatus.h"
#include "public/platform/modules/permissions/WebPermissionType.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class ScriptPromiseResolver;

// PermissionQueryCallback is an implementation of WebPermissionCallbacks
// that will resolve the underlying promise depending on the result passed to
// the callback. It takes a WebPermissionType in its constructor and will pass
// it to the PermissionStatus.
class PermissionsCallback final
    : public WebCallbacks<WebPassOwnPtr<WebVector<WebPermissionStatus>>, void> {
public:
    PermissionsCallback(ScriptPromiseResolver*, PassOwnPtr<WebVector<WebPermissionType>>);
    ~PermissionsCallback() = default;

    void onSuccess(WebPassOwnPtr<WebVector<WebPermissionStatus>>) override;
    void onError() override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    OwnPtr<WebVector<WebPermissionType>> m_permissionTypes;

    WTF_MAKE_NONCOPYABLE(PermissionsCallback);
};

} // namespace blink

#endif // PermissionsCallback_h
