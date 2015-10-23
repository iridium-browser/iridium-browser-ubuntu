// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/certificate_error_reporter.h"

#include <set>

#include "base/logging.h"
#include "chrome/browser/net/encrypted_cert_logger.pb.h"

#if defined(USE_OPENSSL)
#include "crypto/aead_openssl.h"
#endif

#include "crypto/curve25519.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "net/url_request/certificate_report_sender.h"

namespace {

// Constants used for crypto. The corresponding private key is used by
// the SafeBrowsing client-side detection server to decrypt reports.
static const uint8 kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};
static const uint32 kServerPublicKeyVersion = 1;

#if defined(USE_OPENSSL)

static const char kHkdfLabel[] = "certificate report";

std::string GetHkdfSubkeySecret(size_t subkey_length,
                                const uint8* private_key,
                                const uint8* public_key) {
  uint8 shared_secret[crypto::curve25519::kBytes];
  crypto::curve25519::ScalarMult(private_key, public_key, shared_secret);

  // By mistake, the HKDF label here ends up with an extra null byte on
  // the end, due to using sizeof(kHkdfLabel) in the StringPiece
  // constructor instead of strlen(kHkdfLabel). Ideally this code should
  // be just passing kHkdfLabel directly into the HKDF constructor.
  //
  // TODO(estark): fix this in coordination with the server-side code --
  // perhaps by rolling the public key version forward and using the
  // version to decide whether to use the extra-null-byte version of the
  // label. https://crbug.com/517746
  crypto::HKDF hkdf(base::StringPiece(reinterpret_cast<char*>(shared_secret),
                                      sizeof(shared_secret)),
                    "" /* salt */,
                    base::StringPiece(kHkdfLabel, sizeof(kHkdfLabel)),
                    0 /* key bytes */, 0 /* iv bytes */, subkey_length);

  return hkdf.subkey_secret().as_string();
}

bool EncryptSerializedReport(
    const uint8* server_public_key,
    uint32 server_public_key_version,
    const std::string& report,
    chrome_browser_net::EncryptedCertLoggerRequest* encrypted_report) {
  // Generate an ephemeral key pair to generate a shared secret.
  uint8 public_key[crypto::curve25519::kBytes];
  uint8 private_key[crypto::curve25519::kScalarBytes];

  crypto::RandBytes(private_key, sizeof(private_key));
  crypto::curve25519::ScalarBaseMult(private_key, public_key);

  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  const std::string key =
      GetHkdfSubkeySecret(aead.KeyLength(), private_key,
                          reinterpret_cast<const uint8*>(server_public_key));
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), '\0');

  std::string ciphertext;
  if (!aead.Seal(report, nonce, std::string(), &ciphertext)) {
    LOG(ERROR) << "Error sealing certificate report.";
    return false;
  }

  encrypted_report->set_encrypted_report(ciphertext);
  encrypted_report->set_server_public_key_version(server_public_key_version);
  encrypted_report->set_client_public_key(reinterpret_cast<char*>(public_key),
                                          sizeof(public_key));
  encrypted_report->set_algorithm(
      chrome_browser_net::EncryptedCertLoggerRequest::
          AEAD_ECDH_AES_128_CTR_HMAC_SHA256);
  return true;
}
#endif

}  // namespace

namespace chrome_browser_net {

CertificateErrorReporter::CertificateErrorReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url,
    net::CertificateReportSender::CookiesPreference cookies_preference)
    : CertificateErrorReporter(upload_url,
                               kServerPublicKey,
                               kServerPublicKeyVersion,
                               make_scoped_ptr(new net::CertificateReportSender(
                                   request_context,
                                   cookies_preference))) {}

CertificateErrorReporter::CertificateErrorReporter(
    const GURL& upload_url,
    const uint8 server_public_key[/* 32 */],
    const uint32 server_public_key_version,
    scoped_ptr<net::CertificateReportSender> certificate_report_sender)
    : certificate_report_sender_(certificate_report_sender.Pass()),
      upload_url_(upload_url),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK(certificate_report_sender_);
  DCHECK(!upload_url.is_empty());
}

CertificateErrorReporter::~CertificateErrorReporter() {
}

void CertificateErrorReporter::SendExtendedReportingReport(
    const std::string& serialized_report) {
  if (upload_url_.SchemeIsCryptographic()) {
    certificate_report_sender_->Send(upload_url_, serialized_report);
  } else {
    DCHECK(IsHttpUploadUrlSupported());
#if defined(USE_OPENSSL)
    EncryptedCertLoggerRequest encrypted_report;
    if (!EncryptSerializedReport(server_public_key_, server_public_key_version_,
                                 serialized_report, &encrypted_report)) {
      LOG(ERROR) << "Failed to encrypt serialized report.";
      return;
    }
    std::string serialized_encrypted_report;
    encrypted_report.SerializeToString(&serialized_encrypted_report);
    certificate_report_sender_->Send(upload_url_, serialized_encrypted_report);
#endif
  }
}

bool CertificateErrorReporter::IsHttpUploadUrlSupported() {
#if defined(USE_OPENSSL)
  return true;
#else
  return false;
#endif
}

// Used only by tests.
#if defined(USE_OPENSSL)
bool CertificateErrorReporter::DecryptCertificateErrorReport(
    const uint8 server_private_key[32],
    const EncryptedCertLoggerRequest& encrypted_report,
    std::string* decrypted_serialized_report) {
  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  const std::string key =
      GetHkdfSubkeySecret(aead.KeyLength(), server_private_key,
                          reinterpret_cast<const uint8*>(
                              encrypted_report.client_public_key().data()));
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), 0);

  return aead.Open(encrypted_report.encrypted_report(), nonce, std::string(),
                   decrypted_serialized_report);
}
#endif

}  // namespace chrome_browser_net
