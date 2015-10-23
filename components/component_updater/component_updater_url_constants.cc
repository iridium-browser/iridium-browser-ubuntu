// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_url_constants.h"

namespace component_updater {

// The alternative URL for the v3 protocol service endpoint.
const char kUpdaterAltUrl[] = "http://clients2.google.com/service/update2";

// The default URL for the v3 protocol service endpoint. In some cases, the
// component updater is allowed to fall back to and alternate URL source, if
// the request to the default URL source fails.
// The value of |kDefaultUrlSource| can be overridden with
// --component-updater=url-source=someurl.
const char kUpdaterDefaultUrl[] = "https://clients2.google.com/service/update2";

}  // namespace component_updater
