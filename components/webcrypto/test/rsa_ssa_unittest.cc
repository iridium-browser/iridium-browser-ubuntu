// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/crypto_data.h"
#include "components/webcrypto/status.h"
#include "components/webcrypto/test/test_helpers.h"
#include "components/webcrypto/webcrypto_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace webcrypto {

namespace {

// Helper for ImportJwkRsaFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkRsaDictionary(base::DictionaryValue* dict) {
  dict->Clear();
  dict->SetString("kty", "RSA");
  dict->SetString("alg", "RS256");
  dict->SetString("use", "sig");
  dict->SetBoolean("ext", false);
  dict->SetString(
      "n",
      "qLOyhK-OtQs4cDSoYPFGxJGfMYdjzWxVmMiuSBGh4KvEx-CwgtaTpef87Wdc9GaFEncsDLxk"
      "p0LGxjD1M8jMcvYq6DPEC_JYQumEu3i9v5fAEH1VvbZi9cTg-rmEXLUUjvc5LdOq_5OuHmtm"
      "e7PUJHYW1PW6ENTP0ibeiNOfFvs");
  dict->SetString("e", "AQAB");
}

TEST(WebCryptoRsaSsaTest, ImportExportSpki) {
  // Passing case: Import a valid RSA key in SPKI format.
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      true, blink::WebCryptoKeyUsageVerify, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
  EXPECT_EQ(kModulusLengthBits,
            key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_BYTES_EQ_HEX(
      "010001",
      CryptoData(key.algorithm().rsaHashedParams()->publicExponent()));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), true,
                      blink::WebCryptoKeyUsageEncrypt, &key));

  // Passing case: Export a previously imported RSA public key in SPKI format
  // and compare to original data.
  std::vector<uint8_t> output;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatSpki, key, &output));
  EXPECT_BYTES_EQ_HEX(kPublicKeySpkiDerHex, output);

  // Failing case: Try to export a previously imported RSA public key in raw
  // format (not allowed for a public key).
  EXPECT_EQ(Status::ErrorUnsupportedExportKeyFormat(),
            ExportKey(blink::WebCryptoKeyFormatRaw, key, &output));

  // Failing case: Try to export a non-extractable key
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      false, blink::WebCryptoKeyUsageVerify, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(Status::ErrorKeyNotExtractable(),
            ExportKey(blink::WebCryptoKeyFormatSpki, key, &output));

  // TODO(eroman): Failing test: Import a SPKI with an unrecognized hash OID
  // TODO(eroman): Failing test: Import a SPKI with invalid algorithm params
  // TODO(eroman): Failing test: Import a SPKI with inconsistent parameters
  // (e.g. SHA-1 in OID, SHA-256 in params)
  // TODO(eroman): Failing test: Import a SPKI for RSA-SSA, but with params
  // as OAEP/PSS
}

TEST(WebCryptoRsaSsaTest, ImportExportPkcs8) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  // Passing case: Import a valid RSA key in PKCS#8 format.
  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, blink::WebCryptoKeyUsageSign, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, key.usages());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
            key.algorithm().rsaHashedParams()->hash().id());
  EXPECT_EQ(kModulusLengthBits,
            key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_BYTES_EQ_HEX(
      "010001",
      CryptoData(key.algorithm().rsaHashedParams()->publicExponent()));

  std::vector<uint8_t> exported_key;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatPkcs8, key, &exported_key));
  EXPECT_BYTES_EQ_HEX(kPrivateKeyPkcs8DerHex, exported_key);

  // Failing case: Import RSA key but provide an inconsistent input algorithm
  // and usage. Several issues here:
  //   * AES-CBC doesn't support PKCS8 key format
  //   * AES-CBC doesn't support "sign" usage
  EXPECT_EQ(Status::ErrorUnsupportedImportKeyFormat(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc), true,
                      blink::WebCryptoKeyUsageSign, &key));
}

