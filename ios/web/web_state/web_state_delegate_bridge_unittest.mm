// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#import <Foundation/Foundation.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/web_state/web_state_delegate_stub.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

namespace web {

// Test fixture to test WebStateDelegateBridge class.
class WebStateDelegateBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebStateDelegate)];
    delegate_.reset([[CRWWebStateDelegateStub alloc]
        initWithRepresentedObject:originalMockDelegate]);

    bridge_.reset(new WebStateDelegateBridge(delegate_.get()));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    PlatformTest::TearDown();
  }

  base::scoped_nsprotocol<id> delegate_;
  std::unique_ptr<WebStateDelegateBridge> bridge_;
  web::TestWebState test_web_state_;
};

// Tests |webState:openURLWithParams:| forwarding.
TEST_F(WebStateDelegateBridgeTest, OpenURLFromWebState) {
  ASSERT_FALSE([delegate_ webState]);
  ASSERT_FALSE([delegate_ openURLParams]);

  web::WebState::OpenURLParams params(
      GURL("https://chromium.test/"),
      web::Referrer(GURL("https://chromium2.test/"), ReferrerPolicyNever),
      WindowOpenDisposition::NEW_WINDOW, ui::PAGE_TRANSITION_FORM_SUBMIT, true);
  EXPECT_EQ(&test_web_state_,
            bridge_->OpenURLFromWebState(&test_web_state_, params));

  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
  const web::WebState::OpenURLParams* result_params = [delegate_ openURLParams];
  ASSERT_TRUE(result_params);
  EXPECT_EQ(params.url, result_params->url);
  EXPECT_EQ(params.referrer.url, result_params->referrer.url);
  EXPECT_EQ(params.referrer.policy, result_params->referrer.policy);
  EXPECT_EQ(params.disposition, result_params->disposition);
  EXPECT_EQ(static_cast<int>(params.transition),
            static_cast<int>(result_params->transition));
  EXPECT_EQ(params.is_renderer_initiated, result_params->is_renderer_initiated);
}

// Tests |LoadProgressChanged| forwarding.
TEST_F(WebStateDelegateBridgeTest, LoadProgressChanged) {
  ASSERT_EQ(0.0, [delegate_ changedProgress]);
  bridge_->LoadProgressChanged(nullptr, 1.0);
  EXPECT_EQ(1.0, [delegate_ changedProgress]);
}

// Tests |HandleContextMenu| forwarding.
TEST_F(WebStateDelegateBridgeTest, HandleContextMenu) {
  EXPECT_EQ(nil, [delegate_ contextMenuParams]);
  web::ContextMenuParams context_menu_params;
  context_menu_params.menu_title.reset([@"Menu title" copy]);
  context_menu_params.link_url = GURL("http://www.url.com");
  context_menu_params.src_url = GURL("http://www.url.com/image.jpeg");
  context_menu_params.referrer_policy = web::ReferrerPolicyOrigin;
  context_menu_params.view.reset([[UIView alloc] init]);
  context_menu_params.location = CGPointMake(5.0, 5.0);
  bridge_->HandleContextMenu(nullptr, context_menu_params);
  web::ContextMenuParams* result_params = [delegate_ contextMenuParams];
  EXPECT_NE(nullptr, result_params);
  EXPECT_EQ(context_menu_params.menu_title, result_params->menu_title);
  EXPECT_EQ(context_menu_params.link_url, result_params->link_url);
  EXPECT_EQ(context_menu_params.src_url, result_params->src_url);
  EXPECT_EQ(context_menu_params.referrer_policy,
            result_params->referrer_policy);
  EXPECT_EQ(context_menu_params.view, result_params->view);
  EXPECT_EQ(context_menu_params.location.x, result_params->location.x);
  EXPECT_EQ(context_menu_params.location.y, result_params->location.y);
}

// Tests |GetJavaScriptDialogPresenter| forwarding.
TEST_F(WebStateDelegateBridgeTest, GetJavaScriptDialogPresenter) {
  EXPECT_FALSE([delegate_ javaScriptDialogPresenterRequested]);
  bridge_->GetJavaScriptDialogPresenter(nullptr);
  EXPECT_TRUE([delegate_ javaScriptDialogPresenterRequested]);
}

// Tests |OnAuthRequired| forwarding.
TEST_F(WebStateDelegateBridgeTest, OnAuthRequired) {
  EXPECT_FALSE([delegate_ authenticationRequested]);
  EXPECT_FALSE([delegate_ webState]);
  base::scoped_nsobject<NSURLProtectionSpace> protection_space(
      [[NSURLProtectionSpace alloc] init]);
  base::scoped_nsobject<NSURLCredential> credential(
      [[NSURLCredential alloc] init]);
  WebStateDelegate::AuthCallback callback;
  bridge_->OnAuthRequired(&test_web_state_, protection_space.get(),
                          credential.get(), callback);
  EXPECT_TRUE([delegate_ authenticationRequested]);
  EXPECT_EQ(&test_web_state_, [delegate_ webState]);
}

}  // namespace web
