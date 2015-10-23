// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_INTERSTITIALS_NATIVE_WEB_INTERSTITIAL_IMPL_H_
#define IOS_WEB_INTERSTITIALS_NATIVE_WEB_INTERSTITIAL_IMPL_H_

#include "ios/web/interstitials/web_interstitial_impl.h"

#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"

namespace web {

class NativeWebInterstitialDelegate;

// A concrete subclass of WebInterstitialImpl that is used to display
// interstitials created via native views.
class NativeWebInterstitialImpl : public WebInterstitialImpl {
 public:
  NativeWebInterstitialImpl(WebStateImpl* web_state,
                            const GURL& url,
                            scoped_ptr<NativeWebInterstitialDelegate> delegate);
  ~NativeWebInterstitialImpl() override;

  // WebInterstitialImpl implementation:
  CRWContentView* GetContentView() const override;

 protected:
  // WebInterstitialImpl implementation:
  void PrepareForDisplay() override;
  WebInterstitialDelegate* GetDelegate() const override;
  void EvaluateJavaScript(NSString* script,
                          JavaScriptCompletion completionHandler) override;

 private:
  // The native interstitial delegate.
  scoped_ptr<NativeWebInterstitialDelegate> delegate_;
  // The transient content view containing interstitial content.
  base::scoped_nsobject<CRWContentView> content_view_;
};

}  // namespace web

#endif  // IOS_WEB_INTERSTITIALS_NATIVE_WEB_INTERSTITIAL_IMPL_H_
