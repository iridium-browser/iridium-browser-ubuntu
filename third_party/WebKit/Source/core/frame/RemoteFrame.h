// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrame_h
#define RemoteFrame_h

#include "core/CoreExport.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/Frame.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class Event;
class LocalFrame;
class RemoteFrameClient;
class RemoteFrameView;
class RemoteWindowProxyManager;
class WebLayer;
struct FrameLoadRequest;

class CORE_EXPORT RemoteFrame final : public Frame {
 public:
  static RemoteFrame* create(RemoteFrameClient*, FrameHost*, FrameOwner*);

  ~RemoteFrame() override;

  // Frame overrides:
  DECLARE_VIRTUAL_TRACE();
  WindowProxy* windowProxy(DOMWrapperWorld&) override;
  void navigate(Document& originDocument,
                const KURL&,
                bool replaceCurrentItem,
                UserGestureStatus) override;
  void navigate(const FrameLoadRequest& passedRequest) override;
  void reload(FrameLoadType, ClientRedirectPolicy) override;
  void detach(FrameDetachType) override;
  RemoteSecurityContext* securityContext() const override;
  void printNavigationErrorMessage(const Frame&, const char* reason) override {}
  void printNavigationWarning(const String&) override {}
  bool prepareForCommit() override;
  bool shouldClose() override;

  // FIXME: Remove this method once we have input routing in the browser
  // process. See http://crbug.com/339659.
  void forwardInputEvent(Event*);

  void setWebLayer(WebLayer*);
  WebLayer* webLayer() const { return m_webLayer; }

  void advanceFocus(WebFocusType, LocalFrame* source);

  void setView(RemoteFrameView*);
  void createView();

  RemoteFrameView* view() const;

  RemoteFrameClient* client() const;

 private:
  RemoteFrame(RemoteFrameClient*, FrameHost*, FrameOwner*);

  // Internal Frame helper overrides:
  WindowProxyManagerBase* getWindowProxyManager() const override;

  // Intentionally private to prevent redundant checks when the type is
  // already RemoteFrame.
  bool isLocalFrame() const override { return false; }
  bool isRemoteFrame() const override { return true; }

  void detachChildren();

  Member<RemoteFrameView> m_view;
  Member<RemoteSecurityContext> m_securityContext;
  Member<RemoteWindowProxyManager> m_windowProxyManager;
  WebLayer* m_webLayer = nullptr;
};

inline RemoteFrameView* RemoteFrame::view() const {
  return m_view.get();
}

DEFINE_TYPE_CASTS(RemoteFrame,
                  Frame,
                  remoteFrame,
                  remoteFrame->isRemoteFrame(),
                  remoteFrame.isRemoteFrame());

}  // namespace blink

#endif  // RemoteFrame_h
