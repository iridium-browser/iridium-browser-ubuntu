// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WK_WEB_VIEW_SECURITY_UTIL_H_
#define IOS_WEB_WEB_STATE_WK_WEB_VIEW_SECURITY_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"

namespace net {
class SSLInfo;
class X509Certificate;
}

namespace web {

// NSErrorPeerCertificateChainKey from NSError's userInfo dict.
extern NSString* const kNSErrorPeerCertificateChainKey;

// Creates a certificate from an array of SecCertificateRef objects.
// Returns null if |certs| is nil or empty.
scoped_refptr<net::X509Certificate> CreateCertFromChain(NSArray* certs);

// Creates a certificate from a SecTrustRef object.
// Returns null if trust is null or does not have any certs.
scoped_refptr<net::X509Certificate> CreateCertFromTrust(SecTrustRef trust);

// Makes SecTrustEvaluate call to return kSecTrustResultProceed.
// Should be called only if the user expilitely agreed to proceed with |trust|
// or trust represents a valid certificate chain.
void EnsureFutureTrustEvaluationSucceeds(SecTrustRef trust);

// Returns YES if geven error is a SSL error.
BOOL IsWKWebViewSSLError(NSError* error);

// Fills SSLInfo object with information extracted from |error|. Callers are
// responsible to ensure that given |error| is an SSL error by calling
// |web::IsSSLError| function.
void GetSSLInfoFromWKWebViewSSLError(NSError* error, net::SSLInfo* ssl_info);

}  // namespace web

#endif // IOS_WEB_WEB_STATE_WK_WEB_VIEW_SECURITY_UTIL_H_
