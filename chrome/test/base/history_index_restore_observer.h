// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_HISTORY_INDEX_RESTORE_OBSERVER_H_
#define CHROME_TEST_BASE_HISTORY_INDEX_RESTORE_OBSERVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "components/omnibox/browser/in_memory_url_index.h"

// HistoryIndexRestoreObserver is used when blocking until the InMemoryURLIndex
// finishes restoring. As soon as the InMemoryURLIndex finishes restoring the
// provided Closure is invoked.
class HistoryIndexRestoreObserver
    : public InMemoryURLIndex::RestoreCacheObserver {
 public:
  explicit HistoryIndexRestoreObserver(const base::Closure& task);
  ~HistoryIndexRestoreObserver() override;

  bool succeeded() const { return succeeded_; }

  // RestoreCacheObserver implementation.
  void OnCacheRestoreFinished(bool success) override;

 private:
  base::Closure task_;
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(HistoryIndexRestoreObserver);
};

#endif  // CHROME_TEST_BASE_HISTORY_INDEX_RESTORE_OBSERVER_H_
