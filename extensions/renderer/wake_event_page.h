// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_
#define EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_sync_message_filter.h"
#include "v8/include/v8.h"

namespace content {
class RenderThread;
}

namespace extensions {
class ScriptContext;

// This class implements the wake-event-page JavaScript function, which wakes
// an event page and runs a callback when done.
//
// Note, the function will do a round trip to the browser even if event page is
// open. Any optimisation to prevent this must be at the JavaScript level.
class WakeEventPage : public content::RenderProcessObserver {
 public:
  WakeEventPage();
  ~WakeEventPage() override;

  // Returns the single instance of the WakeEventPage object.
  //
  // Thread safe.
  static WakeEventPage* Get();

  // Initializes the WakeEventPage.
  //
  // This must be called before any bindings are installed, and must be called
  // on the render thread.
  void Init(content::RenderThread* render_thread);

  // Returns the wake-event-page function bound to a given context. The
  // function will be cached as a hidden value in the context's global object.
  //
  // To mix C++ and JavaScript, example usage might be:
  //
  // WakeEventPage::Get().GetForContext(context)(function() {
  //   ...
  // });
  //
  // Thread safe.
  v8::Local<v8::Function> GetForContext(ScriptContext* context);

 private:
  class WakeEventPageNativeHandler;

  // The response from an ExtensionHostMsg_WakeEvent call, passed true if the
  // call was successful, false on failure.
  using OnResponseCallback = base::Callback<void(bool)>;

  // Makes an ExtensionHostMsg_WakeEvent request for an extension ID. The
  // second argument is a callback to run when the request has completed.
  using MakeRequestCallback =
      base::Callback<void(const std::string&, const OnResponseCallback&)>;

  // For |requests_|.
  struct RequestData {
    explicit RequestData(const OnResponseCallback& on_response);
    ~RequestData();
    OnResponseCallback on_response;
  };

  // Runs |on_response|, passing it |success|.
  static void RunOnResponseWithResult(const OnResponseCallback& on_response,
                                      bool success);

  // Sends the ExtensionHostMsg_WakeEvent IPC for |extension_id|, and
  // updates |requests_| bookkeeping.
  void MakeRequest(const std::string& extension_id,
                   const OnResponseCallback& on_response);

  // content::RenderProcessObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;

  // OnControlMessageReceived handlers:
  void OnWakeEventPageResponse(int request_id, bool success);

  // IPC sender. Belongs to the render thread, but thread safe.
  scoped_refptr<IPC::SyncMessageFilter> message_filter_;

  // All in-flight requests, keyed by request ID.
  base::ScopedPtrHashMap<int, scoped_ptr<RequestData>> requests_;

  base::WeakPtrFactory<WakeEventPage> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeEventPage);
};

}  //  namespace extensions

#endif  // EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_
