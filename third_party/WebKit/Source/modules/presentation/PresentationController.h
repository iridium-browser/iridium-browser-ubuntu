// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationController_h
#define PresentationController_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/LocalFrame.h"
#include "modules/ModulesExport.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationRequest.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"
#include "public/platform/modules/presentation/WebPresentationController.h"

namespace blink {

class PresentationConnection;

// The coordinator between the various page exposed properties and the content
// layer represented via |WebPresentationClient|.
class MODULES_EXPORT PresentationController final
    : public GarbageCollectedFinalized<PresentationController>,
      public Supplement<LocalFrame>,
      public ContextLifecycleObserver,
      public WebPresentationController {
  USING_GARBAGE_COLLECTED_MIXIN(PresentationController);
  WTF_MAKE_NONCOPYABLE(PresentationController);

 public:
  ~PresentationController() override;

  static PresentationController* create(LocalFrame&, WebPresentationClient*);

  static const char* supplementName();
  static PresentationController* from(LocalFrame&);

  static void provideTo(LocalFrame&, WebPresentationClient*);

  WebPresentationClient* client();

  // Implementation of Supplement.
  DECLARE_VIRTUAL_TRACE();

  // Implementation of WebPresentationController.
  void didStartDefaultSession(const WebPresentationSessionInfo&) override;
  void didChangeSessionState(const WebPresentationSessionInfo&,
                             WebPresentationConnectionState) override;
  void didCloseConnection(const WebPresentationSessionInfo&,
                          WebPresentationConnectionCloseReason,
                          const WebString& message) override;
  void didReceiveSessionTextMessage(const WebPresentationSessionInfo&,
                                    const WebString&) override;
  void didReceiveSessionBinaryMessage(const WebPresentationSessionInfo&,
                                      const uint8_t* data,
                                      size_t length) override;

  // Called by the Presentation object to advertize itself to the controller.
  // The Presentation object is kept as a WeakMember in order to avoid keeping
  // it alive when it is no longer in the tree.
  void setPresentation(Presentation*);

  // Called by the Presentation object when the default request is updated
  // in order to notify the client about the change of default presentation
  // url.
  void setDefaultRequestUrl(const WTF::Vector<KURL>&);

  // Handling of running connections.
  void registerConnection(PresentationConnection*);

  // Return a connection in |m_connections| with id equals to |presentationId|,
  // url equals to one of |presentationUrls|, and state is not terminated.
  // Return null if such a connection does not exist.
  PresentationConnection* findExistingConnection(
      const blink::WebVector<blink::WebURL>& presentationUrls,
      const blink::WebString& presentationId);

 private:
  PresentationController(LocalFrame&, WebPresentationClient*);

  // Implementation of ContextLifecycleObserver.
  void contextDestroyed(ExecutionContext*) override;

  // Return the connection associated with the given |connectionClient| or
  // null if it doesn't exist.
  PresentationConnection* findConnection(const WebPresentationSessionInfo&);

  // The WebPresentationClient which allows communicating with the embedder.
  // It is not owned by the PresentationController but the controller will
  // set it to null when the LocalFrame will be detached at which point the
  // client can't be used.
  WebPresentationClient* m_client;

  // Default PresentationRequest used by the embedder.
  // Member<PresentationRequest> m_defaultRequest;
  WeakMember<Presentation> m_presentation;

  // The presentation connections associated with that frame.
  // TODO(mlamouri): the PresentationController will keep any created
  // connections alive until the frame is detached. These should be weak ptr
  // so that the connection can be GC'd.
  HeapHashSet<Member<PresentationConnection>> m_connections;
};

}  // namespace blink

#endif  // PresentationController_h
