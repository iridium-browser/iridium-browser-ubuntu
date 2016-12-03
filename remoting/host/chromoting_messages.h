// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_MESSAGES_H_
#define REMOTING_HOST_CHROMOTING_MESSAGES_H_

#include <stdint.h>

#include "base/memory/shared_memory_handle.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/ip_endpoint.h"
#include "remoting/host/chromoting_param_traits.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#endif  // REMOTING_HOST_CHROMOTING_MESSAGES_H_

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon.

// Requests the receiving process to crash producing a crash dump. The daemon
// sends this message when a fatal error has been detected indicating that
// the receiving process misbehaves. The daemon passes the location of the code
// that detected the error.
IPC_MESSAGE_CONTROL3(ChromotingDaemonMsg_Crash,
                     std::string /* function_name */,
                     std::string /* file_name */,
                     int /* line_number */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon to the network process.

// Delivers the host configuration (and updates) to the network process.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_Configuration, std::string)

// Initializes the pairing registry on Windows. The passed key handles are
// already duplicated by the sender.
IPC_MESSAGE_CONTROL2(ChromotingDaemonNetworkMsg_InitializePairingRegistry,
                     IPC::PlatformFileForTransit /* privileged_key */,
                     IPC::PlatformFileForTransit /* unprivileged_key */)

// Notifies the network process that the terminal |terminal_id| has been
// disconnected from the desktop session.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                     int /* terminal_id */)

// Notifies the network process that |terminal_id| is now attached to
// a desktop integration process. |desktop_process| is the handle of the desktop
// process. |desktop_pipe| is the client end of the desktop-to-network pipe
// opened.
//
// Windows only: |desktop_pipe| has to be duplicated from the desktop process
// by the receiver of the message. |desktop_process| is already duplicated by
// the sender.
IPC_MESSAGE_CONTROL3(ChromotingDaemonNetworkMsg_DesktopAttached,
                     int /* terminal_id */,
                     base::ProcessHandle /* desktop_process */,
                     IPC::PlatformFileForTransit /* desktop_pipe */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the daemon process.

// Connects the terminal |terminal_id| (i.e. a remote client) to a desktop
// session.
IPC_MESSAGE_CONTROL3(ChromotingNetworkHostMsg_ConnectTerminal,
                     int /* terminal_id */,
                     remoting::ScreenResolution /* resolution */,
                     bool /* virtual_terminal */)

// Disconnects the terminal |terminal_id| from the desktop session it was
// connected to.
IPC_MESSAGE_CONTROL1(ChromotingNetworkHostMsg_DisconnectTerminal,
                     int /* terminal_id */)

// Changes the screen resolution in the given desktop session.
IPC_MESSAGE_CONTROL2(ChromotingNetworkDaemonMsg_SetScreenResolution,
                     int /* terminal_id */,
                     remoting::ScreenResolution /* resolution */)

// Serialized remoting::protocol::TransportRoute structure.
IPC_STRUCT_BEGIN(SerializedTransportRoute)
  IPC_STRUCT_MEMBER(remoting::protocol::TransportRoute::RouteType, type)
  IPC_STRUCT_MEMBER(net::IPEndPoint, remote_address)
  IPC_STRUCT_MEMBER(net::IPEndPoint, local_address)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(remoting::protocol::TransportRoute::RouteType,
                          remoting::protocol::TransportRoute::ROUTE_TYPE_MAX)

// Hosts status notifications (see HostStatusObserver interface) sent by
// IpcHostEventLogger.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_AccessDenied,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientAuthenticated,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientConnected,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientDisconnected,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL3(ChromotingNetworkDaemonMsg_ClientRouteChange,
                     std::string /* jid */,
                     std::string /* channel_name */,
                     SerializedTransportRoute /* route */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_HostStarted,
                     std::string /* xmpp_login */)

IPC_MESSAGE_CONTROL0(ChromotingNetworkDaemonMsg_HostShutdown)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the daemon process.

// Notifies the daemon that a desktop integration process has been initialized.
// |desktop_pipe| specifies the client end of the desktop pipe. It is to be
// forwarded to the desktop environment stub.
//
// Windows only: |desktop_pipe| has to be duplicated from the desktop process by
// the receiver of the message.
IPC_MESSAGE_CONTROL1(ChromotingDesktopDaemonMsg_DesktopAttached,
                     IPC::PlatformFileForTransit /* desktop_pipe */)

// Asks the daemon to inject Secure Attention Sequence (SAS) in the session
// where the desktop process is running.
IPC_MESSAGE_CONTROL0(ChromotingDesktopDaemonMsg_InjectSas)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the network process.

// Notifies the network process that a shared buffer has been created.
IPC_MESSAGE_CONTROL3(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                     int /* id */,
                     base::SharedMemoryHandle /* handle */,
                     uint32_t /* size */)

// Request the network process to stop using a shared buffer.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
                     int /* id */)

