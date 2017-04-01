// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/pmp_column_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <cstring>
#include <limits>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

namespace picasa {

namespace {

static_assert(sizeof(double) == 8, "double must be 8 bytes long");
const int64_t kPmpMaxFilesize =
    50 * 1024 * 1024;  // Arbitrary maximum of 50 MB.

}  // namespace

PmpColumnReader::PmpColumnReader()
    : length_(0),
      field_type_(PMP_TYPE_INVALID),
      rows_read_(0) {}

PmpColumnReader::~PmpColumnReader() {}

bool PmpColumnReader::ReadFile(base::File* file,
                               const PmpFieldType expected_type) {
  DCHECK(!data_.get());
  base::ThreadRestrictions::AssertIOAllowed();

  if (!file->IsValid())
    return false;

  base::File::Info info;
  if (!file->GetInfo(&info))
    return false;
  length_ = info.size;

  if (length_ < kPmpHeaderSize || length_ > kPmpMaxFilesize)
    return false;

  data_.reset(new uint8_t[length_]);

  char* data_begin = reinterpret_cast<char*>(data_.get());

  DCHECK(length_ <
         std::numeric_limits<int32_t>::max());  // ReadFile expects an int.

  bool success = file->Read(0, data_begin, length_) &&
                 ParseData(expected_type);

  // If any of the reading or parsing fails, prevent Read* calls.
  if (!success)
    rows_read_ = 0;

  return success;
}

bool PmpColumnReader::ReadString(const uint32_t row,
                                 std::string* result) const {
  DCHECK(data_.get() != NULL);

  if (field_type_ != PMP_TYPE_STRING || row >= rows_read_)
    return false;

  DCHECK_LT(row, strings_.size());
  *result = strings_[row];
  return true;
}

bool PmpColumnReader::ReadUInt32(const uint32_t row, uint32_t* result) const {
  DCHECK(data_.get() != NULL);

  if (field_type_ != PMP_TYPE_UINT32 || row >= rows_read_)
    return false;

  *result = reinterpret_cast<uint32_t*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadDouble64(const uint32_t row, double* result) const {
  DCHECK(data_.get() != NULL);

  if (field_type_ != PMP_TYPE_DOUBLE64 || row >= rows_read_)
    return false;

  *result = reinterpret_cast<double*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadUInt8(const uint32_t row, uint8_t* result) const {
  DCHECK(data_.get() != NULL);

  if (field_type_ != PMP_TYPE_UINT8 || row >= rows_read_)
    return false;

  *result = reinterpret_cast<uint8_t*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

bool PmpColumnReader::ReadUInt64(const uint32_t row, uint64_t* result) const {
  DCHECK(data_.get() != NULL);

  if (field_type_ != PMP_TYPE_UINT64 || row >= rows_read_)
    return false;

  *result = reinterpret_cast<uint64_t*>(data_.get() + kPmpHeaderSize)[row];
  return true;
}

uint32_t PmpColumnReader::rows_read() const {
  DCHECK(data_.get() != NULL);
  return rows_read_;
}

bool PmpColumnReader::ParseData(const PmpFieldType expected_type) {
  DCHECK(data_.get() != NULL);
  DCHECK_GE(length_, kPmpHeaderSize);

  // Check all magic bytes.
  if (memcmp(&kPmpMagic1, &data_[kPmpMagic1Offset], sizeof(kPmpMagic1)) != 0 ||
      memcmp(&kPmpMagic2, &data_[kPmpMagic2Offset], sizeof(kPmpMagic2)) != 0 ||
      memcmp(&kPmpMagic3, &data_[kPmpMagic3Offset], sizeof(kPmpMagic3)) != 0 ||
      memcmp(&kPmpMagic4, &data_[kPmpMagic4Offset], sizeof(kPmpMagic4)) != 0) {
    return false;
  }

  uint16_t field_type_data =
      *(reinterpret_cast<uint16_t*>(&data_[kPmpFieldType1Offset]));

  // Verify if field type matches second declaration
  if (field_type_data !=
      *(reinterpret_cast<uint16_t*>(&data_[kPmpFieldType2Offset]))) {
    return false;
  }

  field_type_ = static_cast<PmpFieldType>(field_type_data);

  if (field_type_ != expected_type)
    return false;

  rows_read_ = *(reinterpret_cast<uint32_t*>(&data_[kPmpRowCountOffset]));

  // Sanity check against malicious row field.
  if (rows_read_ > (kPmpMaxFilesize - kPmpHeaderSize))
    return false;

  DCHECK_GE(length_, kPmpHeaderSize);
  int64_t body_length = length_ - kPmpHeaderSize;
  int64_t expected_body_length = 0;
  switch (field_type_) {
    case PMP_TYPE_STRING:
      expected_body_length = IndexStrings();
      break;
    case PMP_TYPE_UINT32:
      expected_body_length =
          static_cast<int64_t>(rows_read_) * sizeof(uint32_t);
      break;
    case PMP_TYPE_DOUBLE64:
      expected_body_length = static_cast<int64_t>(rows_read_) * sizeof(double);
      break;
    case PMP_TYPE_UINT8:
      expected_body_length = static_cast<int64_t>(rows_read_) * sizeof(uint8_t);
      break;
    case PMP_TYPE_UINT64:
      expected_body_length =
          static_cast<int64_t>(rows_read_) * sizeof(uint64_t);
      break;
    default:
      return false;
      break;
  }

  return body_length == expected_body_length;
}

int64_t PmpColumnReader::IndexStrings() {
  DCHECK(data_.get() != NULL);
  DCHECK_GE(length_, kPmpHeaderSize);

  strings_.reserve(rows_read_);

  int64_t bytes_parsed = kPmpHeaderSize;
  const uint8_t* data_cursor = data_.get() + kPmpHeaderSize;

  while (strings_.size() < rows_read_) {
    const uint8_t* string_end = static_cast<const uint8_t*>(
        memchr(data_cursor, '\0', length_ - bytes_parsed));

    // Fail if cannot find null termination. String runs on past file end.
    if (string_end == NULL)
      return -1;

    // Length of string. (+1 to include the termination character).
    ptrdiff_t length_in_bytes = string_end - data_cursor + 1;

    strings_.push_back(reinterpret_cast<const char*>(data_cursor));
    data_cursor += length_in_bytes;
    bytes_parsed += length_in_bytes;
  }

  return bytes_parsed - kPmpHeaderSize;
}

}  // namespace picasa
