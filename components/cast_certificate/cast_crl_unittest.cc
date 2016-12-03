// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "components/cast_certificate/cast_cert_validator.h"
#include "components/cast_certificate/cast_cert_validator_test_helpers.h"
#include "components/cast_certificate/cast_crl.h"
#include "components/cast_certificate/proto/test_suite.pb.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_certificate {
namespace {

// Creates a trust store using the test roots encoded in the PEM file at |path|.
std::unique_ptr<net::TrustStoreInMemory> CreateTrustStoreFromFile(
    const std::string& path) {
  std::unique_ptr<net::TrustStoreInMemory> trust_store(
      new net::TrustStoreInMemory());
  const auto trusted_test_roots =
      cast_certificate::testing::ReadCertificateChainFromFile(path);
  for (const auto& trusted_root : trusted_test_roots) {
    scoped_refptr<net::ParsedCertificate> cert(
        net::ParsedCertificate::CreateFromCertificateCopy(trusted_root, {}));
    EXPECT_TRUE(cert);
    scoped_refptr<net::TrustAnchor> anchor =
        net::TrustAnchor::CreateFromCertificateWithConstraints(std::move(cert));
    trust_store->AddTrustAnchor(std::move(anchor));
  }
  return trust_store;
}

// Converts uint64_t unix timestamp in seconds to base::Time.
base::Time ConvertUnixTimestampSeconds(uint64_t time) {
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(time * 1000);
}

// Indicates the expected result of test step's verification.
enum TestStepResult {
  RESULT_SUCCESS,
  RESULT_FAIL,
};

// Verifies that the provided certificate chain is valid at the specified time
// and chains up to a trust anchor.
bool TestVerifyCertificate(TestStepResult expected_result,
                           const std::vector<std::string>& certificate_chain,
                           const base::Time& time,
                           net::TrustStore* cast_trust_store) {
  std::unique_ptr<CertVerificationContext> context;
  CastDeviceCertPolicy policy;
  int result;
  if (cast_trust_store != nullptr) {
    result = VerifyDeviceCertForTest(certificate_chain, time, &context, &policy,
                                     nullptr, CRLPolicy::CRL_OPTIONAL,
                                     cast_trust_store);
  } else {
    result = VerifyDeviceCert(certificate_chain, time, &context, &policy,
                              nullptr, CRLPolicy::CRL_OPTIONAL);
  }
  if (expected_result != RESULT_SUCCESS) {
    EXPECT_FALSE(result);
    return !result;
  }
  EXPECT_TRUE(result);
  return result;
}

// Verifies that the provided Cast CRL is signed by a trusted issuer
// and that the CRL can be parsed successfully.
// The validity of the CRL is also checked at the specified time.
bool TestVerifyCRL(TestStepResult expected_result,
                   const std::string& crl_bundle,
                   const base::Time& time,
                   net::TrustStore* crl_trust_store) {
  std::unique_ptr<CastCRL> crl;
  if (crl_trust_store != nullptr) {
    crl = ParseAndVerifyCRLForTest(crl_bundle, time, crl_trust_store);
  } else {
    crl = ParseAndVerifyCRL(crl_bundle, time);
  }
  if (expected_result != RESULT_SUCCESS) {
    EXPECT_EQ(crl, nullptr);
    return crl == nullptr;
  }
  EXPECT_NE(crl, nullptr);
  return crl != nullptr;
}

// Verifies that the certificate chain provided is not revoked according to
// the provided Cast CRL at |cert_time|.
// The provided CRL is verified at |crl_time|.
// If |crl_required| is set, then a valid Cast CRL must be provided.
// Otherwise, a missing CRL is be ignored.
bool TestVerifyRevocation(TestStepResult expected_result,
                          const std::vector<std::string>& certificate_chain,
                          const std::string& crl_bundle,
                          const base::Time& crl_time,
                          const base::Time& cert_time,
                          bool crl_required,
                          net::TrustStore* cast_trust_store,
                          net::TrustStore* crl_trust_store) {
  std::unique_ptr<CastCRL> crl;
  if (!crl_bundle.empty()) {
    if (crl_trust_store != nullptr) {
      crl = ParseAndVerifyCRLForTest(crl_bundle, crl_time, crl_trust_store);
    } else {
      crl = ParseAndVerifyCRL(crl_bundle, crl_time);
    }
    EXPECT_NE(crl.get(), nullptr);
  }

  std::unique_ptr<CertVerificationContext> context;
  CastDeviceCertPolicy policy;
  CRLPolicy crl_policy = CRLPolicy::CRL_REQUIRED;
  if (!crl_required)
    crl_policy = CRLPolicy::CRL_OPTIONAL;
  int result;
  if (cast_trust_store != nullptr) {
    result =
        VerifyDeviceCertForTest(certificate_chain, cert_time, &context, &policy,
                                crl.get(), crl_policy, cast_trust_store);
  } else {
    result = VerifyDeviceCert(certificate_chain, cert_time, &context, &policy,
                              crl.get(), crl_policy);
  }
  if (expected_result != RESULT_SUCCESS) {
    EXPECT_FALSE(result);
    return !result;
  }
  EXPECT_TRUE(result);
  return result;
}

// Runs a single test case.
bool RunTest(const DeviceCertTest& test_case) {
  std::unique_ptr<net::TrustStoreInMemory> crl_trust_store;
  std::unique_ptr<net::TrustStoreInMemory> cast_trust_store;
  if (test_case.use_test_trust_anchors()) {
    crl_trust_store =
        CreateTrustStoreFromFile("certificates/cast_crl_test_root_ca.pem");
    cast_trust_store =
        CreateTrustStoreFromFile("certificates/cast_test_root_ca.pem");

    EXPECT_TRUE(crl_trust_store.get());
    EXPECT_TRUE(cast_trust_store.get());
  }

  std::vector<std::string> certificate_chain;
  for (auto const& cert : test_case.der_cert_path()) {
    certificate_chain.push_back(cert);
  }

  base::Time cert_verification_time =
      ConvertUnixTimestampSeconds(test_case.cert_verification_time_seconds());

  uint64_t crl_verify_time = test_case.crl_verification_time_seconds();
  base::Time crl_verification_time =
      ConvertUnixTimestampSeconds(crl_verify_time);
  if (crl_verify_time == 0)
    crl_verification_time = cert_verification_time;

  std::string crl_bundle = test_case.crl_bundle();
  switch (test_case.expected_result()) {
    case PATH_VERIFICATION_FAILED:
      return TestVerifyCertificate(RESULT_FAIL, certificate_chain,
                                   cert_verification_time,
                                   cast_trust_store.get());
      break;
    case CRL_VERIFICATION_FAILED:
      return TestVerifyCRL(RESULT_FAIL, crl_bundle, crl_verification_time,
                           crl_trust_store.get());
      break;
    case REVOCATION_CHECK_FAILED_WITHOUT_CRL:
      return TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time,
                                   cast_trust_store.get()) &&
             TestVerifyCRL(RESULT_FAIL, crl_bundle, crl_verification_time,
                           crl_trust_store.get()) &&
             TestVerifyRevocation(RESULT_FAIL, certificate_chain, crl_bundle,
                                  crl_verification_time, cert_verification_time,
                                  true, cast_trust_store.get(),
                                  crl_trust_store.get());
      break;
    case REVOCATION_CHECK_FAILED:
      return TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time,
                                   cast_trust_store.get()) &&
             TestVerifyCRL(RESULT_SUCCESS, crl_bundle, crl_verification_time,
                           crl_trust_store.get()) &&
             TestVerifyRevocation(RESULT_FAIL, certificate_chain, crl_bundle,
                                  crl_verification_time, cert_verification_time,
                                  false, cast_trust_store.get(),
                                  crl_trust_store.get());
      break;
    case SUCCESS:
      return (crl_bundle.empty() ||
              TestVerifyCRL(RESULT_SUCCESS, crl_bundle, crl_verification_time,
                            crl_trust_store.get())) &&
             TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time,
                                   cast_trust_store.get()) &&
             TestVerifyRevocation(RESULT_SUCCESS, certificate_chain, crl_bundle,
                                  crl_verification_time, cert_verification_time,
                                  !crl_bundle.empty(), cast_trust_store.get(),
                                  crl_trust_store.get());
      break;
    case UNSPECIFIED:
      return false;
      break;
  }
  return false;
}

