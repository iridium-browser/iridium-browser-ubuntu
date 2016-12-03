// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_POLICY_H_
#define CONTENT_BROWSER_SSL_SSL_POLICY_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/security_style.h"
#include "net/cert/cert_status_flags.h"

class GURL;

namespace content {
class NavigationEntryImpl;
class SSLErrorHandler;
class SSLPolicyBackend;
class WebContents;

// SSLPolicy
//
// This class is responsible for making the security decisions that concern the
// SSL trust indicators.  It relies on the SSLPolicyBackend to actually enact
// the decisions it reaches.
//
class CONTENT_EXPORT SSLPolicy {
 public:
  explicit SSLPolicy(SSLPolicyBackend* backend);

  // An error occurred with the certificate in an SSL connection.
  void OnCertError(std::unique_ptr<SSLErrorHandler> handler);

  void DidRunInsecureContent(NavigationEntryImpl* entry,
                             const GURL& security_origin);

  void DidRunContentWithCertErrors(NavigationEntryImpl* entry,
                                   const GURL& security_origin);

  // We have started a resource request for |url| with the given |cert_id| and
  // |cert_status|.
  void OnRequestStarted(const GURL& url,
                        int cert_id,
                        net::CertStatus cert_status);

  // Update the SSL information in |entry| to match the current state.
  // |web_contents| is the WebContents associated with this entry.
  void UpdateEntry(NavigationEntryImpl* entry, WebContents* web_contents);

  SSLPolicyBackend* backend() const { return backend_; }

  // Returns a security style describing an individual resource. Does
  // not take into account any of the page- or host-level state such as
  // mixed content or whether the host has run insecure content.
  static SecurityStyle GetSecurityStyleForResource(const GURL& url,
                                                   int cert_id,
                                                   net::CertStatus cert_status);

 private:
  enum OnCertErrorInternalOptionsMask {
    OVERRIDABLE = 1 << 0,
    STRICT_ENFORCEMENT = 1 << 1,
    EXPIRED_PREVIOUS_DECISION = 1 << 2
  };

  // Helper method for derived classes handling certificate errors.
  //
  // Options should be a bitmask combination of OnCertErrorInternalOptionsMask.
  // OVERRIDABLE indicates whether or not the user could (assuming perfect
  // knowledge) successfully override the error and still get the security
  // guarantees of TLS. STRICT_ENFORCEMENT indicates whether or not the site the
  // user is trying to connect to has requested strict enforcement of
  // certificate validation (e.g. with HTTP Strict-Transport-Security).
  // EXPIRED_PREVIOUS_DECISION indicates whether a user decision had been
  // previously made but the decision has expired.
  void OnCertErrorInternal(std::unique_ptr<SSLErrorHandler> handler,
                           int options_mask);

  // If the security style of |entry| has not been initialized, then initialize
  // it with the default style for its URL.
  void InitializeEntryIfNeeded(NavigationEntryImpl* entry);

  // The backend we use to enact our decisions.
  SSLPolicyBackend* backend_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SSL_SSL_POLICY_H_
