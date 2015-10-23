// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"

namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  LOG(ERROR) << "Terminating renderer for bad IPC message, reason " << reason;
  UMA_HISTOGRAM_SPARSE_SLOWLY("Stability.BadMessageTerminated.Chrome", reason);
}

}  // namespace

void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  LogBadMessage(reason);
  host->ShutdownForBadMessage();
}

void ReceivedBadMessage(content::BrowserMessageFilter* filter,
                        BadMessageReason reason) {
  LogBadMessage(reason);
  filter->ShutdownForBadMessage();
}

}  // namespace bad_message
