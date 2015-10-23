// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_state_impl.h"

#include "base/strings/sys_string_conversions.h"
#include "ios/web/interstitials/web_interstitial_impl.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_state/credential.h"
#include "ios/web/public/web_state/ui/crw_content_view.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_policy_decider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#include "ios/web/web_state/web_state_facade_delegate.h"
#import "ios/web/webui/web_ui_ios_controller_factory_registry.h"
#import "ios/web/webui/web_ui_ios_impl.h"
#include "net/http/http_response_headers.h"

namespace web {

WebStateImpl::WebStateImpl(BrowserState* browser_state)
    : is_loading_(false),
      facade_delegate_(nullptr),
      web_controller_(nil),
      navigation_manager_(this, browser_state),
      interstitial_(nullptr),
      cache_mode_(net::RequestTracker::CACHE_NORMAL) {
}

WebStateImpl::~WebStateImpl() {
  // WebUI depends on web state so it must be destroyed first in case any WebUI
  // implementations depends on accessing web state during destruction.
  ClearWebUI();

  // The facade layer (owned by the delegate) should be destroyed before the web
  // layer.
  DCHECK(!facade_delegate_);

  FOR_EACH_OBSERVER(WebStateObserver, observers_, WebStateDestroyed());
  FOR_EACH_OBSERVER(WebStateObserver, observers_, ResetWebState());
  FOR_EACH_OBSERVER(WebStatePolicyDecider, policy_deciders_,
                    WebStateDestroyed());
  FOR_EACH_OBSERVER(WebStatePolicyDecider, policy_deciders_, ResetWebState());
  DCHECK(script_command_callbacks_.empty());
  if (request_tracker_.get())
    CloseRequestTracker();
}

void WebStateImpl::AddObserver(WebStateObserver* observer) {
  DCHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void WebStateImpl::RemoveObserver(WebStateObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void WebStateImpl::AddPolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  DCHECK(!policy_deciders_.HasObserver(decider));
  policy_deciders_.AddObserver(decider);
}

void WebStateImpl::RemovePolicyDecider(WebStatePolicyDecider* decider) {
  // Despite the name, ObserverList is actually generic, so it is used for
  // deciders. This makes the call here odd looking, but it's really just
  // managing the list, not setting observers on deciders.
  DCHECK(policy_deciders_.HasObserver(decider));
  policy_deciders_.RemoveObserver(decider);
}

bool WebStateImpl::Configured() const {
  return web_controller_ != nil;
}

void WebStateImpl::SetWebController(CRWWebController* web_controller) {
  DCHECK(!web_controller_);
  web_controller_ = web_controller;
}

WebStateFacadeDelegate* WebStateImpl::GetFacadeDelegate() const {
  return facade_delegate_;
}

void WebStateImpl::SetFacadeDelegate(WebStateFacadeDelegate* facade_delegate) {
  facade_delegate_ = facade_delegate;
}

WebStateImpl* WebStateImpl::CopyForSessionWindow() {
  WebStateImpl* copy = new WebStateImpl(GetBrowserState());
  copy->GetNavigationManagerImpl().CopyState(&navigation_manager_);
  return copy;
}

void WebStateImpl::OnUrlHashChanged() {
  FOR_EACH_OBSERVER(WebStateObserver, observers_, UrlHashChanged());
}

void WebStateImpl::OnHistoryStateChanged() {
  FOR_EACH_OBSERVER(WebStateObserver, observers_, HistoryStateChanged());
}

bool WebStateImpl::OnScriptCommandReceived(const std::string& command,
                                           const base::DictionaryValue& value,
                                           const GURL& url,
                                           bool user_is_interacting) {
  size_t dot_position = command.find_first_of('.');
  if (dot_position == 0 || dot_position == std::string::npos)
    return false;

  std::string prefix = command.substr(0, dot_position);
  auto it = script_command_callbacks_.find(prefix);
  if (it == script_command_callbacks_.end())
    return false;

  return it->second.Run(value, url, user_is_interacting);
}

void WebStateImpl::SetIsLoading(bool is_loading) {
  DCHECK(Configured());
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;
  if (facade_delegate_)
    facade_delegate_->OnLoadingStateChanged();

  if (is_loading) {
    FOR_EACH_OBSERVER(WebStateObserver, observers_, DidStartLoading());
  } else {
    FOR_EACH_OBSERVER(WebStateObserver, observers_, DidStopLoading());
  }
}

bool WebStateImpl::IsLoading() const {
  return is_loading_;
}

void WebStateImpl::OnPageLoaded(const GURL& url, bool load_success) {
  UpdateHttpResponseHeaders(url);
  if (facade_delegate_)
    facade_delegate_->OnPageLoaded();

  PageLoadCompletionStatus load_completion_status =
      load_success ? PageLoadCompletionStatus::SUCCESS
                   : PageLoadCompletionStatus::FAILURE;
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    PageLoaded(load_completion_status));
}

void WebStateImpl::OnFormActivityRegistered(const std::string& form_name,
                                            const std::string& field_name,
                                            const std::string& type,
                                            const std::string& value,
                                            int key_code,
                                            bool input_missing) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    FormActivityRegistered(form_name, field_name, type, value,
                                           key_code, input_missing));
}

