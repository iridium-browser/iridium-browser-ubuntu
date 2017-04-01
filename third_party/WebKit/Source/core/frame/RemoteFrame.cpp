// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrame.h"

#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WindowProxyManager.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteDOMWindow.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/paint/PaintLayer.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/UserGestureIndicator.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebLayer.h"

namespace blink {

inline RemoteFrame::RemoteFrame(RemoteFrameClient* client,
                                FrameHost* host,
                                FrameOwner* owner)
    : Frame(client, host, owner),
      m_securityContext(RemoteSecurityContext::create()),
      m_windowProxyManager(RemoteWindowProxyManager::create(*this)) {
  m_domWindow = RemoteDOMWindow::create(*this);
}

RemoteFrame* RemoteFrame::create(RemoteFrameClient* client,
                                 FrameHost* host,
                                 FrameOwner* owner) {
  return new RemoteFrame(client, host, owner);
}

RemoteFrame::~RemoteFrame() {
  ASSERT(!m_view);
}

DEFINE_TRACE(RemoteFrame) {
  visitor->trace(m_view);
  visitor->trace(m_securityContext);
  visitor->trace(m_windowProxyManager);
  Frame::trace(visitor);
}

WindowProxy* RemoteFrame::windowProxy(DOMWrapperWorld& world) {
  WindowProxy* windowProxy = m_windowProxyManager->windowProxy(world);
  ASSERT(windowProxy);
  windowProxy->initializeIfNeeded();
  return windowProxy;
}

void RemoteFrame::navigate(Document& originDocument,
                           const KURL& url,
                           bool replaceCurrentItem,
                           UserGestureStatus userGestureStatus) {
  FrameLoadRequest frameRequest(&originDocument, url);
  frameRequest.setReplacesCurrentItem(replaceCurrentItem);
  frameRequest.resourceRequest().setHasUserGesture(userGestureStatus ==
                                                   UserGestureStatus::Active);
  navigate(frameRequest);
}

void RemoteFrame::navigate(const FrameLoadRequest& passedRequest) {
  FrameLoadRequest frameRequest(passedRequest);

  // The process where this frame actually lives won't have sufficient
  // information to determine correct referrer, since it won't have access to
  // the originDocument. Set it now.
  FrameLoader::setReferrerForFrameRequest(frameRequest);

  frameRequest.resourceRequest().setHasUserGesture(
      UserGestureIndicator::processingUserGesture());
  client()->navigate(frameRequest.resourceRequest(),
                     frameRequest.replacesCurrentItem());
}

void RemoteFrame::reload(FrameLoadType frameLoadType,
                         ClientRedirectPolicy clientRedirectPolicy) {
  client()->reload(frameLoadType, clientRedirectPolicy);
}

void RemoteFrame::detach(FrameDetachType type) {
  m_isDetaching = true;

  PluginScriptForbiddenScope forbidPluginDestructorScripting;
  detachChildren();
  if (!client())
    return;

  // Clean up the frame's view if needed. A remote frame only has a view if
  // the parent is a local frame.
  if (m_view)
    m_view->dispose();
  client()->willBeDetached();
  m_windowProxyManager->clearForClose();
  setView(nullptr);
  // ... the RemoteDOMWindow will need to be informed of detachment,
  // as otherwise it will keep a strong reference back to this RemoteFrame.
  // That combined with wrappers (owned and kept alive by RemoteFrame) keeping
  // persistent strong references to RemoteDOMWindow will prevent the GCing
  // of all these objects. Break the cycle by notifying of detachment.
  toRemoteDOMWindow(m_domWindow)->frameDetached();
  if (m_webLayer)
    setWebLayer(nullptr);
  Frame::detach(type);
}

bool RemoteFrame::prepareForCommit() {
  detachChildren();
  return !!host();
}

RemoteSecurityContext* RemoteFrame::securityContext() const {
  return m_securityContext.get();
}

bool RemoteFrame::shouldClose() {
  // TODO(nasko): Implement running the beforeunload handler in the actual
  // LocalFrame running in a different process and getting back a real result.
  return true;
}

void RemoteFrame::forwardInputEvent(Event* event) {
  client()->forwardInputEvent(event);
}

void RemoteFrame::setView(RemoteFrameView* view) {
  // Oilpan: as RemoteFrameView performs no finalization actions,
  // no explicit dispose() of it needed here. (cf. FrameView::dispose().)
  m_view = view;
}

void RemoteFrame::createView() {
  // If the RemoteFrame does not have a LocalFrame parent, there's no need to
  // create a widget for it.
  if (!deprecatedLocalOwner())
    return;

  ASSERT(!deprecatedLocalOwner()->ownedWidget());

  setView(RemoteFrameView::create(this));

  if (!ownerLayoutItem().isNull())
    deprecatedLocalOwner()->setWidget(m_view);
}

RemoteFrameClient* RemoteFrame::client() const {
  return static_cast<RemoteFrameClient*>(Frame::client());
}

void RemoteFrame::setWebLayer(WebLayer* webLayer) {
  if (m_webLayer)
    GraphicsLayer::unregisterContentsLayer(m_webLayer);
  m_webLayer = webLayer;
  if (m_webLayer)
    GraphicsLayer::registerContentsLayer(m_webLayer);

  ASSERT(owner());
  toHTMLFrameOwnerElement(owner())->setNeedsCompositingUpdate();
}

void RemoteFrame::advanceFocus(WebFocusType type, LocalFrame* source) {
  client()->advanceFocus(type, source);
}

WindowProxyManagerBase* RemoteFrame::getWindowProxyManager() const {
  return m_windowProxyManager.get();
}

void RemoteFrame::detachChildren() {
  using FrameVector = HeapVector<Member<Frame>>;
  FrameVector childrenToDetach;
  childrenToDetach.reserveCapacity(tree().childCount());
  for (Frame* child = tree().firstChild(); child;
       child = child->tree().nextSibling())
    childrenToDetach.push_back(child);
  for (const auto& child : childrenToDetach)
    child->detach(FrameDetachType::Remove);
}

}  // namespace blink
