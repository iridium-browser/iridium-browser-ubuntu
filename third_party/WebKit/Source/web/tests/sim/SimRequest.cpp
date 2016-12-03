// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimRequest.h"

#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "web/tests/sim/SimNetwork.h"

namespace blink {

SimRequest::SimRequest(String url, String mimeType)
    : m_url(url)
    , m_loader(nullptr)
    , m_client(nullptr)
    , m_totalEncodedDataLength(0)
    , m_isReady(false)
{
    KURL fullUrl(ParsedURLString, url);
    WebURLResponse response(fullUrl);
    response.setMIMEType(mimeType);
    response.setHTTPStatusCode(200);
    Platform::current()->getURLLoaderMockFactory()->registerURL(fullUrl, response, "");
    SimNetwork::current().addRequest(*this);
}

SimRequest::~SimRequest()
{
    DCHECK(!m_isReady);
}

void SimRequest::didReceiveResponse(WebURLLoaderClient* client, WebURLLoader* loader, const WebURLResponse& response)
{
    m_client = client;
    m_loader = loader;
    m_response = response;
    m_isReady = true;
}

void SimRequest::didFail(const WebURLError& error)
{
    m_error = error;
}

void SimRequest::start()
{
    SimNetwork::current().servePendingRequests();
    DCHECK(m_isReady);
    m_client->didReceiveResponse(m_loader, m_response);
}

void SimRequest::write(const String& data)
{
    DCHECK(m_isReady);
    DCHECK(!m_error.reason);
    m_totalEncodedDataLength += data.length();
    m_client->didReceiveData(m_loader, data.utf8().data(), data.length(), data.length(), data.length());
}

void SimRequest::finish()
{
    DCHECK(m_isReady);
    if (m_error.reason) {
        m_client->didFail(m_loader, m_error);
    } else {
        // TODO(esprehn): Is claiming a request time of 0 okay for tests?
        m_client->didFinishLoading(m_loader, 0, m_totalEncodedDataLength);
    }
    reset();
}

void SimRequest::complete(const String& data)
{
    start();
    if (!data.isEmpty())
        write(data);
    finish();
}

void SimRequest::reset()
{
    m_isReady = false;
    m_client = nullptr;
    m_loader = nullptr;
    SimNetwork::current().removeRequest(*this);
}

} // namespace blink
