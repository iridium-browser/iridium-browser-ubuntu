// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/external_feedback_reporter.h"
#include "components/dom_distiller/core/feedback_reporter.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/viewer.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request.h"

namespace dom_distiller {

namespace {

class ContentDataCallback : public DistillerDataCallback {
 public:
  ContentDataCallback(const content::URLDataSource::GotDataCallback& callback);
  // Runs the callback.
  void RunCallback(std::string& data) override;

 private:
  // The callback that actually gets run.
  content::URLDataSource::GotDataCallback callback_;
};

ContentDataCallback::ContentDataCallback(
    const content::URLDataSource::GotDataCallback& callback) {
  callback_ = callback;
}

void ContentDataCallback::RunCallback(std::string& data) {
  callback_.Run(base::RefCountedString::TakeString(&data));
}

}  // namespace

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source. Lifetime matches that of
// the current main frame's page in the Viewer instance.
class DomDistillerViewerSource::RequestViewerHandle
    : public DomDistillerRequestViewBase,
      public content::WebContentsObserver {
 public:
  RequestViewerHandle(content::WebContents* web_contents,
                      const std::string& expected_scheme,
                      const std::string& expected_request_path,
                      scoped_ptr<ContentDataCallback> callback,
                      DistilledPagePrefs* distilled_page_prefs);
  ~RequestViewerHandle() override;

  // content::WebContentsObserver implementation:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  // Sends JavaScript to the attached Viewer, buffering data if the viewer isn't
  // ready.
  void SendJavaScript(const std::string& buffer) override;

  // Cancels the current view request. Once called, no updates will be
  // propagated to the view, and the request to DomDistillerService will be
  // cancelled.
  void Cancel();

  // The scheme hosting the current view request;
  std::string expected_scheme_;

  // The query path for the current view request.
  std::string expected_request_path_;

  // Whether the page is sufficiently initialized to handle updates from the
  // distiller.
  bool waiting_for_page_ready_;

  // Temporary store of pending JavaScript if the page isn't ready to receive
  // data from distillation.
  std::string buffer_;
};

DomDistillerViewerSource::RequestViewerHandle::RequestViewerHandle(
    content::WebContents* web_contents,
    const std::string& expected_scheme,
    const std::string& expected_request_path,
    scoped_ptr<ContentDataCallback> callback,
    DistilledPagePrefs* distilled_page_prefs)
    : DomDistillerRequestViewBase(callback.Pass(), distilled_page_prefs),
      expected_scheme_(expected_scheme),
      expected_request_path_(expected_request_path),
      waiting_for_page_ready_(true) {
  content::WebContentsObserver::Observe(web_contents);
  distilled_page_prefs_->AddObserver(this);
}

DomDistillerViewerSource::RequestViewerHandle::~RequestViewerHandle() {
  distilled_page_prefs_->RemoveObserver(this);
}

void DomDistillerViewerSource::RequestViewerHandle::SendJavaScript(
    const std::string& buffer) {
  if (waiting_for_page_ready_) {
    buffer_ += buffer;
  } else {
    if (web_contents()) {
      web_contents()->GetMainFrame()->ExecuteJavaScript(
          base::UTF8ToUTF16(buffer));
    }
  }
}

void DomDistillerViewerSource::RequestViewerHandle::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  const GURL& navigation = details.entry->GetURL();
  if (details.is_in_page || (
      navigation.SchemeIs(expected_scheme_.c_str()) &&
      expected_request_path_ == navigation.query())) {
    // In-page navigations, as well as the main view request can be ignored.
    return;
  }

  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::RenderProcessGone(
    base::TerminationStatus status) {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::WebContentsDestroyed() {
  Cancel();
}

void DomDistillerViewerSource::RequestViewerHandle::Cancel() {
  // No need to listen for notifications.
  content::WebContentsObserver::Observe(NULL);

  // Schedule the Viewer for deletion. Ensures distillation is cancelled, and
  // any pending data stored in |buffer_| is released.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void DomDistillerViewerSource::RequestViewerHandle::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (IsErrorPage()) {
    waiting_for_page_ready_ = false;
    SendJavaScript(viewer::GetErrorPageJs());
    SendJavaScript(viewer::GetShowFeedbackFormJs());
    Cancel(); // This will cause the object to clean itself up.
    return;
  }

  if (render_frame_host->GetParent()) {
    return;
  }
  waiting_for_page_ready_ = false;
  if (buffer_.empty()) {
    return;
  }
  web_contents()->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(buffer_));
  buffer_.clear();
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerServiceInterface* dom_distiller_service,
    const std::string& scheme,
    scoped_ptr<ExternalFeedbackReporter> external_reporter)
    : scheme_(scheme),
      dom_distiller_service_(dom_distiller_service),
      external_feedback_reporter_(external_reporter.Pass()) {
}

