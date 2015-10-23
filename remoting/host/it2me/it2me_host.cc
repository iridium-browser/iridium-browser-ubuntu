// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "net/socket/client_socket_factory.h"
#include "policy/policy_constants.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/logging.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_event_logger.h"
#include "remoting/host/host_secret.h"
#include "remoting/host/host_status_logger.h"
#include "remoting/host/it2me/it2me_confirmation_dialog.h"
#include "remoting/host/it2me_desktop_environment.h"
#include "remoting/host/policy_watcher.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/session_manager_factory.h"
#include "remoting/protocol/it2me_host_authenticator_factory.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/signaling/server_log_entry.h"

namespace remoting {

namespace {

// This is used for tagging system event logs.
const char kApplicationName[] = "chromoting";
const int kMaxLoginAttempts = 5;

}  // namespace

It2MeHost::It2MeHost(
    scoped_ptr<ChromotingHostContext> host_context,
    scoped_ptr<PolicyWatcher> policy_watcher,
    scoped_ptr<It2MeConfirmationDialogFactory> confirmation_dialog_factory,
    base::WeakPtr<It2MeHost::Observer> observer,
    const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
    const std::string& directory_bot_jid)
    : host_context_(host_context.Pass()),
      task_runner_(host_context_->ui_task_runner()),
      observer_(observer),
      xmpp_server_config_(xmpp_server_config),
      directory_bot_jid_(directory_bot_jid),
      state_(kDisconnected),
      failed_login_attempts_(0),
      policy_watcher_(policy_watcher.Pass()),
      confirmation_dialog_factory_(confirmation_dialog_factory.Pass()),
      nat_traversal_enabled_(false),
      policy_received_(false) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

void It2MeHost::Connect() {
  if (!host_context_->ui_task_runner()->BelongsToCurrentThread()) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    host_context_->ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::Connect, this));
    return;
  }

  desktop_environment_factory_.reset(new It2MeDesktopEnvironmentFactory(
      host_context_->network_task_runner(),
      host_context_->input_task_runner(),
      host_context_->ui_task_runner()));

  // Start monitoring configured policies.
  policy_watcher_->StartWatching(
      base::Bind(&It2MeHost::OnPolicyUpdate, this),
      base::Bind(&It2MeHost::OnPolicyError, this));

  // Switch to the network thread to start the actual connection.
  host_context_->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::ShowConfirmationPrompt, this));
}

void It2MeHost::Disconnect() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::Disconnect, this));
    return;
  }

  switch (state_) {
    case kDisconnected:
      ShutdownOnNetworkThread();
      return;

    case kStarting:
      SetState(kDisconnecting, "");
      SetState(kDisconnected, "");
      ShutdownOnNetworkThread();
      return;

    case kDisconnecting:
      return;

    default:
      SetState(kDisconnecting, "");

      if (!host_) {
        SetState(kDisconnected, "");
        ShutdownOnNetworkThread();
        return;
      }

      // Deleting the host destroys SignalStrategy synchronously, but
      // SignalStrategy::Listener handlers are not allowed to destroy
      // SignalStrategy, so post task to destroy the host later.
      host_context_->network_task_runner()->PostTask(
          FROM_HERE, base::Bind(&It2MeHost::ShutdownOnNetworkThread, this));
      return;
  }
}

void It2MeHost::RequestNatPolicy() {
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    host_context_->network_task_runner()->PostTask(
        FROM_HERE, base::Bind(&It2MeHost::RequestNatPolicy, this));
    return;
  }

  if (policy_received_)
    UpdateNatPolicy(nat_traversal_enabled_);
}

void It2MeHost::ShowConfirmationPrompt() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  SetState(kStarting, "");

  scoped_ptr<It2MeConfirmationDialog> confirmation_dialog =
      confirmation_dialog_factory_->Create();

  // TODO(dcaiafa): Remove after dialog implementations for all platforms exist.
  if (!confirmation_dialog) {
    ReadPolicyAndConnect();
    return;
  }

  confirmation_dialog_proxy_.reset(
      new It2MeConfirmationDialogProxy(host_context_->ui_task_runner(),
                                       confirmation_dialog.Pass()));

  confirmation_dialog_proxy_->Show(
      base::Bind(&It2MeHost::OnConfirmationResult, base::Unretained(this)));
}