void WebStateImpl::OnFaviconUrlUpdated(
    const std::vector<FaviconURL>& candidates) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    FaviconUrlUpdated(candidates));
}

void WebStateImpl::OnCredentialsRequested(
    int request_id,
    const GURL& source_url,
    bool suppress_ui,
    const std::vector<std::string>& federations,
    bool user_interaction) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    CredentialsRequested(request_id, source_url, suppress_ui,
                                         federations, user_interaction));
}

void WebStateImpl::OnSignedIn(int request_id,
                              const GURL& source_url,
                              const web::Credential& credential) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    SignedIn(request_id, source_url, credential));
}

void WebStateImpl::OnSignedIn(int request_id, const GURL& source_url) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    SignedIn(request_id, source_url));
}

void WebStateImpl::OnSignedOut(int request_id, const GURL& source_url) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    SignedOut(request_id, source_url));
}

void WebStateImpl::OnSignInFailed(int request_id,
                                  const GURL& source_url,
                                  const web::Credential& credential) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    SignInFailed(request_id, source_url, credential));
}

void WebStateImpl::OnSignInFailed(int request_id, const GURL& source_url) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    SignInFailed(request_id, source_url));
}

void WebStateImpl::OnDocumentSubmitted(const std::string& form_name,
                                       bool user_initiated) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    DocumentSubmitted(form_name, user_initiated));
}

NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() {
  return navigation_manager_;
}

const NavigationManagerImpl& WebStateImpl::GetNavigationManagerImpl() const {
  return navigation_manager_;
}

// There are currently two kinds of WebUI: those that have been adapted to
// web::WebUIIOS, and those that are still using content::WebUI. Try to create
// it as the first, and then fall back to the latter if necessary.
void WebStateImpl::CreateWebUI(const GURL& url) {
  web_ui_.reset(CreateWebUIIOS(url));
  if (!web_ui_ && facade_delegate_)
    facade_delegate_->CreateLegacyWebUI(url);
}

void WebStateImpl::ClearWebUI() {
  if (facade_delegate_)
    facade_delegate_->ClearLegacyWebUI();
  web_ui_.reset();
}

bool WebStateImpl::HasWebUI() {
  return web_ui_ || (facade_delegate_ && facade_delegate_->HasLegacyWebUI());
}

void WebStateImpl::ProcessWebUIMessage(const GURL& source_url,
                                       const std::string& message,
                                       const base::ListValue& args) {
  if (web_ui_) {
    DCHECK(!facade_delegate_ || !facade_delegate_->HasLegacyWebUI());
    web_ui_->ProcessWebUIIOSMessage(source_url, message, args);
  } else if (facade_delegate_) {
    facade_delegate_->ProcessLegacyWebUIMessage(source_url, message, args);
  }
}

void WebStateImpl::LoadWebUIHtml(const base::string16& html, const GURL& url) {
  CHECK(web::GetWebClient()->IsAppSpecificURL(url));
  [web_controller_ loadHTML:base::SysUTF16ToNSString(html)
          forAppSpecificURL:url];
}

const base::string16& WebStateImpl::GetTitle() const {
  // TODO(stuartmorgan): Implement the NavigationManager logic necessary to
  // match the WebContents implementation of this method.
  DCHECK(Configured());
  web::NavigationItem* item = navigation_manager_.GetLastCommittedItem();
  if (item) {
    return item->GetTitleForDisplay(
        web::GetWebClient()->GetAcceptLangs(GetBrowserState()));
  }
  return empty_string16_;
}

void WebStateImpl::ShowTransientContentView(CRWContentView* content_view) {
  DCHECK(Configured());
  DCHECK(content_view);
  DCHECK(content_view.scrollView);
  DCHECK([content_view.scrollView isDescendantOfView:content_view]);
  [web_controller_ showTransientContentView:content_view];
}

bool WebStateImpl::IsShowingWebInterstitial() const {
  // Technically we could have |interstitial_| set but its view isn't
  // being displayed, but there's no code path where that could occur.
  return interstitial_ != nullptr;
}

WebInterstitial* WebStateImpl::GetWebInterstitial() const {
  return interstitial_;
}

