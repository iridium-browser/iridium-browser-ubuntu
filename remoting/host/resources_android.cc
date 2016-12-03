// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

namespace remoting {

bool LoadResources(const std::string& pref_locale) {
  // Do nothing since .pak files are not used on Android.
  return false;
}

void UnloadResources() {
}

}  // namespace remoting
