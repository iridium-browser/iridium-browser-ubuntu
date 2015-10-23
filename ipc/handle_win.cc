// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/handle_win.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "ipc/handle_attachment_win.h"

namespace IPC {

HandleWin::HandleWin(const HANDLE& handle, Permissions permissions)
    : handle_(handle), permissions_(permissions) {}

// static
void ParamTraits<HandleWin>::Write(Message* m, const param_type& p) {
  scoped_refptr<IPC::internal::HandleAttachmentWin> attachment(
      new IPC::internal::HandleAttachmentWin(p.get_handle(),
                                             p.get_permissions()));
  if (!m->WriteAttachment(attachment.Pass()))
    NOTREACHED();
}

// static
bool ParamTraits<HandleWin>::Read(const Message* m,
                                  base::PickleIterator* iter,
                                  param_type* r) {
  scoped_refptr<MessageAttachment> attachment;
  if (!m->ReadAttachment(iter, &attachment))
    return false;
  if (attachment->GetType() != MessageAttachment::TYPE_BROKERABLE_ATTACHMENT)
    return false;
  BrokerableAttachment* brokerable_attachment =
      static_cast<BrokerableAttachment*>(attachment.get());
  if (brokerable_attachment->GetBrokerableType() !=
      BrokerableAttachment::WIN_HANDLE) {
    return false;
  }
  IPC::internal::HandleAttachmentWin* handle_attachment =
      static_cast<IPC::internal::HandleAttachmentWin*>(brokerable_attachment);
  r->set_handle(handle_attachment->get_handle());
  return true;
}

// static
void ParamTraits<HandleWin>::Log(const param_type& p, std::string* l) {
  l->append(base::StringPrintf("0x%X", p.get_handle()));
  l->append(base::IntToString(p.get_permissions()));
}

}  // namespace IPC
