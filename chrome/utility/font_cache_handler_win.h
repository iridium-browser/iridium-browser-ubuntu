// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_FONT_CACHE_HANDLER_WIN_H_
#define CHROME_UTILITY_FONT_CACHE_HANDLER_WIN_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/utility/utility_message_handler.h"

namespace base {
class TaskRunner;
class Thread;
}

// Handles requests to build a static direct write font cache. Must be invoked
// in a non-sandboxed utility process. We build static font cache in utility
// process as it is time consuming as well as crash prone thing. We already
// have fall back of loading fonts from system fonts directory in place, so even
// if we fail to build static cache in utility process, chrome will still
// continue to run as is.
class FontCacheHandler : public UtilityMessageHandler {
 public:
  FontCacheHandler();
  ~FontCacheHandler() override;

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void OnBuildFontCache(const base::FilePath& full_path);
  void StartBuildingFontCache(const base::FilePath& full_path);
  void Cleanup();

  scoped_refptr<base::TaskRunner> utility_task_runner_;

  // Thread that caching process runs on, while FontCacheHandler handles
  // messages from the browser process.
  scoped_ptr<base::Thread> cache_thread_;

  DISALLOW_COPY_AND_ASSIGN(FontCacheHandler);
};

#endif  // CHROME_UTILITY_FONT_CACHE_HANDLER_WIN_H_
