// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "net/cookies/canonical_cookie.h"

namespace content {

class RenderFrameHostImpl;

namespace devtools {
namespace network {

class NetworkHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;
  NetworkHandler();
  virtual ~NetworkHandler();

  void SetRenderFrameHost(RenderFrameHostImpl* host);
  void SetClient(std::unique_ptr<Client> client);

  Response Enable(const int* max_total_size,
                  const int* max_resource_size);
  Response Disable();
  Response ClearBrowserCache();
  Response ClearBrowserCookies();
  Response GetCookies(DevToolsCommandId command_id);
  Response DeleteCookie(DevToolsCommandId command_id,
                        const std::string& cookie_name,
                        const std::string& url);

  Response SetCookie(DevToolsCommandId command_id,
      const std::string& url,
      const std::string& name,
      const std::string& value,
      const std::string* domain,
      const std::string* path,
      bool* secure,
      bool* http_only,
      const std::string* same_site,
      double* expires);

  Response CanEmulateNetworkConditions(bool* result);
  Response EmulateNetworkConditions(bool offline,
                                    double latency,
                                    double download_throughput,
                                    double upload_throughput,
                                    const std::string* connection_type);
  Response GetCertificateDetails(int certificate_id,
                                 scoped_refptr<CertificateDetails>* result);
  Response ShowCertificateViewer(int certificate_id);

 private:
  void SendGetCookiesResponse(
      DevToolsCommandId command_id,
      const net::CookieList& cookie_list);
  void SendDeleteCookieResponse(DevToolsCommandId command_id);
  void SendSetCookieResponse(DevToolsCommandId command_id, bool success);

  RenderFrameHostImpl* host_;
  std::unique_ptr<Client> client_;
  bool enabled_;
  base::WeakPtrFactory<NetworkHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHandler);
};

}  // namespace network
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NETWORK_HANDLER_H_
