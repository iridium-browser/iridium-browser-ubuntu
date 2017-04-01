// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/common/content_export.h"
#include "net/base/io_buffer.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
class UploadProgress;
}

namespace content {
class ResourceBuffer;
class ResourceDispatcherHostImpl;
class UploadProgressTracker;

// Used to complete an asynchronous resource request in response to resource
// load events from the resource dispatcher host.
class CONTENT_EXPORT AsyncResourceHandler : public ResourceHandler,
                                            public ResourceMessageDelegate {
 public:
  AsyncResourceHandler(net::URLRequest* request,
                       ResourceDispatcherHostImpl* rdh);
  ~AsyncResourceHandler() override;

  bool OnMessageReceived(const IPC::Message& message) override;

  // ResourceHandler implementation:
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           bool* defer) override;
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  class InliningHelper;

  // IPC message handlers:
  void OnFollowRedirect(int request_id);
  void OnDataReceivedACK(int request_id);
  void OnUploadProgressACK(int request_id);

  bool EnsureResourceBufferIsInitialized();
  void ResumeIfDeferred();
  void OnDefer();
  bool CheckForSufficientResource();
  int CalculateEncodedDataLengthToReport();
  int CalculateEncodedBodyLengthToReport();
  void RecordHistogram();
  void SendUploadProgress(const net::UploadProgress& progress);

  scoped_refptr<ResourceBuffer> buffer_;
  ResourceDispatcherHostImpl* rdh_;

  // Number of messages we've sent to the renderer that we haven't gotten an
  // ACK for. This allows us to avoid having too many messages in flight.
  int pending_data_count_;

  int allocation_size_;

  bool first_chunk_read_ = false;

  bool did_defer_;

  bool has_checked_for_sufficient_resources_;
  bool sent_received_response_msg_;
  bool sent_data_buffer_msg_;

  std::unique_ptr<InliningHelper> inlining_helper_;
  base::TimeTicks response_started_ticks_;

  std::unique_ptr<UploadProgressTracker> upload_progress_tracker_;

  int64_t reported_transfer_size_;

  DISALLOW_COPY_AND_ASSIGN(AsyncResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_ASYNC_RESOURCE_HANDLER_H_
