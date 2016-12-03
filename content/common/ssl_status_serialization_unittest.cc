// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/ssl_status_serialization.h"

#include "net/ssl/ssl_connection_status_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SetTestStatus(SSLStatus* status) {
  status->security_style = SECURITY_STYLE_AUTHENTICATED;
  status->cert_id = 1;
  status->cert_status = net::CERT_STATUS_DATE_INVALID;
  status->security_bits = 80;
  status->key_exchange_info = 23;
  status->connection_status = net::SSL_CONNECTION_VERSION_TLS1_2;
  status->sct_statuses.push_back(net::ct::SCT_STATUS_OK);
}

bool SSLStatusAreEqual(const SSLStatus& a, const SSLStatus &b) {
  return a.Equals(b);
}

} // namespace

std::ostream& operator<<(std::ostream& os, const SSLStatus& status) {
  return os << "Security Style: " << status.security_style
            << "\nCert ID: " << status.cert_id
            << "\nCert Status: " << status.cert_status
            << "\nSecurity bits: " << status.security_bits
            << "\nKey exchange info: " << status.key_exchange_info
            << "\nConnection status: " << status.connection_status
            << "\nContent Status: " << status.content_status
            << "\nNumber of SCTs: " << status.sct_statuses.size();
}

// Test that a valid serialized SSLStatus returns true on
// deserialization and deserializes correctly.
TEST(SSLStatusSerializationTest, DeserializeSerializedStatus) {
  // Serialize dummy data and test that it deserializes properly.
  SSLStatus status;
  SetTestStatus(&status);
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, status, deserialized);
  // Test that |content_status| has the default (initialized) value.
  EXPECT_EQ(SSLStatus::NORMAL_CONTENT, deserialized.content_status);
}

// Test that an invalid serialized SSLStatus returns false on
// deserialization.
TEST(SSLStatusSerializationTest, DeserializeBogusStatus) {
  // Test that a failure to deserialize returns false and returns
  // initialized, default data.
  SSLStatus invalid_deserialized;
  ASSERT_FALSE(
      DeserializeSecurityInfo("not an SSLStatus", &invalid_deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, SSLStatus(), invalid_deserialized);
}

// Serialize a status with a bad |security_bits| value and test that
// deserializing it fails.
TEST(SSLStatusSerializationTest, DeserializeBogusSecurityBits) {
  SSLStatus status;
  SetTestStatus(&status);
  // |security_bits| must be <-1. (-1 means the strength is unknown, and
  // |0 means the connection is not encrypted).
  status.security_bits = -5;
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus invalid_deserialized;
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &invalid_deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, SSLStatus(), invalid_deserialized);
}

// Serialize a status with a bad |key_exchange_info| value and test that
// deserializing it fails.
TEST(SSLStatusSerializationTest, DeserializeBogusKeyExchangeInfo) {
  SSLStatus status;
  SetTestStatus(&status);
  status.key_exchange_info = -1;

  SSLStatus invalid_deserialized;
  std::string serialized = SerializeSecurityInfo(status);
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &invalid_deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, SSLStatus(), invalid_deserialized);
}

// Serialize a status with a bad |security_style| value and test that
// deserializing it fails.
TEST(SSLStatusSerializationTest, DeserializeBogusSecurityStyle) {
  SSLStatus status;
  SetTestStatus(&status);
  status.security_style = static_cast<SecurityStyle>(100);
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus invalid_deserialized;
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &invalid_deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, SSLStatus(), invalid_deserialized);
}

// Serialize a status with an empty |sct_statuses| field and test that
// deserializing works.
TEST(SSLStatusSerializationTest, DeserializeEmptySCTStatuses) {
  SSLStatus status;
  SetTestStatus(&status);
  status.sct_statuses.clear();
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, status, deserialized);
}

// Serialize a status with multiple different |sct_statuses| and test that
// deserializing works.
TEST(SSLStatusSerializationTest, DeserializeMultipleSCTStatuses) {
  SSLStatus status;
  SetTestStatus(&status);
  status.sct_statuses.push_back(net::ct::SCT_STATUS_LOG_UNKNOWN);
  status.sct_statuses.push_back(net::ct::SCT_STATUS_LOG_UNKNOWN);
  status.sct_statuses.push_back(net::ct::SCT_STATUS_OK);
  status.sct_statuses.push_back(net::ct::SCT_STATUS_INVALID_SIGNATURE);
  status.sct_statuses.push_back(net::ct::SCT_STATUS_INVALID_TIMESTAMP);
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, status, deserialized);
}

// Serialize a status with a bad SCTVerifyStatus value and test that
// deserializing it fails.
TEST(SSLStatusSerializationTest, DeserializeBogusSCTVerifyStatus) {
  SSLStatus status;
  SetTestStatus(&status);
  status.sct_statuses.push_back(static_cast<net::ct::SCTVerifyStatus>(100));
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_FALSE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, SSLStatus(), deserialized);
}

// Test that SCTVerifyStatus INVALID can be deserialized; even though
// this value is deprecated, it may still appear in previously written
// disk cache entries. Regression test for https://crbug.com/640296
TEST(SSLStatusSerializationTest, DeserializeInvalidSCT) {
  SSLStatus status;
  SetTestStatus(&status);
  status.sct_statuses.push_back(
      static_cast<net::ct::SCTVerifyStatus>(net::ct::SCT_STATUS_INVALID));
  std::string serialized = SerializeSecurityInfo(status);

  SSLStatus deserialized;
  ASSERT_TRUE(DeserializeSecurityInfo(serialized, &deserialized));
  EXPECT_PRED2(SSLStatusAreEqual, status, deserialized);
}

}  // namespace