// Serialized webrtc::DesktopFrame.
IPC_STRUCT_BEGIN(SerializedDesktopFrame)
  // ID of the shared memory buffer containing the pixels.
  IPC_STRUCT_MEMBER(int, shared_buffer_id)

  // Width of a single row of pixels in bytes.
  IPC_STRUCT_MEMBER(int, bytes_per_row)

  // Captured region.
  IPC_STRUCT_MEMBER(std::vector<webrtc::DesktopRect>, dirty_region)

  // Dimensions of the buffer in pixels.
  IPC_STRUCT_MEMBER(webrtc::DesktopSize, dimensions)

  // Time spent in capture. Unit is in milliseconds.
  IPC_STRUCT_MEMBER(int64_t, capture_time_ms)

  // Latest event timestamp supplied by the client for performance tracking.
  IPC_STRUCT_MEMBER(int64_t, latest_event_timestamp)

  // DPI for this frame.
  IPC_STRUCT_MEMBER(webrtc::DesktopVector, dpi)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(webrtc::DesktopCapturer::Result,
                          webrtc::DesktopCapturer::Result::MAX_VALUE)

// Notifies the network process that a shared buffer has been created.
IPC_MESSAGE_CONTROL2(ChromotingDesktopNetworkMsg_CaptureResult,
                     webrtc::DesktopCapturer::Result /* result */,
                     SerializedDesktopFrame /* frame */)

// Carries a cursor share update from the desktop session agent to the client.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_MouseCursor,
                     webrtc::MouseCursor /* cursor */ )

// Carries a clipboard event from the desktop session agent to the client.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_InjectClipboardEvent,
                     std::string /* serialized_event */ )

IPC_ENUM_TRAITS_MAX_VALUE(remoting::protocol::ErrorCode,
                          remoting::protocol::ERROR_CODE_MAX)

// Requests the network process to terminate the client session.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_DisconnectSession,
                     remoting::protocol::ErrorCode /* error */)

// Carries an audio packet from the desktop session agent to the client.
// |serialized_packet| is a serialized AudioPacket.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_AudioPacket,
                     std::string /* serialized_packet */ )

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the desktop process.

// Passes the client session data to the desktop session agent and starts it.
// This must be the first message received from the host.
IPC_MESSAGE_CONTROL3(ChromotingNetworkDesktopMsg_StartSessionAgent,
                     std::string /* authenticated_jid */,
                     remoting::ScreenResolution /* resolution */,
                     bool /* virtual_terminal */)

IPC_MESSAGE_CONTROL0(ChromotingNetworkDesktopMsg_CaptureFrame)

// Carries a clipboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                     std::string /* serialized_event */ )

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::KeyEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                     std::string /* serialized_event */ )

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TextEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectTextEvent,
                     std::string /* serialized_event */ )

// Carries a mouse event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::MouseEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                     std::string /* serialized_event */ )

// Carries a touch event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TouchEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectTouchEvent,
                     std::string /* serialized_event */ )

// Changes the screen resolution in the desktop session.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_SetScreenResolution,
                     remoting::ScreenResolution /* resolution */)

//---------------------------------------------------------------------
// Chromoting messages sent from the remote_security_key process to the
// network process.

// The array of bytes representing a security key request to be sent to the
// remote client.
IPC_MESSAGE_CONTROL1(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                     std::string /* request bytes */)

//---------------------------------------------------------
// Chromoting messages sent from the network process to the remote_security_key
// process.  The network process uses two types of IPC channels to communicate
// with the remote_security_key process.  The first is the 'service' channel.
// It uses a hard-coded path known by the client and server classes and its job
// is to create a new, private IPC channel for the client and provide the path
// to that channel over the original IPC channel.  This purpose for this
// mechanism is to allow the network process to service multiple concurrent
// security key requests.  Once a client receives the connection details for
// its private IPC channel, the server channel is reset and can be called by
// another client.
// The second type of IPC channel is strictly used for passing security key
// request and response messages.  It is destroyed once the client disconnects.

// The IPC channel path for this remote_security_key connection.  This message
// is sent from the well-known IPC server channel.
IPC_MESSAGE_CONTROL1(ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionDetails,
                     std::string /* IPC Server path */)

// The array of bytes representing a security key response from the remote
// client.  This message is sent over the per-client IPC channel.
IPC_MESSAGE_CONTROL1(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                     std::string /* response bytes */)