net::HttpResponseHeaders* WebStateImpl::GetHttpResponseHeaders() const {
  return http_response_headers_.get();
}

void WebStateImpl::OnHttpResponseHeadersReceived(
    net::HttpResponseHeaders* response_headers,
    const GURL& resource_url) {
  // Store the headers in a map until the page finishes loading, as we do not
  // know which URL corresponds to the main page yet.
  // Remove the hash (if any) as it is sometimes altered by in-page navigations.
  const GURL& url = GURLByRemovingRefFromGURL(resource_url);
  response_headers_map_[url] = response_headers;
}

void WebStateImpl::UpdateHttpResponseHeaders(const GURL& url) {
  // Reset the state.
  http_response_headers_ = NULL;
  mime_type_.clear();
  content_language_header_.clear();

  // Discard all the response headers except the ones for |main_page_url|.
  auto it = response_headers_map_.find(GURLByRemovingRefFromGURL(url));
  if (it != response_headers_map_.end())
    http_response_headers_ = it->second;
  response_headers_map_.clear();

  if (!http_response_headers_.get())
    return;

  // MIME type.
  std::string mime_type;
  http_response_headers_->GetMimeType(&mime_type);
  mime_type_ = mime_type;

  // Content-Language
  std::string content_language;
  http_response_headers_->GetNormalizedHeader("content-language",
                                              &content_language);
  // Remove everything after the comma ',' if any.
  size_t comma_index = content_language.find_first_of(',');
  if (comma_index != std::string::npos)
    content_language.resize(comma_index);
  content_language_header_ = content_language;
}

void WebStateImpl::ShowWebInterstitial(WebInterstitialImpl* interstitial) {
  DCHECK(Configured());
  DCHECK(interstitial);
  interstitial_ = interstitial;
  ShowTransientContentView(interstitial_->GetContentView());
}

void WebStateImpl::ClearTransientContentView() {
  if (interstitial_) {
    CRWSessionController* sessionController =
        navigation_manager_.GetSessionController();
    web::NavigationItem* currentItem =
        [sessionController.currentEntry navigationItem];
    if (currentItem->IsUnsafe()) {
      // The unsafe page should be removed from history, and, in fact,
      // SafeBrowsingBlockingPage will do just that *provided* that it
      // isn't the current page. So to make this happen, before removing the
      // interstitial, have the session controller go back one page.
      [sessionController goBack];
    }
    [sessionController discardNonCommittedEntries];
    // Store the currently displayed interstitial in a local variable and reset
    // |interstitial_| early.  This is to prevent an infinite loop, as
    // |DontProceed()| internally calls |ClearTransientContentView()|.
    web::WebInterstitial* interstitial = interstitial_;
    interstitial_ = nullptr;
    interstitial->DontProceed();
    // Don't access |interstitial| after calling |DontProceed()|, as it triggers
    // deletion.
    FOR_EACH_OBSERVER(WebStateObserver, observers_, InsterstitialDismissed());
  }
  [web_controller_ clearTransientContentView];
}

WebUIIOS* WebStateImpl::CreateWebUIIOS(const GURL& url) {
  WebUIIOSControllerFactory* factory =
      WebUIIOSControllerFactoryRegistry::GetInstance();
  if (!factory)
    return NULL;
  WebUIIOSImpl* web_ui = new WebUIIOSImpl(this);
  WebUIIOSController* controller =
      factory->CreateWebUIIOSControllerForURL(web_ui, url);
  if (controller) {
    web_ui->SetController(controller);
    return web_ui;
  }

  delete web_ui;
  return NULL;
}

void WebStateImpl::ExecuteJavaScriptAsync(const base::string16& javascript) {
  DCHECK(Configured());
  [web_controller_ evaluateJavaScript:base::SysUTF16ToNSString(javascript)
                  stringResultHandler:nil];
}

bool WebStateImpl::ShouldAllowRequest(NSURLRequest* request) {
  base::ObserverListBase<WebStatePolicyDecider>::Iterator it(&policy_deciders_);
  WebStatePolicyDecider* policy_decider = nullptr;
  while ((policy_decider = it.GetNext()) != nullptr) {
    if (!policy_decider->ShouldAllowRequest(request))
      return false;
  }
  return true;
}

bool WebStateImpl::ShouldAllowResponse(NSURLResponse* response) {
  base::ObserverListBase<WebStatePolicyDecider>::Iterator it(&policy_deciders_);
  WebStatePolicyDecider* policy_decider = nullptr;
  while ((policy_decider = it.GetNext()) != nullptr) {
    if (!policy_decider->ShouldAllowResponse(response))
      return false;
  }
  return true;
}

#pragma mark - RequestTracker management

