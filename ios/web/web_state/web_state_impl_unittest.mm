// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/web_state/web_state_impl.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

namespace web {
namespace {

// Test observer to check that the WebStateObserver methods are called as
// expected.
class TestWebStateObserver : public WebStateObserver {
 public:
  TestWebStateObserver(WebState* web_state)
      : WebStateObserver(web_state),
        provisional_navigation_started_called_(false),
        navigation_item_committed_called_(false),
        page_loaded_called_(false),
        url_hash_changed_called_(false),
        history_state_changed_called_(false),
        web_state_destroyed_called_(false) {}

  // Methods returning true if the corresponding WebStateObserver method has
  // been called.
  bool provisional_navigation_started_called() {
    return provisional_navigation_started_called_;
  };
  bool navigation_item_committed_called() {
    return navigation_item_committed_called_;
  }
  bool page_loaded_called() { return page_loaded_called_; }
  bool url_hash_changed_called() { return url_hash_changed_called_; }
  bool history_state_changed_called() { return history_state_changed_called_; }
  bool web_state_destroyed_called() { return web_state_destroyed_called_; }

 private:
  // WebStateObserver implementation:
  void ProvisionalNavigationStarted(const GURL& url) override {
    provisional_navigation_started_called_ = true;
  }
  void NavigationItemCommitted(
      const LoadCommittedDetails& load_details) override {
    navigation_item_committed_called_ = true;
  }
  void PageLoaded(PageLoadCompletionStatus load_completion_status) override {
    page_loaded_called_ =
        load_completion_status == PageLoadCompletionStatus::SUCCESS;
  }
  void UrlHashChanged() override { url_hash_changed_called_ = true; }
  void HistoryStateChanged() override { history_state_changed_called_ = true; }
  void WebStateDestroyed() override {
    web_state_destroyed_called_ = true;
    Observe(nullptr);
  }

  bool provisional_navigation_started_called_;
  bool navigation_item_committed_called_;
  bool page_loaded_called_;
  bool url_hash_changed_called_;
  bool history_state_changed_called_;
  bool web_state_destroyed_called_;
};

// Creates and returns an HttpResponseHeader using the string representation.
scoped_refptr<net::HttpResponseHeaders> HeadersFromString(const char* string) {
  std::string raw_string(string);
  std::string headers_string = net::HttpUtil::AssembleRawHeaders(
      raw_string.c_str(), raw_string.length());
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(headers_string));
  return headers;
}

// Test callback for script commands.
// Sets |is_called| to true if it is called, and checks that the parameters
// match their expected values.
// |user_is_interacting| is not checked because Bind() has a maximum of 7
// parameters.
bool HandleScriptCommand(bool* is_called,
                         bool should_handle,
                         base::DictionaryValue* expected_value,
                         const GURL& expected_url,
                         const base::DictionaryValue& value,
                         const GURL& url,
                         bool user_is_interacting) {
  *is_called = true;
  EXPECT_TRUE(expected_value->Equals(&value));
  EXPECT_EQ(expected_url, url);
  return should_handle;
}

class WebStateTest : public PlatformTest {
 protected:
  void SetUp() override {
    web_state_.reset(new WebStateImpl(&browser_state_));
    web_state_->SetWebController(nil);
  }

  web::TestBrowserState browser_state_;
  scoped_ptr<WebStateImpl> web_state_;
};

TEST_F(WebStateTest, ResponseHeaders) {
  GURL real_url("http://foo.com/bar");
  GURL frame_url("http://frames-r-us.com/");
  scoped_refptr<net::HttpResponseHeaders> real_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Language: en\r\n"
      "X-Should-Be-Here: yep\r\n"
      "\r\n"));
  scoped_refptr<net::HttpResponseHeaders> frame_headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/pdf\r\n"
      "Content-Language: fr\r\n"
      "X-Should-Not-Be-Here: oops\r\n"
      "\r\n"));
  // Simulate a load of a page with a frame.
  web_state_->OnHttpResponseHeadersReceived(real_headers.get(), real_url);
  web_state_->OnHttpResponseHeadersReceived(frame_headers.get(), frame_url);
  // Include a hash to be sure it's handled correctly.
  web_state_->OnPageLoaded(GURL(real_url.spec() + std::string("#baz")), true);

  // Verify that the right header set was kept.
  EXPECT_TRUE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Be-Here"));
  EXPECT_FALSE(
      web_state_->GetHttpResponseHeaders()->HasHeader("X-Should-Not-Be-Here"));

  // And that it was parsed correctly.
  EXPECT_EQ("text/html", web_state_->GetContentsMimeType());
  EXPECT_EQ("en", web_state_->GetContentLanguageHeader());
}

