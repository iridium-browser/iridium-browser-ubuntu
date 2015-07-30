// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/me2me_native_messaging_host.h"
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "ipc/ipc_channel.h"
#include "net/base/net_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/native_messaging/pipe_messaging_channel.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/oauth_client.h"
#include "remoting/protocol/pairing_registry.h"

#if defined(OS_WIN)
#include <shellapi.h>
#include "base/win/win_util.h"
#include "remoting/host/win/security_descriptor.h"
#endif  // defined(OS_WIN)

namespace {

#if defined(OS_WIN)
// Windows will use default buffer size when 0 is passed to CreateNamedPipeW().
const DWORD kBufferSize = 0;
const int kTimeOutMilliseconds = 2000;
const char kChromePipeNamePrefix[] = "\\\\.\\pipe\\chrome_remote_desktop.";
const int kElevatedHostTimeoutSeconds = 300;
#endif  // defined(OS_WIN)

// redirect_uri to use when authenticating service accounts (service account
// codes are obtained "out-of-band", i.e., not through an OAuth redirect).
const char* kServiceAccountRedirectUri = "oob";

// Features supported in addition to the base protocol.
const char* kSupportedFeatures[] = {
  "pairingRegistry",
  "oauthClient",
  "getRefreshTokenFromAuthCode",
};

// Helper to extract the "config" part of a message as a DictionaryValue.
// Returns nullptr on failure, and logs an error message.
scoped_ptr<base::DictionaryValue> ConfigDictionaryFromMessage(
    scoped_ptr<base::DictionaryValue> message) {
  scoped_ptr<base::DictionaryValue> result;
  const base::DictionaryValue* config_dict;
  if (message->GetDictionary("config", &config_dict)) {
    result.reset(config_dict->DeepCopy());
  } else {
    LOG(ERROR) << "'config' dictionary not found";
  }
  return result.Pass();
}

}  // namespace

namespace remoting {

Me2MeNativeMessagingHost::Me2MeNativeMessagingHost(
    bool needs_elevation,
    intptr_t parent_window_handle,
    scoped_ptr<extensions::NativeMessagingChannel> channel,
    scoped_refptr<DaemonController> daemon_controller,
    scoped_refptr<protocol::PairingRegistry> pairing_registry,
    scoped_ptr<OAuthClient> oauth_client)
    : needs_elevation_(needs_elevation),
#if defined(OS_WIN)
      parent_window_handle_(parent_window_handle),
#endif
      channel_(channel.Pass()),
      daemon_controller_(daemon_controller),
      pairing_registry_(pairing_registry),
      oauth_client_(oauth_client.Pass()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

Me2MeNativeMessagingHost::~Me2MeNativeMessagingHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void Me2MeNativeMessagingHost::Start(
      const base::Closure& quit_closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!quit_closure.is_null());

  quit_closure_ = quit_closure;

  channel_->Start(this);
}

void Me2MeNativeMessagingHost::OnMessage(scoped_ptr<base::Value> message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!message->IsType(base::Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Received a message that's not a dictionary.";
    channel_->SendMessage(nullptr);
    return;
  }

  scoped_ptr<base::DictionaryValue> message_dict(
      static_cast<base::DictionaryValue*>(message.release()));
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue());

  // If the client supplies an ID, it will expect it in the response. This
  // might be a string or a number, so cope with both.
  const base::Value* id;
  if (message_dict->Get("id", &id))
    response->Set("id", id->DeepCopy());

  std::string type;
  if (!message_dict->GetString("type", &type)) {
    LOG(ERROR) << "'type' not found";
    channel_->SendMessage(nullptr);
    return;
  }

  response->SetString("type", type + "Response");