// Tests JWK import and export by doing a roundtrip key conversion and ensuring
// it was lossless:
//
//   PKCS8 --> JWK --> PKCS8
TEST(WebCryptoRsaSsaTest, ImportRsaPrivateKeyJwkToPkcs8RoundTrip) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  blink::WebCryptoKey key;
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, blink::WebCryptoKeyUsageSign, &key));

  std::vector<uint8_t> exported_key_jwk;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatJwk, key, &exported_key_jwk));

  // All of the optional parameters (p, q, dp, dq, qi) should be present in the
  // output.
  const char* expected_jwk =
      "{\"alg\":\"RS1\",\"d\":\"M6UEKpCyfU9UUcqbu9C0R3GhAa-IQ0Cu-YhfKku-"
      "kuiUpySsPFaMj5eFOtB8AmbIxqPKCSnx6PESMYhEKfxNmuVf7olqEM5wfD7X5zTkRyejlXRQ"
      "GlMmgxCcKrrKuig8MbS9L1PD7jfjUs7jT55QO9gMBiKtecbc7og1R8ajsyU\",\"dp\":"
      "\"KPoTk4ZVvh-"
      "KFZy6ylpy6hkMMAieGc0nSlVvNsT24Z9VSzTAd3kEJ7vdjdPt4kSDKPOF2Bsw6OQ7L_-"
      "gJ4YZeQ\",\"dq\":\"Gos485j6cSBJiY1_t57gp3ZoeRKZzfoJ78DlB6yyHtdDAe9b_Ui-"
      "RV6utuFnglWCdYCo5OjhQVHRUQqCo_LnKQ\",\"e\":\"AQAB\",\"ext\":true,\"key_"
      "ops\":[\"sign\"],\"kty\":\"RSA\",\"n\":"
      "\"pW5KDnAQF1iaUYfcfqhB0Vby7A42rVKkTf6x5h962ZHYxRBW_-2xYrTA8oOhKoijlN_"
      "1JqtykcuzB86r_OCx39XNlQgJbVsri2311nHvY3fAkhyyPCcKcOJZjm_4nRnxBazC0_"
      "DLNfKSgOE4a29kxO8i4eHyDQzoz_siSb2aITc\",\"p\":\"5-"
      "iUJyCod1Fyc6NWBT6iobwMlKpy1VxuhilrLfyWeUjApyy8zKfqyzVwbgmh31WhU1vZs8w0Fg"
      "s7bc0-2o5kQw\",\"q\":\"tp3KHPfU1-yB51uQ_MqHSrzeEj_"
      "ScAGAqpBHm25I3o1n7ST58Z2FuidYdPVCzSDccj5pYzZKH5QlRSsmmmeZ_Q\",\"qi\":"
      "\"JxVqukEm0kqB86Uoy_sn9WiG-"
      "ECp9uhuF6RLlP6TGVhLjiL93h5aLjvYqluo2FhBlOshkKz4MrhH8To9JKefTQ\"}";

  ASSERT_EQ(CryptoData(std::string(expected_jwk)),
            CryptoData(exported_key_jwk));

  ASSERT_EQ(
      Status::Success(),
      ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(exported_key_jwk),
                CreateRsaHashedImportAlgorithm(
                    blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                    blink::WebCryptoAlgorithmIdSha1),
                true, blink::WebCryptoKeyUsageSign, &key));

  std::vector<uint8_t> exported_key_pkcs8;
  ASSERT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatPkcs8, key,
                                         &exported_key_pkcs8));

  ASSERT_EQ(CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
            CryptoData(exported_key_pkcs8));
}

