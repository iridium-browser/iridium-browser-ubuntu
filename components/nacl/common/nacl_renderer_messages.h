// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the renderer and the NaCl process.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

#define IPC_MESSAGE_START NaClHostMsgStart

// This message must be synchronous to ensure that the exit status is sent from
// NaCl to the renderer before the NaCl process exits very soon after.
IPC_SYNC_MESSAGE_CONTROL1_0(NaClRendererMsg_ReportExitStatus,
                            int /* exit_status */)

IPC_ENUM_TRAITS_MAX_VALUE(NaClErrorCode, NACL_ERROR_CODE_MAX)

// This message must be synchronous to ensure that the load status is sent from
// NaCl to the renderer before the NaCl process exits very soon after.
IPC_SYNC_MESSAGE_CONTROL1_0(NaClRendererMsg_ReportLoadStatus,
                            NaClErrorCode /* load_status */)
