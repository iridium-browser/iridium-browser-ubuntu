/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/EmptyClients.h"

#include <memory>
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/loader/DocumentLoader.h"
#include "platform/FileChooser.h"
#include "platform/FrameViewBase.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebMediaPlayer.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "wtf/PtrUtil.h"

namespace blink {

void fillWithEmptyClients(Page::PageClients& pageClients) {
  DEFINE_STATIC_LOCAL(ChromeClient, dummyChromeClient,
                      (EmptyChromeClient::create()));
  pageClients.chromeClient = &dummyChromeClient;

  DEFINE_STATIC_LOCAL(EmptyContextMenuClient, dummyContextMenuClient, ());
  pageClients.contextMenuClient = &dummyContextMenuClient;

  DEFINE_STATIC_LOCAL(EmptyEditorClient, dummyEditorClient, ());
  pageClients.editorClient = &dummyEditorClient;

  DEFINE_STATIC_LOCAL(EmptySpellCheckerClient, dummySpellCheckerClient, ());
  pageClients.spellCheckerClient = &dummySpellCheckerClient;
}

class EmptyPopupMenu : public PopupMenu {
 public:
  void show() override {}
  void hide() override {}
  void updateFromElement(UpdateReason) override {}
  void disconnectClient() override {}
};

class EmptyFrameScheduler : public WebFrameScheduler {
 public:
  EmptyFrameScheduler() { DCHECK(isMainThread()); }
  void setFrameVisible(bool) override {}
  RefPtr<WebTaskRunner> loadingTaskRunner() override;
  RefPtr<WebTaskRunner> timerTaskRunner() override;
  RefPtr<WebTaskRunner> unthrottledTaskRunner() override;
};

RefPtr<WebTaskRunner> EmptyFrameScheduler::loadingTaskRunner() {
  return Platform::current()->mainThread()->getWebTaskRunner();
}

RefPtr<WebTaskRunner> EmptyFrameScheduler::timerTaskRunner() {
  return Platform::current()->mainThread()->getWebTaskRunner();
}

RefPtr<WebTaskRunner> EmptyFrameScheduler::unthrottledTaskRunner() {
  return Platform::current()->mainThread()->getWebTaskRunner();
}

PopupMenu* EmptyChromeClient::openPopupMenu(LocalFrame&, HTMLSelectElement&) {
  return new EmptyPopupMenu();
}

ColorChooser* EmptyChromeClient::openColorChooser(LocalFrame*,
                                                  ColorChooserClient*,
                                                  const Color&) {
  return nullptr;
}

DateTimeChooser* EmptyChromeClient::openDateTimeChooser(
    DateTimeChooserClient*,
    const DateTimeChooserParameters&) {
  return nullptr;
}

void EmptyChromeClient::openTextDataListChooser(HTMLInputElement&) {}

void EmptyChromeClient::openFileChooser(LocalFrame*, PassRefPtr<FileChooser>) {}

void EmptyChromeClient::attachRootGraphicsLayer(GraphicsLayer* layer,
                                                LocalFrame* localRoot) {
  Page* page = localRoot ? localRoot->page() : nullptr;
  if (!page)
    return;
  page->frameHost().visualViewport().attachToLayerTree(layer);
}

String EmptyChromeClient::acceptLanguages() {
  return String();
}

std::unique_ptr<WebFrameScheduler> EmptyChromeClient::createFrameScheduler(
    BlameContext*) {
  return WTF::makeUnique<EmptyFrameScheduler>();
}

NavigationPolicy EmptyLocalFrameClient::decidePolicyForNavigation(
    const ResourceRequest&,
    DocumentLoader*,
    NavigationType,
    NavigationPolicy,
    bool,
    bool,
    HTMLFormElement*) {
  return NavigationPolicyIgnore;
}

void EmptyLocalFrameClient::dispatchWillSendSubmitEvent(HTMLFormElement*) {}

void EmptyLocalFrameClient::dispatchWillSubmitForm(HTMLFormElement*) {}

DocumentLoader* EmptyLocalFrameClient::createDocumentLoader(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& substituteData,
    ClientRedirectPolicy clientRedirectPolicy) {
  DCHECK(frame);

  return DocumentLoader::create(frame, request, substituteData,
                                clientRedirectPolicy);
}

LocalFrame* EmptyLocalFrameClient::createFrame(const FrameLoadRequest&,
                                               const AtomicString&,
                                               HTMLFrameOwnerElement*) {
  return nullptr;
}

FrameViewBase* EmptyLocalFrameClient::createPlugin(HTMLPlugInElement*,
                                                   const KURL&,
                                                   const Vector<String>&,
                                                   const Vector<String>&,
                                                   const String&,
                                                   bool,
                                                   DetachedPluginPolicy) {
  return nullptr;
}

std::unique_ptr<WebMediaPlayer> EmptyLocalFrameClient::createWebMediaPlayer(
    HTMLMediaElement&,
    const WebMediaPlayerSource&,
    WebMediaPlayerClient*) {
  return nullptr;
}

WebRemotePlaybackClient* EmptyLocalFrameClient::createWebRemotePlaybackClient(
    HTMLMediaElement&) {
  return nullptr;
}

void EmptyTextCheckerClient::requestCheckingOfString(TextCheckingRequest*) {}

void EmptyTextCheckerClient::cancelAllPendingRequests() {}

std::unique_ptr<WebServiceWorkerProvider>
EmptyLocalFrameClient::createServiceWorkerProvider() {
  return nullptr;
}

std::unique_ptr<WebApplicationCacheHost>
EmptyLocalFrameClient::createApplicationCacheHost(
    WebApplicationCacheHostClient*) {
  return nullptr;
}

EmptyRemoteFrameClient::EmptyRemoteFrameClient() = default;

}  // namespace blink
