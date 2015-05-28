// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/raw_channel.h"

#include <windows.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/win/windows_version.h"
#include "mojo/edk/embedder/platform_handle.h"

namespace mojo {
namespace system {

namespace {

class VistaOrHigherFunctions {
 public:
  VistaOrHigherFunctions();

  bool is_vista_or_higher() const { return is_vista_or_higher_; }

  BOOL SetFileCompletionNotificationModes(HANDLE handle, UCHAR flags) {
    return set_file_completion_notification_modes_(handle, flags);
  }

  BOOL CancelIoEx(HANDLE handle, LPOVERLAPPED overlapped) {
    return cancel_io_ex_(handle, overlapped);
  }

 private:
  typedef BOOL(WINAPI* SetFileCompletionNotificationModesFunc)(HANDLE, UCHAR);
  typedef BOOL(WINAPI* CancelIoExFunc)(HANDLE, LPOVERLAPPED);

  bool is_vista_or_higher_;
  SetFileCompletionNotificationModesFunc
      set_file_completion_notification_modes_;
  CancelIoExFunc cancel_io_ex_;
};

VistaOrHigherFunctions::VistaOrHigherFunctions()
    : is_vista_or_higher_(base::win::GetVersion() >= base::win::VERSION_VISTA),
      set_file_completion_notification_modes_(nullptr),
      cancel_io_ex_(nullptr) {
  if (!is_vista_or_higher_)
    return;

  HMODULE module = GetModuleHandleW(L"kernel32.dll");
  set_file_completion_notification_modes_ =
      reinterpret_cast<SetFileCompletionNotificationModesFunc>(
          GetProcAddress(module, "SetFileCompletionNotificationModes"));
  DCHECK(set_file_completion_notification_modes_);

  cancel_io_ex_ =
      reinterpret_cast<CancelIoExFunc>(GetProcAddress(module, "CancelIoEx"));
  DCHECK(cancel_io_ex_);
}

base::LazyInstance<VistaOrHigherFunctions> g_vista_or_higher_functions =
    LAZY_INSTANCE_INITIALIZER;

class RawChannelWin : public RawChannel {
 public:
  RawChannelWin(embedder::ScopedPlatformHandle handle);
  ~RawChannelWin() override;

  // |RawChannel| public methods:
  size_t GetSerializedPlatformHandleSize() const override;

 private:
  // RawChannelIOHandler receives OS notifications for I/O completion. It must
  // be created on the I/O thread.
  //
  // It manages its own destruction. Destruction happens on the I/O thread when
  // all the following conditions are satisfied:
  //   - |DetachFromOwnerNoLock()| has been called;
  //   - there is no pending read;
  //   - there is no pending write.
  class RawChannelIOHandler : public base::MessageLoopForIO::IOHandler {
   public:
    RawChannelIOHandler(RawChannelWin* owner,
                        embedder::ScopedPlatformHandle handle);

    HANDLE handle() const { return handle_.get().handle; }

    // The following methods are only called by the owner on the I/O thread.
    bool pending_read() const;
    base::MessageLoopForIO::IOContext* read_context();
    // Instructs the object to wait for an |OnIOCompleted()| notification.
    void OnPendingReadStarted();

    // The following methods are only called by the owner under
    // |owner_->write_lock()|.
    bool pending_write_no_lock() const;
    base::MessageLoopForIO::IOContext* write_context_no_lock();
    // Instructs the object to wait for an |OnIOCompleted()| notification.
    void OnPendingWriteStartedNoLock();

    // |base::MessageLoopForIO::IOHandler| implementation:
    // Must be called on the I/O thread. It could be called before or after
    // detached from the owner.
    void OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                       DWORD bytes_transferred,
                       DWORD error) override;

    // Must be called on the I/O thread under |owner_->write_lock()|.
    // After this call, the owner must not make any further calls on this
    // object, and therefore the object is used on the I/O thread exclusively
    // (if it stays alive).
    void DetachFromOwnerNoLock(scoped_ptr<ReadBuffer> read_buffer,
                               scoped_ptr<WriteBuffer> write_buffer);

   private:
    ~RawChannelIOHandler() override;