// Parses the provided test suite provided in wire-format proto.
// Each test contains the inputs and the expected output.
// To see the description of the test, execute the test.
// These tests are generated by a test generator in google3.
void RunTestSuite(const std::string& test_suite_file_name) {
  std::string testsuite_raw =
      cast_certificate::testing::ReadTestFileToString(test_suite_file_name);
  DeviceCertTestSuite test_suite;
  EXPECT_TRUE(test_suite.ParseFromString(testsuite_raw));
  uint16_t success = 0;
  uint16_t failed = 0;
  std::vector<std::string> failed_tests;

  for (auto const& test_case : test_suite.tests()) {
    LOG(INFO) << "[ RUN      ] " << test_case.description();
    bool result = RunTest(test_case);
    EXPECT_TRUE(result);
    if (!result) {
      LOG(INFO) << "[  FAILED  ] " << test_case.description();
      ++failed;
      failed_tests.push_back(test_case.description());
    } else {
      LOG(INFO) << "[  PASSED  ] " << test_case.description();
      ++success;
    }
  }
  LOG(INFO) << "[  PASSED  ] " << success << " test(s).";
  if (failed) {
    LOG(INFO) << "[  FAILED  ] " << failed << " test(s), listed below:";
    for (const auto& failed_test : failed_tests) {
      LOG(INFO) << "[  FAILED  ] " << failed_test;
    }
  }
}

TEST(CastCertificateTest, TestSuite1) {
  RunTestSuite("testsuite/testsuite1.pb");
}

}  // namespace

}  // namespace cast_certificate
