// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/preconnect.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_transaction_factory.h"
#include "net/log/net_log.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace chrome_browser_net {

void PreconnectOnUIThread(
    const GURL& url,
    const GURL& first_party_for_cookies,
    UrlInfo::ResolutionMotivation motivation,
    int count,
    net::URLRequestContextGetter* getter) {
  // Prewarm connection to Search URL.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PreconnectOnIOThread, url, first_party_for_cookies,
                 motivation, count, make_scoped_refptr(getter)));
  return;
}


void PreconnectOnIOThread(
    const GURL& url,
    const GURL& first_party_for_cookies,
    UrlInfo::ResolutionMotivation motivation,
    int count,
    net::URLRequestContextGetter* getter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    LOG(DFATAL) << "This must be run only on the IO thread.";
    return;
  }
  if (!getter)
    return;
  // We are now commited to doing the async preconnection call.
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectMotivation", motivation,
                            UrlInfo::MAX_MOTIVATED);

  net::URLRequestContext* context = getter->GetURLRequestContext();
  net::HttpTransactionFactory* factory = context->http_transaction_factory();
  net::HttpNetworkSession* session = factory->GetSession();

  std::string user_agent;
  if (context->http_user_agent_settings())
    user_agent = context->http_user_agent_settings()->GetUserAgent();
  net::HttpRequestInfo request_info;
  request_info.url = url;
  request_info.method = "GET";
  request_info.extra_headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                       user_agent);

  net::NetworkDelegate* delegate = context->network_delegate();
  if (delegate->CanEnablePrivacyMode(url, first_party_for_cookies))
    request_info.privacy_mode = net::PRIVACY_MODE_ENABLED;

  // It almost doesn't matter whether we use net::LOWEST or net::HIGHEST
  // priority here, as we won't make a request, and will surrender the created
  // socket to the pool as soon as we can.  However, we would like to mark the
  // speculative socket as such, and IF we use a net::LOWEST priority, and if
  // a navigation asked for a socket (after us) then it would get our socket,
  // and we'd get its later-arriving socket, which might make us record that
  // the speculation didn't help :-/.  By using net::HIGHEST, we ensure that
  // a socket is given to us if "we asked first" and this allows us to mark it
  // as speculative, and better detect stats (if it gets used).
  // TODO(jar): histogram to see how often we accidentally use a previously-
  // unused socket, when a previously used socket was available.
  net::RequestPriority priority = net::HIGHEST;

  // Translate the motivation from UrlRequest motivations to HttpRequest
  // motivations.
  switch (motivation) {
    case UrlInfo::OMNIBOX_MOTIVATED:
      request_info.motivation = net::HttpRequestInfo::OMNIBOX_MOTIVATED;
      break;
    case UrlInfo::LEARNED_REFERAL_MOTIVATED:
      request_info.motivation = net::HttpRequestInfo::PRECONNECT_MOTIVATED;
      break;
    case UrlInfo::MOUSE_OVER_MOTIVATED:
    case UrlInfo::SELF_REFERAL_MOTIVATED:
    case UrlInfo::EARLY_LOAD_MOTIVATED:
      request_info.motivation = net::HttpRequestInfo::EARLY_LOAD_MOTIVATED;
      break;
    default:
      // Other motivations should never happen here.
      NOTREACHED();
      break;
  }

  // Setup the SSL Configuration.
  net::SSLConfig ssl_config;
  session->ssl_config_service()->GetSSLConfig(&ssl_config);
  session->GetNextProtos(&ssl_config.next_protos);

  // All preconnects should perform EV certificate verification.
  ssl_config.verify_ev_cert = true;

  net::HttpStreamFactory* http_stream_factory = session->http_stream_factory();
  http_stream_factory->PreconnectStreams(count, request_info, priority,
                                         ssl_config, ssl_config);
}

}  // namespace chrome_browser_net