// Tests importing multiple RSA private keys from JWK, and then exporting to
// PKCS8.
//
// This is a regression test for http://crbug.com/378315, for which importing
// a sequence of keys from JWK could yield the wrong key. The first key would
// be imported correctly, however every key after that would actually import
// the first key.
TEST(WebCryptoRsaSsaTest, ImportMultipleRSAPrivateKeysJwk) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  scoped_ptr<base::ListValue> key_list;
  ASSERT_TRUE(ReadJsonTestFileToList("rsa_private_keys.json", &key_list));

  // For this test to be meaningful the keys MUST be kept alive before importing
  // new keys.
  std::vector<blink::WebCryptoKey> live_keys;

  for (size_t key_index = 0; key_index < key_list->GetSize(); ++key_index) {
    SCOPED_TRACE(key_index);

    base::DictionaryValue* key_values;
    ASSERT_TRUE(key_list->GetDictionary(key_index, &key_values));

    // Get the JWK representation of the key.
    base::DictionaryValue* key_jwk;
    ASSERT_TRUE(key_values->GetDictionary("jwk", &key_jwk));

    // Get the PKCS8 representation of the key.
    std::string pkcs8_hex_string;
    ASSERT_TRUE(key_values->GetString("pkcs8", &pkcs8_hex_string));
    std::vector<uint8_t> pkcs8_bytes = HexStringToBytes(pkcs8_hex_string);

    // Get the modulus length for the key.
    int modulus_length_bits = 0;
    ASSERT_TRUE(key_values->GetInteger("modulusLength", &modulus_length_bits));

    blink::WebCryptoKey private_key;

    // Import the key from JWK.
    ASSERT_EQ(Status::Success(),
              ImportKeyJwkFromDict(
                  *key_jwk, CreateRsaHashedImportAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256),
                  true, blink::WebCryptoKeyUsageSign, &private_key));

    live_keys.push_back(private_key);

    EXPECT_EQ(
        modulus_length_bits,
        static_cast<int>(
            private_key.algorithm().rsaHashedParams()->modulusLengthBits()));

    // Export to PKCS8 and verify that it matches expectation.
    std::vector<uint8_t> exported_key_pkcs8;
    ASSERT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatPkcs8,
                                           private_key, &exported_key_pkcs8));

    EXPECT_BYTES_EQ(pkcs8_bytes, exported_key_pkcs8);
  }
}

// Import an RSA private key using JWK. Next import a JWK containing the same
// modulus, but mismatched parameters for the rest. It should NOT be possible
// that the second import retrieves the first key. See http://crbug.com/378315
// for how that could happen.
TEST(WebCryptoRsaSsaTest, ImportJwkExistingModulusAndInvalid) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  scoped_ptr<base::ListValue> key_list;
  ASSERT_TRUE(ReadJsonTestFileToList("rsa_private_keys.json", &key_list));

  // Import a 1024-bit private key.
  base::DictionaryValue* key1_props;
  ASSERT_TRUE(key_list->GetDictionary(1, &key1_props));
  base::DictionaryValue* key1_jwk;
  ASSERT_TRUE(key1_props->GetDictionary("jwk", &key1_jwk));

  blink::WebCryptoKey key1;
  ASSERT_EQ(Status::Success(),
            ImportKeyJwkFromDict(*key1_jwk,
                                 CreateRsaHashedImportAlgorithm(
                                     blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256),
                                 true, blink::WebCryptoKeyUsageSign, &key1));

  ASSERT_EQ(1024u, key1.algorithm().rsaHashedParams()->modulusLengthBits());

  // Construct a JWK using the modulus of key1, but all the other fields from
  // another key (also a 1024-bit private key).
  base::DictionaryValue* key2_props;
  ASSERT_TRUE(key_list->GetDictionary(5, &key2_props));
  base::DictionaryValue* key2_jwk;
  ASSERT_TRUE(key2_props->GetDictionary("jwk", &key2_jwk));
  std::string modulus;
  key1_jwk->GetString("n", &modulus);
  key2_jwk->SetString("n", modulus);

  // This should fail, as the n,e,d parameters are not consistent. It MUST NOT
  // somehow return the key created earlier.
  blink::WebCryptoKey key2;
  ASSERT_EQ(Status::OperationError(),
            ImportKeyJwkFromDict(*key2_jwk,
                                 CreateRsaHashedImportAlgorithm(
                                     blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256),
                                 true, blink::WebCryptoKeyUsageSign, &key2));
}

