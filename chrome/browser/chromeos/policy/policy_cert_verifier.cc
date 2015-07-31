// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_cert_verifier.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/multi_threaded_cert_verifier.h"

namespace policy {

namespace {

void MaybeSignalAnchorUse(int error,
                          const base::Closure& anchor_used_callback,
                          const net::CertVerifyResult& verify_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (error != net::OK || !verify_result.is_issued_by_additional_trust_anchor ||
      anchor_used_callback.is_null()) {
    return;
  }
  anchor_used_callback.Run();
}

void CompleteAndSignalAnchorUse(
    const base::Closure& anchor_used_callback,
    const net::CompletionCallback& completion_callback,
    const net::CertVerifyResult* verify_result,
    int error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  MaybeSignalAnchorUse(error, anchor_used_callback, *verify_result);
  if (!completion_callback.is_null())
    completion_callback.Run(error);
}

}  // namespace

PolicyCertVerifier::PolicyCertVerifier(
    const base::Closure& anchor_used_callback)
    : anchor_used_callback_(anchor_used_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PolicyCertVerifier::~PolicyCertVerifier() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
}

void PolicyCertVerifier::InitializeOnIOThread(
    const scoped_refptr<net::CertVerifyProc>& verify_proc) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!verify_proc->SupportsAdditionalTrustAnchors()) {
    LOG(WARNING)
        << "Additional trust anchors not supported on the current platform!";
  }
  net::MultiThreadedCertVerifier* verifier =
      new net::MultiThreadedCertVerifier(verify_proc.get());
  verifier->SetCertTrustAnchorProvider(this);
  delegate_.reset(verifier);
}

void PolicyCertVerifier::SetTrustAnchors(
    const net::CertificateList& trust_anchors) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  trust_anchors_ = trust_anchors;
}

int PolicyCertVerifier::Verify(
    net::X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    net::CRLSet* crl_set,
    net::CertVerifyResult* verify_result,
    const net::CompletionCallback& completion_callback,
    scoped_ptr<Request>* out_req,
    const net::BoundNetLog& net_log) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(delegate_);
  net::CompletionCallback wrapped_callback =
      base::Bind(&CompleteAndSignalAnchorUse,
                 anchor_used_callback_,
                 completion_callback,
                 verify_result);
  int error =
      delegate_->Verify(cert, hostname, ocsp_response, flags, crl_set,
                        verify_result, wrapped_callback, out_req, net_log);
  MaybeSignalAnchorUse(error, anchor_used_callback_, *verify_result);
  return error;
}

bool PolicyCertVerifier::SupportsOCSPStapling() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return delegate_->SupportsOCSPStapling();
}

const net::CertificateList& PolicyCertVerifier::GetAdditionalTrustAnchors() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return trust_anchors_;
}

}  // namespace policy