    // Returns true if |owner_| has been reset and there is not pending read or
    // write.
    // Must be called on the I/O thread.
    bool ShouldSelfDestruct() const;

    // Must be called on the I/O thread. It may be called before or after
    // detaching from the owner.
    void OnReadCompleted(DWORD bytes_read, DWORD error);
    // Must be called on the I/O thread. It may be called before or after
    // detaching from the owner.
    void OnWriteCompleted(DWORD bytes_written, DWORD error);

    embedder::ScopedPlatformHandle handle_;

    // |owner_| is reset on the I/O thread under |owner_->write_lock()|.
    // Therefore, it may be used on any thread under lock; or on the I/O thread
    // without locking.
    RawChannelWin* owner_;

    // The following members must be used on the I/O thread.
    scoped_ptr<ReadBuffer> preserved_read_buffer_after_detach_;
    scoped_ptr<WriteBuffer> preserved_write_buffer_after_detach_;
    bool suppress_self_destruct_;

    bool pending_read_;
    base::MessageLoopForIO::IOContext read_context_;

    // The following members must be used under |owner_->write_lock()| while the
    // object is still attached to the owner, and only on the I/O thread
    // afterwards.
    bool pending_write_;
    base::MessageLoopForIO::IOContext write_context_;

    DISALLOW_COPY_AND_ASSIGN(RawChannelIOHandler);
  };

  // |RawChannel| private methods:
  IOResult Read(size_t* bytes_read) override;
  IOResult ScheduleRead() override;
  embedder::ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_platform_handles,
      const void* platform_handle_table) override;
  IOResult WriteNoLock(size_t* platform_handles_written,
                       size_t* bytes_written) override;
  IOResult ScheduleWriteNoLock() override;
  void OnInit() override;
  void OnShutdownNoLock(scoped_ptr<ReadBuffer> read_buffer,
                        scoped_ptr<WriteBuffer> write_buffer) override;

  // Passed to |io_handler_| during initialization.
  embedder::ScopedPlatformHandle handle_;

  RawChannelIOHandler* io_handler_;

  const bool skip_completion_port_on_success_;

  DISALLOW_COPY_AND_ASSIGN(RawChannelWin);
};

RawChannelWin::RawChannelIOHandler::RawChannelIOHandler(
    RawChannelWin* owner,
    embedder::ScopedPlatformHandle handle)
    : handle_(handle.Pass()),
      owner_(owner),
      suppress_self_destruct_(false),
      pending_read_(false),
      pending_write_(false) {
  memset(&read_context_.overlapped, 0, sizeof(read_context_.overlapped));
  read_context_.handler = this;
  memset(&write_context_.overlapped, 0, sizeof(write_context_.overlapped));
  write_context_.handler = this;

  owner_->message_loop_for_io()->RegisterIOHandler(handle_.get().handle, this);
}

RawChannelWin::RawChannelIOHandler::~RawChannelIOHandler() {
  DCHECK(ShouldSelfDestruct());
}

bool RawChannelWin::RawChannelIOHandler::pending_read() const {
  DCHECK(owner_);
  DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
  return pending_read_;
}

base::MessageLoopForIO::IOContext*
RawChannelWin::RawChannelIOHandler::read_context() {
  DCHECK(owner_);
  DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
  return &read_context_;
}

void RawChannelWin::RawChannelIOHandler::OnPendingReadStarted() {
  DCHECK(owner_);
  DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
  DCHECK(!pending_read_);
  pending_read_ = true;
}

bool RawChannelWin::RawChannelIOHandler::pending_write_no_lock() const {
  DCHECK(owner_);
  owner_->write_lock().AssertAcquired();
  return pending_write_;
}

base::MessageLoopForIO::IOContext*
RawChannelWin::RawChannelIOHandler::write_context_no_lock() {
  DCHECK(owner_);
  owner_->write_lock().AssertAcquired();
  return &write_context_;
}

void RawChannelWin::RawChannelIOHandler::OnPendingWriteStartedNoLock() {
  DCHECK(owner_);
  owner_->write_lock().AssertAcquired();
  DCHECK(!pending_write_);
  pending_write_ = true;
}

void RawChannelWin::RawChannelIOHandler::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(!owner_ ||
         base::MessageLoop::current() == owner_->message_loop_for_io());