  if (type == "hello") {
    ProcessHello(message_dict.Pass(), response.Pass());
  } else if (type == "clearPairedClients") {
    ProcessClearPairedClients(message_dict.Pass(), response.Pass());
  } else if (type == "deletePairedClient") {
    ProcessDeletePairedClient(message_dict.Pass(), response.Pass());
  } else if (type == "getHostName") {
    ProcessGetHostName(message_dict.Pass(), response.Pass());
  } else if (type == "getPinHash") {
    ProcessGetPinHash(message_dict.Pass(), response.Pass());
  } else if (type == "generateKeyPair") {
    ProcessGenerateKeyPair(message_dict.Pass(), response.Pass());
  } else if (type == "updateDaemonConfig") {
    ProcessUpdateDaemonConfig(message_dict.Pass(), response.Pass());
  } else if (type == "getDaemonConfig") {
    ProcessGetDaemonConfig(message_dict.Pass(), response.Pass());
  } else if (type == "getPairedClients") {
    ProcessGetPairedClients(message_dict.Pass(), response.Pass());
  } else if (type == "getUsageStatsConsent") {
    ProcessGetUsageStatsConsent(message_dict.Pass(), response.Pass());
  } else if (type == "startDaemon") {
    ProcessStartDaemon(message_dict.Pass(), response.Pass());
  } else if (type == "stopDaemon") {
    ProcessStopDaemon(message_dict.Pass(), response.Pass());
  } else if (type == "getDaemonState") {
    ProcessGetDaemonState(message_dict.Pass(), response.Pass());
  } else if (type == "getHostClientId") {
    ProcessGetHostClientId(message_dict.Pass(), response.Pass());
  } else if (type == "getCredentialsFromAuthCode") {
    ProcessGetCredentialsFromAuthCode(
        message_dict.Pass(), response.Pass(), true);
  } else if (type == "getRefreshTokenFromAuthCode") {
    ProcessGetCredentialsFromAuthCode(
        message_dict.Pass(), response.Pass(), false);
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
    OnError();
  }
}

void Me2MeNativeMessagingHost::OnDisconnect() {
  if (!quit_closure_.is_null())
    base::ResetAndReturn(&quit_closure_).Run();
}