TEST(WebCryptoRsaSsaTest, GenerateKeyPairRsa) {
  // Note: using unrealistic short key lengths here to avoid bogging down tests.

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation (sha256)
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");
  blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, modulus_length, public_exponent);
  bool extractable = true;
  const blink::WebCryptoKeyUsageMask public_usages =
      blink::WebCryptoKeyUsageVerify;
  const blink::WebCryptoKeyUsageMask private_usages =
      blink::WebCryptoKeyUsageSign;
  const blink::WebCryptoKeyUsageMask usages = public_usages | private_usages;
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, extractable, usages,
                                               &public_key, &private_key));
  ASSERT_FALSE(public_key.isNull());
  ASSERT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(modulus_length,
            public_key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_EQ(modulus_length,
            private_key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            public_key.algorithm().rsaHashedParams()->hash().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            private_key.algorithm().rsaHashedParams()->hash().id());
  EXPECT_TRUE(public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(public_usages, public_key.usages());
  EXPECT_EQ(private_usages, private_key.usages());

  // Try exporting the generated key pair, and then re-importing to verify that
  // the exported data was valid.
  std::vector<uint8_t> public_key_spki;
  EXPECT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatSpki,
                                         public_key, &public_key_spki));

  if (SupportsRsaPrivateKeyImport()) {
    public_key = blink::WebCryptoKey::createNull();
    ASSERT_EQ(
        Status::Success(),
        ImportKey(blink::WebCryptoKeyFormatSpki, CryptoData(public_key_spki),
                  CreateRsaHashedImportAlgorithm(
                      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                      blink::WebCryptoAlgorithmIdSha256),
                  true, public_usages, &public_key));
    EXPECT_EQ(modulus_length,
              public_key.algorithm().rsaHashedParams()->modulusLengthBits());

    std::vector<uint8_t> private_key_pkcs8;
    EXPECT_EQ(Status::Success(), ExportKey(blink::WebCryptoKeyFormatPkcs8,
                                           private_key, &private_key_pkcs8));
    private_key = blink::WebCryptoKey::createNull();
    ASSERT_EQ(
        Status::Success(),
        ImportKey(blink::WebCryptoKeyFormatPkcs8, CryptoData(private_key_pkcs8),
                  CreateRsaHashedImportAlgorithm(
                      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                      blink::WebCryptoAlgorithmIdSha256),
                  true, private_usages, &private_key));
    EXPECT_EQ(modulus_length,
              private_key.algorithm().rsaHashedParams()->modulusLengthBits());
  }

  // Fail with bad modulus.
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, 0, public_exponent);
  EXPECT_EQ(Status::ErrorGenerateRsaUnsupportedModulus(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned int exponent_length = sizeof(unsigned long) + 1;  // NOLINT
  const std::vector<uint8_t> long_exponent(exponent_length, 0x01);
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, modulus_length, long_exponent);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8_t> empty_exponent;
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, modulus_length, empty_exponent);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8_t> exponent_with_leading_zeros(15, 0x00);
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, modulus_length,
      exponent_with_leading_zeros);
  EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
            GenerateKeyPair(algorithm, extractable, usages, &public_key,
                            &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha256, modulus_length,
      exponent_with_leading_zeros);
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, extractable, usages,
                                               &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_TRUE(public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(public_usages, public_key.usages());
  EXPECT_EQ(private_usages, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation (sha1)
  algorithm = CreateRsaHashedKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha1, modulus_length, public_exponent);
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, false, usages,
                                               &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_EQ(modulus_length,
            public_key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_EQ(modulus_length,
            private_key.algorithm().rsaHashedParams()->modulusLengthBits());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
            public_key.algorithm().rsaHashedParams()->hash().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
            private_key.algorithm().rsaHashedParams()->hash().id());
  // Even though "extractable" was set to false, the public key remains
  // extractable.
  EXPECT_TRUE(public_key.extractable());
  EXPECT_FALSE(private_key.extractable());
  EXPECT_EQ(public_usages, public_key.usages());
  EXPECT_EQ(private_usages, private_key.usages());

  // Exporting a private key as SPKI format doesn't make sense. However this
  // will first fail because the key is not extractable.
  std::vector<uint8_t> output;
  EXPECT_EQ(Status::ErrorKeyNotExtractable(),
            ExportKey(blink::WebCryptoKeyFormatSpki, private_key, &output));

  // Re-generate an extractable private_key and try to export it as SPKI format.
  // This should fail since spki is for public keys.
  EXPECT_EQ(Status::Success(), GenerateKeyPair(algorithm, true, usages,
                                               &public_key, &private_key));
  EXPECT_EQ(Status::ErrorUnexpectedKeyType(),
            ExportKey(blink::WebCryptoKeyFormatSpki, private_key, &output));
}

