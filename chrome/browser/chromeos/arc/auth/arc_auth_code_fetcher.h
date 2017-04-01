// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_

#include <string>

#include "base/callback.h"

namespace arc {

// Interface to implement auth_code fetching.
class ArcAuthCodeFetcher {
 public:
  virtual ~ArcAuthCodeFetcher() = default;

  // Fetches the |auth_code|. On success, |callback| is called with the
  // fetched |auth_code|. Otherwise, |callback| is called with empty string.
  // Fetch() should be called once par instance, and it is expected that
  // the inflight operation is cancelled without calling the |callback|
  // when the instance is deleted.
  using FetchCallback = base::Callback<void(const std::string& auth_code)>;
  virtual void Fetch(const FetchCallback& callback) = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_AUTH_ARC_AUTH_CODE_FETCHER_H_
