// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_api_handler.h"

SafeBrowsingApiHandler* SafeBrowsingApiHandler::instance_ = NULL;

// static
void SafeBrowsingApiHandler::SetInstance(SafeBrowsingApiHandler* instance) {
  instance_ = instance;
}

// static
SafeBrowsingApiHandler* SafeBrowsingApiHandler::GetInstance() {
  return instance_;
}
