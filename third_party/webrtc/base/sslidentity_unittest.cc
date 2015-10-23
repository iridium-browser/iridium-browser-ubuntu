/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/base/gunit.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslidentity.h"

using rtc::SSLIdentity;

const char kTestCertificate[] = "-----BEGIN CERTIFICATE-----\n"
    "MIIB6TCCAVICAQYwDQYJKoZIhvcNAQEEBQAwWzELMAkGA1UEBhMCQVUxEzARBgNV\n"
    "BAgTClF1ZWVuc2xhbmQxGjAYBgNVBAoTEUNyeXB0U29mdCBQdHkgTHRkMRswGQYD\n"
    "VQQDExJUZXN0IENBICgxMDI0IGJpdCkwHhcNMDAxMDE2MjIzMTAzWhcNMDMwMTE0\n"
    "MjIzMTAzWjBjMQswCQYDVQQGEwJBVTETMBEGA1UECBMKUXVlZW5zbGFuZDEaMBgG\n"
    "A1UEChMRQ3J5cHRTb2Z0IFB0eSBMdGQxIzAhBgNVBAMTGlNlcnZlciB0ZXN0IGNl\n"
    "cnQgKDUxMiBiaXQpMFwwDQYJKoZIhvcNAQEBBQADSwAwSAJBAJ+zw4Qnlf8SMVIP\n"
    "Fe9GEcStgOY2Ww/dgNdhjeD8ckUJNP5VZkVDTGiXav6ooKXfX3j/7tdkuD8Ey2//\n"
    "Kv7+ue0CAwEAATANBgkqhkiG9w0BAQQFAAOBgQCT0grFQeZaqYb5EYfk20XixZV4\n"
    "GmyAbXMftG1Eo7qGiMhYzRwGNWxEYojf5PZkYZXvSqZ/ZXHXa4g59jK/rJNnaVGM\n"
    "k+xIX8mxQvlV0n5O9PIha5BX5teZnkHKgL8aKKLKW1BK7YTngsfSzzaeame5iKfz\n"
    "itAE+OjGF+PFKbwX8Q==\n"
    "-----END CERTIFICATE-----\n";

const unsigned char kTestCertSha1[] = {
    0xA6, 0xC8, 0x59, 0xEA, 0xC3, 0x7E, 0x6D, 0x33,
    0xCF, 0xE2, 0x69, 0x9D, 0x74, 0xE6, 0xF6, 0x8A,
    0x9E, 0x47, 0xA7, 0xCA};
const unsigned char kTestCertSha224[] = {
    0xd4, 0xce, 0xc6, 0xcf, 0x28, 0xcb, 0xe9, 0x77,
    0x38, 0x36, 0xcf, 0xb1, 0x3b, 0x4a, 0xd7, 0xbd,
    0xae, 0x24, 0x21, 0x08, 0xcf, 0x6a, 0x44, 0x0d,
    0x3f, 0x94, 0x2a, 0x5b};
const unsigned char kTestCertSha256[] = {
    0x41, 0x6b, 0xb4, 0x93, 0x47, 0x79, 0x77, 0x24,
    0x77, 0x0b, 0x8b, 0x2e, 0xa6, 0x2b, 0xe0, 0xf9,
    0x0a, 0xed, 0x1f, 0x31, 0xa6, 0xf7, 0x5c, 0xa1,
    0x5a, 0xc4, 0xb0, 0xa2, 0xa4, 0x78, 0xb9, 0x76};
const unsigned char kTestCertSha384[] = {
    0x42, 0x31, 0x9a, 0x79, 0x1d, 0xd6, 0x08, 0xbf,
    0x3b, 0xba, 0x36, 0xd8, 0x37, 0x4a, 0x9a, 0x75,
    0xd3, 0x25, 0x6e, 0x28, 0x92, 0xbe, 0x06, 0xb7,
    0xc5, 0xa0, 0x83, 0xe3, 0x86, 0xb1, 0x03, 0xfc,
    0x64, 0x47, 0xd6, 0xd8, 0xaa, 0xd9, 0x36, 0x60,
    0x04, 0xcc, 0xbe, 0x7d, 0x6a, 0xe8, 0x34, 0x49};
