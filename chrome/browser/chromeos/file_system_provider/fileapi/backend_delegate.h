// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_

#include "base/basictypes.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

namespace chromeos {
namespace file_system_provider {

// Delegate implementation of the some methods in chromeos::FileSystemBackend
// for provided file systems.
class BackendDelegate : public chromeos::FileSystemBackendDelegate {
 public:
  BackendDelegate();
  ~BackendDelegate() override;

  // FileSystemBackend::Delegate overrides.
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64 offset,
      int64 max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) override;
  scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;
  void GetRedirectURLForContents(const storage::FileSystemURL& url,
                                 const storage::URLCallback& callback) override;

 private:
  scoped_ptr<storage::AsyncFileUtil> async_file_util_;
  scoped_ptr<storage::WatcherManager> watcher_manager_;

  DISALLOW_COPY_AND_ASSIGN(BackendDelegate);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_