TEST(WebCryptoRsaSsaTest, GenerateKeyPairRsaBadModulusLength) {
  const unsigned int kBadModulusBits[] = {
      0,
      248,         // Too small.
      257,         // Not a multiple of 8.
      1023,        // Not a multiple of 8.
      0xFFFFFFFF,  // Too big.
      16384 + 8,   // 16384 is the maxmimum length that NSS succeeds for.
  };

  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  for (size_t i = 0; i < arraysize(kBadModulusBits); ++i) {
    const unsigned int modulus_length_bits = kBadModulusBits[i];
    blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
        blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
        blink::WebCryptoAlgorithmIdSha256, modulus_length_bits,
        public_exponent);
    bool extractable = true;
    const blink::WebCryptoKeyUsageMask usages = blink::WebCryptoKeyUsageSign;
    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    EXPECT_EQ(Status::ErrorGenerateRsaUnsupportedModulus(),
              GenerateKeyPair(algorithm, extractable, usages, &public_key,
                              &private_key));
  }
}

// Try generating RSA key pairs using unsupported public exponents. Only
// exponents of 3 and 65537 are supported. While both OpenSSL and NSS can
// support other values, OpenSSL hangs when given invalid exponents, so use a
// whitelist to validate the parameters.
TEST(WebCryptoRsaSsaTest, GenerateKeyPairRsaBadExponent) {
  const unsigned int modulus_length = 1024;

  const char* const kPublicExponents[] = {
      "11",  // 17 - This is a valid public exponent, but currently disallowed.
      "00",
      "01",
      "02",
      "010000",  // 65536
  };

  for (size_t i = 0; i < arraysize(kPublicExponents); ++i) {
    SCOPED_TRACE(i);
    blink::WebCryptoAlgorithm algorithm = CreateRsaHashedKeyGenAlgorithm(
        blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
        blink::WebCryptoAlgorithmIdSha256, modulus_length,
        HexStringToBytes(kPublicExponents[i]));

    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    EXPECT_EQ(Status::ErrorGenerateKeyPublicExponent(),
              GenerateKeyPair(algorithm, true, blink::WebCryptoKeyUsageSign,
                              &public_key, &private_key));
  }
}

TEST(WebCryptoRsaSsaTest, SignVerifyFailures) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  // Import a key pair.
  blink::WebCryptoAlgorithm import_algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::WebCryptoKeyUsageVerify, blink::WebCryptoKeyUsageSign, &public_key,
      &private_key));

  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5);

  std::vector<uint8_t> signature;
  bool signature_match;

  // Compute a signature.
  const std::vector<uint8_t> data = HexStringToBytes("010203040506070809");
  ASSERT_EQ(Status::Success(),
            Sign(algorithm, private_key, CryptoData(data), &signature));

  // Ensure truncated signature does not verify by passing one less byte.
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key,
                   CryptoData(vector_as_array(&signature),
                              static_cast<unsigned int>(signature.size()) - 1),
                   CryptoData(data), &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure truncated signature does not verify by passing no bytes.
  EXPECT_EQ(Status::Success(), Verify(algorithm, public_key, CryptoData(),
                                      CryptoData(data), &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure corrupted signature does not verify.
  std::vector<uint8_t> corrupt_sig = signature;
  corrupt_sig[corrupt_sig.size() / 2] ^= 0x1;
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key, CryptoData(corrupt_sig),
                   CryptoData(data), &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure signatures that are greater than the modulus size fail.
  const unsigned int long_message_size_bytes = 1024;
  DCHECK_GT(long_message_size_bytes, kModulusLengthBits / 8);
  const unsigned char kLongSignature[long_message_size_bytes] = {0};
  EXPECT_EQ(Status::Success(),
            Verify(algorithm, public_key,
                   CryptoData(kLongSignature, sizeof(kLongSignature)),
                   CryptoData(data), &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure that signing and verifying with an incompatible algorithm fails.
  algorithm = CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaOaep);

  EXPECT_EQ(Status::ErrorUnexpected(),
            Sign(algorithm, private_key, CryptoData(data), &signature));
  EXPECT_EQ(Status::ErrorUnexpected(),
            Verify(algorithm, public_key, CryptoData(signature),
                   CryptoData(data), &signature_match));

  // Some crypto libraries (NSS) can automatically select the RSA SSA inner hash
  // based solely on the contents of the input signature data. In the Web Crypto
  // implementation, the inner hash should be specified uniquely by the key
  // algorithm parameter. To validate this behavior, call Verify with a computed
  // signature that used one hash type (SHA-1), but pass in a key with a
  // different inner hash type (SHA-256). If the hash type is determined by the
  // signature itself (undesired), the verify will pass, while if the hash type
  // is specified by the key algorithm (desired), the verify will fail.

  // Compute a signature using SHA-1 as the inner hash.
  EXPECT_EQ(Status::Success(),
            Sign(CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
                 private_key, CryptoData(data), &signature));

  blink::WebCryptoKey public_key_256;
  EXPECT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      true, blink::WebCryptoKeyUsageVerify, &public_key_256));

  // Now verify using an algorithm whose inner hash is SHA-256, not SHA-1. The
  // signature should not verify.
  // NOTE: public_key was produced by generateKey, and so its associated
  // algorithm has WebCryptoRsaKeyGenParams and not WebCryptoRsaSsaParams. Thus
  // it has no inner hash to conflict with the input algorithm.
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha1,
            private_key.algorithm().rsaHashedParams()->hash().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            public_key_256.algorithm().rsaHashedParams()->hash().id());

  bool is_match;
  EXPECT_EQ(Status::Success(),
            Verify(CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
                   public_key_256, CryptoData(signature), CryptoData(data),
                   &is_match));
  EXPECT_FALSE(is_match);
}