void WebStateImpl::InitializeRequestTracker(
    id<CRWRequestTrackerDelegate> delegate) {
  BrowserState* browser_state = navigation_manager_.GetBrowserState();
  request_tracker_ = RequestTrackerImpl::CreateTrackerForRequestGroupID(
      GetRequestGroupID(), browser_state, browser_state->GetRequestContext(),
      delegate);
}

void WebStateImpl::CloseRequestTracker() {
  request_tracker_->Close();
  request_tracker_ = NULL;
}

RequestTrackerImpl* WebStateImpl::GetRequestTracker() {
  DCHECK(request_tracker_.get());
  return request_tracker_.get();
}

net::RequestTracker::CacheMode WebStateImpl::GetCacheMode() {
  return cache_mode_;
}

void WebStateImpl::SetCacheMode(net::RequestTracker::CacheMode mode) {
  cache_mode_ = mode;
}

NSString* WebStateImpl::GetRequestGroupID() {
  if (request_group_id_.get() == nil)
    request_group_id_.reset([GenerateNewRequestGroupID() copy]);

  return request_group_id_;
}

int WebStateImpl::DownloadImage(
    const GURL& url,
    bool is_favicon,
    uint32_t max_bitmap_size,
    bool bypass_cache,
    const ImageDownloadCallback& callback) {
  // |is_favicon| specifies whether the download of the image occurs with
  // cookies or not. Currently, only downloads without cookies are supported.
  // |bypass_cache| is ignored since the downloads never go through a cache.
  DCHECK(is_favicon);
  return [[web_controller_ delegate] downloadImageAtUrl:url
                                          maxBitmapSize:max_bitmap_size
                                              callback:callback];
}

#pragma mark - WebState implementation

UIView* WebStateImpl::GetView() {
  return [web_controller_ view];
}

WebViewType WebStateImpl::GetWebViewType() const {
  return [web_controller_ webViewType];
}

BrowserState* WebStateImpl::GetBrowserState() const {
  return navigation_manager_.GetBrowserState();
}

void WebStateImpl::OpenURL(const WebState::OpenURLParams& params) {
  DCHECK(Configured());
  ClearTransientContentView();
  [[web_controller_ delegate] openURLWithParams:params];
}

NavigationManager* WebStateImpl::GetNavigationManager() {
  return &GetNavigationManagerImpl();
}

CRWJSInjectionReceiver* WebStateImpl::GetJSInjectionReceiver() const {
  return [web_controller_ jsInjectionReceiver];
}

const std::string& WebStateImpl::GetContentLanguageHeader() const {
  return content_language_header_;
}

const std::string& WebStateImpl::GetContentsMimeType() const {
  return mime_type_;
}

bool WebStateImpl::ContentIsHTML() const {
  return [web_controller_ contentIsHTML];
}

const GURL& WebStateImpl::GetVisibleURL() const {
  web::NavigationItem* item = navigation_manager_.GetVisibleItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

const GURL& WebStateImpl::GetLastCommittedURL() const {
  web::NavigationItem* item = navigation_manager_.GetLastCommittedItem();
  return item ? item->GetVirtualURL() : GURL::EmptyGURL();
}

GURL WebStateImpl::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  return [web_controller_ currentURLWithTrustLevel:trust_level];
}

void WebStateImpl::AddScriptCommandCallback(
    const ScriptCommandCallback& callback,
    const std::string& command_prefix) {
  DCHECK(!command_prefix.empty());
  DCHECK(command_prefix.find_first_of('.') == std::string::npos);
  DCHECK(script_command_callbacks_.find(command_prefix) ==
         script_command_callbacks_.end());
  script_command_callbacks_[command_prefix] = callback;
}

void WebStateImpl::RemoveScriptCommandCallback(
    const std::string& command_prefix) {
  DCHECK(script_command_callbacks_.find(command_prefix) !=
         script_command_callbacks_.end());
  script_command_callbacks_.erase(command_prefix);
}

id<CRWWebViewProxy> WebStateImpl::GetWebViewProxy() const {
  return [web_controller_ webViewProxy];
}

void WebStateImpl::OnProvisionalNavigationStarted(const GURL& url) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    ProvisionalNavigationStarted(url));
}

#pragma mark - NavigationManagerDelegate implementation

// Mirror WebContentsImpl::NavigateToPendingEntry() so that
// NavigationControllerIO::GoBack() actually goes back.
void WebStateImpl::NavigateToPendingEntry() {
  [web_controller_ loadCurrentURL];
}

void WebStateImpl::OnNavigationItemCommitted(
    const LoadCommittedDetails& load_details) {
  FOR_EACH_OBSERVER(WebStateObserver, observers_,
                    NavigationItemCommitted(load_details));
}

WebState* WebStateImpl::GetWebState() {
  return this;
}

}  // namespace web