TEST_F(WebStateTest, ResponseHeaderClearing) {
  GURL url("http://foo.com/");
  scoped_refptr<net::HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Language: en\r\n"
      "\r\n"));
  web_state_->OnHttpResponseHeadersReceived(headers.get(), url);

  // There should be no headers before loading.
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());

  // There should be headers and parsed values after loading.
  web_state_->OnPageLoaded(url, true);
  EXPECT_TRUE(web_state_->GetHttpResponseHeaders()->HasHeader("Content-Type"));
  EXPECT_NE("", web_state_->GetContentsMimeType());
  EXPECT_NE("", web_state_->GetContentLanguageHeader());

  // ... but not after loading another page, nor should there be specific
  // parsed values.
  web_state_->OnPageLoaded(GURL("http://elsewhere.com/"), true);
  EXPECT_EQ(NULL, web_state_->GetHttpResponseHeaders());
  EXPECT_EQ("", web_state_->GetContentsMimeType());
  EXPECT_EQ("", web_state_->GetContentLanguageHeader());
}

TEST_F(WebStateTest, ObserverTest) {
  scoped_ptr<TestWebStateObserver> observer(
      new TestWebStateObserver(web_state_.get()));
  EXPECT_EQ(web_state_.get(), observer->web_state());

  // Test that ProvisionalNavigationStarted() is called.
  EXPECT_FALSE(observer->provisional_navigation_started_called());
  web_state_->OnProvisionalNavigationStarted(GURL("http://test"));
  EXPECT_TRUE(observer->provisional_navigation_started_called());

  // Test that NavigtionItemCommitted() is called.
  EXPECT_FALSE(observer->navigation_item_committed_called());
  LoadCommittedDetails details;
  web_state_->OnNavigationItemCommitted(details);
  EXPECT_TRUE(observer->navigation_item_committed_called());

  // Test that DidFinishLoad() is called, only when there is no error.
  EXPECT_FALSE(observer->page_loaded_called());
  web_state_->OnPageLoaded(GURL("http://test"), false);
  EXPECT_FALSE(observer->page_loaded_called());
  web_state_->OnPageLoaded(GURL("http://test"), true);
  EXPECT_TRUE(observer->page_loaded_called());

  // Test that UrlHashChanged() is called.
  EXPECT_FALSE(observer->url_hash_changed_called());
  web_state_->OnUrlHashChanged();
  EXPECT_TRUE(observer->url_hash_changed_called());

  // Test that HistoryStateChanged() is called.
  EXPECT_FALSE(observer->history_state_changed_called());
  web_state_->OnHistoryStateChanged();
  EXPECT_TRUE(observer->history_state_changed_called());

  // Test that WebStateDestroyed() is called.
  EXPECT_FALSE(observer->web_state_destroyed_called());
  web_state_.reset();
  EXPECT_TRUE(observer->web_state_destroyed_called());

  EXPECT_EQ(nullptr, observer->web_state());
}

// Tests that script command callbacks are called correctly.
TEST_F(WebStateTest, ScriptCommand) {
  // Set up two script command callbacks.
  const std::string kPrefix1("prefix1");
  const std::string kCommand1("prefix1.command1");
  base::DictionaryValue value_1;
  value_1.SetString("a", "b");
  const GURL kUrl1("http://foo");
  bool is_called_1 = false;
  web_state_->AddScriptCommandCallback(
      base::Bind(&HandleScriptCommand, &is_called_1, true, &value_1, kUrl1),
      kPrefix1);

  const std::string kPrefix2("prefix2");
  const std::string kCommand2("prefix2.command2");
  base::DictionaryValue value_2;
  value_2.SetString("c", "d");
  const GURL kUrl2("http://bar");
  bool is_called_2 = false;
  web_state_->AddScriptCommandCallback(
      base::Bind(&HandleScriptCommand, &is_called_2, false, &value_2, kUrl2),
      kPrefix2);

  // Check that a irrelevant or invalid command does not trigger the callbacks.
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived("wohoo.blah", value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  EXPECT_FALSE(web_state_->OnScriptCommandReceived(
      "prefix1ButMissingDot", value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Check that only the callback matching the prefix is called, with the
  // expected parameters and return value;
  EXPECT_TRUE(
      web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1, false));
  EXPECT_TRUE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Remove the callback and check it is no longer called.
  is_called_1 = false;
  web_state_->RemoveScriptCommandCallback(kPrefix1);
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived(kCommand1, value_1, kUrl1, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_FALSE(is_called_2);

  // Check that a false return value is forwarded correctly.
  EXPECT_FALSE(
      web_state_->OnScriptCommandReceived(kCommand2, value_2, kUrl2, false));
  EXPECT_FALSE(is_called_1);
  EXPECT_TRUE(is_called_2);

  web_state_->RemoveScriptCommandCallback(kPrefix2);
}

}  // namespace
}  // namespace web
