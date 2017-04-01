// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_

#include <stddef.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/document_load_statistics.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class FirstPartyOrigin;
class MemoryMappedRuleset;

// Performs filtering of subresource loads in the scope of a given document.
class DocumentSubresourceFilter
    : public blink::WebDocumentSubresourceFilter,
      public base::SupportsWeakPtr<DocumentSubresourceFilter> {
 public:
  // Constructs a new filter that will:
  //  -- Operate at the prescribed |activation_state|, which must be either
  //     ActivationState::DRYRUN or ActivationState::ENABLED. In the former
  //     case filtering will be performed but no loads will be disallowed.
  //  -- Hold a reference to and use |ruleset| for its entire lifetime.
  //  -- Expect |ancestor_document_urls| to be the URLs of documents loaded into
  //     nested frames, starting with the current frame and ending with the main
  //     frame. This provides the context for evaluating domain-specific rules.
  //  -- Invoke |first_disallowed_load_callback|, if it is non-null, on the
  //     first disallowed subresource load.
  DocumentSubresourceFilter(
      ActivationState activation_state,
      bool measure_performance,
      const scoped_refptr<const MemoryMappedRuleset>& ruleset,
      const std::vector<GURL>& ancestor_document_urls,
      const base::Closure& first_disallowed_load_callback);
  ~DocumentSubresourceFilter() override;

  const DocumentLoadStatistics& statistics() const { return statistics_; }

  // blink::WebDocumentSubresourceFilter:
  bool allowLoad(const blink::WebURL& resourceUrl,
                 blink::WebURLRequest::RequestContext) override;

 private:
  const ActivationState activation_state_;
  const bool measure_performance_;

  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  IndexedRulesetMatcher ruleset_matcher_;

  // Note: Equals nullptr iff |filtering_disabled_for_document_|.
  std::unique_ptr<FirstPartyOrigin> document_origin_;

  base::Closure first_disallowed_load_callback_;

  // Even when subresource filtering is activated at the page level by the
  // |activation_state| passed into the constructor, the current document or
  // ancestors thereof may still match special filtering rules that specifically
  // disable the application of other types of rules on these documents. See
  // proto::ActivationType for details.
  //
  // Indicates whether the document is subject to a whitelist rule with DOCUMENT
  // activation type.
  bool filtering_disabled_for_document_ = false;

  // Indicates whether the document is subject to a whitelist rule with
  // GENERICBLOCK activation type. Undefined if
  // |filtering_disabled_for_document_|.
  bool generic_blocking_rules_disabled_ = false;

  DocumentLoadStatistics statistics_;

  DISALLOW_COPY_AND_ASSIGN(DocumentSubresourceFilter);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_RENDERER_DOCUMENT_SUBRESOURCE_FILTER_H_