  {
    // Suppress self-destruction inside |OnReadCompleted()|, etc. (in case they
    // result in a call to |Shutdown()|).
    base::AutoReset<bool> resetter(&suppress_self_destruct_, true);

    if (context == &read_context_)
      OnReadCompleted(bytes_transferred, error);
    else if (context == &write_context_)
      OnWriteCompleted(bytes_transferred, error);
    else
      NOTREACHED();
  }

  if (ShouldSelfDestruct())
    delete this;
}

void RawChannelWin::RawChannelIOHandler::DetachFromOwnerNoLock(
    scoped_ptr<ReadBuffer> read_buffer,
    scoped_ptr<WriteBuffer> write_buffer) {
  DCHECK(owner_);
  DCHECK_EQ(base::MessageLoop::current(), owner_->message_loop_for_io());
  owner_->write_lock().AssertAcquired();

  // If read/write is pending, we have to retain the corresponding buffer.
  if (pending_read_)
    preserved_read_buffer_after_detach_ = read_buffer.Pass();
  if (pending_write_)
    preserved_write_buffer_after_detach_ = write_buffer.Pass();

  owner_ = nullptr;
  if (ShouldSelfDestruct())
    delete this;
}

bool RawChannelWin::RawChannelIOHandler::ShouldSelfDestruct() const {
  if (owner_ || suppress_self_destruct_)
    return false;

  // Note: Detached, hence no lock needed for |pending_write_|.
  return !pending_read_ && !pending_write_;
}

void RawChannelWin::RawChannelIOHandler::OnReadCompleted(DWORD bytes_read,
                                                         DWORD error) {
  DCHECK(!owner_ ||
         base::MessageLoop::current() == owner_->message_loop_for_io());
  DCHECK(suppress_self_destruct_);

  CHECK(pending_read_);
  pending_read_ = false;
  if (!owner_)
    return;

  // Note: |OnReadCompleted()| may detach us from |owner_|.
  if (error == ERROR_SUCCESS) {
    DCHECK_GT(bytes_read, 0u);
    owner_->OnReadCompleted(IO_SUCCEEDED, bytes_read);
  } else if (error == ERROR_BROKEN_PIPE) {
    DCHECK_EQ(bytes_read, 0u);
    owner_->OnReadCompleted(IO_FAILED_SHUTDOWN, 0);
  } else {
    DCHECK_EQ(bytes_read, 0u);
    LOG(WARNING) << "ReadFile: " << logging::SystemErrorCodeToString(error);
    owner_->OnReadCompleted(IO_FAILED_UNKNOWN, 0);
  }
}

void RawChannelWin::RawChannelIOHandler::OnWriteCompleted(DWORD bytes_written,
                                                          DWORD error) {
  DCHECK(!owner_ ||
         base::MessageLoop::current() == owner_->message_loop_for_io());
  DCHECK(suppress_self_destruct_);

  if (!owner_) {
    // No lock needed.
    CHECK(pending_write_);
    pending_write_ = false;
    return;
  }

  {
    base::AutoLock locker(owner_->write_lock());
    CHECK(pending_write_);
    pending_write_ = false;
  }

  // Note: |OnWriteCompleted()| may detach us from |owner_|.
  if (error == ERROR_SUCCESS) {
    owner_->OnWriteCompleted(IO_SUCCEEDED, 0, bytes_written);
  } else if (error == ERROR_BROKEN_PIPE) {
    owner_->OnWriteCompleted(IO_FAILED_SHUTDOWN, 0, 0);
  } else {
    LOG(WARNING) << "WriteFile: " << logging::SystemErrorCodeToString(error);
    owner_->OnWriteCompleted(IO_FAILED_UNKNOWN, 0, 0);
  }
}

RawChannelWin::RawChannelWin(embedder::ScopedPlatformHandle handle)
    : handle_(handle.Pass()),
      io_handler_(nullptr),
      skip_completion_port_on_success_(
          g_vista_or_higher_functions.Get().is_vista_or_higher()) {
  DCHECK(handle_.is_valid());
}

