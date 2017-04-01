/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SyncCallbackHelper_h
#define SyncCallbackHelper_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/fileapi/FileError.h"
#include "core/html/VoidCallback.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/EntriesCallback.h"
#include "modules/filesystem/EntryCallback.h"
#include "modules/filesystem/EntrySync.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/MetadataCallback.h"
#include "platform/heap/Handle.h"

namespace blink {

// A helper template for FileSystemSync implementation.
template <typename SuccessCallback, typename CallbackArg, typename ResultType>
class SyncCallbackHelper final
    : public GarbageCollected<
          SyncCallbackHelper<SuccessCallback, CallbackArg, ResultType>> {
 public:
  typedef SyncCallbackHelper<SuccessCallback, CallbackArg, ResultType>
      HelperType;

  static HelperType* create() { return new SyncCallbackHelper(); }

  ResultType* getResult(ExceptionState& exceptionState) {
    if (m_errorCode)
      FileError::throwDOMException(exceptionState, m_errorCode);

    return m_result;
  }

  SuccessCallback* getSuccessCallback() {
    return SuccessCallbackImpl::create(this);
  }
  ErrorCallbackBase* getErrorCallback() {
    return ErrorCallbackImpl::create(this);
  }

  DEFINE_INLINE_TRACE() { visitor->trace(m_result); }

 private:
  SyncCallbackHelper() : m_errorCode(FileError::kOK), m_completed(false) {}

  class SuccessCallbackImpl final : public SuccessCallback {
   public:
    static SuccessCallbackImpl* create(HelperType* helper) {
      return new SuccessCallbackImpl(helper);
    }

    virtual void handleEvent() { m_helper->setError(FileError::kOK); }

    virtual void handleEvent(CallbackArg arg) { m_helper->setResult(arg); }

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_helper);
      SuccessCallback::trace(visitor);
    }

   private:
    explicit SuccessCallbackImpl(HelperType* helper) : m_helper(helper) {}
    Member<HelperType> m_helper;
  };

  class ErrorCallbackImpl final : public ErrorCallbackBase {
   public:
    static ErrorCallbackImpl* create(HelperType* helper) {
      return new ErrorCallbackImpl(helper);
    }

    void invoke(FileError::ErrorCode error) override {
      m_helper->setError(error);
    }

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_helper);
      ErrorCallbackBase::trace(visitor);
    }

   private:
    explicit ErrorCallbackImpl(HelperType* helper) : m_helper(helper) {}
    Member<HelperType> m_helper;
  };

  void setError(FileError::ErrorCode error) {
    m_errorCode = error;
    m_completed = true;
  }

  void setResult(CallbackArg result) {
    m_result = ResultType::create(result);
    m_completed = true;
  }

  Member<ResultType> m_result;
  FileError::ErrorCode m_errorCode;
  bool m_completed;
};

struct EmptyType : public GarbageCollected<EmptyType> {
  static EmptyType* create(EmptyType*) { return 0; }

  DEFINE_INLINE_TRACE() {}
};

typedef SyncCallbackHelper<EntryCallback, Entry*, EntrySync>
    EntrySyncCallbackHelper;
typedef SyncCallbackHelper<MetadataCallback, Metadata*, Metadata>
    MetadataSyncCallbackHelper;
typedef SyncCallbackHelper<VoidCallback, EmptyType*, EmptyType>
    VoidSyncCallbackHelper;
typedef SyncCallbackHelper<FileSystemCallback,
                           DOMFileSystem*,
                           DOMFileSystemSync>
    FileSystemSyncCallbackHelper;

}  // namespace blink

#endif  // SyncCallbackHelper_h