void Me2MeNativeMessagingHost::ProcessHello(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("version", STRINGIZE(VERSION));
  scoped_ptr<base::ListValue> supported_features_list(new base::ListValue());
  supported_features_list->AppendStrings(std::vector<std::string>(
      kSupportedFeatures, kSupportedFeatures + arraysize(kSupportedFeatures)));
  response->Set("supportedFeatures", supported_features_list.release());
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessClearPairedClients(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (needs_elevation_) {
    if (!DelegateToElevatedHost(message.Pass()))
      SendBooleanResult(response.Pass(), false);
    return;
  }

  if (pairing_registry_.get()) {
    pairing_registry_->ClearAllPairings(
        base::Bind(&Me2MeNativeMessagingHost::SendBooleanResult, weak_ptr_,
                   base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
}

void Me2MeNativeMessagingHost::ProcessDeletePairedClient(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (needs_elevation_) {
    if (!DelegateToElevatedHost(message.Pass()))
      SendBooleanResult(response.Pass(), false);
    return;
  }

  std::string client_id;
  if (!message->GetString(protocol::PairingRegistry::kClientIdKey,
                          &client_id)) {
    LOG(ERROR) << "'" << protocol::PairingRegistry::kClientIdKey
               << "' string not found.";
    OnError();
    return;
  }

  if (pairing_registry_.get()) {
    pairing_registry_->DeletePairing(
        client_id, base::Bind(&Me2MeNativeMessagingHost::SendBooleanResult,
                              weak_ptr_, base::Passed(&response)));
  } else {
    SendBooleanResult(response.Pass(), false);
  }
}

void Me2MeNativeMessagingHost::ProcessGetHostName(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("hostname", net::GetHostName());
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessGetPinHash(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string host_id;
  if (!message->GetString("hostId", &host_id)) {
    LOG(ERROR) << "'hostId' not found: " << message;
    OnError();
    return;
  }
  std::string pin;
  if (!message->GetString("pin", &pin)) {
    LOG(ERROR) << "'pin' not found: " << message;
    OnError();
    return;
  }
  response->SetString("hash", MakeHostPinHash(host_id, pin));
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessGenerateKeyPair(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::Generate();
  response->SetString("privateKey", key_pair->ToString());
  response->SetString("publicKey", key_pair->GetPublicKey());
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessUpdateDaemonConfig(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (needs_elevation_) {
    if (!DelegateToElevatedHost(message.Pass()))
      SendAsyncResult(response.Pass(), DaemonController::RESULT_FAILED);
    return;
  }

  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message.Pass());
  if (!config_dict) {
    OnError();
    return;
  }

  daemon_controller_->UpdateConfig(
      config_dict.Pass(),
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
}

void Me2MeNativeMessagingHost::ProcessGetDaemonConfig(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  daemon_controller_->GetConfig(
      base::Bind(&Me2MeNativeMessagingHost::SendConfigResponse, weak_ptr_,
                 base::Passed(&response)));
}

void Me2MeNativeMessagingHost::ProcessGetPairedClients(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (pairing_registry_.get()) {
    pairing_registry_->GetAllPairings(
        base::Bind(&Me2MeNativeMessagingHost::SendPairedClientsResponse,
                   weak_ptr_, base::Passed(&response)));
  } else {
    scoped_ptr<base::ListValue> no_paired_clients(new base::ListValue);
    SendPairedClientsResponse(response.Pass(), no_paired_clients.Pass());
  }
}

void Me2MeNativeMessagingHost::ProcessGetUsageStatsConsent(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  daemon_controller_->GetUsageStatsConsent(
      base::Bind(&Me2MeNativeMessagingHost::SendUsageStatsConsentResponse,
                 weak_ptr_, base::Passed(&response)));
}

void Me2MeNativeMessagingHost::ProcessStartDaemon(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (needs_elevation_) {
    if (!DelegateToElevatedHost(message.Pass()))
      SendAsyncResult(response.Pass(), DaemonController::RESULT_FAILED);
    return;
  }

  bool consent;
  if (!message->GetBoolean("consent", &consent)) {
    LOG(ERROR) << "'consent' not found.";
    OnError();
    return;
  }

  scoped_ptr<base::DictionaryValue> config_dict =
      ConfigDictionaryFromMessage(message.Pass());
  if (!config_dict) {
    OnError();
    return;
  }

  daemon_controller_->SetConfigAndStart(
      config_dict.Pass(), consent,
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
}

void Me2MeNativeMessagingHost::ProcessStopDaemon(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (needs_elevation_) {
    if (!DelegateToElevatedHost(message.Pass()))
      SendAsyncResult(response.Pass(), DaemonController::RESULT_FAILED);
    return;
  }

  daemon_controller_->Stop(
      base::Bind(&Me2MeNativeMessagingHost::SendAsyncResult, weak_ptr_,
                 base::Passed(&response)));
}

void Me2MeNativeMessagingHost::ProcessGetDaemonState(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DaemonController::State state = daemon_controller_->GetState();
  switch (state) {
    case DaemonController::STATE_NOT_IMPLEMENTED:
      response->SetString("state", "NOT_IMPLEMENTED");
      break;
    case DaemonController::STATE_STOPPED:
      response->SetString("state", "STOPPED");
      break;
    case DaemonController::STATE_STARTING:
      response->SetString("state", "STARTING");
      break;
    case DaemonController::STATE_STARTED:
      response->SetString("state", "STARTED");
      break;
    case DaemonController::STATE_STOPPING:
      response->SetString("state", "STOPPING");
      break;
    case DaemonController::STATE_UNKNOWN:
      response->SetString("state", "UNKNOWN");
      break;
  }
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessGetHostClientId(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetString("clientId", google_apis::GetOAuth2ClientID(
      google_apis::CLIENT_REMOTING_HOST));
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::ProcessGetCredentialsFromAuthCode(
    scoped_ptr<base::DictionaryValue> message,
    scoped_ptr<base::DictionaryValue> response,
    bool need_user_email) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::string auth_code;
  if (!message->GetString("authorizationCode", &auth_code)) {
    LOG(ERROR) << "'authorizationCode' string not found.";
    OnError();
    return;
  }

  gaia::OAuthClientInfo oauth_client_info = {
    google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
    google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST),
    kServiceAccountRedirectUri
  };

  oauth_client_->GetCredentialsFromAuthCode(
      oauth_client_info, auth_code, need_user_email, base::Bind(
          &Me2MeNativeMessagingHost::SendCredentialsResponse, weak_ptr_,
          base::Passed(&response)));
}

void Me2MeNativeMessagingHost::SendConfigResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::DictionaryValue> config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (config) {
    response->Set("config", config.release());
  } else {
    response->Set("config", base::Value::CreateNullValue());
  }
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendPairedClientsResponse(
    scoped_ptr<base::DictionaryValue> response,
    scoped_ptr<base::ListValue> pairings) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->Set("pairedClients", pairings.release());
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendUsageStatsConsentResponse(
    scoped_ptr<base::DictionaryValue> response,
    const DaemonController::UsageStatsConsent& consent) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetBoolean("supported", consent.supported);
  response->SetBoolean("allowed", consent.allowed);
  response->SetBoolean("setByPolicy", consent.set_by_policy);
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendAsyncResult(
    scoped_ptr<base::DictionaryValue> response,
    DaemonController::AsyncResult result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  switch (result) {
    case DaemonController::RESULT_OK:
      response->SetString("result", "OK");
      break;
    case DaemonController::RESULT_FAILED:
      response->SetString("result", "FAILED");
      break;
    case DaemonController::RESULT_CANCELLED:
      response->SetString("result", "CANCELLED");
      break;
    case DaemonController::RESULT_FAILED_DIRECTORY:
      response->SetString("result", "FAILED_DIRECTORY");
      break;
  }
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendBooleanResult(
    scoped_ptr<base::DictionaryValue> response, bool result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  response->SetBoolean("result", result);
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::SendCredentialsResponse(
    scoped_ptr<base::DictionaryValue> response,
    const std::string& user_email,
    const std::string& refresh_token) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!user_email.empty()) {
    response->SetString("userEmail", user_email);
  }
  response->SetString("refreshToken", refresh_token);
  channel_->SendMessage(response.Pass());
}

void Me2MeNativeMessagingHost::OnError() {
  // Trigger a host shutdown by sending a nullptr message.
  channel_->SendMessage(nullptr);
}

void Me2MeNativeMessagingHost::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!quit_closure_.is_null())
    base::ResetAndReturn(&quit_closure_).Run();
}

#if defined(OS_WIN)
Me2MeNativeMessagingHost::ElevatedChannelEventHandler::
    ElevatedChannelEventHandler(Me2MeNativeMessagingHost* host)
    : parent_(host) {
}

void Me2MeNativeMessagingHost::ElevatedChannelEventHandler::OnMessage(
    scoped_ptr<base::Value> message) {
  DCHECK(parent_->thread_checker_.CalledOnValidThread());

  // Simply pass along the response from the elevated host to the client.
  parent_->channel_->SendMessage(message.Pass());
}

void Me2MeNativeMessagingHost::ElevatedChannelEventHandler::OnDisconnect() {
  parent_->OnDisconnect();
}

bool Me2MeNativeMessagingHost::DelegateToElevatedHost(
    scoped_ptr<base::DictionaryValue> message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EnsureElevatedHostCreated();

  // elevated_channel_ will be null if user rejects the UAC request.
  if (elevated_channel_)
    elevated_channel_->SendMessage(message.Pass());

  return elevated_channel_ != nullptr;
}

void Me2MeNativeMessagingHost::EnsureElevatedHostCreated() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(needs_elevation_);

  if (elevated_channel_)
    return;

  // presubmit: allow wstring
  std::wstring user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    LOG(ERROR) << "Failed to query the current user SID.";
    OnError();
    return;
  }

  // Create a security descriptor that gives full access to the caller and
  // denies access by anyone else.
  std::string user_sid_ascii = base::UTF16ToASCII(user_sid);
  std::string security_descriptor =
      base::StringPrintf("O:%sG:%sD:(A;;GA;;;%s)", user_sid_ascii.c_str(),
                         user_sid_ascii.c_str(), user_sid_ascii.c_str());

  ScopedSd sd = ConvertSddlToSd(security_descriptor);
  if (!sd) {
    PLOG(ERROR) << "Failed to create a security descriptor for the"
                << "Chromoting Me2Me native messaging host.";
    OnError();
    return;
  }

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = sd.get();
  security_attributes.bInheritHandle = FALSE;

  // Generate a unique name for the input channel.
  std::string input_pipe_name(kChromePipeNamePrefix);
  input_pipe_name.append(IPC::Channel::GenerateUniqueRandomChannelID());

  base::win::ScopedHandle delegate_write_handle(::CreateNamedPipe(
      base::ASCIIToUTF16(input_pipe_name).c_str(),
      PIPE_ACCESS_OUTBOUND,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
      1,
      kBufferSize,
      kBufferSize,
      kTimeOutMilliseconds,
      &security_attributes));

  if (!delegate_write_handle.IsValid()) {
    PLOG(ERROR) << "Failed to create named pipe '" << input_pipe_name << "'";
    OnError();
    return;
  }

  // Generate a unique name for the input channel.
  std::string output_pipe_name(kChromePipeNamePrefix);
  output_pipe_name.append(IPC::Channel::GenerateUniqueRandomChannelID());

  base::win::ScopedHandle delegate_read_handle(::CreateNamedPipe(
      base::ASCIIToUTF16(output_pipe_name).c_str(),
      PIPE_ACCESS_INBOUND,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS,
      1,
      kBufferSize,
      kBufferSize,
      kTimeOutMilliseconds,
      &security_attributes));

  if (!delegate_read_handle.IsValid()) {
    PLOG(ERROR) << "Failed to create named pipe '" << output_pipe_name << "'";
    OnError();
    return;
  }

  const base::CommandLine* current_command_line =
      base::CommandLine::ForCurrentProcess();
  const base::CommandLine::SwitchMap& switches =
      current_command_line->GetSwitches();
  base::CommandLine::StringVector args = current_command_line->GetArgs();

  // Create the child process command line by copying switches from the current
  // command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(kElevatingSwitchName);
  command_line.AppendSwitchASCII(kInputSwitchName, input_pipe_name);
  command_line.AppendSwitchASCII(kOutputSwitchName, output_pipe_name);

  DCHECK(!current_command_line->HasSwitch(kElevatingSwitchName));
  for (base::CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
      command_line.AppendSwitchNative(i->first, i->second);
  }
  for (base::CommandLine::StringVector::const_iterator i = args.begin();
       i != args.end(); ++i) {
    command_line.AppendArgNative(*i);
  }

  // Get the name of the binary to launch.
  base::FilePath binary = current_command_line->GetProgram();
  base::CommandLine::StringType parameters =
      command_line.GetCommandLineString();

  // Launch the child process requesting elevation.
  SHELLEXECUTEINFO info;
  memset(&info, 0, sizeof(info));
  info.cbSize = sizeof(info);
  info.hwnd = reinterpret_cast<HWND>(parent_window_handle_);
  info.lpVerb = L"runas";
  info.lpFile = binary.value().c_str();
  info.lpParameters = parameters.c_str();
  info.nShow = SW_HIDE;

  if (!ShellExecuteEx(&info)) {
    DWORD error = ::GetLastError();
    PLOG(ERROR) << "Unable to launch '" << binary.value() << "'";
    if (error != ERROR_CANCELLED) {
      OnError();
    }
    return;
  }

  if (!::ConnectNamedPipe(delegate_write_handle.Get(), nullptr)) {
    DWORD error = ::GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "Unable to connect '" << input_pipe_name << "'";
      OnError();
      return;
    }
  }

  if (!::ConnectNamedPipe(delegate_read_handle.Get(), nullptr)) {
    DWORD error = ::GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      PLOG(ERROR) << "Unable to connect '" << output_pipe_name << "'";
      OnError();
      return;
    }
  }

  // Set up the native messaging channel to talk to the elevated host.
  // Note that input for the elevated channel is output for the elevated host.
  elevated_channel_.reset(
      new PipeMessagingChannel(base::File(delegate_read_handle.Take()),
                               base::File(delegate_write_handle.Take())));

  elevated_channel_event_handler_.reset(
      new Me2MeNativeMessagingHost::ElevatedChannelEventHandler(this));
  elevated_channel_->Start(elevated_channel_event_handler_.get());

  elevated_host_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kElevatedHostTimeoutSeconds),
      this, &Me2MeNativeMessagingHost::DisconnectElevatedHost);
}

void Me2MeNativeMessagingHost::DisconnectElevatedHost() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This will send an EOF to the elevated host, triggering its shutdown.
  elevated_channel_.reset();
}

#else  // defined(OS_WIN)

bool Me2MeNativeMessagingHost::DelegateToElevatedHost(
    scoped_ptr<base::DictionaryValue> message) {
  NOTREACHED();
  return false;
}

#endif  // !defined(OS_WIN)

}  // namespace remoting
