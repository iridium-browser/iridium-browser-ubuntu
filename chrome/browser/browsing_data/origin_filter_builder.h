// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
#define CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_

#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "url/gurl.h"
#include "url/origin.h"

// A class that constructs URL deletion filters (represented as GURL->bool
// predicates) that match certain origins.
//
// IMPORTANT NOTE: While this class does define cookie, channel ID, and plugin
// filtering methods, as required by the BrowsingDataFilterBuilder interface,
// it is not suitable for deletion of those data types, as they are scoped
// to eTLD+1. Instead, use RegistrableDomainFilterBuilder and see its
// documenation for more details.
class OriginFilterBuilder : public BrowsingDataFilterBuilder {
 public:
  // Constructs a filter with the given |mode| - whitelist or blacklist.
  explicit OriginFilterBuilder(Mode mode);

  ~OriginFilterBuilder() override;

  // Adds the |origin| to the (white- or black-) list.
  void AddOrigin(const url::Origin& origin);

  // Builds a filter that matches URLs whose origins are in the whitelist,
  // or aren't in the blacklist.
  base::Callback<bool(const GURL&)> BuildGeneralFilter() const override;

  // Cookie filter is not implemented in this subclass. Please use
  // a BrowsingDataFilterBuilder with different scoping,
  // such as RegistrableDomainFilterBuilder.
  base::Callback<bool(const net::CanonicalCookie& cookie)>
      BuildCookieFilter() const override;

  // Channel ID filter is not implemented in this subclasss. Please use
  // a BrowsingDataFilterBuilder with different scoping,
  // such as RegistrableDomainFilterBuilder.
  base::Callback<bool(const std::string& channel_id_server_id)>
      BuildChannelIDFilter() const override;

  // Plugin site filter is not implemented in this subclass. Please use
  // a BrowsingDataFilterBuilder with different scoping,
  // such as RegistrableDomainFilterBuilder.
  base::Callback<bool(const std::string& site)>
      BuildPluginFilter() const override;

  bool operator==(const OriginFilterBuilder& other) const;

 protected:
  bool IsEmpty() const override;

 private:
  // True if the origin of |url| is in the whitelist, or isn't in the blacklist.
  // The whitelist or blacklist is represented as |origins| and |mode|.
  static bool MatchesURL(
      std::set<url::Origin>* origins, Mode mode, const GURL& url);

  // The list of origins and whether they should be interpreted as a whitelist
  // or blacklist.
  std::set<url::Origin> origin_list_;

  DISALLOW_COPY_AND_ASSIGN(OriginFilterBuilder);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_ORIGIN_FILTER_BUILDER_H_
