// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/certificate_error_reporter.h"

#include <set>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/net/encrypted_cert_logger.pb.h"

#if defined(USE_OPENSSL)
#include "crypto/aead_openssl.h"
#endif

#include "crypto/curve25519.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request_context.h"

namespace {

// Constants used for crypto
static const uint8 kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};
static const uint32 kServerPublicKeyVersion = 1;

#if defined(USE_OPENSSL)

static const char kHkdfLabel[] = "certificate report";

bool EncryptSerializedReport(
    const uint8* server_public_key,
    uint32 server_public_key_version,
    const std::string& report,
    chrome_browser_net::EncryptedCertLoggerRequest* encrypted_report) {
  // Generate an ephemeral key pair to generate a shared secret.
  uint8 public_key[crypto::curve25519::kBytes];
  uint8 private_key[crypto::curve25519::kScalarBytes];
  uint8 shared_secret[crypto::curve25519::kBytes];

  crypto::RandBytes(private_key, sizeof(private_key));
  crypto::curve25519::ScalarBaseMult(private_key, public_key);
  crypto::curve25519::ScalarMult(private_key, server_public_key, shared_secret);

  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  crypto::HKDF hkdf(std::string(reinterpret_cast<char*>(shared_secret),
                                sizeof(shared_secret)),
                    std::string(),
                    base::StringPiece(kHkdfLabel, sizeof(kHkdfLabel)), 0, 0,
                    aead.KeyLength());

  const std::string key(hkdf.subkey_secret().data(),
                        hkdf.subkey_secret().size());
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), 0);

  std::string ciphertext;
  if (!aead.Seal(report, nonce, std::string(), &ciphertext)) {
    LOG(ERROR) << "Error sealing certificate report.";
    return false;
  }

  encrypted_report->set_encrypted_report(ciphertext);
  encrypted_report->set_server_public_key_version(server_public_key_version);
  encrypted_report->set_client_public_key(
      std::string(reinterpret_cast<char*>(public_key), sizeof(public_key)));
  encrypted_report->set_algorithm(
      chrome_browser_net::EncryptedCertLoggerRequest::
          AEAD_ECDH_AES_128_CTR_HMAC_SHA256);
  return true;
}
#endif

}  // namespace

namespace chrome_browser_net {

// Constants for the Finch trial that controls whether the
// CertificateErrorReporter supports HTTP uploads.
const char kHttpCertificateUploadExperiment[] =
    "ReportCertificateErrorsOverHttp";
const char kHttpCertificateUploadGroup[] = "UploadReportsOverHttp";

CertificateErrorReporter::CertificateErrorReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url,
    CookiesPreference cookies_preference)
    : CertificateErrorReporter(request_context,
                               upload_url,
                               cookies_preference,
                               kServerPublicKey,
                               kServerPublicKeyVersion) {
}

CertificateErrorReporter::CertificateErrorReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url,
    CookiesPreference cookies_preference,
    const uint8 server_public_key[32],
    const uint32 server_public_key_version)
    : request_context_(request_context),
      upload_url_(upload_url),
      cookies_preference_(cookies_preference),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK(!upload_url.is_empty());
}

CertificateErrorReporter::~CertificateErrorReporter() {
  STLDeleteElements(&inflight_requests_);
}

void CertificateErrorReporter::SendReport(
    ReportType type,
    const std::string& serialized_report) {
  switch (type) {
    case REPORT_TYPE_PINNING_VIOLATION:
      SendSerializedRequest(serialized_report);
      break;
    case REPORT_TYPE_EXTENDED_REPORTING:
      if (upload_url_.SchemeIsCryptographic()) {
        SendSerializedRequest(serialized_report);
      } else {
        DCHECK(IsHttpUploadUrlSupported());
#if defined(USE_OPENSSL)
        EncryptedCertLoggerRequest encrypted_report;
        if (!EncryptSerializedReport(server_public_key_,
                                     server_public_key_version_,
                                     serialized_report, &encrypted_report)) {
          LOG(ERROR) << "Failed to encrypt serialized report.";
          return;
        }
        std::string serialized_encrypted_report;
        encrypted_report.SerializeToString(&serialized_encrypted_report);
        SendSerializedRequest(serialized_encrypted_report);
#endif
      }
      break;
    default:
      NOTREACHED();
  }
}

void CertificateErrorReporter::OnResponseStarted(net::URLRequest* request) {
  const net::URLRequestStatus& status(request->status());
  if (!status.is_success()) {
    LOG(WARNING) << "Certificate upload failed"
                 << " status:" << status.status()
                 << " error:" << status.error();
  } else if (request->GetResponseCode() != 200) {
    LOG(WARNING) << "Certificate upload HTTP status: "
                 << request->GetResponseCode();
  }
  RequestComplete(request);
}

void CertificateErrorReporter::OnReadCompleted(net::URLRequest* request,
                                               int bytes_read) {
}

scoped_ptr<net::URLRequest> CertificateErrorReporter::CreateURLRequest(
    net::URLRequestContext* context) {
  scoped_ptr<net::URLRequest> request =
      context->CreateRequest(upload_url_, net::DEFAULT_PRIORITY, this);
  if (cookies_preference_ != SEND_COOKIES) {
    request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                          net::LOAD_DO_NOT_SAVE_COOKIES);
  }
  return request.Pass();
}

bool CertificateErrorReporter::IsHttpUploadUrlSupported() {
#if defined(USE_OPENSSL)
  return base::FieldTrialList::FindFullName(kHttpCertificateUploadExperiment) ==
         kHttpCertificateUploadGroup;
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
  uint8 shared_secret[crypto::curve25519::kBytes];
  crypto::curve25519::ScalarMult(
      server_private_key, reinterpret_cast<const uint8*>(
                              encrypted_report.client_public_key().data()),
      shared_secret);

  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  crypto::HKDF hkdf(std::string(reinterpret_cast<char*>(shared_secret),
                                sizeof(shared_secret)),
                    std::string(),
                    base::StringPiece(kHkdfLabel, sizeof(kHkdfLabel)), 0, 0,
                    aead.KeyLength());

  const std::string key(hkdf.subkey_secret().data(),
                        hkdf.subkey_secret().size());
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), 0);

  return aead.Open(encrypted_report.encrypted_report(), nonce, std::string(),
                   decrypted_serialized_report);
}
#endif

void CertificateErrorReporter::SendSerializedRequest(
    const std::string& serialized_request) {
  scoped_ptr<net::URLRequest> url_request = CreateURLRequest(request_context_);
  url_request->set_method("POST");

  scoped_ptr<net::UploadElementReader> reader(
      net::UploadOwnedBytesElementReader::CreateWithString(serialized_request));
  url_request->set_upload(
      net::ElementsUploadDataStream::CreateWithReader(reader.Pass(), 0));

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kContentType,
                    "x-application/chrome-fraudulent-cert-report");
  url_request->SetExtraRequestHeaders(headers);

  net::URLRequest* raw_url_request = url_request.get();
  inflight_requests_.insert(url_request.release());
  raw_url_request->Start();
}

void CertificateErrorReporter::RequestComplete(net::URLRequest* request) {
  std::set<net::URLRequest*>::iterator i = inflight_requests_.find(request);
  DCHECK(i != inflight_requests_.end());
  scoped_ptr<net::URLRequest> url_request(*i);
  inflight_requests_.erase(i);
}

}  // namespace chrome_browser_net
