// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "components/google/core/browser/google_util.h"
#include "components/variations/variations_http_header_provider.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace variations {

namespace {

const char* kSuffixesToSetHeadersFor[] = {
    ".android.com",
    ".doubleclick.com",
    ".doubleclick.net",
    ".ggpht.com",
    ".googleadservices.com",
    ".googleapis.com",
    ".googlesyndication.com",
    ".googleusercontent.com",
    ".googlevideo.com",
    ".gstatic.com",
    ".ytimg.com",
};

// Exact hostnames in lowercase to set headers for.
const char* kHostsToSetHeadersFor[] = {
    "googleweblight.com",
};

const char kChromeUMAEnabled[] = "X-Chrome-UMA-Enabled";
const char kClientData[] = "X-Client-Data";

}  // namespace

void AppendVariationHeaders(const GURL& url,
                            bool incognito,
                            bool uma_enabled,
                            net::HttpRequestHeaders* headers) {
  // Note the criteria for attaching client experiment headers:
  // 1. We only transmit to Google owned domains which can evaluate experiments.
  //    1a. These include hosts which have a standard postfix such as:
  //         *.doubleclick.net or *.googlesyndication.com or
  //         exactly www.googleadservices.com or
  //         international TLD domains *.google.<TLD> or *.youtube.<TLD>.
  // 2. Only transmit for non-Incognito profiles.
  // 3. For the X-Chrome-UMA-Enabled bit, only set it if UMA is in fact enabled
  //    for this install of Chrome.
  // 4. For the X-Client-Data header, only include non-empty variation IDs.
  if (incognito || !internal::ShouldAppendVariationHeaders(url))
    return;

  if (uma_enabled)
    headers->SetHeaderIfMissing(kChromeUMAEnabled, "1");

  const std::string variation_ids_header =
      VariationsHttpHeaderProvider::GetInstance()->GetClientDataHeader();
  if (!variation_ids_header.empty()) {
    // Note that prior to M33 this header was named X-Chrome-Variations.
    headers->SetHeaderIfMissing(kClientData, variation_ids_header);
  }
}

std::set<std::string> GetVariationHeaderNames() {
  std::set<std::string> headers;
  headers.insert(kChromeUMAEnabled);
  headers.insert(kClientData);
  return headers;
}

namespace internal {

// static
bool ShouldAppendVariationHeaders(const GURL& url) {
  if (google_util::IsGoogleDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                     google_util::ALLOW_NON_STANDARD_PORTS)) {
    return true;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  // Some domains don't have international TLD extensions, so testing for them
  // is very straight forward.
  const std::string host = url.host();
  for (size_t i = 0; i < arraysize(kSuffixesToSetHeadersFor); ++i) {
    if (base::EndsWith(host, kSuffixesToSetHeadersFor[i],
                       base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }
  for (size_t i = 0; i < arraysize(kHostsToSetHeadersFor); ++i) {
    if (base::LowerCaseEqualsASCII(host, kHostsToSetHeadersFor[i]))
      return true;
  }

  return google_util::IsYoutubeDomainUrl(url, google_util::ALLOW_SUBDOMAIN,
                                         google_util::ALLOW_NON_STANDARD_PORTS);
}

}  // namespace internal

}  // namespace variations
