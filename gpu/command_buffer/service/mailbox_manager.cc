// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include "base/command_line.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/mailbox_manager_sync.h"

namespace gpu {
namespace gles2 {

// static
scoped_refptr<MailboxManager> MailboxManager::Create(
    const GpuPreferences& gpu_preferences) {
  if (gpu_preferences.enable_threaded_texture_mailboxes)
    return scoped_refptr<MailboxManager>(new MailboxManagerSync);
  return scoped_refptr<MailboxManager>(new MailboxManagerImpl);
}

}  // namespage gles2
}  // namespace gpu
