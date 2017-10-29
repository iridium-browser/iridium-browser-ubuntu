// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/proto_utils.h"

#include <limits>

#include "base/sys_byteorder.h"

#define CHECK_PTR_LE(a, b) \
  CHECK_LE(reinterpret_cast<const void*>(a), reinterpret_cast<const void*>(b))
namespace tracing {
namespace v2 {
namespace proto {

const uint8_t* ParseVarInt(const uint8_t* start,
                           const uint8_t* end,
                           uint64_t* value) {
  const uint8_t* pos = start;
  uint64_t shift = 0;
  *value = 0;
  do {
    CHECK_PTR_LE(pos, end - 1);
    DCHECK_LT(shift, 64ull);
    *value |= static_cast<uint64_t>(*pos & 0x7f) << shift;
    shift += 7;
  } while (*pos++ & 0x80);
  return pos;
}

const uint8_t* ParseField(const uint8_t* start,
                          const uint8_t* end,
                          uint32_t* field_id,
                          FieldType* field_type,
                          uint64_t* field_intvalue) {
  // The first byte of a proto field is structured as follows:
  // The least 3 significant bits determine the field type.
  // The most 5 significant bits determine the field id. If MSB == 1, the
  // field id continues on the next bytes following the VarInt encoding.
  const uint8_t kFieldTypeNumBits = 3;
  const uint8_t kFieldTypeMask = (1 << kFieldTypeNumBits) - 1;  // 0000 0111;

  const uint8_t* pos = start;
  CHECK_PTR_LE(pos, end - 1);
  *field_type = static_cast<FieldType>(*pos & kFieldTypeMask);

  uint64_t raw_field_id;
  pos = ParseVarInt(pos, end, &raw_field_id);
  raw_field_id >>= kFieldTypeNumBits;

  DCHECK_LE(raw_field_id, std::numeric_limits<uint32_t>::max());
  *field_id = static_cast<uint32_t>(raw_field_id);

  switch (*field_type) {
    case kFieldTypeFixed64: {
      CHECK_PTR_LE(pos + sizeof(uint64_t), end);
      memcpy(field_intvalue, pos, sizeof(uint64_t));
      *field_intvalue = base::ByteSwapToLE64(*field_intvalue);
      pos += sizeof(uint64_t);
      break;
    }
    case kFieldTypeFixed32: {
      CHECK_PTR_LE(pos + sizeof(uint32_t), end);
      uint32_t tmp;
      memcpy(&tmp, pos, sizeof(uint32_t));
      *field_intvalue = base::ByteSwapToLE32(tmp);
      pos += sizeof(uint32_t);
      break;
    }
    case kFieldTypeVarInt: {
      pos = ParseVarInt(pos, end, field_intvalue);
      break;
    }
    case kFieldTypeLengthDelimited: {
      pos = ParseVarInt(pos, end, field_intvalue);
      pos += *field_intvalue;
      CHECK_PTR_LE(pos, end);
      break;
    }
    default:
      NOTREACHED() << "Unsupported proto field type " << *field_type;
  }
  return pos;
}

}  // namespace proto
}  // namespace v2
}  // namespace tracing
