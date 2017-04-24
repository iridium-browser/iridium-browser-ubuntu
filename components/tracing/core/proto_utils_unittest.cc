// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_utils.h"

#include <limits>

#include "base/logging.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {
namespace proto {
namespace {

struct VarIntExpectation {
  const char* encoded;
  size_t encoded_size;
  uint64_t int_value;
};

const VarIntExpectation kVarIntExpectations[] = {
    {"\x00", 1, 0},
    {"\x01", 1, 0x1},
    {"\x7f", 1, 0x7F},
    {"\xFF\x01", 2, 0xFF},
    {"\xFF\x7F", 2, 0x3FFF},
    {"\x80\x80\x01", 3, 0x4000},
    {"\xFF\xFF\x7F", 3, 0x1FFFFF},
    {"\x80\x80\x80\x01", 4, 0x200000},
    {"\xFF\xFF\xFF\x7F", 4, 0xFFFFFFF},
    {"\x80\x80\x80\x80\x01", 5, 0x10000000},
    {"\xFF\xFF\xFF\xFF\x0F", 5, 0xFFFFFFFF},
    {"\x80\x80\x80\x80\x10", 5, 0x100000000},
    {"\xFF\xFF\xFF\xFF\x7F", 5, 0x7FFFFFFFF},
    {"\x80\x80\x80\x80\x80\x01", 6, 0x800000000},
    {"\xFF\xFF\xFF\xFF\xFF\x7F", 6, 0x3FFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x01", 7, 0x40000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 7, 0x1FFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x01", 8, 0x2000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 8, 0xFFFFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x80\x01", 9, 0x100000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 9, 0x7FFFFFFFFFFFFFFF},
    {"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x01", 10, 0x8000000000000000},
    {"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x01", 10, 0xFFFFFFFFFFFFFFFF},
};

struct FieldExpectation {
  const char* encoded;
  size_t encoded_size;
  uint32_t id;
  FieldType type;
  uint64_t int_value;
};

const FieldExpectation kFieldExpectations[] = {
    {"\x08\x00", 2, 1, kFieldTypeVarInt, 0},
    {"\x08\x42", 2, 1, kFieldTypeVarInt, 0x42},
    {"\xF8\x07\x42", 3, 127, kFieldTypeVarInt, 0x42},
    {"\x90\x4D\xFF\xFF\xFF\xFF\x0F", 7, 1234, kFieldTypeVarInt, 0xFFFFFFFF},
    {"\x7D\x42\x00\x00\x00", 5, 15, kFieldTypeFixed32, 0x42},
    {"\x95\x4D\x78\x56\x34\x12", 6, 1234, kFieldTypeFixed32, 0x12345678},
    {"\x79\x42\x00\x00\x00\x00\x00\x00\x00", 9, 15, kFieldTypeFixed64, 0x42},
    {"\x91\x4D\x08\x07\x06\x05\x04\x03\x02\x01", 10, 1234, kFieldTypeFixed64,
     0x0102030405060708},
    {"\x0A\x00", 2, 1, kFieldTypeLengthDelimited, 0},
    {"\x0A\x04|abc", 6, 1, kFieldTypeLengthDelimited, 4},
    {"\x92\x4D\x04|abc", 7, 1234, kFieldTypeLengthDelimited, 4},
    {"\x92\x4D\x83\x01|abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcd"
     "efghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx",
     135, 1234, kFieldTypeLengthDelimited, 131},
};

TEST(ProtoUtilsTest, FieldPreambleEncoding) {
  // According to C++ standard, right shift of negative value has
  // implementation-defined resulting value.
  if ((static_cast<int32_t>(0x80000000u) >> 31) != -1)
    FAIL() << "Platform has unsupported negative number format or arithmetic";

  EXPECT_EQ(0x08u, MakeTagVarInt(1));
  EXPECT_EQ(0x09u, MakeTagFixed<uint64_t>(1));
  EXPECT_EQ(0x0Au, MakeTagLengthDelimited(1));
  EXPECT_EQ(0x0Du, MakeTagFixed<uint32_t>(1));

  EXPECT_EQ(0x03F8u, MakeTagVarInt(0x7F));
  EXPECT_EQ(0x03F9u, MakeTagFixed<int64_t>(0x7F));
  EXPECT_EQ(0x03FAu, MakeTagLengthDelimited(0x7F));
  EXPECT_EQ(0x03FDu, MakeTagFixed<int32_t>(0x7F));

  EXPECT_EQ(0x0400u, MakeTagVarInt(0x80));
  EXPECT_EQ(0x0401u, MakeTagFixed<double>(0x80));
  EXPECT_EQ(0x0402u, MakeTagLengthDelimited(0x80));
  EXPECT_EQ(0x0405u, MakeTagFixed<float>(0x80));

  EXPECT_EQ(0x01FFF8u, MakeTagVarInt(0x3fff));
  EXPECT_EQ(0x01FFF9u, MakeTagFixed<int64_t>(0x3fff));
  EXPECT_EQ(0x01FFFAu, MakeTagLengthDelimited(0x3fff));
  EXPECT_EQ(0x01FFFDu, MakeTagFixed<int32_t>(0x3fff));

  EXPECT_EQ(0x020000u, MakeTagVarInt(0x4000));
  EXPECT_EQ(0x020001u, MakeTagFixed<int64_t>(0x4000));
  EXPECT_EQ(0x020002u, MakeTagLengthDelimited(0x4000));
  EXPECT_EQ(0x020005u, MakeTagFixed<int32_t>(0x4000));
}

TEST(ProtoUtilsTest, ZigZagEncoding) {
  EXPECT_EQ(0u, ZigZagEncode(0));
  EXPECT_EQ(1u, ZigZagEncode(-1));
  EXPECT_EQ(2u, ZigZagEncode(1));
  EXPECT_EQ(3u, ZigZagEncode(-2));
  EXPECT_EQ(4294967293u, ZigZagEncode(-2147483647));
  EXPECT_EQ(4294967294u, ZigZagEncode(2147483647));
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            ZigZagEncode(std::numeric_limits<int32_t>::min()));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            ZigZagEncode(std::numeric_limits<int64_t>::min()));
}

TEST(ProtoUtilsTest, VarIntEncoding) {
  for (size_t i = 0; i < arraysize(kVarIntExpectations); ++i) {
    const VarIntExpectation& exp = kVarIntExpectations[i];
    uint8_t buf[32];
    uint8_t* res = WriteVarInt<uint64_t>(exp.int_value, buf);
    ASSERT_EQ(exp.encoded_size, static_cast<size_t>(res - buf));
    ASSERT_EQ(0, memcmp(buf, exp.encoded, exp.encoded_size));

    if (exp.int_value <= std::numeric_limits<uint32_t>::max()) {
      uint8_t* res = WriteVarInt<uint32_t>(exp.int_value, buf);
      ASSERT_EQ(exp.encoded_size, static_cast<size_t>(res - buf));
      ASSERT_EQ(0, memcmp(buf, exp.encoded, exp.encoded_size));
    }
  }
}

TEST(ProtoUtilsTest, RedundantVarIntEncoding) {
  uint8_t buf[kMessageLengthFieldSize];

  WriteRedundantVarInt(0, buf);
  EXPECT_EQ(0, memcmp("\x80\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(1, buf);
  EXPECT_EQ(0, memcmp("\x81\x80\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(0x80, buf);
  EXPECT_EQ(0, memcmp("\x80\x81\x80\x00", buf, sizeof(buf)));

  WriteRedundantVarInt(0x332211, buf);
  EXPECT_EQ(0, memcmp("\x91\xC4\xCC\x01", buf, sizeof(buf)));

  // Largest allowed length.
  WriteRedundantVarInt(0x0FFFFFFF, buf);
  EXPECT_EQ(0, memcmp("\xFF\xFF\xFF\x7F", buf, sizeof(buf)));
}

TEST(ProtoUtilsTest, VarIntDecoding) {
  for (size_t i = 0; i < arraysize(kVarIntExpectations); ++i) {
    const VarIntExpectation& exp = kVarIntExpectations[i];
    uint64_t value = std::numeric_limits<uint64_t>::max();
    const uint8_t* res = ParseVarInt(
        reinterpret_cast<const uint8_t*>(exp.encoded),
        reinterpret_cast<const uint8_t*>(exp.encoded + exp.encoded_size),
        &value);
    ASSERT_EQ(reinterpret_cast<const void*>(exp.encoded + exp.encoded_size),
              reinterpret_cast<const void*>(res));
    ASSERT_EQ(exp.int_value, value);
  }
}

TEST(ProtoUtilsTest, FieldDecoding) {
  for (size_t i = 0; i < arraysize(kFieldExpectations); ++i) {
    const FieldExpectation& exp = kFieldExpectations[i];
    FieldType field_type = kFieldTypeVarInt;
    uint32_t field_id = std::numeric_limits<uint32_t>::max();
    uint64_t field_intvalue = std::numeric_limits<uint64_t>::max();
    const uint8_t* res = ParseField(
        reinterpret_cast<const uint8_t*>(exp.encoded),
        reinterpret_cast<const uint8_t*>(exp.encoded + exp.encoded_size),
        &field_id, &field_type, &field_intvalue);
    ASSERT_EQ(reinterpret_cast<const void*>(exp.encoded + exp.encoded_size),
              reinterpret_cast<const void*>(res));
    ASSERT_EQ(exp.id, field_id);
    ASSERT_EQ(exp.type, field_type);
    ASSERT_EQ(exp.int_value, field_intvalue);
  }
}

}  // namespace
}  // namespace proto
}  // namespace v2
}  // namespace tracing
