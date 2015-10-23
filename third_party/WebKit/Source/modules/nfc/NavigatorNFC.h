// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorNFC_h
#define NavigatorNFC_h

#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class NFC;
class Navigator;
class ExecutionContext;

class NavigatorNFC final
    : public GarbageCollected<NavigatorNFC>
    , public HeapSupplement<Navigator> {
    USING_GARBAGE_COLLECTED_MIXIN(NavigatorNFC);

public:
    // Gets, or creates, NavigatorNFC supplement on Navigator.
    static NavigatorNFC& from(Navigator&);

    static NFC* nfc(ExecutionContext*, Navigator&);

    DECLARE_TRACE();

private:
    NavigatorNFC();
    static const char* supplementName();

    Member<NFC> m_nfc;
};

} // namespace blink

#endif // NavigatorNFC_h
