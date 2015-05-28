// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_CONTEXTUAL_SEARCH_PROMO_SOURCE_ANDROID_H_
#define CHROME_BROWSER_SEARCH_CONTEXTUAL_SEARCH_PROMO_SOURCE_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

// Serves HTML for displaying the contextual search first-run promo.
class ContextualSearchPromoSourceAndroid : public content::URLDataSource {
 public:
  ContextualSearchPromoSourceAndroid();
  ~ContextualSearchPromoSourceAndroid() override;

 protected:
  // Overridden from content::URLDataSource:
  void StartDataRequest(
      const std::string& path_and_query,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetSource() const override;
  std::string GetMimeType(const std::string& path_and_query) const override;
  bool ShouldDenyXFrameOptions() const override;
  bool ShouldAddContentSecurityPolicy() const override;

  // Sends unmodified resource bytes.
  void SendResource(
      int resource_id,
      const content::URLDataSource::GotDataCallback& callback);

  // Sends the config JS resource.
  void SendConfigResource(
      const content::URLDataSource::GotDataCallback& callback);

  // Sends HTML with localized strings.
  void SendHtmlWithStrings(
    const content::URLDataSource::GotDataCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(ContextualSearchPromoSourceAndroid);
};

#endif  // CHROME_BROWSER_SEARCH_CONTEXTUAL_SEARCH_PROMO_SOURCE_ANDROID_H_
