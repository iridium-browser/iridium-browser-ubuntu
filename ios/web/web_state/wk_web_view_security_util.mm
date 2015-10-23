// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/wk_web_view_security_util.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"

namespace web {

// This key was determined by inspecting userInfo dict of an SSL error.
NSString* const kNSErrorPeerCertificateChainKey =
    @"NSErrorPeerCertificateChainKey";

}

namespace {

// Creates certificate from subject string.
net::X509Certificate* CreateCertFromSubject(NSString* subject) {
  std::string issuer = "";
  base::Time start_date;
  base::Time expiration_date;
  return new net::X509Certificate(base::SysNSStringToUTF8(subject),
                                  issuer,
                                  start_date,
                                  expiration_date);
}

// Creates certificate using information extracted from NSError.
scoped_refptr<net::X509Certificate> CreateCertFromSSLError(NSError* error) {
  scoped_refptr<net::X509Certificate> cert = web::CreateCertFromChain(
      error.userInfo[web::kNSErrorPeerCertificateChainKey]);
  if (cert)
    return cert;
  return CreateCertFromSubject(
      error.userInfo[NSURLErrorFailingURLStringErrorKey]);
}

// Maps NSError code to net::CertStatus.
net::CertStatus GetCertStatusFromNSErrorCode(NSInteger code) {
  switch (code) {
    // Regardless of real certificate problem the system always returns
    // NSURLErrorServerCertificateUntrusted. The mapping is done in case this
    // bug is fixed (rdar://18517043).
    case NSURLErrorServerCertificateUntrusted:
    case NSURLErrorSecureConnectionFailed:
    case NSURLErrorServerCertificateHasUnknownRoot:
    case NSURLErrorClientCertificateRejected:
    case NSURLErrorClientCertificateRequired:
      return net::CERT_STATUS_INVALID;
    case NSURLErrorServerCertificateHasBadDate:
    case NSURLErrorServerCertificateNotYetValid:
      return net::CERT_STATUS_DATE_INVALID;
  }
  NOTREACHED();
  return 0;
}

}  // namespace


namespace web {

scoped_refptr<net::X509Certificate> CreateCertFromChain(NSArray* certs) {
  if (certs.count == 0)
    return nullptr;
  net::X509Certificate::OSCertHandles intermediates;
  for (NSUInteger i = 1; i < certs.count; i++) {
    intermediates.push_back(reinterpret_cast<SecCertificateRef>(certs[i]));
  }
  return net::X509Certificate::CreateFromHandle(
      reinterpret_cast<SecCertificateRef>(certs[0]), intermediates);
}

scoped_refptr<net::X509Certificate> CreateCertFromTrust(SecTrustRef trust) {
  if (!trust)
    return nullptr;

  CFIndex cert_count = SecTrustGetCertificateCount(trust);
  if (cert_count == 0) {
    // At the moment there is no API which allows trust creation w/o certs.
    return nullptr;
  }

  net::X509Certificate::OSCertHandles intermediates;
  for (CFIndex i = 1; i < cert_count; i++) {
    intermediates.push_back(SecTrustGetCertificateAtIndex(trust, i));
  }
  return net::X509Certificate::CreateFromHandle(
      SecTrustGetCertificateAtIndex(trust, 0), intermediates);
}

void EnsureFutureTrustEvaluationSucceeds(SecTrustRef trust) {
  base::ScopedCFTypeRef<CFDataRef> exceptions(SecTrustCopyExceptions(trust));
  SecTrustSetExceptions(trust, exceptions);
}

BOOL IsWKWebViewSSLError(NSError* error) {
  // SSL errors range is (-2000..-1200], represented by kCFURLError constants:
  // (kCFURLErrorCannotLoadFromNetwork..kCFURLErrorSecureConnectionFailed].
  // It's reasonable to expect that all SSL errors will have the error code
  // less or equal to NSURLErrorSecureConnectionFailed but greater than
  // NSURLErrorCannotLoadFromNetwork.
  return [error.domain isEqualToString:NSURLErrorDomain] &&
         (error.code <= NSURLErrorSecureConnectionFailed &&
          NSURLErrorCannotLoadFromNetwork < error.code);
}

void GetSSLInfoFromWKWebViewSSLError(NSError* error, net::SSLInfo* ssl_info) {
  DCHECK(IsWKWebViewSSLError(error));
  ssl_info->cert_status = GetCertStatusFromNSErrorCode(error.code);
  ssl_info->cert = CreateCertFromSSLError(error);
}

}  // namespace web
