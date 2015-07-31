// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gnubby_auth_handler.h"

#include "base/logging.h"

namespace remoting {

namespace {

class GnubbyAuthHandlerWin : public GnubbyAuthHandler {
 private:
  // GnubbyAuthHandler interface.
  void DeliverClientMessage(const std::string& message) override;
  void DeliverHostDataMessage(int connection_id,
                              const std::string& data) const override;

  DISALLOW_COPY_AND_ASSIGN(GnubbyAuthHandlerWin);
};

}  // namespace

// static
scoped_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    protocol::ClientStub* client_stub) {
  return nullptr;
}

// static
void GnubbyAuthHandler::SetGnubbySocketName(
    const base::FilePath& gnubby_socket_name) {
  NOTIMPLEMENTED();
}

void GnubbyAuthHandlerWin::DeliverClientMessage(const std::string& message) {
  NOTIMPLEMENTED();
}

void GnubbyAuthHandlerWin::DeliverHostDataMessage(int connection_id,
                                                  const std::string& data)
    const {
  NOTIMPLEMENTED();
}

}  // namespace remoting
