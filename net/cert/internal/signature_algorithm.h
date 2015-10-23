// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_
#define NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"

namespace net {

namespace der {
class Input;
}  // namespace der

// The digest algorithm used within a signature.
enum class DigestAlgorithm {
  Sha1,
  Sha256,
  Sha384,
  Sha512,
};

// The signature scheme used within a signature. Parameters are specified
// separately.
enum class SignatureAlgorithmId {
  RsaPkcs1,  // RSA PKCS#1 v1.5
  RsaPss,    // RSASSA-PSS
  Ecdsa,     // ECDSA
};

// Base class for describing algorithm parameters.
class NET_EXPORT SignatureAlgorithmParameters {
 public:
  SignatureAlgorithmParameters() {}
  virtual ~SignatureAlgorithmParameters(){};

 private:
  DISALLOW_COPY_AND_ASSIGN(SignatureAlgorithmParameters);
};

// Parameters for an RSASSA-PSS signature algorithm.
//
// The trailer is assumed to be 1 and the mask generation algorithm to be MGF1,
// as that is all that is implemented, and any other values while parsing the
// AlgorithmIdentifier will thus be rejected.
class NET_EXPORT RsaPssParameters : public SignatureAlgorithmParameters {
 public:
  RsaPssParameters(DigestAlgorithm mgf1_hash, uint32_t salt_length);

  bool Equals(const RsaPssParameters* other) const;

  DigestAlgorithm mgf1_hash() const { return mgf1_hash_; }
  uint32_t salt_length() const { return salt_length_; }

 private:
  const DigestAlgorithm mgf1_hash_;
  const uint32_t salt_length_;
};

// SignatureAlgorithm describes a signature algorithm and its parameters. This
// corresponds to "AlgorithmIdentifier" from RFC 5280.
class NET_EXPORT SignatureAlgorithm {
 public:
  ~SignatureAlgorithm();

  SignatureAlgorithmId algorithm() const { return algorithm_; }
  DigestAlgorithm digest() const { return digest_; }

  // Creates a SignatureAlgorithm by parsing a DER-encoded "AlgorithmIdentifier"
  // (RFC 5280). Returns nullptr on failure.
  static scoped_ptr<SignatureAlgorithm> CreateFromDer(
      const der::Input& algorithm_identifier);

  // Creates a new SignatureAlgorithm with the given type and parameters.
  static scoped_ptr<SignatureAlgorithm> CreateRsaPkcs1(DigestAlgorithm digest);
  static scoped_ptr<SignatureAlgorithm> CreateEcdsa(DigestAlgorithm digest);
  static scoped_ptr<SignatureAlgorithm> CreateRsaPss(DigestAlgorithm digest,
                                                     DigestAlgorithm mgf1_hash,
                                                     uint32_t salt_length);

  // Returns true if |*this| is equivalent to |other|. This compares both the
  // algorithm ID and each parameter for equality.
  bool Equals(const SignatureAlgorithm& other) const WARN_UNUSED_RESULT;

  // The following methods retrieve the parameters for the signature algorithm.
  //
  // The correct parameters should be chosen based on the algorithm ID. For
  // instance a SignatureAlgorithm with |algorithm() == RsaPss| should retrieve
  // parameters via ParametersForRsaPss().
  //
  // The returned pointer is non-owned, and has the same lifetime as |this|.
  const RsaPssParameters* ParamsForRsaPss() const;

 private:
  SignatureAlgorithm(SignatureAlgorithmId algorithm,
                     DigestAlgorithm digest,
                     scoped_ptr<SignatureAlgorithmParameters> params);

  const SignatureAlgorithmId algorithm_;
  const DigestAlgorithm digest_;
  const scoped_ptr<SignatureAlgorithmParameters> params_;

  DISALLOW_COPY_AND_ASSIGN(SignatureAlgorithm);
};

}  // namespace net

#endif  // NET_CERT_INTERNAL_SIGNATURE_ALGORITHM_H_
