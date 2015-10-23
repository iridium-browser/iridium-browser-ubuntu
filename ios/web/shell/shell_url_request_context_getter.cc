// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/shell_url_request_context_getter.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/shell/shell_network_delegate.h"
#include "net/base/cache_type.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service_ios.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace web {

ShellURLRequestContextGetter::ShellURLRequestContextGetter(
    const base::FilePath& base_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& cache_task_runner)
    : base_path_(base_path),
      file_task_runner_(file_task_runner),
      network_task_runner_(network_task_runner),
      cache_task_runner_(cache_task_runner),
      proxy_config_service_(new net::ProxyConfigServiceIOS),
      net_log_(new net::NetLog()) {
}

ShellURLRequestContextGetter::~ShellURLRequestContextGetter() {
}

net::URLRequestContext* ShellURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!url_request_context_) {
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_net_log(net_log_.get());
    DCHECK(!network_delegate_.get());
    network_delegate_.reset(new ShellNetworkDelegate);
    url_request_context_->set_network_delegate(network_delegate_.get());

    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));

    // Setup the cookie store.
    base::FilePath cookie_path;
    bool cookie_path_found = PathService::Get(base::DIR_APP_DATA, &cookie_path);
    DCHECK(cookie_path_found);
    cookie_path = cookie_path.Append("WebShell").Append("Cookies");
    scoped_refptr<net::CookieMonster::PersistentCookieStore> persistent_store =
        new net::SQLitePersistentCookieStore(
            cookie_path, network_task_runner_,
            web::WebThread::GetBlockingPool()->GetSequencedTaskRunner(
                web::WebThread::GetBlockingPool()->GetSequenceToken()),
            true, nullptr);
    scoped_refptr<net::CookieStoreIOS> cookie_store =
        new net::CookieStoreIOS(persistent_store.get());
    storage_->set_cookie_store(cookie_store.get());
    net::CookieStoreIOS::SwitchSynchronizedStore(nullptr, cookie_store.get());

    std::string user_agent = web::GetWebClient()->GetUserAgent(false);
    storage_->set_http_user_agent_settings(
        new net::StaticHttpUserAgentSettings("en-us,en", user_agent));
    storage_->set_proxy_service(
        net::ProxyService::CreateUsingSystemProxyResolver(
            proxy_config_service_.release(), 0,
            url_request_context_->net_log()));
    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
    storage_->set_cert_verifier(net::CertVerifier::CreateDefault());

    net::TransportSecurityState* transport_security_state =
        new net::TransportSecurityState();
    storage_->set_transport_security_state(transport_security_state);
    transport_security_persister_.reset(new net::TransportSecurityPersister(
        transport_security_state, base_path_, file_task_runner_, false));
    storage_->set_channel_id_service(make_scoped_ptr(
        new net::ChannelIDService(new net::DefaultChannelIDStore(nullptr),
                                  base::WorkerPool::GetTaskRunner(true))));
    storage_->set_http_server_properties(scoped_ptr<net::HttpServerProperties>(
        new net::HttpServerPropertiesImpl()));

    scoped_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(
            url_request_context_->net_log()));
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));
    storage_->set_host_resolver(host_resolver.Pass());

    net::HttpNetworkSession::Params network_session_params;
    network_session_params.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_params.transport_security_state =
        url_request_context_->transport_security_state();
    network_session_params.channel_id_service =
        url_request_context_->channel_id_service();
    network_session_params.net_log = url_request_context_->net_log();
    network_session_params.proxy_service =
        url_request_context_->proxy_service();
    network_session_params.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_params.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_params.network_delegate = network_delegate_.get();
    network_session_params.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_params.host_resolver =
        url_request_context_->host_resolver();

    base::FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    net::HttpCache::DefaultBackend* main_backend =
        new net::HttpCache::DefaultBackend(net::DISK_CACHE,
                                           net::CACHE_BACKEND_DEFAULT,
                                           cache_path, 0, cache_task_runner_);

    net::HttpCache* main_cache =
        new net::HttpCache(network_session_params, main_backend);
    storage_->set_http_transaction_factory(main_cache);

    scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());
    bool set_protocol =
        job_factory->SetProtocolHandler("data", new net::DataProtocolHandler);
    DCHECK(set_protocol);

    storage_->set_job_factory(job_factory.release());
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ShellURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

}  // namespace web
