// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_util.h"

#include <stdint.h>

#include <limits>

#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }
  if (!validation_context->IsValidRange(data, sizeof(StructHeader))) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  const StructHeader* header = static_cast<const StructHeader*>(data);

  if (header->num_bytes < sizeof(StructHeader)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  if (!validation_context->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateNonInlinedUnionHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }

  if (!validation_context->ClaimMemory(data, kUnionDataSize) ||
      *static_cast<const uint32_t*>(data) != kUnionDataSize) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      !message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsResponse(const Message* message,
                               ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagExpectsResponse) ||
      !message->has_flag(Message::kFlagIsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input) {
  return IsValidInterfaceId(input.interface_id);
}

bool IsHandleOrInterfaceValid(const AssociatedInterfaceRequest_Data& input) {
  return IsValidInterfaceId(input.interface_id);
}

bool IsHandleOrInterfaceValid(const Interface_Data& input) {
  return input.handle.is_valid();
}

bool IsHandleOrInterfaceValid(const Handle_Data& input) {
  return input.is_valid();
}

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    const char* error_message,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
                        error_message);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterfaceRequest_Data& input,
    const char* error_message,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
                        error_message);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    const char* error_message,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                        error_message);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    const char* error_message,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                        error_message);
  return false;
}

bool ValidateHandleOrInterface(const AssociatedInterface_Data& input,
                               ValidationContext* validation_context) {
  if (!IsMasterInterfaceId(input.interface_id))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

bool ValidateHandleOrInterface(const AssociatedInterfaceRequest_Data& input,
                               ValidationContext* validation_context) {
  if (!IsMasterInterfaceId(input.interface_id))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

bool ValidateHandleOrInterface(const Interface_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input.handle))
    return true;

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

bool ValidateHandleOrInterface(const Handle_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input))
    return true;

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

}  // namespace internal
}  // namespace mojo
