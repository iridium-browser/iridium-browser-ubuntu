/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaDevicesRequest_h
#define MediaDevicesRequest_h

#include "core/dom/ActiveDOMObject.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/MediaDeviceInfo.h"
#include "modules/mediastream/MediaDeviceInfoCallback.h"
#include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class Document;
class ExceptionState;
class UserMediaController;

class MODULES_EXPORT MediaDevicesRequest final : public GarbageCollectedFinalized<MediaDevicesRequest>, public ActiveDOMObject {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaDevicesRequest);
public:
    static MediaDevicesRequest* create(ExecutionContext*, UserMediaController*, MediaDeviceInfoCallback*, ExceptionState&);
    virtual ~MediaDevicesRequest();

    MediaDeviceInfoCallback* callback() const { return m_callback.get(); }
    Document* ownerDocument();

    void start();

    void succeed(const MediaDeviceInfoVector&);

    // ActiveDOMObject
    virtual void stop() override;

    DECLARE_VIRTUAL_TRACE();

private:
    MediaDevicesRequest(ExecutionContext*, UserMediaController*, MediaDeviceInfoCallback*);

    RawPtrWillBeMember<UserMediaController> m_controller;

    Member<MediaDeviceInfoCallback> m_callback;
};

} // namespace blink

#endif // MediaDevicesRequest_h
