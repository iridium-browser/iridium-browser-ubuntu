// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_resources_provider.h"

#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace {

void AddIcons(content::WebUIDataSource* html_source) {
  html_source->AddResourcePath("elements/icon/sad-face.png",
                              IDR_SAD_FACE_ICON);
  html_source->AddResourcePath("elements/icon/sad-face2x.png",
                              IDR_SAD_FACE_2X_ICON);
}

void AddMainWebResources(content::WebUIDataSource* html_source) {
  html_source->AddResourcePath("media_router.js", IDR_MEDIA_ROUTER_JS);
  html_source->AddResourcePath("media_router_common.css",
                               IDR_MEDIA_ROUTER_COMMON_CSS);
  html_source->AddResourcePath("media_router.css",
                               IDR_MEDIA_ROUTER_CSS);
  html_source->AddResourcePath("media_router_data.js",
                               IDR_MEDIA_ROUTER_DATA_JS);
  html_source->AddResourcePath("media_router_ui_interface.js",
                               IDR_MEDIA_ROUTER_UI_INTERFACE_JS);
}

void AddPolymerElements(content::WebUIDataSource* html_source) {
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.css",
      IDR_ISSUE_BANNER_CSS);
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.html",
      IDR_ISSUE_BANNER_HTML);
  html_source->AddResourcePath(
      "elements/issue_banner/issue_banner.js",
      IDR_ISSUE_BANNER_JS);
  html_source->AddResourcePath(
      "elements/media_router_container/media_router_container.css",
      IDR_MEDIA_ROUTER_CONTAINER_CSS);
  html_source->AddResourcePath(
      "elements/media_router_container/media_router_container.html",
      IDR_MEDIA_ROUTER_CONTAINER_HTML);
  html_source->AddResourcePath(
      "elements/media_router_container/media_router_container.js",
      IDR_MEDIA_ROUTER_CONTAINER_JS);
  html_source->AddResourcePath(
      "elements/route_details/route_details.css",
      IDR_ROUTE_DETAILS_CSS);
  html_source->AddResourcePath(
      "elements/route_details/route_details.html",
      IDR_ROUTE_DETAILS_HTML);
  html_source->AddResourcePath(
      "elements/route_details/route_details.js",
      IDR_ROUTE_DETAILS_JS);
}

}  // namespace

namespace media_router {

void AddMediaRouterUIResources(content::WebUIDataSource* html_source) {
  AddIcons(html_source);
  AddMainWebResources(html_source);
  AddPolymerElements(html_source);
  html_source->SetDefaultResource(IDR_MEDIA_ROUTER_HTML);
}

}  // namespace media_router
