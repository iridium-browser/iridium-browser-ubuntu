// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_classification.h"

#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using content::WebContents;

class SSLErrorClassificationTest : public ChromeRenderViewHostTestHarness {
 public:
  SSLErrorClassificationTest() {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
  }
};

TEST_F(SSLErrorClassificationTest, TestNameMismatch) {
  scoped_refptr<net::X509Certificate> google_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(google_der), sizeof(google_der)));
  ASSERT_NE(static_cast<net::X509Certificate*>(NULL), google_cert.get());
  base::Time time = base::Time::NowFromSystemTime();
  std::vector<std::string> dns_names_google;
  dns_names_google.push_back("www");
  dns_names_google.push_back("google");
  dns_names_google.push_back("com");
  std::vector<std::vector<std::string>> dns_name_tokens_google;
  dns_name_tokens_google.push_back(dns_names_google);
  int cert_error = net::ERR_CERT_COMMON_NAME_INVALID;
  WebContents* contents = web_contents();
  {
    GURL origin("https://google.com");
    std::string host_name = origin.host();
    std::vector<std::string> host_name_tokens;
    base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
    SSLErrorClassification ssl_error(contents,
                                     time,
                                     origin,
                                     cert_error,
                                     *google_cert);
    EXPECT_TRUE(ssl_error.IsWWWSubDomainMatch());
    EXPECT_FALSE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                             dns_name_tokens_google));
    EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_google,
                                             host_name_tokens));
    EXPECT_FALSE(ssl_error.IsSubDomainOutsideWildcard(host_name_tokens));
    EXPECT_FALSE(ssl_error.IsCertLikelyFromMultiTenantHosting());
  }

  {
    GURL origin("https://foo.blah.google.com");
    std::string host_name = origin.host();
    std::vector<std::string> host_name_tokens;
    base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
    SSLErrorClassification ssl_error(contents,
                                     time,
                                     origin,
                                     cert_error,
                                     *google_cert);
    EXPECT_FALSE(ssl_error.IsWWWSubDomainMatch());
    EXPECT_FALSE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                             dns_name_tokens_google));
    EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_google,
                                             host_name_tokens));
  }

  {
    GURL origin("https://foo.www.google.com");
    std::string host_name = origin.host();
    std::vector<std::string> host_name_tokens;
    base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
    SSLErrorClassification ssl_error(contents,
                                     time,
                                     origin,
                                     cert_error,
                                     *google_cert);
    EXPECT_FALSE(ssl_error.IsWWWSubDomainMatch());
    EXPECT_TRUE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                            dns_name_tokens_google));
    EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_google,
                                             host_name_tokens));
  }

  {
     GURL origin("https://www.google.com.foo");
     std::string host_name = origin.host();
     std::vector<std::string> host_name_tokens;
     base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
     SSLErrorClassification ssl_error(contents,
                                      time,
                                      origin,
                                      cert_error,
                                      *google_cert);
     EXPECT_FALSE(ssl_error.IsWWWSubDomainMatch());
     EXPECT_FALSE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                              dns_name_tokens_google));
     EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_google,
                                              host_name_tokens));
  }

  {
    GURL origin("https://www.foogoogle.com.");
    std::string host_name = origin.host();
    std::vector<std::string> host_name_tokens;
    base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
    SSLErrorClassification ssl_error(contents,
                                     time,
                                     origin,
                                     cert_error,
                                     *google_cert);
    EXPECT_FALSE(ssl_error.IsWWWSubDomainMatch());
    EXPECT_FALSE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                             dns_name_tokens_google));
    EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_google,
                                             host_name_tokens));
  }

  scoped_refptr<net::X509Certificate> webkit_cert(
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(webkit_der), sizeof(webkit_der)));
  ASSERT_NE(static_cast<net::X509Certificate*>(NULL), webkit_cert.get());
  std::vector<std::string> dns_names_webkit;
  dns_names_webkit.push_back("webkit");
  dns_names_webkit.push_back("org");
  std::vector<std::vector<std::string>> dns_name_tokens_webkit;
  dns_name_tokens_webkit.push_back(dns_names_webkit);
  {
    GURL origin("https://a.b.webkit.org");
    std::string host_name = origin.host();
    std::vector<std::string> host_name_tokens;
    base::SplitStringDontTrim(host_name, '.', &host_name_tokens);
    SSLErrorClassification ssl_error(contents,
                                     time,
                                     origin,
                                     cert_error,
                                     *webkit_cert);
    EXPECT_FALSE(ssl_error.IsWWWSubDomainMatch());
    EXPECT_FALSE(ssl_error.NameUnderAnyNames(host_name_tokens,
                                             dns_name_tokens_webkit));
    EXPECT_FALSE(ssl_error.AnyNamesUnderName(dns_name_tokens_webkit,
                                             host_name_tokens));
    EXPECT_TRUE(ssl_error.IsSubDomainOutsideWildcard(host_name_tokens));
    EXPECT_FALSE(ssl_error.IsCertLikelyFromMultiTenantHosting());
  }
}

TEST_F(SSLErrorClassificationTest, TestHostNameHasKnownTLD) {
  EXPECT_TRUE(SSLErrorClassification::IsHostNameKnownTLD("www.google.com"));
  EXPECT_TRUE(SSLErrorClassification::IsHostNameKnownTLD("b.appspot.com"));
  EXPECT_FALSE(SSLErrorClassification::IsHostNameKnownTLD("a.private"));
}

TEST_F(SSLErrorClassificationTest, TestPrivateURL) {
  EXPECT_FALSE(SSLErrorClassification::IsHostnameNonUniqueOrDotless(
      "www.foogoogle.com."));
  EXPECT_TRUE(SSLErrorClassification::IsHostnameNonUniqueOrDotless("go"));
  EXPECT_TRUE(
      SSLErrorClassification::IsHostnameNonUniqueOrDotless("172.17.108.108"));
  EXPECT_TRUE(SSLErrorClassification::IsHostnameNonUniqueOrDotless("foo.blah"));
}
