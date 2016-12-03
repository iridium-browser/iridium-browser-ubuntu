// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"

using net::registry_controlled_domains::GetDomainAndRegistry;
using net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES;
using Relation = ContentSettingsPattern::Relation;

namespace {

// Whether this is a registrable domain.
bool IsRegistrableDomain(const std::string& domain) {
  return GetDomainAndRegistry(domain, INCLUDE_PRIVATE_REGISTRIES) == domain;
}

// Whether this is a subdomain of a registrable domain.
bool IsSubdomainOfARegistrableDomain(const std::string& domain) {
  std::string registrable_domain =
      GetDomainAndRegistry(domain, INCLUDE_PRIVATE_REGISTRIES);
  return registrable_domain != domain && registrable_domain != "";
}

// Note that for every domain, exactly one of the following holds:
// 1. IsRegistrableDomain(domain)                  - e.g. google.com
// 2. IsSubdomainOfARegistrableDomain(domain)      - e.g. www.google.com
// 3. GetDomainAndRegistry(domain, _) == ""        - e.g. localhost, 127.0.0.1

}  // namespace

RegistrableDomainFilterBuilder::RegistrableDomainFilterBuilder(Mode mode)
    : BrowsingDataFilterBuilder(mode) {
}

RegistrableDomainFilterBuilder::~RegistrableDomainFilterBuilder() {}

void RegistrableDomainFilterBuilder::AddRegisterableDomain(
    const std::string& domain) {
  // We check that the domain we're given is actually a eTLD+1, an IP address,
  // or an internal hostname.
  DCHECK(!IsSubdomainOfARegistrableDomain(domain));
  domain_list_.insert(domain);
}

base::Callback<bool(const GURL&)>
RegistrableDomainFilterBuilder::BuildGeneralFilter() const {
  std::set<std::string>* domains = new std::set<std::string>(domain_list_);
  return base::Bind(&RegistrableDomainFilterBuilder::MatchesURL,
                    base::Owned(domains), mode());
}

base::Callback<bool(const ContentSettingsPattern& pattern)>
RegistrableDomainFilterBuilder
    ::BuildWebsiteSettingsPatternMatchesFilter() const {
  std::vector<ContentSettingsPattern>* patterns_from_domains =
      new std::vector<ContentSettingsPattern>();
  patterns_from_domains->reserve(domain_list_.size());

  for (const std::string& domain : domain_list_) {
    std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder(
        ContentSettingsPattern::CreateBuilder(/* use_legacy_validate */ false));
    builder->WithSchemeWildcard()
        ->WithPortWildcard()
        ->WithPathWildcard()
        ->WithHost(domain);
    if (IsRegistrableDomain(domain))
      builder->WithDomainWildcard();

    patterns_from_domains->push_back(builder->Build());
  }

  for (const ContentSettingsPattern& domain : *patterns_from_domains) {
    DCHECK(domain.IsValid());
  }

  return base::Bind(
      &RegistrableDomainFilterBuilder::MatchesWebsiteSettingsPattern,
      base::Owned(patterns_from_domains), mode());
}

base::Callback<bool(const net::CanonicalCookie& cookie)>
RegistrableDomainFilterBuilder::BuildCookieFilter() const {
  std::set<std::string>* domains_and_ips =
      new std::set<std::string>(domain_list_);
  return base::Bind(
      &RegistrableDomainFilterBuilder
          ::MatchesCookieForRegisterableDomainsAndIPs,
      base::Owned(domains_and_ips), mode());
}

base::Callback<bool(const std::string& cookie)>
RegistrableDomainFilterBuilder::BuildChannelIDFilter() const {
  std::set<std::string>* domains_and_ips =
      new std::set<std::string>(domain_list_);
  return base::Bind(
      &RegistrableDomainFilterBuilder
          ::MatchesChannelIDForRegisterableDomainsAndIPs,
      base::Owned(domains_and_ips), mode());
}

base::Callback<bool(const std::string& site)>
RegistrableDomainFilterBuilder::BuildPluginFilter() const {
  std::set<std::string>* domains_and_ips =
      new std::set<std::string>(domain_list_);
  return base::Bind(
      &RegistrableDomainFilterBuilder
          ::MatchesPluginSiteForRegisterableDomainsAndIPs,
      base::Owned(domains_and_ips), mode());
}

bool RegistrableDomainFilterBuilder::operator==(
    const RegistrableDomainFilterBuilder& other) const {
  return domain_list_ == other.domain_list_ && mode() == other.mode();
}

bool RegistrableDomainFilterBuilder::IsEmpty() const {
  return domain_list_.empty();
}

// static
bool RegistrableDomainFilterBuilder::MatchesURL(
    std::set<std::string>* registerable_domains,
    Mode mode,
    const GURL& url) {
  std::string url_registerable_domain =
      GetDomainAndRegistry(url, INCLUDE_PRIVATE_REGISTRIES);
  return (registerable_domains->find(
              url_registerable_domain != "" ? url_registerable_domain
                                            : url.host()) !=
          registerable_domains->end()) ==
         (mode == WHITELIST);
}

// static
bool RegistrableDomainFilterBuilder::MatchesWebsiteSettingsPattern(
    std::vector<ContentSettingsPattern>* domain_patterns,
    Mode mode,
    const ContentSettingsPattern& pattern) {
  for (const ContentSettingsPattern& domain : *domain_patterns) {
    DCHECK(domain.IsValid());
    Relation relation = pattern.Compare(domain);
    if (relation == Relation::IDENTITY || relation == Relation::PREDECESSOR)
      return mode == WHITELIST;
  }
  return mode != WHITELIST;
}

// static
bool RegistrableDomainFilterBuilder::MatchesCookieForRegisterableDomainsAndIPs(
    std::set<std::string>* domains_and_ips,
    Mode mode,
    const net::CanonicalCookie& cookie) {
  if (domains_and_ips->empty())
    return mode == BLACKLIST;
  std::string cookie_domain = cookie.Domain();
  if (cookie.IsDomainCookie())
    cookie_domain = cookie_domain.substr(1);
  std::string parsed_cookie_domain =
      GetDomainAndRegistry(cookie_domain, INCLUDE_PRIVATE_REGISTRIES);
  // This means we're an IP address or an internal hostname.
  if (parsed_cookie_domain.empty())
    parsed_cookie_domain = cookie_domain;
  return (mode == WHITELIST) == (domains_and_ips->find(parsed_cookie_domain) !=
                                 domains_and_ips->end());
}

// static
bool
RegistrableDomainFilterBuilder::MatchesChannelIDForRegisterableDomainsAndIPs(
    std::set<std::string>* domains_and_ips,
    Mode mode,
    const std::string& channel_id_server_id) {
  return ((mode == WHITELIST) == (domains_and_ips->find(channel_id_server_id) !=
                                  domains_and_ips->end()));
}

// static
bool
RegistrableDomainFilterBuilder::MatchesPluginSiteForRegisterableDomainsAndIPs(
    std::set<std::string>* domains_and_ips,
    Mode mode,
    const std::string& site) {
  // If |site| is a third- or lower-level domain, find the corresponding eTLD+1.
  std::string domain_or_ip =
      GetDomainAndRegistry(site, INCLUDE_PRIVATE_REGISTRIES);
  if (domain_or_ip.empty())
    domain_or_ip = site;

  return ((mode == WHITELIST) == (domains_and_ips->find(domain_or_ip) !=
                                  domains_and_ips->end()));
}
