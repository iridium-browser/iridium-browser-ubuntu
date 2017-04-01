// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_TEST_RUNNER_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

namespace blink {
class WebURL;
}  // namespace blink

namespace test_runner {

class MockWebDocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter {
 public:
  explicit MockWebDocumentSubresourceFilter(
      const std::vector<std::string>& disallowed_path_suffixes);
  ~MockWebDocumentSubresourceFilter() override;

  // blink::WebDocumentSubresourceFilter:
  bool allowLoad(const blink::WebURL& resource_url,
                 blink::WebURLRequest::RequestContext) override;

 private:
  std::vector<std::string> disallowed_path_suffixes_;

  DISALLOW_COPY_AND_ASSIGN(MockWebDocumentSubresourceFilter);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_MOCK_WEB_DOCUMENT_SUBRESOURCE_FILTER_H_