const unsigned char kTestCertSha512[] = {
    0x51, 0x1d, 0xec, 0x02, 0x3d, 0x51, 0x45, 0xd3,
    0xd8, 0x1d, 0xa4, 0x9d, 0x43, 0xc9, 0xee, 0x32,
    0x6f, 0x4f, 0x37, 0xee, 0xab, 0x3f, 0x25, 0xdf,
    0x72, 0xfc, 0x61, 0x1a, 0xd5, 0x92, 0xff, 0x6b,
    0x28, 0x71, 0x58, 0xb3, 0xe1, 0x8a, 0x18, 0xcf,
    0x61, 0x33, 0x0e, 0x14, 0xc3, 0x04, 0xaa, 0x07,
    0xf6, 0xa5, 0xda, 0xdc, 0x42, 0x42, 0x22, 0x35,
    0xce, 0x26, 0x58, 0x4a, 0x33, 0x6d, 0xbc, 0xb6};

class SSLIdentityTest : public testing::Test {
 public:
  SSLIdentityTest() {}

  ~SSLIdentityTest() {
  }

  virtual void SetUp() {
    identity_rsa1_.reset(SSLIdentity::Generate("test1", rtc::KT_RSA));
    identity_rsa2_.reset(SSLIdentity::Generate("test2", rtc::KT_RSA));
    identity_ecdsa1_.reset(SSLIdentity::Generate("test3", rtc::KT_ECDSA));
    identity_ecdsa2_.reset(SSLIdentity::Generate("test4", rtc::KT_ECDSA));

    ASSERT_TRUE(identity_rsa1_);
    ASSERT_TRUE(identity_rsa2_);
    ASSERT_TRUE(identity_ecdsa1_);
    ASSERT_TRUE(identity_ecdsa2_);

    test_cert_.reset(rtc::SSLCertificate::FromPEMString(kTestCertificate));
    ASSERT_TRUE(test_cert_);
  }

  void TestGetSignatureDigestAlgorithm() {
    std::string digest_algorithm;

    ASSERT_TRUE(identity_rsa1_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);

    ASSERT_TRUE(identity_rsa2_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);

    ASSERT_TRUE(identity_ecdsa1_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);

    ASSERT_TRUE(identity_ecdsa2_->certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);

    // The test certificate has an MD5-based signature.
    ASSERT_TRUE(test_cert_->GetSignatureDigestAlgorithm(&digest_algorithm));
    ASSERT_EQ(rtc::DIGEST_MD5, digest_algorithm);
  }

  typedef unsigned char DigestType[rtc::MessageDigest::kMaxSize];

  void TestDigestHelper(DigestType digest,
                        const SSLIdentity* identity,
                        const std::string& algorithm,
                        size_t expected_len) {
    DigestType digest1;
    size_t digest_len;
    bool rv;

    memset(digest, 0, expected_len);
    rv = identity->certificate().ComputeDigest(algorithm, digest,
                                               sizeof(DigestType), &digest_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest_len);

    // Repeat digest computation for the identity as a sanity check.
    memset(digest1, 0xff, expected_len);
    rv = identity->certificate().ComputeDigest(algorithm, digest1,
                                               sizeof(DigestType), &digest_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest_len);

    EXPECT_EQ(0, memcmp(digest, digest1, expected_len));
  }

  void TestDigestForGeneratedCert(const std::string& algorithm,
                                  size_t expected_len) {
    DigestType digest[4];

    ASSERT_TRUE(expected_len <= sizeof(DigestType));

    TestDigestHelper(digest[0], identity_rsa1_.get(), algorithm, expected_len);
    TestDigestHelper(digest[1], identity_rsa2_.get(), algorithm, expected_len);
    TestDigestHelper(digest[2], identity_ecdsa1_.get(), algorithm,
                     expected_len);
    TestDigestHelper(digest[3], identity_ecdsa2_.get(), algorithm,
                     expected_len);

    // Sanity check that all four digests are unique.  This could theoretically
    // fail, since cryptographic hash collisions have a non-zero probability.
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (i != j)
          EXPECT_NE(0, memcmp(digest[i], digest[j], expected_len));
      }
    }
  }

  void TestDigestForFixedCert(const std::string& algorithm,
                              size_t expected_len,
                              const unsigned char* expected_digest) {
    bool rv;
    DigestType digest;
    size_t digest_len;

    ASSERT_TRUE(expected_len <= sizeof(DigestType));

    rv = test_cert_->ComputeDigest(algorithm, digest, sizeof(digest),
                                   &digest_len);
    EXPECT_TRUE(rv);
    EXPECT_EQ(expected_len, digest_len);
    EXPECT_EQ(0, memcmp(digest, expected_digest, expected_len));
  }

 private:
  rtc::scoped_ptr<SSLIdentity> identity_rsa1_;
  rtc::scoped_ptr<SSLIdentity> identity_rsa2_;
  rtc::scoped_ptr<SSLIdentity> identity_ecdsa1_;
  rtc::scoped_ptr<SSLIdentity> identity_ecdsa2_;
  rtc::scoped_ptr<rtc::SSLCertificate> test_cert_;
};

