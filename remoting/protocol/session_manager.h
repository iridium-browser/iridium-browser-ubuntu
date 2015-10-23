// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of SessionManager is to facilitate creation of chromotocol
// sessions. Both host and client use it to establish chromotocol
// sessions. JingleChromotocolServer implements this inteface using
// libjingle.
//
// OUTGOING SESSIONS
// Connect() must be used to create new session to a remote host. The
// returned session is initially in INITIALIZING state. Later state is
// changed to CONNECTED if the session is accepted by the host or
// CLOSED if the session is rejected.
//
// INCOMING SESSIONS
// The IncomingSessionCallback is called when a client attempts to connect.
// The callback function decides whether the session should be accepted or
// rejected.
//
// AUTHENTICATION
// Implementations of the Session and SessionManager interfaces
// delegate authentication to an Authenticator implementation. For
// incoming connections authenticators are created using an
// AuthenticatorFactory set via the set_authenticator_factory()
// method. For outgoing sessions authenticator must be passed to the
// Connect() method. The Session's state changes to AUTHENTICATED once
// authentication succeeds.
//
// SESSION OWNERSHIP AND SHUTDOWN
// The SessionManager must not be closed or destroyed before all sessions
// created by that SessionManager are destroyed. Caller owns Sessions
// created by a SessionManager (except rejected
// sessions). The SignalStrategy must outlive the SessionManager.
//
// PROTOCOL VERSION NEGOTIATION
// When client connects to a host it sends a session-initiate stanza with list
// of supported configurations for each channel. If the host decides to accept
// session, then it selects configuration that is supported by both sides
// and then replies with the session-accept stanza that contans selected
// configuration. The configuration specified in the session-accept is used
// for the session.
//
// The CandidateSessionConfig class represents list of configurations
// supported by an endpoint. The |candidate_config| argument in the Connect()
// specifies configuration supported on the client side. When the host receives
// session-initiate stanza, the IncomingSessionCallback is called. The
// configuration sent in the session-intiate staza is available via
// ChromotocolConnnection::candidate_config(). If an incoming session is
// being accepted then the IncomingSessionCallback callback function must
// select session configuration and then set it with Session::set_config().

#ifndef REMOTING_PROTOCOL_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_SESSION_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/session.h"

namespace remoting {

class SignalStrategy;

namespace protocol {

class Authenticator;
class AuthenticatorFactory;

// Generic interface for Chromoting session manager.
//
// TODO(sergeyu): Split this into two separate interfaces: one for the
// client side and one for the host side.
class SessionManager : public base::NonThreadSafe {
 public:
  SessionManager() {}
  virtual ~SessionManager() {}

  enum IncomingSessionResponse {
    // Accept the session.
    ACCEPT,

    // Reject the session because the host is currently disabled due
    // to previous login attempts.
    OVERLOAD,

    // Reject the session because the client is not allowed to connect
    // to the host.
    DECLINE,
  };

  class Listener {
   public:
    Listener() {}

    // Called when the session manager is ready to create outgoing
    // sessions. May be called from Init() or after Init()
    // returns.
    virtual void OnSessionManagerReady() = 0;

    // Called when a new session is received. If the host decides to
    // accept the session it should set the |response| to
    // ACCEPT. Otherwise it should set it to DECLINE, or
    // INCOMPATIBLE. INCOMPATIBLE indicates that the session has
    // incompatible configuration, and cannot be accepted. If the
    // callback accepts the |session| then it must also set
    // configuration for the |session| using Session::set_config().
    // The callback must take ownership of the |session| if it ACCEPTs it.
    virtual void OnIncomingSession(Session* session,
                                   IncomingSessionResponse* response) = 0;

   protected:
    ~Listener() {}
  };

  // Initializes the session client. Caller retains ownership of the
  // |signal_strategy| and |listener|.
  virtual void Init(SignalStrategy* signal_strategy,
                    Listener* listener) = 0;

  // Sets local protocol configuration to be used when negotiating outgoing and
  // incoming connections.
  virtual void set_protocol_config(
      scoped_ptr<CandidateSessionConfig> config) = 0;

  // Tries to create a session to the host |jid|. Must be called only
  // after initialization has finished successfully, i.e. after
  // Listener::OnInitialized() has been called.
  //
  // |host_jid| is the full jid of the host to connect to.
  // |authenticator| is a client authenticator for the session.
  virtual scoped_ptr<Session> Connect(
      const std::string& host_jid,
      scoped_ptr<Authenticator> authenticator) = 0;

  // Close session manager. Can be called only after all corresponding
  // sessions are destroyed. No callbacks are called after this method
  // returns.
  virtual void Close() = 0;

  // Set authenticator factory that should be used to authenticate
  // incoming connection. No connections will be accepted if
  // authenticator factory isn't set. Must not be called more than
  // once per SessionManager because it may not be safe to delete
  // factory before all authenticators it created are deleted.
  virtual void set_authenticator_factory(
      scoped_ptr<AuthenticatorFactory> authenticator_factory) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_MANAGER_H_
