// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
#define COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_local_storage.h"
#include "base/timer/timer.h"
#include "cc/blink/web_compositor_support_impl.h"
#include "components/html_viewer/mock_web_blob_registry_impl.h"
#include "components/html_viewer/web_mime_registry_impl.h"
#include "components/html_viewer/web_notification_manager_impl.h"
#include "components/html_viewer/web_theme_engine_impl.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebScrollbarBehavior.h"

namespace scheduler {
class RendererScheduler;
class WebThreadImplForRendererScheduler;
}

namespace mojo {
class ApplicationImpl;
}

namespace html_viewer {

class WebClipboardImpl;
class WebCookieJarImpl;

class BlinkPlatformImpl : public blink::Platform {
 public:
  // |app| may be null in tests.
  BlinkPlatformImpl(mojo::ApplicationImpl* app,
                    scheduler::RendererScheduler* renderer_scheduler);
  virtual ~BlinkPlatformImpl();

  // blink::Platform methods:
  virtual blink::WebCookieJar* cookieJar();
  virtual blink::WebClipboard* clipboard();
  virtual blink::WebMimeRegistry* mimeRegistry();
  virtual blink::WebThemeEngine* themeEngine();
  virtual blink::WebString defaultLocale();
  virtual blink::WebBlobRegistry* blobRegistry();
  virtual double currentTime();
  virtual double monotonicallyIncreasingTime();
  virtual void cryptographicallyRandomValues(unsigned char* buffer,
                                             size_t length);
  virtual bool isThreadedCompositingEnabled();
  virtual blink::WebCompositorSupport* compositorSupport();
  uint32_t getUniqueIdForProcess() override;
  void createMessageChannel(blink::WebMessagePortChannel** channel1,
                            blink::WebMessagePortChannel** channel2) override;
  virtual blink::WebURLLoader* createURLLoader();
  virtual blink::WebSocketHandle* createWebSocketHandle();
  virtual blink::WebString userAgent();
  virtual blink::WebData parseDataURL(
      const blink::WebURL& url, blink::WebString& mime_type,
      blink::WebString& charset);
  virtual bool isReservedIPAddress(const blink::WebString& host) const;
  virtual blink::WebURLError cancelledError(const blink::WebURL& url) const;
  virtual blink::WebThread* createThread(const char* name);
  virtual blink::WebThread* currentThread();
  virtual void yieldCurrentThread();
  virtual blink::WebWaitableEvent* createWaitableEvent(
      blink::WebWaitableEvent::ResetPolicy policy,
      blink::WebWaitableEvent::InitialState state);
  virtual blink::WebWaitableEvent* waitMultipleEvents(
      const blink::WebVector<blink::WebWaitableEvent*>& events);
  virtual blink::WebScrollbarBehavior* scrollbarBehavior();
  virtual const unsigned char* getTraceCategoryEnabledFlag(
      const char* category_name);
  virtual blink::WebData loadResource(const char* name);
  virtual blink::WebGestureCurve* createFlingAnimationCurve(
      blink::WebGestureDevice device_source,
      const blink::WebFloatPoint& velocity,
      const blink::WebSize& cumulative_scroll);
  virtual blink::WebCrypto* crypto();
  virtual blink::WebNotificationManager* notificationManager();

 private:
  void UpdateWebThreadTLS(blink::WebThread* thread);

  static void DestroyCurrentThread(void*);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_ptr<scheduler::WebThreadImplForRendererScheduler> main_thread_;
  base::ThreadLocalStorage::Slot current_thread_slot_;
  cc_blink::WebCompositorSupportImpl compositor_support_;
  WebThemeEngineImpl theme_engine_;
  WebMimeRegistryImpl mime_registry_;
  webcrypto::WebCryptoImpl web_crypto_;
  WebNotificationManagerImpl web_notification_manager_;
  blink::WebScrollbarBehavior scrollbar_behavior_;
  mojo::WebSocketFactoryPtr web_socket_factory_;
  mojo::URLLoaderFactoryPtr url_loader_factory_;
  MockWebBlobRegistryImpl blob_registry_;
  scoped_ptr<WebCookieJarImpl> cookie_jar_;
  scoped_ptr<WebClipboardImpl> clipboard_;

  DISALLOW_COPY_AND_ASSIGN(BlinkPlatformImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_BLINK_PLATFORM_IMPL_H_
