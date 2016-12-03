// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "content/public/browser/resource_throttle.h"

class GURL;

// DownloadResourceThrottle is used to determine if a download should be
// allowed. When a DownloadResourceThrottle is created it pauses the download
// and asks the DownloadRequestLimiter if the download should be allowed. The
// DownloadRequestLimiter notifies us asynchronously as to whether the download
// is allowed or not. If the download is allowed the request is resumed.  If
// the download is not allowed the request is canceled.

class DownloadResourceThrottle
    : public content::ResourceThrottle,
      public base::SupportsWeakPtr<DownloadResourceThrottle> {
 public:
  // Information passed between callbacks to check whether download can proceed.
  struct DownloadRequestInfo {
    DownloadRequestInfo(
        scoped_refptr<DownloadRequestLimiter> limiter,
        const content::ResourceRequestInfo::WebContentsGetter&
            web_contents_getter,
        const GURL& url,
        const std::string& request_method,
        const DownloadRequestLimiter::Callback& continue_callback);
    ~DownloadRequestInfo();

    scoped_refptr<DownloadRequestLimiter> limiter;
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter;
    GURL url;
    std::string request_method;
    DownloadRequestLimiter::Callback continue_callback;
   private:
    DISALLOW_COPY_AND_ASSIGN(DownloadRequestInfo);
  };

  DownloadResourceThrottle(
      scoped_refptr<DownloadRequestLimiter> limiter,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      const GURL& url,
      const std::string& request_method);

  // content::ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

  void ContinueDownload(bool allow);

 private:
  ~DownloadResourceThrottle() override;

  void WillDownload(bool* defer);

  // Set to true when we are querying the DownloadRequestLimiter.
  bool querying_limiter_;

  // Set to true when we know that the request is allowed to start.
  bool request_allowed_;

  // Set to true when we have deferred the request.
  bool request_deferred_;

  DISALLOW_COPY_AND_ASSIGN(DownloadResourceThrottle);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_RESOURCE_THROTTLE_H_
