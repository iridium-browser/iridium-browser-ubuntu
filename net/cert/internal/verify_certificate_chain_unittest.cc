// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_certificate_chain.h"

#include "net/cert/internal/signature_policy.h"
#include "net/cert/internal/trust_store.h"
#include "net/cert/internal/verify_certificate_chain_typed_unittest.h"

namespace net {

namespace {

class VerifyCertificateChainDelegate {
 public:
  static void Verify(const ParsedCertificateList& chain,
                     const scoped_refptr<TrustAnchor>& trust_anchor,
                     const der::GeneralizedTime& time,
                     bool expected_result) {
    ASSERT_TRUE(trust_anchor);

    SimpleSignaturePolicy signature_policy(1024);

    bool result = VerifyCertificateChain(chain, trust_anchor.get(),
                                         &signature_policy, time);

    ASSERT_EQ(expected_result, result);
  }
};

}  // namespace

INSTANTIATE_TYPED_TEST_CASE_P(VerifyCertificateChain,
                              VerifyCertificateChainSingleRootTest,
                              VerifyCertificateChainDelegate);

}  // namespace net
