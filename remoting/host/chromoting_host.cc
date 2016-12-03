// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "jingle/glue/thread_wrapper.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/host_config.h"
#include "remoting/host/input_injector.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_connection_to_client.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/webrtc_connection_to_client.h"

using remoting::protocol::ConnectionToClient;
using remoting::protocol::InputStub;

namespace remoting {

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  2000,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  -1,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

ChromotingHost::ChromotingHost(
    DesktopEnvironmentFactory* desktop_environment_factory,
    std::unique_ptr<protocol::SessionManager> session_manager,
    scoped_refptr<protocol::TransportContext> transport_context,
    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> video_encode_task_runner)
    : desktop_environment_factory_(desktop_environment_factory),
      session_manager_(std::move(session_manager)),
      transport_context_(transport_context),
      audio_task_runner_(audio_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      started_(false),
      login_backoff_(&kDefaultBackoffPolicy),
      enable_curtaining_(false),
      weak_factory_(this) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
}

ChromotingHost::~ChromotingHost() {
  DCHECK(CalledOnValidThread());

  // Disconnect all of the clients.
  while (!clients_.empty()) {
    clients_.front()->DisconnectSession(protocol::OK);
  }

  // Destroy the session manager to make sure that |signal_strategy_| does not
  // have any listeners registered.
  session_manager_.reset();

  // Notify observers.
  if (started_)
    FOR_EACH_OBSERVER(HostStatusObserver, status_observers_, OnShutdown());
}

void ChromotingHost::Start(const std::string& host_owner_email) {
  DCHECK(CalledOnValidThread());
  DCHECK(!started_);

  HOST_LOG << "Starting host";
  started_ = true;
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnStart(host_owner_email));

  session_manager_->AcceptIncoming(
      base::Bind(&ChromotingHost::OnIncomingSession, base::Unretained(this)));
}

void ChromotingHost::AddStatusObserver(HostStatusObserver* observer) {
  DCHECK(CalledOnValidThread());
  status_observers_.AddObserver(observer);
}

void ChromotingHost::RemoveStatusObserver(HostStatusObserver* observer) {
  DCHECK(CalledOnValidThread());
  status_observers_.RemoveObserver(observer);
}

void ChromotingHost::AddExtension(std::unique_ptr<HostExtension> extension) {
  extensions_.push_back(extension.release());
}

void ChromotingHost::SetAuthenticatorFactory(
    std::unique_ptr<protocol::AuthenticatorFactory> authenticator_factory) {
  DCHECK(CalledOnValidThread());
  session_manager_->set_authenticator_factory(std::move(authenticator_factory));
}

void ChromotingHost::SetEnableCurtaining(bool enable) {
  DCHECK(CalledOnValidThread());

  if (enable_curtaining_ == enable)
    return;

  enable_curtaining_ = enable;
  desktop_environment_factory_->SetEnableCurtaining(enable_curtaining_);

  // Disconnect all existing clients because they might be running not
  // curtained.
  // TODO(alexeypa): fix this such that the curtain is applied to the not
  // curtained sessions or disconnect only the client connected to not
  // curtained sessions.
  if (enable_curtaining_)
    DisconnectAllClients();
}

void ChromotingHost::SetMaximumSessionDuration(
    const base::TimeDelta& max_session_duration) {
  max_session_duration_ = max_session_duration;
}

////////////////////////////////////////////////////////////////////////////
// protocol::ClientSession::EventHandler implementation.
void ChromotingHost::OnSessionAuthenticating(ClientSession* client) {
  // We treat each incoming connection as a failure to authenticate,
  // and clear the backoff when a connection successfully
  // authenticates. This allows the backoff to protect from parallel
  // connection attempts as well as sequential ones.
  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Disconnecting client " << client->client_jid() << " due to"
                    " an overload of failed login attempts.";
    client->DisconnectSession(protocol::HOST_OVERLOAD);
    return;
  }
  login_backoff_.InformOfRequest(false);
}

void ChromotingHost::OnSessionAuthenticated(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  login_backoff_.Reset();

  // Disconnect all other clients. |it| should be advanced before Disconnect()
  // is called to avoid it becoming invalid when the client is removed from
  // the list.
  ClientList::iterator it = clients_.begin();
  base::WeakPtr<ChromotingHost> self = weak_factory_.GetWeakPtr();
  while (it != clients_.end()) {
    ClientSession* other_client = *it++;
    if (other_client != client) {
      other_client->DisconnectSession(protocol::OK);

      // Quit if the host was destroyed.
      if (!self)
        return;
    }
  }

  // Disconnects above must have destroyed all other clients.
  DCHECK_EQ(clients_.size(), 1U);

  // Notify observers that there is at least one authenticated client.
  const std::string& jid = client->client_jid();

  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientAuthenticated(jid));
}

void ChromotingHost::OnSessionChannelsConnected(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientConnected(client->client_jid()));
}

void ChromotingHost::OnSessionAuthenticationFailed(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  // Notify observers.
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnAccessDenied(client->client_jid()));
}

void ChromotingHost::OnSessionClosed(ClientSession* client) {
  DCHECK(CalledOnValidThread());

  ClientList::iterator it = std::find(clients_.begin(), clients_.end(), client);
  CHECK(it != clients_.end());

  bool was_authenticated = client->is_authenticated();
  std::string jid = client->client_jid();
  clients_.erase(it);
  delete client;

  if (was_authenticated) {
    FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                      OnClientDisconnected(jid));
  }
}

void ChromotingHost::OnSessionRouteChange(
    ClientSession* session,
    const std::string& channel_name,
    const protocol::TransportRoute& route) {
  DCHECK(CalledOnValidThread());
  FOR_EACH_OBSERVER(HostStatusObserver, status_observers_,
                    OnClientRouteChange(session->client_jid(), channel_name,
                                        route));
}

void ChromotingHost::OnIncomingSession(
      protocol::Session* session,
      protocol::SessionManager::IncomingSessionResponse* response) {
  DCHECK(CalledOnValidThread());
  DCHECK(started_);

  if (login_backoff_.ShouldRejectRequest()) {
    LOG(WARNING) << "Rejecting connection due to"
                    " an overload of failed login attempts.";
    *response = protocol::SessionManager::OVERLOAD;
    return;
  }

  *response = protocol::SessionManager::ACCEPT;

  HOST_LOG << "Client connected: " << session->jid();

  // Create either IceConnectionToClient or WebrtcConnectionToClient.
  // TODO(sergeyu): Move this logic to the protocol layer.
  std::unique_ptr<protocol::ConnectionToClient> connection;
  if (session->config().protocol() ==
      protocol::SessionConfig::Protocol::WEBRTC) {
    connection.reset(new protocol::WebrtcConnectionToClient(
        base::WrapUnique(session), transport_context_,
        video_encode_task_runner_));
  } else {
    connection.reset(new protocol::IceConnectionToClient(
        base::WrapUnique(session), transport_context_,
        video_encode_task_runner_));
  }

  // Create a ClientSession object.
  ClientSession* client =
      new ClientSession(this, audio_task_runner_, std::move(connection),
                        desktop_environment_factory_, max_session_duration_,
                        pairing_registry_, extensions_.get());

  clients_.push_back(client);
}

void ChromotingHost::DisconnectAllClients() {
  DCHECK(CalledOnValidThread());

  while (!clients_.empty()) {
    size_t size = clients_.size();
    clients_.front()->DisconnectSession(protocol::OK);
    CHECK_EQ(clients_.size(), size - 1);
  }
}

}  // namespace remoting
