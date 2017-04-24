// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/beacon/NavigatorBeacon.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/ArrayBufferViewOrBlobOrStringOrFormData.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/FormData.h"
#include "core/loader/PingLoader.h"
#include "platform/loader/fetch/FetchUtils.h"

namespace blink {

NavigatorBeacon::NavigatorBeacon(Navigator& navigator)
    : Supplement<Navigator>(navigator), m_transmittedBytes(0) {}

NavigatorBeacon::~NavigatorBeacon() {}

DEFINE_TRACE(NavigatorBeacon) {
  Supplement<Navigator>::trace(visitor);
}

const char* NavigatorBeacon::supplementName() {
  return "NavigatorBeacon";
}

NavigatorBeacon& NavigatorBeacon::from(Navigator& navigator) {
  NavigatorBeacon* supplement = static_cast<NavigatorBeacon*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorBeacon(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

bool NavigatorBeacon::canSendBeacon(ExecutionContext* context,
                                    const KURL& url,
                                    ExceptionState& exceptionState) {
  if (!url.isValid()) {
    exceptionState.throwDOMException(
        SyntaxError, "The URL argument is ill-formed or unsupported.");
    return false;
  }
  // For now, only support HTTP and related.
  if (!url.protocolIsInHTTPFamily()) {
    exceptionState.throwDOMException(
        SyntaxError, "Beacons are only supported over HTTP(S).");
    return false;
  }
  // FIXME: CSP is not enforced on redirects, crbug.com/372197
  if (!ContentSecurityPolicy::shouldBypassMainWorld(context) &&
      !context->contentSecurityPolicy()->allowConnectToSource(url)) {
    // We can safely expose the URL to JavaScript, as these checks happen
    // synchronously before redirection. JavaScript receives no new information.
    exceptionState.throwSecurityError(
        "Refused to send beacon to '" + url.elidedString() +
        "' because it violates the document's Content Security Policy.");
    return false;
  }

  // If detached from frame, do not allow sending a Beacon.
  if (!supplementable()->frame())
    return false;

  return true;
}

int NavigatorBeacon::maxAllowance() const {
  DCHECK(supplementable()->frame());
  const Settings* settings = supplementable()->frame()->settings();
  if (settings) {
    int maxAllowed = settings->getMaxBeaconTransmission();
    if (maxAllowed < m_transmittedBytes)
      return 0;
    return maxAllowed - m_transmittedBytes;
  }
  return m_transmittedBytes;
}

void NavigatorBeacon::addTransmittedBytes(int sentBytes) {
  DCHECK_GE(sentBytes, 0);
  m_transmittedBytes += sentBytes;
}

bool NavigatorBeacon::sendBeacon(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& urlstring,
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exceptionState) {
  return NavigatorBeacon::from(navigator).sendBeaconImpl(scriptState, urlstring,
                                                         data, exceptionState);
}

bool NavigatorBeacon::sendBeaconImpl(
    ScriptState* scriptState,
    const String& urlstring,
    const ArrayBufferViewOrBlobOrStringOrFormData& data,
    ExceptionState& exceptionState) {
  ExecutionContext* context = scriptState->getExecutionContext();
  KURL url = context->completeURL(urlstring);
  if (!canSendBeacon(context, url, exceptionState))
    return false;

  int allowance = maxAllowance();
  int bytes = 0;
  bool allowed;

  if (data.isArrayBufferView()) {
    allowed = PingLoader::sendBeacon(supplementable()->frame(), allowance, url,
                                     data.getAsArrayBufferView(), bytes);
  } else if (data.isBlob()) {
    Blob* blob = data.getAsBlob();
    if (!FetchUtils::isSimpleContentType(AtomicString(blob->type()))) {
      UseCounter::count(context,
                        UseCounter::SendBeaconWithNonSimpleContentType);
      if (RuntimeEnabledFeatures::
              sendBeaconThrowForBlobWithNonSimpleTypeEnabled()) {
        exceptionState.throwSecurityError(
            "sendBeacon() with a Blob whose type is not CORS-safelisted MIME "
            "type is disallowed experimentally. See http://crbug.com/490015 "
            "for details.");
        return false;
      }
    }
    allowed = PingLoader::sendBeacon(supplementable()->frame(), allowance, url,
                                     blob, bytes);
  } else if (data.isString()) {
    allowed = PingLoader::sendBeacon(supplementable()->frame(), allowance, url,
                                     data.getAsString(), bytes);
  } else if (data.isFormData()) {
    allowed = PingLoader::sendBeacon(supplementable()->frame(), allowance, url,
                                     data.getAsFormData(), bytes);
  } else {
    allowed = PingLoader::sendBeacon(supplementable()->frame(), allowance, url,
                                     String(), bytes);
  }

  if (allowed) {
    addTransmittedBytes(bytes);
    return true;
  }

  UseCounter::count(context, UseCounter::SendBeaconQuotaExceeded);
  return false;
}

}  // namespace blink