RawChannelWin::~RawChannelWin() {
  DCHECK(!io_handler_);
}

size_t RawChannelWin::GetSerializedPlatformHandleSize() const {
  // TODO(vtl): Implement.
  return 0;
}

RawChannel::IOResult RawChannelWin::Read(size_t* bytes_read) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  DCHECK(io_handler_);
  DCHECK(!io_handler_->pending_read());

  char* buffer = nullptr;
  size_t bytes_to_read = 0;
  read_buffer()->GetBuffer(&buffer, &bytes_to_read);

  BOOL result =
      ReadFile(io_handler_->handle(), buffer, static_cast<DWORD>(bytes_to_read),
               nullptr, &io_handler_->read_context()->overlapped);
  if (!result) {
    DWORD error = GetLastError();
    if (error == ERROR_BROKEN_PIPE)
      return IO_FAILED_SHUTDOWN;
    if (error != ERROR_IO_PENDING) {
      LOG(WARNING) << "ReadFile: " << logging::SystemErrorCodeToString(error);
      return IO_FAILED_UNKNOWN;
    }
  }

  if (result && skip_completion_port_on_success_) {
    DWORD bytes_read_dword = 0;
    BOOL get_size_result = GetOverlappedResult(
        io_handler_->handle(), &io_handler_->read_context()->overlapped,
        &bytes_read_dword, FALSE);
    DPCHECK(get_size_result);
    *bytes_read = bytes_read_dword;
    return IO_SUCCEEDED;
  }

  // If the read is pending or the read has succeeded but we don't skip
  // completion port on success, instruct |io_handler_| to wait for the
  // completion packet.
  //
  // TODO(yzshen): It seems there isn't document saying that all error cases
  // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
  // packet. If we do get one for errors, |RawChannelIOHandler::OnIOCompleted()|
  // will crash so we will learn about it.

  io_handler_->OnPendingReadStarted();
  return IO_PENDING;
}

RawChannel::IOResult RawChannelWin::ScheduleRead() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  DCHECK(io_handler_);
  DCHECK(!io_handler_->pending_read());

  size_t bytes_read = 0;
  IOResult io_result = Read(&bytes_read);
  if (io_result == IO_SUCCEEDED) {
    DCHECK(skip_completion_port_on_success_);

    // We have finished reading successfully. Queue a notification manually.
    io_handler_->OnPendingReadStarted();
    // |io_handler_| won't go away before the task is run, so it is safe to use
    // |base::Unretained()|.
    message_loop_for_io()->PostTask(
        FROM_HERE, base::Bind(&RawChannelIOHandler::OnIOCompleted,
                              base::Unretained(io_handler_),
                              base::Unretained(io_handler_->read_context()),
                              static_cast<DWORD>(bytes_read), ERROR_SUCCESS));
    return IO_PENDING;
  }

  return io_result;
}

embedder::ScopedPlatformHandleVectorPtr RawChannelWin::GetReadPlatformHandles(
    size_t num_platform_handles,
    const void* platform_handle_table) {
  // TODO(vtl): Implement.
  NOTIMPLEMENTED();
  return embedder::ScopedPlatformHandleVectorPtr();
}