TEST(WebCryptoRsaSsaTest, SignVerifyKnownAnswer) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("pkcs1v15_sign.json", &tests));

  // Import the key pair.
  blink::WebCryptoAlgorithm import_algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;
  ASSERT_NO_FATAL_FAILURE(ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex), import_algorithm, false,
      blink::WebCryptoKeyUsageVerify, blink::WebCryptoKeyUsageSign, &public_key,
      &private_key));

  blink::WebCryptoAlgorithm algorithm =
      CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5);

  // Validate the signatures are computed and verified as expected.
  std::vector<uint8_t> signature;
  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    std::vector<uint8_t> test_message =
        GetBytesFromHexString(test, "message_hex");
    std::vector<uint8_t> test_signature =
        GetBytesFromHexString(test, "signature_hex");

    signature.clear();
    ASSERT_EQ(Status::Success(), Sign(algorithm, private_key,
                                      CryptoData(test_message), &signature));
    EXPECT_BYTES_EQ(test_signature, signature);

    bool is_match = false;
    ASSERT_EQ(Status::Success(),
              Verify(algorithm, public_key, CryptoData(test_signature),
                     CryptoData(test_message), &is_match));
    EXPECT_TRUE(is_match);
  }
}

// Try importing an RSA-SSA public key with unsupported key usages using SPKI
// format. RSA-SSA public keys only support the 'verify' usage.
TEST(WebCryptoRsaSsaTest, ImportRsaSsaPublicKeyBadUsage_SPKI) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256);

  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      blink::WebCryptoKeyUsageEncrypt,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
  };

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKey(blink::WebCryptoKeyFormatSpki,
                        CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                        algorithm, false, bad_usages[i], &public_key));
  }
}

// Try importing an RSA-SSA public key with unsupported key usages using JWK
// format. RSA-SSA public keys only support the 'verify' usage.
TEST(WebCryptoRsaSsaTest, ImportRsaSsaPublicKeyBadUsage_JWK) {
  const blink::WebCryptoAlgorithm algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256);

  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageSign,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      blink::WebCryptoKeyUsageEncrypt,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
  };

  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  dict.Remove("use", NULL);
  dict.SetString("alg", "RS256");

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              ImportKeyJwkFromDict(dict, algorithm, false, bad_usages[i],
                                   &public_key));
  }
}

// Generate an RSA-SSA key pair with invalid usages. RSA-SSA supports:
//   'sign', 'verify'
TEST(WebCryptoRsaSsaTest, GenerateKeyBadUsages) {
  blink::WebCryptoKeyUsageMask bad_usages[] = {
      blink::WebCryptoKeyUsageDecrypt,
      blink::WebCryptoKeyUsageVerify | blink::WebCryptoKeyUsageDecrypt,
      blink::WebCryptoKeyUsageWrapKey,
  };

  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  for (size_t i = 0; i < arraysize(bad_usages); ++i) {
    SCOPED_TRACE(i);

    blink::WebCryptoKey public_key;
    blink::WebCryptoKey private_key;

    ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
              GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                  blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                  blink::WebCryptoAlgorithmIdSha256,
                                  modulus_length, public_exponent),
                              true, bad_usages[i], &public_key, &private_key));
  }
}

