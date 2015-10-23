// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/shell_browser_state.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/shell/shell_url_request_context_getter.h"

namespace web {

ShellBrowserState::ShellBrowserState() : BrowserState() {
  CHECK(PathService::Get(base::DIR_APP_DATA, &path_));

  request_context_getter_ = new ShellURLRequestContextGetter(
      GetStatePath(),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::IO),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::CACHE));
}

ShellBrowserState::~ShellBrowserState() {
}

bool ShellBrowserState::IsOffTheRecord() const {
  return false;
}

base::FilePath ShellBrowserState::GetStatePath() const {
  return path_;
}

net::URLRequestContextGetter* ShellBrowserState::GetRequestContext() {
  return request_context_getter_.get();
}

}  // namespace web