TEST_F(SSLIdentityTest, FixedDigestSHA1) {
  TestDigestForFixedCert(rtc::DIGEST_SHA_1, 20, kTestCertSha1);
}

// HASH_AlgSHA224 is not supported in the chromium linux build.
#if SSL_USE_NSS
TEST_F(SSLIdentityTest, DISABLED_FixedDigestSHA224) {
#else
TEST_F(SSLIdentityTest, FixedDigestSHA224) {
#endif
  TestDigestForFixedCert(rtc::DIGEST_SHA_224, 28, kTestCertSha224);
}

TEST_F(SSLIdentityTest, FixedDigestSHA256) {
  TestDigestForFixedCert(rtc::DIGEST_SHA_256, 32, kTestCertSha256);
}

TEST_F(SSLIdentityTest, FixedDigestSHA384) {
  TestDigestForFixedCert(rtc::DIGEST_SHA_384, 48, kTestCertSha384);
}

TEST_F(SSLIdentityTest, FixedDigestSHA512) {
  TestDigestForFixedCert(rtc::DIGEST_SHA_512, 64, kTestCertSha512);
}

// HASH_AlgSHA224 is not supported in the chromium linux build.
#if SSL_USE_NSS
TEST_F(SSLIdentityTest, DISABLED_DigestSHA224) {
#else
TEST_F(SSLIdentityTest, DigestSHA224) {
#endif
  TestDigestForGeneratedCert(rtc::DIGEST_SHA_224, 28);
}

TEST_F(SSLIdentityTest, DigestSHA256) {
  TestDigestForGeneratedCert(rtc::DIGEST_SHA_256, 32);
}

TEST_F(SSLIdentityTest, DigestSHA384) {
  TestDigestForGeneratedCert(rtc::DIGEST_SHA_384, 48);
}

TEST_F(SSLIdentityTest, DigestSHA512) {
  TestDigestForGeneratedCert(rtc::DIGEST_SHA_512, 64);
}

TEST_F(SSLIdentityTest, FromPEMStringsRSA) {
  static const char kRSA_PRIVATE_KEY_PEM[] =
      "-----BEGIN RSA PRIVATE KEY-----\n"
      "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAMYRkbhmI7kVA/rM\n"
      "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
      "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
      "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAECgYAvgOs4FJcgvp+TuREx7YtiYVsH\n"
      "mwQPTum2z/8VzWGwR8BBHBvIpVe1MbD/Y4seyI2aco/7UaisatSgJhsU46/9Y4fq\n"
      "2TwXH9QANf4at4d9n/R6rzwpAJOpgwZgKvdQjkfrKTtgLV+/dawvpxUYkRH4JZM1\n"
      "CVGukMfKNrSVH4Ap4QJBAOJmGV1ASPnB4r4nc99at7JuIJmd7fmuVUwUgYi4XgaR\n"
      "WhScBsgYwZ/JoywdyZJgnbcrTDuVcWG56B3vXbhdpMsCQQDf9zeJrjnPZ3Cqm79y\n"
      "kdqANep0uwZciiNiWxsQrCHztywOvbFhdp8iYVFG9EK8DMY41Y5TxUwsHD+67zao\n"
      "ZNqJAkEA1suLUP/GvL8IwuRneQd2tWDqqRQ/Td3qq03hP7e77XtF/buya3Ghclo5\n"
      "54czUR89QyVfJEC6278nzA7n2h1uVQJAcG6mztNL6ja/dKZjYZye2CY44QjSlLo0\n"
      "MTgTSjdfg/28fFn2Jjtqf9Pi/X+50LWI/RcYMC2no606wRk9kyOuIQJBAK6VSAim\n"
      "1pOEjsYQn0X5KEIrz1G3bfCbB848Ime3U2/FWlCHMr6ch8kCZ5d1WUeJD3LbwMNG\n"
      "UCXiYxSsu20QNVw=\n"
      "-----END RSA PRIVATE KEY-----\n";

  static const char kCERT_PEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIBmTCCAQKgAwIBAgIEbzBSAjANBgkqhkiG9w0BAQsFADARMQ8wDQYDVQQDEwZX\n"
      "ZWJSVEMwHhcNMTQwMTAyMTgyNDQ3WhcNMTQwMjAxMTgyNDQ3WjARMQ8wDQYDVQQD\n"
      "EwZXZWJSVEMwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMYRkbhmI7kVA/rM\n"
      "czsZ+6JDhDvnkF+vn6yCAGuRPV03zuRqZtDy4N4to7PZu9PjqrRl7nDMXrG3YG9y\n"
      "rlIAZ72KjcKKFAJxQyAKLCIdawKRyp8RdK3LEySWEZb0AV58IadqPZDTNHHRX8dz\n"
      "5aTSMsbbkZ+C/OzTnbiMqLL/vg6jAgMBAAEwDQYJKoZIhvcNAQELBQADgYEAUflI\n"
      "VUe5Krqf5RVa5C3u/UTAOAUJBiDS3VANTCLBxjuMsvqOG0WvaYWP3HYPgrz0jXK2\n"
      "LJE/mGw3MyFHEqi81jh95J+ypl6xKW6Rm8jKLR87gUvCaVYn/Z4/P3AqcQTB7wOv\n"
      "UD0A8qfhfDM+LK6rPAnCsVN0NRDY3jvd6rzix9M=\n"
      "-----END CERTIFICATE-----\n";

  rtc::scoped_ptr<SSLIdentity> identity(
      SSLIdentity::FromPEMStrings(kRSA_PRIVATE_KEY_PEM, kCERT_PEM));
  EXPECT_TRUE(identity);
  EXPECT_EQ(kCERT_PEM, identity->certificate().ToPEMString());
}

#if SSL_USE_OPENSSL
// This will not work on NSS as PK11_ImportDERPrivateKeyInfoAndReturnKey is not
// ready for EC keys.  Furthermore, NSSIdentity::FromPEMStrings is currently
// hardwired for RSA (the header matching via kPemTypeRsaPrivateKey needs
// trivial generalization).
TEST_F(SSLIdentityTest, FromPEMStringsEC) {
  static const char kRSA_PRIVATE_KEY_PEM[] =
      "-----BEGIN EC PRIVATE KEY-----\n"
      "MHcCAQEEIKkIztWLPbs4Y2zWv7VW2Ov4is2ifleCuPgRB8fRv3IkoAoGCCqGSM49\n"
      "AwEHoUQDQgAEDPV33NrhSdhg9cBRkUWUXnVMXc3h17i9ARbSmNgminKcBXb8/y8L\n"
      "A76cMWQPPM0ybHO8OS7ZVg2U/m+TwE1M2g==\n"
      "-----END EC PRIVATE KEY-----\n";
  static const char kCERT_PEM[] =
      "-----BEGIN CERTIFICATE-----\n"
      "MIIB0jCCAXmgAwIBAgIJAMCjpFt9t6LMMAoGCCqGSM49BAMCMEUxCzAJBgNVBAYT\n"
      "AkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRn\n"
      "aXRzIFB0eSBMdGQwIBcNMTUwNjMwMTMwMTIyWhgPMjI4OTA0MTMxMzAxMjJaMEUx\n"
      "CzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRl\n"
      "cm5ldCBXaWRnaXRzIFB0eSBMdGQwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQM\n"
      "9Xfc2uFJ2GD1wFGRRZRedUxdzeHXuL0BFtKY2CaKcpwFdvz/LwsDvpwxZA88zTJs\n"
      "c7w5LtlWDZT+b5PATUzao1AwTjAdBgNVHQ4EFgQUYHq6nxNNIE832ZmaHc/noODO\n"
      "rtAwHwYDVR0jBBgwFoAUYHq6nxNNIE832ZmaHc/noODOrtAwDAYDVR0TBAUwAwEB\n"
      "/zAKBggqhkjOPQQDAgNHADBEAiAQRojsTyZG0BlKoU7gOt5h+yAMLl2cxmDtOIQr\n"
      "GWP/PwIgJynB4AUDsPT0DWmethOXYijB5sY5UPd9DvgmiS/Mr6s=\n"
      "-----END CERTIFICATE-----\n";

  rtc::scoped_ptr<SSLIdentity> identity(
      SSLIdentity::FromPEMStrings(kRSA_PRIVATE_KEY_PEM, kCERT_PEM));
  EXPECT_TRUE(identity);
  EXPECT_EQ(kCERT_PEM, identity->certificate().ToPEMString());
}
#endif

TEST_F(SSLIdentityTest, PemDerConversion) {
  std::string der;
  EXPECT_TRUE(SSLIdentity::PemToDer("CERTIFICATE", kTestCertificate, &der));

  EXPECT_EQ(kTestCertificate, SSLIdentity::DerToPem(
      "CERTIFICATE",
      reinterpret_cast<const unsigned char*>(der.data()), der.length()));
}

TEST_F(SSLIdentityTest, GetSignatureDigestAlgorithm) {
  TestGetSignatureDigestAlgorithm();
}
