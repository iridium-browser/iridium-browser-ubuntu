/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_
#define SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include "perfetto/base/weak_ptr.h"
#include "perfetto/ipc/basic_types.h"
#include "perfetto/tracing/core/consumer.h"
#include "perfetto/tracing/core/service.h"

#include "protos/tracing_service/consumer_port.ipc.h"

namespace perfetto {

namespace ipc {
class Host;
}  // namespace ipc

// Implements the Consumer port of the IPC service. This class proxies requests
// and responses between the core service logic (|svc_|) and remote Consumer(s)
// on the IPC socket, through the methods overriddden from ConsumerPort.
class ConsumerIPCService : public ConsumerPort /* from consumer_port.proto */ {
 public:
  using Service = ::perfetto::Service;  // To avoid collisions w/ ipc::Service.
  explicit ConsumerIPCService(Service* core_service);
  ~ConsumerIPCService() override;

  // ConsumerPort implementation (from .proto IPC definition).
  void EnableTracing(const EnableTracingRequest&,
                     DeferredEnableTracingResponse) override;
  void DisableTracing(const DisableTracingRequest&,
                      DeferredDisableTracingResponse) override;
  void ReadBuffers(const ReadBuffersRequest&,
                   DeferredReadBuffersResponse) override;
  void FreeBuffers(const FreeBuffersRequest&,
                   DeferredFreeBuffersResponse) override;
  void OnClientDisconnected() override;

 private:
  // Acts like a Consumer with the core Service business logic (which doesn't
  // know anything about the remote transport), but all it does is proxying
  // methods to the remote Consumer on the other side of the IPC channel.
  class RemoteConsumer : public Consumer {
   public:
    RemoteConsumer();
    ~RemoteConsumer() override;

    // These methods are called by the |core_service_| business logic. There is
    // no connection here, these methods are posted straight away.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTraceData(std::vector<TracePacket>, bool has_more) override;

    // The interface obtained from the core service business logic through
    // Service::ConnectConsumer(this). This allows to invoke methods for a
    // specific Consumer on the Service business logic.
    std::unique_ptr<Service::ConsumerEndpoint> service_endpoint;

    // After DisableTracing() is invoked, this binds the async callback that
    // allows to stream trace packets back to the client.
    DeferredReadBuffersResponse read_buffers_response;
  };

  ConsumerIPCService(const ConsumerIPCService&) = delete;
  ConsumerIPCService& operator=(const ConsumerIPCService&) = delete;

  // Returns the ConsumerEndpoint in the core business logic that corresponds to
  // the current IPC request.
  RemoteConsumer* GetConsumerForCurrentRequest();

  Service* const core_service_;

  // Maps IPC clients to ConsumerEndpoint instances registered on the
  // |core_service_| business logic.
  std::map<ipc::ClientID, std::unique_ptr<RemoteConsumer>> consumers_;

  base::WeakPtrFactory<ConsumerIPCService> weak_ptr_factory_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_IPC_SERVICE_CONSUMER_IPC_SERVICE_H_
