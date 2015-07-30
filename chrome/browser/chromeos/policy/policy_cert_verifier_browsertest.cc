// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "crypto/nss_util_internal.h"
#include "crypto/scoped_test_nss_chromeos_user.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyCertVerifierTest : public testing::Test {
 public:
  PolicyCertVerifierTest()
      : trust_anchor_used_(false), test_nss_user_("user1") {}

  ~PolicyCertVerifierTest() override {}

  void SetUp() override {
    ASSERT_TRUE(test_nss_user_.constructed_successfully());
    test_nss_user_.FinishInit();

    test_cert_db_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::GetPublicSlotForChromeOSUser(test_nss_user_.username_hash()),
        crypto::GetPrivateSlotForChromeOSUser(
            test_nss_user_.username_hash(),
            base::Callback<void(crypto::ScopedPK11Slot)>())));
    test_cert_db_->SetSlowTaskRunnerForTest(
        base::ThreadTaskRunnerHandle::Get());

    cert_verifier_.reset(new PolicyCertVerifier(base::Bind(
        &PolicyCertVerifierTest::OnTrustAnchorUsed, base::Unretained(this))));
    cert_verifier_->InitializeOnIOThread(new chromeos::CertVerifyProcChromeOS(
        crypto::GetPublicSlotForChromeOSUser(test_nss_user_.username_hash())));

    test_ca_cert_ = LoadCertificate("root_ca_cert.pem", net::CA_CERT);
    ASSERT_TRUE(test_ca_cert_.get());
    test_server_cert_ = LoadCertificate("ok_cert.pem", net::SERVER_CERT);
    ASSERT_TRUE(test_server_cert_.get());
    test_ca_cert_list_.push_back(test_ca_cert_);
  }

  void TearDown() override {
    // Destroy |cert_verifier_| before destroying the ThreadBundle, otherwise
    // BrowserThread::CurrentlyOn checks fail.
    cert_verifier_.reset();
  }

 protected:
  int VerifyTestServerCert(const net::TestCompletionCallback& test_callback,
                           net::CertVerifyResult* verify_result,
                           scoped_ptr<net::CertVerifier::Request>* request) {
    return cert_verifier_->Verify(
        test_server_cert_.get(), "127.0.0.1", std::string(), 0, NULL,
        verify_result, test_callback.callback(), request, net::BoundNetLog());
  }

  bool SupportsAdditionalTrustAnchors() {
    scoped_refptr<net::CertVerifyProc> proc =
        net::CertVerifyProc::CreateDefault();
    return proc->SupportsAdditionalTrustAnchors();
  }

  // Returns whether |cert_verifier| signalled usage of one of the additional
  // trust anchors (i.e. of |test_ca_cert_|) for the first time or since the
  // last call of this function.
  bool WasTrustAnchorUsedAndReset() {
    base::RunLoop().RunUntilIdle();
    bool result = trust_anchor_used_;
    trust_anchor_used_ = false;
    return result;
  }

  // |test_ca_cert_| is the issuer of |test_server_cert_|.
  scoped_refptr<net::X509Certificate> test_ca_cert_;
  scoped_refptr<net::X509Certificate> test_server_cert_;
  net::CertificateList test_ca_cert_list_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_cert_db_;
  scoped_ptr<PolicyCertVerifier> cert_verifier_;

 private:
  void OnTrustAnchorUsed() {
    trust_anchor_used_ = true;
  }

  scoped_refptr<net::X509Certificate> LoadCertificate(const std::string& name,
                                                      net::CertType type) {
    scoped_refptr<net::X509Certificate> cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), name);

    // No certificate is trusted right after it's loaded.
    net::NSSCertDatabase::TrustBits trust =
        test_cert_db_->GetCertTrust(cert.get(), type);
    EXPECT_EQ(net::NSSCertDatabase::TRUST_DEFAULT, trust);

    return cert;
  }

  bool trust_anchor_used_;
  crypto::ScopedTestNSSChromeOSUser test_nss_user_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(PolicyCertVerifierTest, VerifyUntrustedCert) {
  // |test_server_cert_| is untrusted, so Verify() fails.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }

  // Issuing the same request again hits the cache. This tests the synchronous
  // path.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }

  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

TEST_F(PolicyCertVerifierTest, VerifyTrustedCert) {
  // Make the database trust |test_ca_cert_|.
  net::NSSCertDatabase::ImportCertFailureList failure_list;
  ASSERT_TRUE(test_cert_db_->ImportCACerts(
      test_ca_cert_list_, net::NSSCertDatabase::TRUSTED_SSL, &failure_list));
  ASSERT_TRUE(failure_list.empty());

  // Verify that it is now trusted.
  net::NSSCertDatabase::TrustBits trust =
      test_cert_db_->GetCertTrust(test_ca_cert_.get(), net::CA_CERT);
  EXPECT_EQ(net::NSSCertDatabase::TRUSTED_SSL, trust);

  // Verify() successfully verifies |test_server_cert_| after it was imported.
  net::CertVerifyResult verify_result;
  net::TestCompletionCallback callback;
  scoped_ptr<net::CertVerifier::Request> request;
  int error = VerifyTestServerCert(callback, &verify_result, &request);
  ASSERT_EQ(net::ERR_IO_PENDING, error);
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_EQ(net::OK, error);

  // The additional trust anchors were not used, since the certificate is
  // trusted from the database.
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

TEST_F(PolicyCertVerifierTest, VerifyUsingAdditionalTrustAnchor) {
  ASSERT_TRUE(SupportsAdditionalTrustAnchors());

  // |test_server_cert_| is untrusted, so Verify() fails.
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());

  // Verify() again with the additional trust anchors.
  cert_verifier_->SetTrustAnchors(test_ca_cert_list_);
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    ASSERT_EQ(net::ERR_IO_PENDING, error);
    EXPECT_TRUE(request);
    error = callback.WaitForResult();
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());

  // Verify() again with the additional trust anchors will hit the cache.
  cert_verifier_->SetTrustAnchors(test_ca_cert_list_);
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    EXPECT_EQ(net::OK, error);
  }
  EXPECT_TRUE(WasTrustAnchorUsedAndReset());

  // Verifying after removing the trust anchors should now fail.
  cert_verifier_->SetTrustAnchors(net::CertificateList());
  {
    net::CertVerifyResult verify_result;
    net::TestCompletionCallback callback;
    scoped_ptr<net::CertVerifier::Request> request;
    int error = VerifyTestServerCert(callback, &verify_result, &request);
    // Note: this hits the cached result from the first Verify() in this test.
    EXPECT_EQ(net::ERR_CERT_AUTHORITY_INVALID, error);
  }
  // The additional trust anchors were reset, thus |cert_verifier_| should not
  // signal it's usage anymore.
  EXPECT_FALSE(WasTrustAnchorUsedAndReset());
}

}  // namespace policy