// Generate an RSA-SSA key pair. The public and private keys should select the
// key usages which are applicable, and not have the exact same usages as was
// specified to GenerateKey
TEST(WebCryptoRsaSsaTest, GenerateKeyPairIntersectUsages) {
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, blink::WebCryptoKeyUsageSign |
                                      blink::WebCryptoKeyUsageVerify,
                            &public_key, &private_key));

  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, public_key.usages());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, private_key.usages());

  // Try again but this time without the Verify usages.
  ASSERT_EQ(Status::Success(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, blink::WebCryptoKeyUsageSign, &public_key,
                            &private_key));

  EXPECT_EQ(0, public_key.usages());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, private_key.usages());
}

TEST(WebCryptoRsaSsaTest, GenerateKeyPairEmptyUsages) {
  const unsigned int modulus_length = 256;
  const std::vector<uint8_t> public_exponent = HexStringToBytes("010001");

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            GenerateKeyPair(CreateRsaHashedKeyGenAlgorithm(
                                blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                blink::WebCryptoAlgorithmIdSha256,
                                modulus_length, public_exponent),
                            true, 0, &public_key, &private_key));
}

TEST(WebCryptoRsaSsaTest, ImportKeyEmptyUsages) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  blink::WebCryptoKey public_key;
  blink::WebCryptoKey private_key;

  // Public without usage does not throw an error.
  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatSpki,
                      CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      true, 0, &public_key));
  EXPECT_EQ(0, public_key.usages());

  // Private empty usage will throw an error.
  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::WebCryptoKeyFormatPkcs8,
                      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, 0, &private_key));

  std::vector<uint8_t> public_jwk;
  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatJwk, public_key, &public_jwk));

  ASSERT_EQ(Status::Success(),
            ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(public_jwk),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      true, 0, &public_key));
  EXPECT_EQ(0, public_key.usages());

  // With correct usage to get correct imported private_key
  std::vector<uint8_t> private_jwk;
  ImportKey(
      blink::WebCryptoKeyFormatPkcs8,
      CryptoData(HexStringToBytes(kPrivateKeyPkcs8DerHex)),
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha1),
      true, blink::WebCryptoKeyUsageSign, &private_key);

  ASSERT_EQ(Status::Success(),
            ExportKey(blink::WebCryptoKeyFormatJwk, private_key, &private_jwk));

  ASSERT_EQ(Status::ErrorCreateKeyEmptyUsages(),
            ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(private_jwk),
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha1),
                      true, 0, &private_key));
}

TEST(WebCryptoRsaSsaTest, ImportExportJwkRsaPublicKey) {
  struct TestCase {
    const blink::WebCryptoAlgorithmId hash;
    const blink::WebCryptoKeyUsageMask usage;
    const char* const jwk_alg;
  };
  const TestCase kTests[] = {
      {blink::WebCryptoAlgorithmIdSha1, blink::WebCryptoKeyUsageVerify, "RS1"},
      {blink::WebCryptoAlgorithmIdSha256,
       blink::WebCryptoKeyUsageVerify,
       "RS256"},
      {blink::WebCryptoAlgorithmIdSha384,
       blink::WebCryptoKeyUsageVerify,
       "RS384"},
      {blink::WebCryptoAlgorithmIdSha512,
       blink::WebCryptoKeyUsageVerify,
       "RS512"}};

  for (size_t test_index = 0; test_index < arraysize(kTests); ++test_index) {
    SCOPED_TRACE(test_index);
    const TestCase& test = kTests[test_index];

    const blink::WebCryptoAlgorithm import_algorithm =
        CreateRsaHashedImportAlgorithm(
            blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, test.hash);

    // Import the spki to create a public key
    blink::WebCryptoKey public_key;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatSpki,
                        CryptoData(HexStringToBytes(kPublicKeySpkiDerHex)),
                        import_algorithm, true, test.usage, &public_key));

    // Export the public key as JWK and verify its contents
    std::vector<uint8_t> jwk;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatJwk, public_key, &jwk));
    EXPECT_TRUE(VerifyPublicJwk(jwk, test.jwk_alg, kPublicKeyModulusHex,
                                kPublicKeyExponentHex, test.usage));

    // Import the JWK back in to create a new key
    blink::WebCryptoKey public_key2;
    ASSERT_EQ(Status::Success(),
              ImportKey(blink::WebCryptoKeyFormatJwk, CryptoData(jwk),
                        import_algorithm, true, test.usage, &public_key2));
    ASSERT_TRUE(public_key2.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key2.type());
    EXPECT_TRUE(public_key2.extractable());
    EXPECT_EQ(import_algorithm.id(), public_key2.algorithm().id());

    // Export the new key as spki and compare to the original.
    std::vector<uint8_t> spki;
    ASSERT_EQ(Status::Success(),
              ExportKey(blink::WebCryptoKeyFormatSpki, public_key2, &spki));
    EXPECT_BYTES_EQ_HEX(kPublicKeySpkiDerHex, CryptoData(spki));
  }
}