void It2MeHost::OnConfirmationResult(It2MeConfirmationDialog::Result result) {
  switch (result) {
    case It2MeConfirmationDialog::Result::OK:
      ReadPolicyAndConnect();
      break;

    case It2MeConfirmationDialog::Result::CANCEL:
      Disconnect();
      break;

    default:
      NOTREACHED();
      return;
  }
}

void It2MeHost::ReadPolicyAndConnect() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK_EQ(kStarting, state_);

  // Only proceed to FinishConnect() if at least one policy update has been
  // received.
  if (policy_received_) {
    FinishConnect();
  } else {
    // Otherwise, create the policy watcher, and thunk the connect.
    pending_connect_ =
        base::Bind(&It2MeHost::FinishConnect, this);
  }
}

void It2MeHost::FinishConnect() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ != kStarting) {
    // Host has been stopped while we were fetching policy.
    return;
  }

  // Check the host domain policy.
  if (!required_host_domain_.empty() &&
      !base::EndsWith(xmpp_server_config_.username,
                      std::string("@") + required_host_domain_,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    SetState(kInvalidDomainError, "");
    return;
  }

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  host_key_pair_ = RsaKeyPair::Generate();

  // Create XMPP connection.
  scoped_ptr<SignalStrategy> signal_strategy(
      new XmppSignalStrategy(net::ClientSocketFactory::GetDefaultFactory(),
                             host_context_->url_request_context_getter(),
                             xmpp_server_config_));

  // Request registration of the host for support.
  scoped_ptr<RegisterSupportHostRequest> register_request(
      new RegisterSupportHostRequest(
          signal_strategy.get(), host_key_pair_, directory_bot_jid_,
          base::Bind(&It2MeHost::OnReceivedSupportID,
                     base::Unretained(this))));

  // Beyond this point nothing can fail, so save the config and request.
  signal_strategy_ = signal_strategy.Pass();
  register_request_ = register_request.Pass();

  // If NAT traversal is off then limit port range to allow firewall pin-holing.
  HOST_LOG << "NAT state: " << nat_traversal_enabled_;
  protocol::NetworkSettings network_settings(
     nat_traversal_enabled_ ?
     protocol::NetworkSettings::NAT_TRAVERSAL_FULL :
     protocol::NetworkSettings::NAT_TRAVERSAL_DISABLED);
  if (!nat_traversal_enabled_) {
    network_settings.port_range.min_port =
        protocol::NetworkSettings::kDefaultMinPort;
    network_settings.port_range.max_port =
        protocol::NetworkSettings::kDefaultMaxPort;
  }

  scoped_ptr<protocol::SessionManager> session_manager =
      CreateHostSessionManager(signal_strategy_.get(), network_settings,
                               host_context_->url_request_context_getter());
  scoped_ptr<protocol::CandidateSessionConfig> protocol_config =
      protocol::CandidateSessionConfig::CreateDefault();
  // Disable audio by default.
  // TODO(sergeyu): Add UI to enable it.
  protocol_config->DisableAudioChannel();
  session_manager->set_protocol_config(protocol_config.Pass());

  // Create the host.
  host_.reset(new ChromotingHost(
      signal_strategy_.get(), desktop_environment_factory_.get(),
      session_manager.Pass(), host_context_->audio_task_runner(),
      host_context_->input_task_runner(),
      host_context_->video_capture_task_runner(),
      host_context_->video_encode_task_runner(),
      host_context_->network_task_runner(), host_context_->ui_task_runner()));
  host_->AddStatusObserver(this);
  host_status_logger_.reset(
      new HostStatusLogger(host_->AsWeakPtr(), ServerLogEntry::IT2ME,
                           signal_strategy_.get(), directory_bot_jid_));

  // Create event logger.
  host_event_logger_ =
      HostEventLogger::Create(host_->AsWeakPtr(), kApplicationName);

  // Connect signaling and start the host.
  signal_strategy_->Connect();
  host_->Start(xmpp_server_config_.username);

  SetState(kRequestedAccessCode, "");
  return;
}

