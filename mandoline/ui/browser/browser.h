// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_BROWSER_H_
#define MANDOLINE_UI_BROWSER_BROWSER_H_

#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/interfaces/view_manager_root.mojom.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mandoline/ui/browser/navigator_host_impl.h"
#include "mandoline/ui/browser/omnibox.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "url/gurl.h"

namespace mojo {
class ViewManagerInit;
}

namespace mandoline {

class BrowserUI;
class MergedServiceProvider;

class Browser : public mojo::ApplicationDelegate,
                public mojo::ViewManagerDelegate,
                public mojo::ViewManagerRootClient,
                public OmniboxClient,
                public mojo::InterfaceFactory<mojo::NavigatorHost> {
 public:
  Browser();
  ~Browser() override;

  void ReplaceContentWithURL(const mojo::String& url);

  mojo::View* content() { return content_; }
  mojo::View* omnibox() { return omnibox_; }

  const GURL& current_url() const { return current_url_; }

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;
  bool ConfigureOutgoingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::ViewManagerDelegate:
  void OnEmbed(mojo::View* root,
               mojo::InterfaceRequest<mojo::ServiceProvider> services,
               mojo::ServiceProviderPtr exposed_services) override;
  void OnViewManagerDisconnected(mojo::ViewManager* view_manager) override;

  // Overridden from ViewManagerRootClient:
  void OnAccelerator(mojo::EventPtr event) override;

  // Overridden from OmniboxClient:
  void OpenURL(const mojo::String& url) override;

  // Overridden from mojo::InterfaceFactory<mojo::NavigatorHost>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::NavigatorHost> request) override;

  void Embed(const mojo::String& url,
             mojo::InterfaceRequest<mojo::ServiceProvider> services,
             mojo::ServiceProviderPtr exposed_services);

  void ShowOmnibox(const mojo::String& url,
                   mojo::InterfaceRequest<mojo::ServiceProvider> services,
                   mojo::ServiceProviderPtr exposed_services);

  scoped_ptr<mojo::ViewManagerInit> view_manager_init_;

  // Only support being embedded once, so both application-level
  // and embedding-level state are shared on the same object.
  mojo::View* root_;
  mojo::View* content_;
  mojo::View* omnibox_;
  std::string default_url_;
  std::string pending_url_;

  mojo::ServiceProviderImpl exposed_services_impl_;
  scoped_ptr<MergedServiceProvider> merged_service_provider_;

  NavigatorHostImpl navigator_host_;

  GURL current_url_;

  scoped_ptr<BrowserUI> ui_;

  DISALLOW_COPY_AND_ASSIGN(Browser);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_BROWSER_H_