TEST(WebCryptoRsaSsaTest, ImportJwkRsaFailures) {
  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm =
      CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                     blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usages = blink::WebCryptoKeyUsageVerify;
  blink::WebCryptoKey key;

  // An RSA public key JWK _must_ have an "n" (modulus) and an "e" (exponent)
  // entry, while an RSA private key must have those plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.

  // Baseline pass.
  EXPECT_EQ(Status::Success(),
            ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());

  // The following are specific failure cases for when kty = "RSA".

  // Fail if either "n" or "e" is not present or malformed.
  const std::string kKtyParmName[] = {"n", "e"};
  for (size_t idx = 0; idx < arraysize(kKtyParmName); ++idx) {
    // Fail on missing parameter.
    dict.Remove(kKtyParmName[idx], NULL);
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on bad b64 parameter encoding.
    dict.SetString(kKtyParmName[idx], "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_NE(Status::Success(),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on empty parameter.
    dict.SetString(kKtyParmName[idx], "");
    EXPECT_EQ(Status::ErrorJwkEmptyBigInteger(kKtyParmName[idx]),
              ImportKeyJwkFromDict(dict, algorithm, false, usages, &key));
    RestoreJwkRsaDictionary(&dict);
  }
}

// Try importing an RSA-SSA key from JWK format, having specified both Sign and
// Verify usage, and an invalid JWK.
//
// The test must fail with a usage error BEFORE attempting to read the JWK data.
// Although both Sign and Verify are valid usages for RSA-SSA keys, it is
// invalid to have them both at the same time for one key (since Sign applies to
// private keys, whereas Verify applies to public keys).
//
// If the implementation does not fail fast, this test will crash dereferencing
// invalid memory.
TEST(WebCryptoRsaSsaTest, ImportRsaSsaJwkBadUsageFailFast) {
  CryptoData bad_data(NULL, 128);  // Invalid buffer of length 128.

  blink::WebCryptoKey key;
  ASSERT_EQ(Status::ErrorCreateKeyBadUsages(),
            ImportKey(blink::WebCryptoKeyFormatJwk, bad_data,
                      CreateRsaHashedImportAlgorithm(
                          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                          blink::WebCryptoAlgorithmIdSha256),
                      true, blink::WebCryptoKeyUsageVerify |
                                blink::WebCryptoKeyUsageSign,
                      &key));
}

// Imports invalid JWK/SPKI/PKCS8 data and verifies that it fails as expected.
TEST(WebCryptoRsaSsaTest, ImportInvalidKeyData) {
  if (!SupportsRsaPrivateKeyImport())
    return;

  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("bad_rsa_keys.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    const base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoKeyFormat key_format = GetKeyFormatFromJsonTestCase(test);
    std::vector<uint8_t> key_data =
        GetKeyDataFromJsonTestCase(test, key_format);
    std::string test_error;
    ASSERT_TRUE(test->GetString("error", &test_error));

    blink::WebCryptoKeyUsageMask usages = blink::WebCryptoKeyUsageSign;
    if (key_format == blink::WebCryptoKeyFormatSpki)
      usages = blink::WebCryptoKeyUsageVerify;
    blink::WebCryptoKey key;
    Status status = ImportKey(key_format, CryptoData(key_data),
                              CreateRsaHashedImportAlgorithm(
                                  blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                                  blink::WebCryptoAlgorithmIdSha256),
                              true, usages, &key);
    EXPECT_EQ(test_error, StatusToString(status));
  }
}

}  // namespace

}  // namespace webcrypto
