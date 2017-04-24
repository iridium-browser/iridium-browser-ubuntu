// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_handler.h"

namespace net {
class IOBuffer;
class URLRequest;
}  // namespace net

namespace content {

class ResourceController;

// A ResourceHandler which delegates all calls to the next handler, unless
// detached. Once detached, it drives the request to completion itself. This is
// used for requests which outlive the owning renderer, such as <link
// rel=prefetch> and <a ping>. Requests do not start out detached so, e.g.,
// prefetches appear in DevTools and get placed in the renderer's local
// cache. If the request does not complete after a timeout on detach, it is
// cancelled.
//
// Note that, once detached, the request continues without the original next
// handler, so any policy decisions in that handler are skipped.
class DetachableResourceHandler : public ResourceHandler {
 public:
  DetachableResourceHandler(net::URLRequest* request,
                            base::TimeDelta cancel_delay,
                            std::unique_ptr<ResourceHandler> next_handler);
  ~DetachableResourceHandler() override;

  void SetDelegate(Delegate* delegate) override;

  bool is_detached() const { return next_handler_ == NULL; }
  void Detach();

  void set_cancel_delay(base::TimeDelta cancel_delay) {
    cancel_delay_ = cancel_delay;
  }

  // ResourceHandler implementation:
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnResponseStarted(
      ResourceResponse* response,
      std::unique_ptr<ResourceController> controller) override;
  void OnWillStart(const GURL& url,
                   std::unique_ptr<ResourceController> controller) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size) override;
  void OnReadCompleted(int bytes_read,
                       std::unique_ptr<ResourceController> controller) override;
  void OnResponseCompleted(
      const net::URLRequestStatus& status,
      std::unique_ptr<ResourceController> controller) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  class Controller;

  void OnTimedOut();

  std::unique_ptr<ResourceHandler> next_handler_;
  scoped_refptr<net::IOBuffer> read_buffer_;

  std::unique_ptr<base::OneShotTimer> detached_timer_;
  base::TimeDelta cancel_delay_;

  bool is_finished_;

  DISALLOW_COPY_AND_ASSIGN(DetachableResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_DETACHABLE_RESOURCE_HANDLER_H_