DomDistillerViewerSource::~DomDistillerViewerSource() {
}

std::string DomDistillerViewerSource::GetSource() const {
  return scheme_ + "://";
}

void DomDistillerViewerSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host) return;
  content::RenderViewHost* render_view_host =
      render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host);
  CHECK_EQ(0, render_view_host->GetEnabledBindings());

  if (kViewerCssPath == path) {
    std::string css = viewer::GetCss();
    callback.Run(base::RefCountedString::TakeString(&css));
    return;
  } else if (kViewerJsPath == path) {
    std::string js = viewer::GetJavaScript();
    callback.Run(base::RefCountedString::TakeString(&js));
    return;
  } else if (kViewerViewOriginalPath == path) {
    content::RecordAction(base::UserMetricsAction("DomDistiller_ViewOriginal"));
    callback.Run(NULL);
    return;
  } else if (kFeedbackBad == path) {
    FeedbackReporter::ReportQuality(false);
    callback.Run(NULL);
    if (!external_feedback_reporter_)
      return;
    content::WebContents* contents =
        content::WebContents::FromRenderFrameHost(render_frame_host);
    external_feedback_reporter_->ReportExternalFeedback(
        contents, contents->GetURL(), false);
    return;
  } else if (kFeedbackGood == path) {
    FeedbackReporter::ReportQuality(true);
    callback.Run(NULL);
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);
  // An empty |path| is invalid, but guard against it. If not empty, assume
  // |path| starts with '?', which is stripped away.
  scoped_ptr<ContentDataCallback> data_callback(
      new ContentDataCallback(callback));
  const std::string path_after_query_separator =
      path.size() > 0 ? path.substr(1) : "";
  RequestViewerHandle* request_viewer_handle = new RequestViewerHandle(
      web_contents, scheme_, path_after_query_separator, data_callback.Pass(),
      dom_distiller_service_->GetDistilledPagePrefs());
  scoped_ptr<ViewerHandle> viewer_handle = viewer::CreateViewRequest(
      dom_distiller_service_, path, request_viewer_handle,
      web_contents->GetContainerBounds().size());

  if (viewer_handle) {
    // The service returned a |ViewerHandle| and guarantees it will call
    // the |RequestViewerHandle|, so passing ownership to it, to ensure the
    // request is not cancelled. The |RequestViewerHandle| will delete itself
    // after receiving the callback.
    request_viewer_handle->TakeViewerHandle(viewer_handle.Pass());
  } else {
    request_viewer_handle->FlagAsErrorPage();
  }
};

std::string DomDistillerViewerSource::GetMimeType(
    const std::string& path) const {
  if (kViewerCssPath == path) {
    return "text/css";
  }
  if (kViewerJsPath == path) {
    return "text/javascript";
  }
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(scheme_.c_str());
}

// TODO(nyquist): Start tracking requests using this method.
void DomDistillerViewerSource::WillServiceRequest(
    const net::URLRequest* request,
    std::string* path) const {
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyObjectSrc()
    const {
  return "object-src 'none'; style-src 'self' https://fonts.googleapis.com;";
}

std::string DomDistillerViewerSource::GetContentSecurityPolicyFrameSrc() const {
  return "frame-src *;";
}

}  // namespace dom_distiller
