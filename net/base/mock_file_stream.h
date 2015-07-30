// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines MockFileStream, a test class for FileStream.

#ifndef NET_BASE_MOCK_FILE_STREAM_H_
#define NET_BASE_MOCK_FILE_STREAM_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

class IOBuffer;

namespace testing {

class MockFileStream : public FileStream {
 public:
  explicit MockFileStream(const scoped_refptr<base::TaskRunner>& task_runner);
  MockFileStream(base::File file,
                 const scoped_refptr<base::TaskRunner>& task_runner);
  ~MockFileStream() override;

  // FileStream methods.
  int Seek(base::File::Whence whence,
           int64 offset,
           const Int64CompletionCallback& callback) override;
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int Flush(const CompletionCallback& callback) override;

  void set_forced_error_async(int error) {
    forced_error_ = error;
    async_error_ = true;
  }
  void set_forced_error(int error) {
    forced_error_ = error;
    async_error_ = false;
  }
  void clear_forced_error() {
    forced_error_ = OK;
    async_error_ = false;
  }
  int forced_error() const { return forced_error_; }
  const base::FilePath& get_path() const { return path_; }

  // Throttles all asynchronous callbacks, including forced errors, until a
  // matching ReleaseCallbacks call.
  void ThrottleCallbacks();

  // Resumes running asynchronous callbacks and runs any throttled callbacks.
  void ReleaseCallbacks();

 private:
  int ReturnError(int function_error) {
    if (forced_error_ != OK) {
      int ret = forced_error_;
      clear_forced_error();
      return ret;
    }

    return function_error;
  }

  int64 ReturnError64(int64 function_error) {
    if (forced_error_ != OK) {
      int64 ret = forced_error_;
      clear_forced_error();
      return ret;
    }

    return function_error;
  }

  // Wrappers for callbacks to make them honor ThrottleCallbacks and
  // ReleaseCallbacks.
  void DoCallback(const CompletionCallback& callback, int result);
  void DoCallback64(const Int64CompletionCallback& callback, int64 result);

  // Depending on |async_error_|, either synchronously returns |forced_error_|
  // asynchronously calls |callback| with |async_error_|.
  int ErrorCallback(const CompletionCallback& callback);
  int64 ErrorCallback64(const Int64CompletionCallback& callback);

  int forced_error_;
  bool async_error_;
  bool throttled_;
  base::Closure throttled_task_;
  base::FilePath path_;

  base::WeakPtrFactory<MockFileStream> weak_factory_;
};

}  // namespace testing

}  // namespace net

#endif  // NET_BASE_MOCK_FILE_STREAM_H_
