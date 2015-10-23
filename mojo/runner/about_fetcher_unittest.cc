// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/runner/about_fetcher.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/application_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {
namespace {

class TestContentHandler : public ApplicationDelegate,
                           public InterfaceFactory<ContentHandler>,
                           public ContentHandler {
 public:
  TestContentHandler() : response_number_(0) {}
  ~TestContentHandler() override {}

  size_t response_number() const { return response_number_; }
  const URLResponse* latest_response() const { return latest_response_.get(); }

 private:
  // Overridden from ApplicationDelegate:
  void Initialize(ApplicationImpl* app) override {}
  bool ConfigureIncomingConnection(ApplicationConnection* connection) override {
    connection->AddService<ContentHandler>(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>:
  void Create(ApplicationConnection* connection,
              InterfaceRequest<ContentHandler> request) override {
    bindings_.AddBinding(this, request.Pass());
  }

  // Overridden from ContentHandler:
  void StartApplication(InterfaceRequest<Application> application,
                        URLResponsePtr response) override {
    response_number_++;
    latest_response_ = response.Pass();

    // Drop |application| request. This results in the application manager
    // dropping the ServiceProvider interface request provided by the client
    // who made the ConnectToApplication() call. Therefore the client could
    // listen for connection error of the ServiceProvider interface to learn
    // that StartApplication() has been called.
  }

  size_t response_number_;
  URLResponsePtr latest_response_;
  WeakBindingSet<ContentHandler> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestContentHandler);
};

class TestApplicationManagerDelegate
    : public shell::ApplicationManager::Delegate {
 public:
  TestApplicationManagerDelegate() {}
  ~TestApplicationManagerDelegate() override {}

 private:
  // Overridden from ApplicationManager::Delegate:
  GURL ResolveMappings(const GURL& url) override { return url; }
  GURL ResolveMojoURL(const GURL& url) override { return url; }
  bool CreateFetcher(
      const GURL& url,
      const shell::Fetcher::FetchCallback& loader_callback) override {
    if (url.SchemeIs(AboutFetcher::kAboutScheme)) {
      AboutFetcher::Start(url, loader_callback);
      return true;
    }

    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(TestApplicationManagerDelegate);
};

class TestLoader : public shell::ApplicationLoader {
 public:
  explicit TestLoader(ApplicationDelegate* delegate) : delegate_(delegate) {}
  ~TestLoader() override {}

 private:
  // Overridden from ApplicationLoader:
  void Load(const GURL& url, InterfaceRequest<Application> request) override {
    app_.reset(new ApplicationImpl(delegate_, request.Pass()));
  }

  ApplicationDelegate* delegate_;
  scoped_ptr<ApplicationImpl> app_;

  DISALLOW_COPY_AND_ASSIGN(TestLoader);
};

class AboutFetcherTest : public testing::Test {
 public:
  AboutFetcherTest() {}
  ~AboutFetcherTest() override {}

 protected:
  const TestContentHandler* html_content_handler() const {
    return &html_content_handler_;
  }

  void ConnectAndWait(const std::string& url) {
    base::RunLoop run_loop;

    ServiceProviderPtr service_provider;
    InterfaceRequest<ServiceProvider> service_provider_request =
        GetProxy(&service_provider);
    // This connection error handler will be called when:
    // - TestContentHandler::StartApplication() has been called (please see
    //   comments in that method); or
    // - the application manager fails to fetch the requested URL.
    service_provider.set_connection_error_handler(
        [&run_loop]() { run_loop.Quit(); });

    URLRequestPtr request(URLRequest::New());
    request->url = url;
    application_manager_->ConnectToApplication(
        nullptr, request.Pass(), std::string(), GURL(),
        service_provider_request.Pass(), nullptr, shell::CapabilityFilter(),
        base::Closure());

    run_loop.Run();
  }

  // Overridden from testing::Test:
  void SetUp() override {
    application_manager_.reset(new shell::ApplicationManager(&test_delegate_));
    application_manager_->SetLoaderForURL(
        make_scoped_ptr(new TestLoader(&html_content_handler_)),
        GURL("test:html_content_handler"));
    application_manager_->RegisterContentHandler(
        "text/html", GURL("test:html_content_handler"));
  }

  void TearDown() override { application_manager_.reset(); }

 private:
  base::ShadowingAtExitManager at_exit_;
  TestApplicationManagerDelegate test_delegate_;
  TestContentHandler html_content_handler_;
  base::MessageLoop loop_;
  scoped_ptr<shell::ApplicationManager> application_manager_;

  DISALLOW_COPY_AND_ASSIGN(AboutFetcherTest);
};

TEST_F(AboutFetcherTest, AboutBlank) {
  ConnectAndWait("about:blank");

  ASSERT_EQ(1u, html_content_handler()->response_number());

  const URLResponse* response = html_content_handler()->latest_response();
  EXPECT_EQ("about:blank", response->url);
  EXPECT_EQ(200u, response->status_code);
  EXPECT_EQ("text/html", response->mime_type);
  EXPECT_FALSE(response->body.is_valid());
}

TEST_F(AboutFetcherTest, UnrecognizedURL) {
  ConnectAndWait("about:some_unrecognized_url");

  ASSERT_EQ(0u, html_content_handler()->response_number());
}

}  // namespace
}  // namespace runner
}  // namespace mojo
