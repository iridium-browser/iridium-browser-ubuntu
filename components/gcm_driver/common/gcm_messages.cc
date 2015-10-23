// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/common/gcm_messages.h"

namespace gcm {

// static
const int OutgoingMessage::kMaximumTTL = 24 * 60 * 60;  // 1 day.

OutgoingMessage::OutgoingMessage() : time_to_live(kMaximumTTL) {
}

OutgoingMessage::~OutgoingMessage() {
}

IncomingMessage::IncomingMessage() {
}

IncomingMessage::~IncomingMessage() {
}

}  // namespace gcm