void It2MeHost::ShutdownOnNetworkThread() {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());
  DCHECK(state_ == kDisconnecting || state_ == kDisconnected);

  confirmation_dialog_proxy_.reset();

  if (state_ == kDisconnecting) {
    host_event_logger_.reset();
    host_->RemoveStatusObserver(this);
    host_.reset();

    register_request_.reset();
    host_status_logger_.reset();
    signal_strategy_.reset();
    SetState(kDisconnected, "");
  }

  host_context_->ui_task_runner()->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::ShutdownOnUiThread, this));
}

void It2MeHost::ShutdownOnUiThread() {
  DCHECK(host_context_->ui_task_runner()->BelongsToCurrentThread());

  // Destroy the DesktopEnvironmentFactory, to free thread references.
  desktop_environment_factory_.reset();

  // Stop listening for policy updates.
  policy_watcher_.reset();
}

void It2MeHost::OnAccessDenied(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  ++failed_login_attempts_;
  if (failed_login_attempts_ == kMaxLoginAttempts) {
    Disconnect();
  }
}

void It2MeHost::OnClientAuthenticated(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (state_ == kDisconnecting) {
    // Ignore the new connection if we are disconnecting.
    return;
  }
  if (state_ == kConnected) {
    // If we already connected another client then one of the connections may be
    // an attacker, so both are suspect and we have to reject the second
    // connection and shutdown the host.
    host_->RejectAuthenticatingClient();
    Disconnect();
    return;
  }

  std::string client_username = jid;
  size_t pos = client_username.find('/');
  if (pos != std::string::npos)
    client_username.replace(pos, std::string::npos, "");

  HOST_LOG << "Client " << client_username << " connected.";

  // Pass the client user name to the script object before changing state.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnClientAuthenticated,
                            observer_, client_username));

  SetState(kConnected, "");
}

void It2MeHost::OnClientDisconnected(const std::string& jid) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  Disconnect();
}

void It2MeHost::OnPolicyUpdate(scoped_ptr<base::DictionaryValue> policies) {
  // The policy watcher runs on the |ui_task_runner|.
  if (!host_context_->network_task_runner()->BelongsToCurrentThread()) {
    host_context_->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&It2MeHost::OnPolicyUpdate, this, base::Passed(&policies)));
    return;
  }

  bool nat_policy;
  if (policies->GetBoolean(policy::key::kRemoteAccessHostFirewallTraversal,
                           &nat_policy)) {
    UpdateNatPolicy(nat_policy);
  }
  std::string host_domain;
  if (policies->GetString(policy::key::kRemoteAccessHostDomain, &host_domain)) {
    UpdateHostDomainPolicy(host_domain);
  }

  policy_received_ = true;

  if (!pending_connect_.is_null()) {
    base::ResetAndReturn(&pending_connect_).Run();
  }
}

void It2MeHost::OnPolicyError() {
  // TODO(lukasza): Report the policy error to the user.  crbug.com/433009
  NOTIMPLEMENTED();
}

void It2MeHost::UpdateNatPolicy(bool nat_traversal_enabled) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateNatPolicy: " << nat_traversal_enabled;

  // When transitioning from enabled to disabled, force disconnect any
  // existing session.
  if (nat_traversal_enabled_ && !nat_traversal_enabled && IsConnected()) {
    Disconnect();
  }

  nat_traversal_enabled_ = nat_traversal_enabled;

  // Notify the web-app of the policy setting.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnNatPolicyChanged,
                            observer_, nat_traversal_enabled_));
}

void It2MeHost::UpdateHostDomainPolicy(const std::string& host_domain) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  VLOG(2) << "UpdateHostDomainPolicy: " << host_domain;

  // When setting a host domain policy, force disconnect any existing session.
  if (!host_domain.empty() && IsConnected()) {
    Disconnect();
  }

  required_host_domain_ = host_domain;
}

