// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/mock_image_data_fetcher.h"

namespace web {

MockImageDataFetcher::MockImageDataFetcher(
    const scoped_refptr<base::TaskRunner>& task_runner)
    : ImageDataFetcher(task_runner) {}

MockImageDataFetcher::~MockImageDataFetcher() {}

}  // namespace web
