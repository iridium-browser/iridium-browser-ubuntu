// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/response_providers/html_response_provider_impl.h"

#include "ios/web/public/test/response_providers/response_provider.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace {
std::map<GURL, HtmlResponseProviderImpl::Response> BuildResponseMap(
    const std::map<GURL, std::string>& responses_body,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  std::map<GURL, HtmlResponseProviderImpl::Response> responses;
  for (const std::pair<const GURL, std::string>& pair : responses_body) {
    responses.insert(std::make_pair(
        pair.first, HtmlResponseProviderImpl::Response(pair.second, headers)));
  }
  return responses;
}
}  // namespace

HtmlResponseProviderImpl::Response::Response(
    const std::string& body,
    const scoped_refptr<net::HttpResponseHeaders>& headers)
    : body(body), headers(headers) {}

HtmlResponseProviderImpl::Response::Response(const Response& response)
    : body(response.body), headers(response.headers) {}

HtmlResponseProviderImpl::Response::Response() : body(), headers() {}

HtmlResponseProviderImpl::Response::~Response() {}

HtmlResponseProviderImpl::HtmlResponseProviderImpl() : responses_() {}

// static
HtmlResponseProviderImpl::Response
HtmlResponseProviderImpl::GetRedirectResponse(
    const GURL& destination_url,
    const net::HttpStatusCode& http_status) {
  auto response_headers = web::ResponseProvider::GetRedirectResponseHeaders(
      destination_url.spec(), http_status);
  return HtmlResponseProviderImpl::Response("", response_headers);
}

// static
HtmlResponseProviderImpl::Response HtmlResponseProviderImpl::GetSimpleResponse(
    const std::string& body) {
  auto response_headers =
      web::ResponseProvider::GetResponseHeaders("text/html", net::HTTP_OK);
  return HtmlResponseProviderImpl::Response(body, response_headers);
}

HtmlResponseProviderImpl::HtmlResponseProviderImpl(
    const std::map<GURL, std::string>& responses)
    : responses_(BuildResponseMap(
          responses,
          web::ResponseProvider::GetDefaultResponseHeaders())) {}

HtmlResponseProviderImpl::HtmlResponseProviderImpl(
    const std::map<GURL, HtmlResponseProviderImpl::Response>& responses)
    : responses_(responses) {}

HtmlResponseProviderImpl::~HtmlResponseProviderImpl() {}

bool HtmlResponseProviderImpl::CanHandleRequest(
    const web::ResponseProvider::Request& request) {
  return responses_.find(request.url) != responses_.end();
}

void HtmlResponseProviderImpl::GetResponseHeadersAndBody(
    const web::ResponseProvider::Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const Response& response = responses_.at(request.url);
  *headers = response.headers;
  *response_body = response.body;
}
