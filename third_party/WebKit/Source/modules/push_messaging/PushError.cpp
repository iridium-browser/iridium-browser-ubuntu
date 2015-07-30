// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/OwnPtr.h"

namespace blink {

DOMException* PushError::take(ScriptPromiseResolver*, WebType* webErrorRaw)
{
    OwnPtr<WebType> webError = adoptPtr(webErrorRaw);
    switch (webError->errorType) {
    case WebPushError::ErrorTypeAbort:
        return DOMException::create(AbortError, webError->message);
    case WebPushError::ErrorTypeNetwork:
        return DOMException::create(NetworkError, webError->message);
    case WebPushError::ErrorTypeNotFound:
        return DOMException::create(NotFoundError, webError->message);
    case WebPushError::ErrorTypeNotSupported:
        return DOMException::create(NotSupportedError, webError->message);
    case WebPushError::ErrorTypeUnknown:
        return DOMException::create(UnknownError, webError->message);
    }
    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

void PushError::dispose(WebType* webErrorRaw)
{
    delete webErrorRaw;
}

} // namespace blink
