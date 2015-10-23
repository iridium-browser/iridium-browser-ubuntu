// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_THREAD_IMPL_H_
#define IOS_WEB_WEB_THREAD_IMPL_H_

#include "base/threading/thread.h"
#include "ios/web/public/web_thread.h"

namespace web {

class WebThreadDelegate;

class WebThreadImpl : public WebThread, public base::Thread {
 public:
  // Construct a WebThreadImpl with the supplied identifier.  It is an error
  // to construct a WebThreadImpl that already exists.
  explicit WebThreadImpl(WebThread::ID identifier);

  // Special constructor for the main (UI) thread and unittests. If a
  // |message_loop| is provied, we use a dummy thread here since the main
  // thread already exists.
  WebThreadImpl(WebThread::ID identifier, base::MessageLoop* message_loop);
  ~WebThreadImpl() override;

  static void ShutdownThreadPool();

  // TODO(stuartmorgan): Move this to WebThread (where it belongs) once
  // the alternate BrowserThread-backed-WebThread implementation goes away. See
  // the note in web_thread_delegate.h.
  //
  // Sets the delegate for the specified WebThread.
  //
  // Only one delegate may be registered at a time. Delegates may be
  // unregistered by providing a nullptr pointer.
  //
  // If the caller unregisters a delegate before CleanUp has been
  // called, it must perform its own locking to ensure the delegate is
  // not deleted while unregistering.
  static void SetDelegate(ID identifier, WebThreadDelegate* delegate);

 protected:
  void Init() override;
  void Run(base::MessageLoop* message_loop) override;
  void CleanUp() override;

 private:
  // This class implements all the functionality of the public WebThread
  // functions, but state is stored in the WebThreadImpl to keep
  // the API cleaner. Therefore make WebThread a friend class.
  friend class WebThread;

  // The following are unique function names that makes it possible to tell
  // the thread id from the callstack alone in crash dumps.
  void UIThreadRun(base::MessageLoop* message_loop);
  void DBThreadRun(base::MessageLoop* message_loop);
  void FileThreadRun(base::MessageLoop* message_loop);
  void FileUserBlockingThreadRun(base::MessageLoop* message_loop);
  void CacheThreadRun(base::MessageLoop* message_loop);
  void IOThreadRun(base::MessageLoop* message_loop);

  static bool PostTaskHelper(WebThread::ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task,
                             base::TimeDelta delay,
                             bool nestable);

  // Common initialization code for the constructors.
  void Initialize();

  // Performs cleanup that needs to happen on the IO thread before calling the
  // embedder's CleanUp function.
  void IOThreadPreCleanUp();

  // For testing.
  friend class TestWebThreadBundle;
  friend class TestWebThreadBundleImpl;
  static void FlushThreadPoolHelperForTesting();

  // The identifier of this thread.  Only one thread can exist with a given
  // identifier at a given time.
  ID identifier_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_THREAD_IMPL_H_
