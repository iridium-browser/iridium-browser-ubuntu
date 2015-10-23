// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/core/v8/Dictionary.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class FetchDataConsumerHandle;
class Headers;

// FIXME: Use IDL dictionary instead of this class.
class RequestInit {
    STACK_ALLOCATED();
public:
    explicit RequestInit(ExecutionContext*, const Dictionary&, ExceptionState&);

    String method;
    Member<Headers> headers;
    Dictionary headersDictionary;
    String contentType;
    OwnPtr<FetchDataConsumerHandle> body;
    String mode;
    String credentials;
    String redirect;
    String integrity;
};

}

#endif // RequestInit_h
