// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_
#define CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/web/WebWorkerContentSettingsClientProxy.h"
#include "url/gurl.h"

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class RenderFrame;
}

namespace blink {
class WebFrame;
}

// This proxy is created on the main renderer thread then passed onto
// the blink's worker thread.
class WorkerContentSettingsClientProxy
    : public blink::WebWorkerContentSettingsClientProxy {
 public:
  WorkerContentSettingsClientProxy(content::RenderFrame* render_frame,
                              blink::WebFrame* frame);
  ~WorkerContentSettingsClientProxy() override;

  // WebWorkerContentSettingsClientProxy overrides.
  bool requestFileSystemAccessSync() override;
  bool allowIndexedDB(const blink::WebString& name) override;

 private:
  // Loading document context for this worker.
  const int routing_id_;
  bool is_unique_origin_;
  GURL document_origin_url_;
  GURL top_frame_origin_url_;
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WorkerContentSettingsClientProxy);
};

#endif  // CHROME_RENDERER_WORKER_CONTENT_SETTINGS_CLIENT_PROXY_H_
