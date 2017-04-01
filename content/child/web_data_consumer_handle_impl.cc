// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_data_consumer_handle_impl.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/c/system/types.h"

namespace content {

using Result = blink::WebDataConsumerHandle::Result;

class WebDataConsumerHandleImpl::Context
    : public base::RefCountedThreadSafe<Context> {
 public:
  explicit Context(Handle handle) : handle_(std::move(handle)) {}

  const Handle& handle() { return handle_; }

 private:
  friend class base::RefCountedThreadSafe<Context>;
  ~Context() {}
  Handle handle_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

WebDataConsumerHandleImpl::ReaderImpl::ReaderImpl(
    scoped_refptr<Context> context,
    Client* client)
    : context_(context), client_(client) {
  if (client_)
    StartWatching();
}

WebDataConsumerHandleImpl::ReaderImpl::~ReaderImpl() {
}

Result WebDataConsumerHandleImpl::ReaderImpl::read(void* data,
                                                   size_t size,
                                                   Flags flags,
                                                   size_t* read_size) {
  // We need this variable definition to avoid a link error.
  const Flags kNone = FlagNone;
  DCHECK_EQ(flags, kNone);
  DCHECK_LE(size, std::numeric_limits<uint32_t>::max());

  *read_size = 0;

  if (!size) {
    // Even if there is unread data available, mojo::ReadDataRaw() returns
    // FAILED_PRECONDITION when |size| is 0 and the producer handle was closed.
    // But in this case, WebDataConsumerHandle::Reader::read() must return Ok.
    // So we use mojo::Wait() with 0 deadline to check whether readable or not.
    MojoResult wait_result = mojo::Wait(
        context_->handle().get(), MOJO_HANDLE_SIGNAL_READABLE, 0, nullptr);
    switch (wait_result) {
      case MOJO_RESULT_OK:
        return Ok;
      case MOJO_RESULT_FAILED_PRECONDITION:
        return Done;
      case MOJO_RESULT_DEADLINE_EXCEEDED:
        return ShouldWait;
      default:
        NOTREACHED();
        return UnexpectedError;
    }
  }

  uint32_t size_to_pass = size;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;
  MojoResult rv = mojo::ReadDataRaw(context_->handle().get(), data,
                                    &size_to_pass, flags_to_pass);
  if (rv == MOJO_RESULT_OK)
    *read_size = size_to_pass;

  return HandleReadResult(rv);
}

Result WebDataConsumerHandleImpl::ReaderImpl::beginRead(const void** buffer,
                                                        Flags flags,
                                                        size_t* available) {
  // We need this variable definition to avoid a link error.
  const Flags kNone = FlagNone;
  DCHECK_EQ(flags, kNone);

  *buffer = nullptr;
  *available = 0;

  uint32_t size_to_pass = 0;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;

  MojoResult rv = mojo::BeginReadDataRaw(context_->handle().get(), buffer,
                                         &size_to_pass, flags_to_pass);
  if (rv == MOJO_RESULT_OK)
    *available = size_to_pass;
  return HandleReadResult(rv);
}

Result WebDataConsumerHandleImpl::ReaderImpl::endRead(size_t read_size) {
  MojoResult rv = mojo::EndReadDataRaw(context_->handle().get(), read_size);
  return rv == MOJO_RESULT_OK ? Ok : UnexpectedError;
}

Result WebDataConsumerHandleImpl::ReaderImpl::HandleReadResult(
    MojoResult mojo_result) {
  switch (mojo_result) {
    case MOJO_RESULT_OK:
      return Ok;
    case MOJO_RESULT_FAILED_PRECONDITION:
      return Done;
    case MOJO_RESULT_BUSY:
      return Busy;
    case MOJO_RESULT_SHOULD_WAIT:
      return ShouldWait;
    case MOJO_RESULT_RESOURCE_EXHAUSTED:
      return ResourceExhausted;
    default:
      return UnexpectedError;
  }
}

void WebDataConsumerHandleImpl::ReaderImpl::StartWatching() {
  handle_watcher_.Start(
      context_->handle().get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::Bind(&ReaderImpl::OnHandleGotReadable, base::Unretained(this)));
}

void WebDataConsumerHandleImpl::ReaderImpl::OnHandleGotReadable(MojoResult) {
  DCHECK(client_);
  client_->didGetReadable();
}

WebDataConsumerHandleImpl::WebDataConsumerHandleImpl(Handle handle)
    : context_(new Context(std::move(handle))) {}

WebDataConsumerHandleImpl::~WebDataConsumerHandleImpl() {
}

std::unique_ptr<blink::WebDataConsumerHandle::Reader>
WebDataConsumerHandleImpl::obtainReader(Client* client) {
  return base::WrapUnique(new ReaderImpl(context_, client));
}

const char* WebDataConsumerHandleImpl::debugName() const {
  return "WebDataConsumerHandleImpl";
}

}  // namespace content
