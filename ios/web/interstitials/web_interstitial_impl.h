// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_IMPL_H_
#define IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_IMPL_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/web_state/ui/crw_content_view.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#include "url/gurl.h"

namespace web {

class WebInterstitialDelegate;
class WebInterstitialFacadeDelegate;
class WebInterstitialImpl;
class WebStateImpl;

// May be implemented in tests to run JavaScript on interstitials. This function
// has access to private EvaluateJavaScript method to be used for testing.
void EvaluateScriptForTesting(WebInterstitialImpl*,
                              NSString*,
                              JavaScriptCompletion);

// An abstract subclass of WebInterstitial that exposes the views necessary to
// embed the interstitial into a WebState.
class WebInterstitialImpl : public WebInterstitial, public WebStateObserver {
 public:
  WebInterstitialImpl(WebStateImpl* web_state, const GURL& url);
  ~WebInterstitialImpl() override;

  // Returns the transient content view used to display interstitial content.
  virtual CRWContentView* GetContentView() const = 0;

  // Returns the url corresponding to this interstitial.
  const GURL& GetUrl() const;

  // Sets the delegate used to drive the InterstitialPage facade.
  void SetFacadeDelegate(WebInterstitialFacadeDelegate* facade_delegate);
  WebInterstitialFacadeDelegate* GetFacadeDelegate() const;

  // WebInterstitial implementation:
  void Show() override;
  void Hide() override;
  void DontProceed() override;
  void Proceed() override;

  // WebStateObserver implementation:
  void WebStateDestroyed() override;

 protected:
  // Called before the WebInterstitialImpl is shown, giving subclasses a chance
  // to instantiate its view.
  virtual void PrepareForDisplay() {}

  // Returns the WebInterstitialDelegate that will handle Proceed/DontProceed
  // user actions.
  virtual WebInterstitialDelegate* GetDelegate() const = 0;

  // Convenience method for getting the WebStateImpl.
  WebStateImpl* GetWebStateImpl() const;

  // Evaluates the given |script| on interstitial's web view if there is one.
  // Calls |completionHandler| with results of the evaluation.
  // The |completionHandler| can be nil. Must be used only for testing.
  virtual void EvaluateJavaScript(NSString* script,
                                  JavaScriptCompletion completionHandler) = 0;

 private:
  // The URL corresponding to the page that resulted in this interstitial.
  GURL url_;
  // The delegate used to communicate with the InterstitialPageImplsIOS facade.
  WebInterstitialFacadeDelegate* facade_delegate_;
  // Whether or not either Proceed() or DontProceed() has been called.
  bool action_taken_;

  // Must be implemented only for testing purposes.
  friend void web::EvaluateScriptForTesting(WebInterstitialImpl*,
                                            NSString*,
                                            JavaScriptCompletion);
};

}  // namespace web

#endif  // IOS_WEB_INTERSTITIALS_WEB_INTERSTITIAL_IMPL_H_
