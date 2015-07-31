// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "components/dom_distiller/core/external_feedback_reporter.h"
#include "content/public/browser/url_data_source.h"

namespace dom_distiller {

class DomDistillerServiceInterface;
class DomDistillerViewerSourceTest;
class ViewerHandle;
class ViewRequestDelegate;

// Serves HTML and resources for viewing distilled articles.
class DomDistillerViewerSource : public content::URLDataSource {
 public:
  DomDistillerViewerSource(
      DomDistillerServiceInterface* dom_distiller_service,
      const std::string& scheme,
      scoped_ptr<ExternalFeedbackReporter> external_reporter);
  ~DomDistillerViewerSource() override;

  class RequestViewerHandle;

  // Overridden from content::URLDataSource:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  bool ShouldServiceRequest(const net::URLRequest* request) const override;
  void WillServiceRequest(const net::URLRequest* request,
                          std::string* path) const override;
  std::string GetContentSecurityPolicyObjectSrc() const override;
  std::string GetContentSecurityPolicyFrameSrc() const override;

 private:
  friend class DomDistillerViewerSourceTest;

  // The scheme this URLDataSource is hosted under.
  std::string scheme_;

  // The service which contains all the functionality needed to interact with
  // the list of articles.
  DomDistillerServiceInterface* dom_distiller_service_;

  // A means for starting/opening an external service for feedback reporting.
  scoped_ptr<ExternalFeedbackReporter> external_feedback_reporter_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerViewerSource);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_
