// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/document_subresource_filter.h"

#include <climits>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace subresource_filter {

namespace {

proto::ElementType ToElementType(
    blink::WebURLRequest::RequestContext request_context) {
  switch (request_context) {
    case blink::WebURLRequest::RequestContextAudio:
    case blink::WebURLRequest::RequestContextVideo:
    case blink::WebURLRequest::RequestContextTrack:
      return proto::ELEMENT_TYPE_MEDIA;
    case blink::WebURLRequest::RequestContextBeacon:
    case blink::WebURLRequest::RequestContextPing:
      return proto::ELEMENT_TYPE_PING;
    case blink::WebURLRequest::RequestContextEmbed:
    case blink::WebURLRequest::RequestContextObject:
    case blink::WebURLRequest::RequestContextPlugin:
      return proto::ELEMENT_TYPE_OBJECT;
    case blink::WebURLRequest::RequestContextEventSource:
    case blink::WebURLRequest::RequestContextFetch:
    case blink::WebURLRequest::RequestContextXMLHttpRequest:
      return proto::ELEMENT_TYPE_XMLHTTPREQUEST;
    case blink::WebURLRequest::RequestContextFavicon:
    case blink::WebURLRequest::RequestContextImage:
    case blink::WebURLRequest::RequestContextImageSet:
      return proto::ELEMENT_TYPE_IMAGE;
    case blink::WebURLRequest::RequestContextFont:
      return proto::ELEMENT_TYPE_FONT;
    case blink::WebURLRequest::RequestContextFrame:
    case blink::WebURLRequest::RequestContextForm:
    case blink::WebURLRequest::RequestContextHyperlink:
    case blink::WebURLRequest::RequestContextIframe:
    case blink::WebURLRequest::RequestContextInternal:
    case blink::WebURLRequest::RequestContextLocation:
      return proto::ELEMENT_TYPE_SUBDOCUMENT;
    case blink::WebURLRequest::RequestContextScript:
    case blink::WebURLRequest::RequestContextServiceWorker:
    case blink::WebURLRequest::RequestContextSharedWorker:
      return proto::ELEMENT_TYPE_SCRIPT;
    case blink::WebURLRequest::RequestContextStyle:
    case blink::WebURLRequest::RequestContextXSLT:
      return proto::ELEMENT_TYPE_STYLESHEET;

    case blink::WebURLRequest::RequestContextPrefetch:
    case blink::WebURLRequest::RequestContextSubresource:
      return proto::ELEMENT_TYPE_OTHER;

    case blink::WebURLRequest::RequestContextCSPReport:
    case blink::WebURLRequest::RequestContextDownload:
    case blink::WebURLRequest::RequestContextImport:
    case blink::WebURLRequest::RequestContextManifest:
    case blink::WebURLRequest::RequestContextUnspecified:
    default:
      return proto::ELEMENT_TYPE_UNSPECIFIED;
  }
}

}  // namespace

DocumentSubresourceFilter::DocumentSubresourceFilter(
    ActivationState activation_state,
    bool measure_performance,
    const scoped_refptr<const MemoryMappedRuleset>& ruleset,
    const std::vector<GURL>& ancestor_document_urls,
    const base::Closure& first_disallowed_load_callback)
    : activation_state_(activation_state),
      measure_performance_(measure_performance),
      ruleset_(ruleset),
      ruleset_matcher_(ruleset_->data(), ruleset_->length()),
      first_disallowed_load_callback_(first_disallowed_load_callback) {
  TRACE_EVENT1("loader", "DocumentSubresourceFilter::DocumentSubresourceFilter",
               "document_url", ancestor_document_urls.empty()
                                   ? std::string()
                                   : ancestor_document_urls[0].spec());

  SCOPED_UMA_HISTOGRAM_MICRO_TIMER(
      "SubresourceFilter.DocumentLoad.Activation.WallDuration");
  SCOPED_UMA_HISTOGRAM_MICRO_THREAD_TIMER(
      "SubresourceFilter.DocumentLoad.Activation.CPUDuration");

  DCHECK_NE(activation_state_, ActivationState::DISABLED);
  DCHECK(ruleset);

  url::Origin parent_document_origin;
  for (auto iter = ancestor_document_urls.rbegin(),
            rend = ancestor_document_urls.rend();
       iter != rend; ++iter) {
    const GURL& document_url(*iter);
    if (ruleset_matcher_.ShouldDisableFilteringForDocument(
            document_url, parent_document_origin,
            proto::ACTIVATION_TYPE_DOCUMENT)) {
      filtering_disabled_for_document_ = true;
      return;
    }
    // TODO(pkalinnikov): Match several activation types in a batch.
    generic_blocking_rules_disabled_ =
        generic_blocking_rules_disabled_ ||
        ruleset_matcher_.ShouldDisableFilteringForDocument(
            document_url, parent_document_origin,
            proto::ACTIVATION_TYPE_GENERICBLOCK);

    // TODO(pkalinnikov): Think about avoiding this conversion.
    parent_document_origin = url::Origin(document_url);
  }

  url::Origin document_origin = std::move(parent_document_origin);
  document_origin_.reset(new FirstPartyOrigin(std::move(document_origin)));
}

DocumentSubresourceFilter::~DocumentSubresourceFilter() = default;

bool DocumentSubresourceFilter::allowLoad(
    const blink::WebURL& resourceUrl,
    blink::WebURLRequest::RequestContext request_context) {
  TRACE_EVENT1("loader", "DocumentSubresourceFilter::allowLoad", "url",
               resourceUrl.string().utf8());

  auto wall_duration_timer = ScopedTimers::StartIf(
      measure_performance_ && ScopedThreadTimers::IsSupported(),
      [this](base::TimeDelta delta) {
        statistics_.evaluation_total_wall_duration += delta;
        UMA_HISTOGRAM_MICRO_TIMES(
            "SubresourceFilter.SubresourceLoad.Evaluation.WallDuration", delta);
      });
  auto cpu_duration_timer = ScopedThreadTimers::StartIf(
      measure_performance_, [this](base::TimeDelta delta) {
        statistics_.evaluation_total_cpu_duration += delta;
        UMA_HISTOGRAM_MICRO_TIMES(
            "SubresourceFilter.SubresourceLoad.Evaluation.CPUDuration", delta);
      });

  ++statistics_.num_loads_total;

  if (filtering_disabled_for_document_)
    return true;

  if (resourceUrl.protocolIs(url::kDataScheme))
    return true;

  ++statistics_.num_loads_evaluated;
  DCHECK(document_origin_);
  if (ruleset_matcher_.ShouldDisallowResourceLoad(
          GURL(resourceUrl), *document_origin_, ToElementType(request_context),
          generic_blocking_rules_disabled_)) {
    ++statistics_.num_loads_matching_rules;
    if (activation_state_ == ActivationState::ENABLED) {
      if (!first_disallowed_load_callback_.is_null()) {
        DCHECK_EQ(statistics_.num_loads_disallowed, 0);
        first_disallowed_load_callback_.Run();
        first_disallowed_load_callback_.Reset();
      }
      ++statistics_.num_loads_disallowed;
      return false;
    }
  }
  return true;
}

}  // namespace subresource_filter
