// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_HANDLE_WIN_H_
#define IPC_HANDLE_WIN_H_

#include <windows.h>

#include "ipc/ipc_export.h"
#include "ipc/ipc_message_macros.h"

namespace IPC {

// HandleWin is a wrapper around a Windows HANDLE that can be transported
// across Chrome IPC channels that support attachment brokering. The HANDLE will
// be duplicated into the destination process.
class IPC_EXPORT HandleWin {
 public:
  enum Permissions {
    // A placeholder value to be used by the receiving IPC channel, since the
    // permissions information is only used by the broker process.
    INVALID,
    // The new HANDLE will have the same permissions as the old HANDLE.
    DUPLICATE,
    // The new HANDLE will have file read and write permissions.
    FILE_READ_WRITE,
    MAX_PERMISSIONS = FILE_READ_WRITE
  };

  HandleWin(const HANDLE& handle, Permissions permissions);

  HANDLE get_handle() const { return handle_; }
  void set_handle(HANDLE handle) { handle_ = handle; }
  Permissions get_permissions() const { return permissions_; }

 private:
  HANDLE handle_;
  Permissions permissions_;
};

template <>
struct IPC_EXPORT ParamTraits<HandleWin> {
  typedef HandleWin param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, base::PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_HANDLE_WIN_H_
