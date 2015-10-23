// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/renderer/net/net_error_page_controller.h"
#include "components/error_page/common/net_error_info.h"
#include "components/error_page/renderer/net_error_helper_core.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_process_observer.h"

class GURL;

namespace blink {
class WebFrame;
class WebURLResponse;
struct WebURLError;
}

namespace content {
class ResourceFetcher;
}

namespace error_page {
struct ErrorPageParams;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
class NetErrorHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<NetErrorHelper>,
      public content::RenderProcessObserver,
      public error_page::NetErrorHelperCore::Delegate,
      public NetErrorPageController::Delegate {
 public:
  explicit NetErrorHelper(content::RenderFrame* render_view);
  ~NetErrorHelper() override;

  // NetErrorPageController::Delegate implementation
  void ButtonPressed(error_page::NetErrorHelperCore::Button button) override;
  void TrackClick(int tracking_id) override;

  // RenderFrameObserver implementation.
  void DidStartProvisionalLoad() override;
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
  void DidFinishLoad() override;
  void OnStop() override;
  void WasShown() override;
  void WasHidden() override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // RenderProcessObserver implementation.
  void NetworkStateChanged(bool online) override;

  // Examines |frame| and |error| to see if this is an error worthy of a DNS
  // probe.  If it is, initializes |error_strings| based on |error|,
  // |is_failed_post|, and |locale| with suitable strings and returns true.
  // If not, returns false, in which case the caller should look up error
  // strings directly using LocalizedError::GetNavigationErrorStrings.
  //
  // Updates the NetErrorHelper with the assumption the page will be loaded
  // immediately.
  void GetErrorHTML(blink::WebFrame* frame,
                    const blink::WebURLError& error,
                    bool is_failed_post,
                    std::string* error_html);

  // Returns whether a load for |url| in |frame| should have its error page
  // suppressed.
  bool ShouldSuppressErrorPage(blink::WebFrame* frame, const GURL& url);

 private:
  // NetErrorHelperCore::Delegate implementation:
  void GenerateLocalizedErrorPage(
      const blink::WebURLError& error,
      bool is_failed_post,
      bool can_use_local_diagnostics_service,
      scoped_ptr<error_page::ErrorPageParams> params,
      bool* reload_button_shown,
      bool* show_saved_copy_button_shown,
      bool* show_cached_copy_button_shown,
      std::string* html) const override;
  void LoadErrorPageInMainFrame(const std::string& html,
                                const GURL& failed_url) override;
  void EnablePageHelperFunctions() override;
  void UpdateErrorPage(const blink::WebURLError& error,
                       bool is_failed_post,
                       bool can_use_local_diagnostics_service) override;
  void FetchNavigationCorrections(
      const GURL& navigation_correction_url,
      const std::string& navigation_correction_request_body) override;
  void CancelFetchNavigationCorrections() override;
  void SendTrackingRequest(const GURL& tracking_url,
                           const std::string& tracking_request_body) override;
  void ReloadPage() override;
  void LoadPageFromCache(const GURL& page_url) override;
  void DiagnoseError(const GURL& page_url) override;

  void OnNetErrorInfo(int status);
  void OnSetCanShowNetworkDiagnosticsDialog(
      bool can_use_local_diagnostics_service);
  void OnSetNavigationCorrectionInfo(const GURL& navigation_correction_url,
                                     const std::string& language,
                                     const std::string& country_code,
                                     const std::string& api_key,
                                     const GURL& search_url);

  void OnNavigationCorrectionsFetched(const blink::WebURLResponse& response,
                                      const std::string& data);

  void OnTrackingRequestComplete(const blink::WebURLResponse& response,
                                 const std::string& data);

  scoped_ptr<content::ResourceFetcher> correction_fetcher_;
  scoped_ptr<content::ResourceFetcher> tracking_fetcher_;

  scoped_ptr<error_page::NetErrorHelperCore> core_;

  // Weak factory for vending a weak pointer to a NetErrorPageController. Weak
  // pointers are invalidated on each commit, to prevent getting messages from
  // Controllers used for the previous commit that haven't yet been cleaned up.
  base::WeakPtrFactory<NetErrorPageController::Delegate>
      weak_controller_delegate_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetErrorHelper);
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
