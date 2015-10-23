// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CRASHPAD_UTIL_WIN_EXCEPTION_HANDLER_SERVER_H_
#define CRASHPAD_UTIL_WIN_EXCEPTION_HANDLER_SERVER_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "util/win/address_types.h"
#include "util/win/scoped_handle.h"

namespace crashpad {

namespace internal {
class PipeServiceContext;
class ClientData;
}  // namespace internal

//! \brief Runs the main exception-handling server in Crashpad's handler
//!     process.
class ExceptionHandlerServer {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    //! \brief Called when the server has created the named pipe connection
    //!     points and is ready to service requests.
    virtual void ExceptionHandlerServerStarted() = 0;

    //! \brief Called when the client has signalled that it has encountered an
    //!     exception and so wants a crash dump to be taken.
    //!
    //! \param[in] process A handle to the client process. Ownership of the
    //!     lifetime of this handle is not passed to the delegate.
    //! \param[in] exception_information_address The address in the client's
    //!     address space of an ExceptionInformation structure.
    //! \return The exit code that should be used when terminating the client
    //!     process.
    virtual unsigned int ExceptionHandlerServerException(
        HANDLE process,
        WinVMAddress exception_information_address) = 0;
  };

  //! \brief Constructs the exception handling server.
  ExceptionHandlerServer();
  ~ExceptionHandlerServer();

  //! \brief Runs the exception-handling server.
  //!
  //! \param[in] delegate The interface to which the exceptions are delegated
  //!     when they are caught in Run(). Ownership is not transferred.
  //! \param[in] pipe_name The name of the pipe to listen on. Must be of the
  //!     form "\\.\pipe\<some_name>".
  void Run(Delegate* delegate, const std::string& pipe_name);

  //! \brief Stops the exception-handling server. Returns immediately. The
  //!     object must not be destroyed until Run() returns.
  void Stop();

 private:
  static bool ServiceClientConnection(
      const internal::PipeServiceContext& service_context);
  static DWORD __stdcall PipeServiceProc(void* ctx);
  static void __stdcall OnDumpEvent(void* ctx, BOOLEAN);
  static void __stdcall OnProcessEnd(void* ctx, BOOLEAN);

  ScopedKernelHANDLE port_;

  base::Lock clients_lock_;
  std::set<internal::ClientData*> clients_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServer);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_EXCEPTION_HANDLER_SERVER_H_