It2MeHost::~It2MeHost() {
  // Check that resources that need to be torn down on the UI thread are gone.
  DCHECK(!desktop_environment_factory_.get());
  DCHECK(!policy_watcher_.get());
}

void It2MeHost::SetState(It2MeHostState state,
                         const std::string& error_message) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  switch (state_) {
    case kDisconnected:
      DCHECK(state == kStarting ||
             state == kError) << state;
      break;
    case kStarting:
      DCHECK(state == kRequestedAccessCode ||
             state == kDisconnecting ||
             state == kError ||
             state == kInvalidDomainError) << state;
      break;
    case kRequestedAccessCode:
      DCHECK(state == kReceivedAccessCode ||
             state == kDisconnecting ||
             state == kError) << state;
      break;
    case kReceivedAccessCode:
      DCHECK(state == kConnected ||
             state == kDisconnecting ||
             state == kError) << state;
      break;
    case kConnected:
      DCHECK(state == kDisconnecting ||
             state == kDisconnected ||
             state == kError) << state;
      break;
    case kDisconnecting:
      DCHECK(state == kDisconnected) << state;
      break;
    case kError:
      DCHECK(state == kDisconnecting) << state;
      break;
    case kInvalidDomainError:
      DCHECK(state == kDisconnecting) << state;
      break;
  };

  state_ = state;

  // Post a state-change notification to the web-app.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnStateChanged,
                            observer_, state, error_message));
}

bool It2MeHost::IsConnected() const {
  return state_ == kRequestedAccessCode || state_ == kReceivedAccessCode ||
      state_ == kConnected;
}

void It2MeHost::OnReceivedSupportID(
    const std::string& support_id,
    const base::TimeDelta& lifetime,
    const std::string& error_message) {
  DCHECK(host_context_->network_task_runner()->BelongsToCurrentThread());

  if (!error_message.empty()) {
    SetState(kError, error_message);
    Disconnect();
    return;
  }

  std::string host_secret = GenerateSupportHostSecret();
  std::string access_code = support_id + host_secret;

  std::string local_certificate = host_key_pair_->GenerateCertificate();
  if (local_certificate.empty()) {
    std::string error_message = "Failed to generate host certificate.";
    LOG(ERROR) << error_message;
    SetState(kError, error_message);
    Disconnect();
    return;
  }

  scoped_ptr<protocol::AuthenticatorFactory> factory(
      new protocol::It2MeHostAuthenticatorFactory(
          local_certificate, host_key_pair_, access_code));
  host_->SetAuthenticatorFactory(factory.Pass());

  // Pass the Access Code to the script object before changing state.
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&It2MeHost::Observer::OnStoreAccessCode,
                            observer_, access_code, lifetime));

  SetState(kReceivedAccessCode, "");
}

It2MeHostFactory::It2MeHostFactory() : policy_service_(nullptr) {
}

It2MeHostFactory::~It2MeHostFactory() {}

void It2MeHostFactory::set_policy_service(
    policy::PolicyService* policy_service) {
  DCHECK(policy_service);
  DCHECK(!policy_service_) << "|policy_service| can only be set once.";
  policy_service_ = policy_service;
}

scoped_refptr<It2MeHost> It2MeHostFactory::CreateIt2MeHost(
    scoped_ptr<ChromotingHostContext> context,
    base::WeakPtr<It2MeHost::Observer> observer,
    const XmppSignalStrategy::XmppServerConfig& xmpp_server_config,
    const std::string& directory_bot_jid) {
  DCHECK(context->ui_task_runner()->BelongsToCurrentThread());

  scoped_ptr<It2MeConfirmationDialogFactory> confirmation_dialog_factory(
      new It2MeConfirmationDialogFactory());
  scoped_ptr<PolicyWatcher> policy_watcher =
      PolicyWatcher::Create(policy_service_, context->file_task_runner());
  return new It2MeHost(context.Pass(), policy_watcher.Pass(),
                       confirmation_dialog_factory.Pass(),
                       observer, xmpp_server_config, directory_bot_jid);
}

}  // namespace remoting
