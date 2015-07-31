// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_
#define CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "content/child/worker_task_runner.h"
#include "content/common/bluetooth/bluetooth_device.h"
#include "content/common/bluetooth/bluetooth_error.h"
#include "third_party/WebKit/public/platform/modules/bluetooth/WebBluetooth.h"

namespace base {
class MessageLoop;
class TaskRunner;
}

namespace IPC {
class Message;
}

namespace content {
class ThreadSafeSender;

// Dispatcher for child process threads which communicates to the browser's
// BluetoothDispatcherHost.
//
// Instances are created for each thread as necessary by WebBluetoothImpl.
//
// Incoming IPC messages are received by the BluetoothMessageFilter and
// directed to the thread specific instance of this class.
// Outgoing messages come from WebBluetoothImpl.
class BluetoothDispatcher : public WorkerTaskRunner::Observer {
 public:
  explicit BluetoothDispatcher(ThreadSafeSender* sender);
  ~BluetoothDispatcher() override;

  // Gets or Creates a BluetoothDispatcher for the current thread.
  // |thread_safe_sender| is required when constructing a BluetoothDispatcher.
  static BluetoothDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender);

  // IPC Send and Receiving interface, see IPC::Sender and IPC::Listener.
  bool Send(IPC::Message* msg);
  void OnMessageReceived(const IPC::Message& msg);

  // Corresponding to WebBluetoothImpl methods.
  void requestDevice(blink::WebBluetoothRequestDeviceCallbacks* callbacks);
  void connectGATT(const blink::WebString& device_instance_id,
                   blink::WebBluetoothConnectGATTCallbacks* callbacks);
  void SetBluetoothMockDataSetForTesting(const std::string& name);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

 private:
  // IPC Handlers, see definitions in bluetooth_messages.h.
  void OnRequestDeviceSuccess(int thread_id,
                              int request_id,
                              const BluetoothDevice& device);
  void OnRequestDeviceError(int thread_id,
                            int request_id,
                            BluetoothError error_type);

  void OnConnectGATTSuccess(int thread_id,
                            int request_id,
                            const std::string& message);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  // Tracks device requests sent to browser to match replies with callbacks.
  // Owns callback objects.
  IDMap<blink::WebBluetoothRequestDeviceCallbacks, IDMapOwnPointer>
      pending_requests_;
  // Tracks requests to connect to a device.
  // Owns callback objects.
  IDMap<blink::WebBluetoothConnectGATTCallbacks, IDMapOwnPointer>
      pending_connect_requests_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_BLUETOOTH_BLUETOOTH_DISPATCHER_H_