RawChannel::IOResult RawChannelWin::WriteNoLock(
    size_t* platform_handles_written,
    size_t* bytes_written) {
  write_lock().AssertAcquired();

  DCHECK(io_handler_);
  DCHECK(!io_handler_->pending_write_no_lock());

  if (write_buffer_no_lock()->HavePlatformHandlesToSend()) {
    // TODO(vtl): Implement.
    NOTIMPLEMENTED();
  }

  std::vector<WriteBuffer::Buffer> buffers;
  write_buffer_no_lock()->GetBuffers(&buffers);
  DCHECK(!buffers.empty());

  // TODO(yzshen): Handle multi-segment writes more efficiently.
  DWORD bytes_written_dword = 0;
  BOOL result =
      WriteFile(io_handler_->handle(), buffers[0].addr,
                static_cast<DWORD>(buffers[0].size), &bytes_written_dword,
                &io_handler_->write_context_no_lock()->overlapped);
  if (!result) {
    DWORD error = GetLastError();
    if (error == ERROR_BROKEN_PIPE)
      return IO_FAILED_SHUTDOWN;
    if (error != ERROR_IO_PENDING) {
      LOG(WARNING) << "WriteFile: " << logging::SystemErrorCodeToString(error);
      return IO_FAILED_UNKNOWN;
    }
  }

  if (result && skip_completion_port_on_success_) {
    *platform_handles_written = 0;
    *bytes_written = bytes_written_dword;
    return IO_SUCCEEDED;
  }

  // If the write is pending or the write has succeeded but we don't skip
  // completion port on success, instruct |io_handler_| to wait for the
  // completion packet.
  //
  // TODO(yzshen): it seems there isn't document saying that all error cases
  // (other than ERROR_IO_PENDING) are guaranteed to *not* queue a completion
  // packet. If we do get one for errors, |RawChannelIOHandler::OnIOCompleted()|
  // will crash so we will learn about it.

  io_handler_->OnPendingWriteStartedNoLock();
  return IO_PENDING;
}

RawChannel::IOResult RawChannelWin::ScheduleWriteNoLock() {
  write_lock().AssertAcquired();

  DCHECK(io_handler_);
  DCHECK(!io_handler_->pending_write_no_lock());

  // TODO(vtl): Do something with |platform_handles_written|.
  size_t platform_handles_written = 0;
  size_t bytes_written = 0;
  IOResult io_result = WriteNoLock(&platform_handles_written, &bytes_written);
  if (io_result == IO_SUCCEEDED) {
    DCHECK(skip_completion_port_on_success_);

    // We have finished writing successfully. Queue a notification manually.
    io_handler_->OnPendingWriteStartedNoLock();
    // |io_handler_| won't go away before that task is run, so it is safe to use
    // |base::Unretained()|.
    message_loop_for_io()->PostTask(
        FROM_HERE,
        base::Bind(&RawChannelIOHandler::OnIOCompleted,
                   base::Unretained(io_handler_),
                   base::Unretained(io_handler_->write_context_no_lock()),
                   static_cast<DWORD>(bytes_written), ERROR_SUCCESS));
    return IO_PENDING;
  }

  return io_result;
}

void RawChannelWin::OnInit() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());

  DCHECK(handle_.is_valid());
  if (skip_completion_port_on_success_) {
    // I don't know how this can fail (unless |handle_| is bad, in which case
    // it's a bug in our code).
    CHECK(g_vista_or_higher_functions.Get().SetFileCompletionNotificationModes(
        handle_.get().handle, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS));
  }

  DCHECK(!io_handler_);
  io_handler_ = new RawChannelIOHandler(this, handle_.Pass());
}

void RawChannelWin::OnShutdownNoLock(scoped_ptr<ReadBuffer> read_buffer,
                                     scoped_ptr<WriteBuffer> write_buffer) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io());
  DCHECK(io_handler_);

  write_lock().AssertAcquired();

  if (io_handler_->pending_read() || io_handler_->pending_write_no_lock()) {
    // |io_handler_| will be alive until pending read/write (if any) completes.
    // Call |CancelIoEx()| or |CancelIo()| so that resources can be freed up as
    // soon as possible.
    // Note: |CancelIo()| only cancels read/write requests started from this
    // thread.
    if (g_vista_or_higher_functions.Get().is_vista_or_higher()) {
      g_vista_or_higher_functions.Get().CancelIoEx(io_handler_->handle(),
                                                   nullptr);
    } else {
      CancelIo(io_handler_->handle());
    }
  }

  io_handler_->DetachFromOwnerNoLock(read_buffer.Pass(), write_buffer.Pass());
  io_handler_ = nullptr;
}

}  // namespace

// -----------------------------------------------------------------------------

// Static factory method declared in raw_channel.h.
// static
scoped_ptr<RawChannel> RawChannel::Create(
    embedder::ScopedPlatformHandle handle) {
  return make_scoped_ptr(new RawChannelWin(handle.Pass()));
}

}  // namespace system
}  // namespace mojo
