// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_KASKO_CLIENT_H_
#define CHROME_APP_KASKO_CLIENT_H_

#if defined(KASKO)

#include "base/macros.h"
#include "syzygy/kasko/api/minidump_type.h"

class ChromeWatcherClient;

// Manages the lifetime of Chrome's Kasko client, which permits crash reporting
// via Kasko. Only a single instance of this class may be instantiated at any
// time, and it must only be destroyed when the process is single-threaded.
class KaskoClient {
 public:
  // Initializes a Kasko client that will communicate with the Kasko reporter
  // hosted by the Chrome watcher process managed by |chrome_watcher_client|.
  // All generated reports will use |minidump_type|.
  KaskoClient(ChromeWatcherClient* chrome_watcher_client,
              kasko::api::MinidumpType minidump_type);
  ~KaskoClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(KaskoClient);
};

#endif  // KASKO

#endif  // CHROME_APP_KASKO_CLIENT_H_
