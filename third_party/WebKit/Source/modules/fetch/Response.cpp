// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Response.h"

#include <memory>
#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8ArrayBufferView.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8FormData.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8URLSearchParams.h"
#include "bindings/modules/v8/ByteStringSequenceSequenceOrDictionaryOrHeaders.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/URLSearchParams.h"
#include "core/fileapi/Blob.h"
#include "core/frame/UseCounter.h"
#include "core/html/FormData.h"
#include "core/streams/ReadableStreamOperations.h"
#include "modules/fetch/BlobBytesConsumer.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/loader/fetch/FetchUtils.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/NetworkUtils.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "wtf/RefPtr.h"

namespace blink {

namespace {

FetchResponseData* createFetchResponseDataFromWebResponse(
    ScriptState* scriptState,
    const WebServiceWorkerResponse& webResponse) {
  FetchResponseData* response = nullptr;
  if (webResponse.status() > 0)
    response = FetchResponseData::create();
  else
    response = FetchResponseData::createNetworkErrorResponse();

  const WebVector<WebURL>& webURLList = webResponse.urlList();
  Vector<KURL> urlList(webURLList.size());
  std::transform(webURLList.begin(), webURLList.end(), urlList.begin(),
                 [](const WebURL& url) { return url; });
  response->setURLList(urlList);
  response->setStatus(webResponse.status());
  response->setStatusMessage(webResponse.statusText());
  response->setResponseTime(webResponse.responseTime());
  response->setCacheStorageCacheName(webResponse.cacheStorageCacheName());

  for (HTTPHeaderMap::const_iterator i = webResponse.headers().begin(),
                                     end = webResponse.headers().end();
       i != end; ++i) {
    response->headerList()->append(i->key, i->value);
  }

  response->replaceBodyStreamBuffer(new BodyStreamBuffer(
      scriptState, new BlobBytesConsumer(scriptState->getExecutionContext(),
                                         webResponse.blobDataHandle())));

  // Filter the response according to |webResponse|'s ResponseType.
  switch (webResponse.responseType()) {
    case WebServiceWorkerResponseTypeBasic:
      response = response->createBasicFilteredResponse();
      break;
    case WebServiceWorkerResponseTypeCORS: {
      HTTPHeaderSet headerNames;
      for (const auto& header : webResponse.corsExposedHeaderNames())
        headerNames.insert(String(header));
      response = response->createCORSFilteredResponse(headerNames);
      break;
    }
    case WebServiceWorkerResponseTypeOpaque:
      response = response->createOpaqueFilteredResponse();
      break;
    case WebServiceWorkerResponseTypeOpaqueRedirect:
      response = response->createOpaqueRedirectFilteredResponse();
      break;
    case WebServiceWorkerResponseTypeDefault:
      break;
    case WebServiceWorkerResponseTypeError:
      ASSERT(response->getType() == FetchResponseData::ErrorType);
      break;
  }

  return response;
}

// Checks whether |status| is a null body status.
// Spec: https://fetch.spec.whatwg.org/#null-body-status
bool isNullBodyStatus(unsigned short status) {
  if (status == 101 || status == 204 || status == 205 || status == 304)
    return true;

  return false;
}

// Check whether |statusText| is a ByteString and
// matches the Reason-Phrase token production.
// RFC 2616: https://tools.ietf.org/html/rfc2616
// RFC 7230: https://tools.ietf.org/html/rfc7230
// "reason-phrase = *( HTAB / SP / VCHAR / obs-text )"
bool isValidReasonPhrase(const String& statusText) {
  for (unsigned i = 0; i < statusText.length(); ++i) {
    UChar c = statusText[i];
    if (!(c == 0x09                      // HTAB
          || (0x20 <= c && c <= 0x7E)    // SP / VCHAR
          || (0x80 <= c && c <= 0xFF)))  // obs-text
      return false;
  }
  return true;
}

}  // namespace

Response* Response::create(ScriptState* scriptState,
                           ExceptionState& exceptionState) {
  return create(scriptState, nullptr, String(), ResponseInit(), exceptionState);
}

Response* Response::create(ScriptState* scriptState,
                           ScriptValue bodyValue,
                           const ResponseInit& init,
                           ExceptionState& exceptionState) {
  v8::Local<v8::Value> body = bodyValue.v8Value();
  v8::Isolate* isolate = scriptState->isolate();
  ExecutionContext* executionContext = scriptState->getExecutionContext();

  BodyStreamBuffer* bodyBuffer = nullptr;
  String contentType;
  if (bodyValue.isUndefined() || bodyValue.isNull()) {
    // Note: The IDL processor cannot handle this situation. See
    // https://crbug.com/335871.
  } else if (V8Blob::hasInstance(body, isolate)) {
    Blob* blob = V8Blob::toImpl(body.As<v8::Object>());
    bodyBuffer = new BodyStreamBuffer(
        scriptState,
        new BlobBytesConsumer(executionContext, blob->blobDataHandle()));
    contentType = blob->type();
  } else if (body->IsArrayBuffer()) {
    bodyBuffer = new BodyStreamBuffer(
        scriptState, new FormDataBytesConsumer(
                         V8ArrayBuffer::toImpl(body.As<v8::Object>())));
  } else if (body->IsArrayBufferView()) {
    bodyBuffer = new BodyStreamBuffer(
        scriptState, new FormDataBytesConsumer(
                         V8ArrayBufferView::toImpl(body.As<v8::Object>())));
  } else if (V8FormData::hasInstance(body, isolate)) {
    RefPtr<EncodedFormData> formData =
        V8FormData::toImpl(body.As<v8::Object>())->encodeMultiPartFormData();
    // Here we handle formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    contentType = AtomicString("multipart/form-data; boundary=") +
                  formData->boundary().data();
    bodyBuffer = new BodyStreamBuffer(
        scriptState,
        new FormDataBytesConsumer(executionContext, formData.release()));
  } else if (V8URLSearchParams::hasInstance(body, isolate)) {
    RefPtr<EncodedFormData> formData =
        V8URLSearchParams::toImpl(body.As<v8::Object>())->toEncodedFormData();
    bodyBuffer = new BodyStreamBuffer(
        scriptState,
        new FormDataBytesConsumer(executionContext, formData.release()));
    contentType = "application/x-www-form-urlencoded;charset=UTF-8";
  } else if (ReadableStreamOperations::isReadableStream(scriptState,
                                                        bodyValue)) {
    UseCounter::count(executionContext,
                      UseCounter::FetchResponseConstructionWithStream);
    bodyBuffer = new BodyStreamBuffer(scriptState, bodyValue);
  } else {
    String string = toUSVString(isolate, body, exceptionState);
    if (exceptionState.hadException())
      return nullptr;
    bodyBuffer =
        new BodyStreamBuffer(scriptState, new FormDataBytesConsumer(string));
    contentType = "text/plain;charset=UTF-8";
  }
  return create(scriptState, bodyBuffer, contentType, init, exceptionState);
}

Response* Response::create(ScriptState* scriptState,
                           BodyStreamBuffer* body,
                           const String& contentType,
                           const ResponseInit& init,
                           ExceptionState& exceptionState) {
  unsigned short status = init.status();

  // "1. If |init|'s status member is not in the range 200 to 599, inclusive,
  //     throw a RangeError."
  if (status < 200 || 599 < status) {
    exceptionState.throwRangeError(
        ExceptionMessages::indexOutsideRange<unsigned>(
            "status", status, 200, ExceptionMessages::InclusiveBound, 599,
            ExceptionMessages::InclusiveBound));
    return nullptr;
  }

  // "2. If |init|'s statusText member does not match the Reason-Phrase
  // token production, throw a TypeError."
  if (!isValidReasonPhrase(init.statusText())) {
    exceptionState.throwTypeError("Invalid statusText");
    return nullptr;
  }

  // "3. Let |r| be a new Response object, associated with a new response,
  // Headers object, and Body object."
  Response* r = new Response(scriptState->getExecutionContext());

  // "4. Set |r|'s response's status to |init|'s status member."
  r->m_response->setStatus(init.status());

  // "5. Set |r|'s response's status message to |init|'s statusText member."
  r->m_response->setStatusMessage(AtomicString(init.statusText()));

  // "6. If |init|'s headers member is present, run these substeps:"
  if (init.hasHeaders()) {
    // "1. Empty |r|'s response's header list."
    r->m_response->headerList()->clearList();
    // "2. Fill |r|'s Headers object with |init|'s headers member. Rethrow
    // any exceptions."
    if (init.headers().isByteStringSequenceSequence()) {
      r->m_headers->fillWith(init.headers().getAsByteStringSequenceSequence(),
                             exceptionState);
    } else if (init.headers().isDictionary()) {
      r->m_headers->fillWith(init.headers().getAsDictionary(), exceptionState);
    } else if (init.headers().isHeaders()) {
      r->m_headers->fillWith(init.headers().getAsHeaders(), exceptionState);
    }
    if (exceptionState.hadException())
      return nullptr;
  }
  // "7. If body is given, run these substeps:"
  if (body) {
    // "1. If |init|'s status member is a null body status, throw a
    //     TypeError."
    // "2. Let |stream| and |Content-Type| be the result of extracting
    //     body."
    // "3. Set |r|'s response's body to |stream|."
    // "4. If |Content-Type| is non-null and |r|'s response's header list
    // contains no header named `Content-Type`, append `Content-Type`/
    // |Content-Type| to |r|'s response's header list."
    // https://fetch.spec.whatwg.org/#concept-bodyinit-extract
    // Step 3, Blob:
    // "If object's type attribute is not the empty byte sequence, set
    // Content-Type to its value."
    if (isNullBodyStatus(status)) {
      exceptionState.throwTypeError(
          "Response with null body status cannot have body");
      return nullptr;
    }
    r->m_response->replaceBodyStreamBuffer(body);
    r->refreshBody(scriptState);
    if (!contentType.isEmpty() &&
        !r->m_response->headerList()->has("Content-Type"))
      r->m_response->headerList()->append("Content-Type", contentType);
  }

  // "8. Set |r|'s MIME type to the result of extracting a MIME type
  // from |r|'s response's header list."
  r->m_response->setMIMEType(r->m_response->headerList()->extractMIMEType());

  // "9. Return |r|."
  return r;
}

Response* Response::create(ExecutionContext* context,
                           FetchResponseData* response) {
  return new Response(context, response);
}

Response* Response::create(ScriptState* scriptState,
                           const WebServiceWorkerResponse& webResponse) {
  FetchResponseData* responseData =
      createFetchResponseDataFromWebResponse(scriptState, webResponse);
  return new Response(scriptState->getExecutionContext(), responseData);
}

Response* Response::error(ScriptState* scriptState) {
  FetchResponseData* responseData =
      FetchResponseData::createNetworkErrorResponse();
  Response* r = new Response(scriptState->getExecutionContext(), responseData);
  r->m_headers->setGuard(Headers::ImmutableGuard);
  return r;
}

Response* Response::redirect(ScriptState* scriptState,
                             const String& url,
                             unsigned short status,
                             ExceptionState& exceptionState) {
  KURL parsedURL = scriptState->getExecutionContext()->completeURL(url);
  if (!parsedURL.isValid()) {
    exceptionState.throwTypeError("Failed to parse URL from " + url);
    return nullptr;
  }

  if (!NetworkUtils::isRedirectResponseCode(status)) {
    exceptionState.throwRangeError("Invalid status code");
    return nullptr;
  }

  Response* r = new Response(scriptState->getExecutionContext());
  r->m_headers->setGuard(Headers::ImmutableGuard);
  r->m_response->setStatus(status);
  r->m_response->headerList()->set("Location", parsedURL);

  return r;
}

String Response::type() const {
  // "The type attribute's getter must return response's type."
  switch (m_response->getType()) {
    case FetchResponseData::BasicType:
      return "basic";
    case FetchResponseData::CORSType:
      return "cors";
    case FetchResponseData::DefaultType:
      return "default";
    case FetchResponseData::ErrorType:
      return "error";
    case FetchResponseData::OpaqueType:
      return "opaque";
    case FetchResponseData::OpaqueRedirectType:
      return "opaqueredirect";
  }
  ASSERT_NOT_REACHED();
  return "";
}

String Response::url() const {
  // "The url attribute's getter must return the empty string if response's
  // url is null and response's url, serialized with the exclude fragment
  // flag set, otherwise."
  const KURL* responseURL = m_response->url();
  if (!responseURL)
    return emptyString;
  if (!responseURL->hasFragmentIdentifier())
    return *responseURL;
  KURL url(*responseURL);
  url.removeFragmentIdentifier();
  return url;
}

bool Response::redirected() const {
  return m_response->urlList().size() > 1;
}

unsigned short Response::status() const {
  // "The status attribute's getter must return response's status."
  return m_response->status();
}

bool Response::ok() const {
  // "The ok attribute's getter must return true
  // if response's status is in the range 200 to 299, and false otherwise."
  return FetchUtils::isOkStatus(status());
}

String Response::statusText() const {
  // "The statusText attribute's getter must return response's status message."
  return m_response->statusMessage();
}

Headers* Response::headers() const {
  // "The headers attribute's getter must return the associated Headers object."
  return m_headers;
}

Response* Response::clone(ScriptState* scriptState,
                          ExceptionState& exceptionState) {
  if (isBodyLocked() || bodyUsed()) {
    exceptionState.throwTypeError("Response body is already used");
    return nullptr;
  }

  FetchResponseData* response = m_response->clone(scriptState);
  refreshBody(scriptState);
  Headers* headers = Headers::create(response->headerList());
  headers->setGuard(m_headers->getGuard());
  return new Response(getExecutionContext(), response, headers);
}

bool Response::hasPendingActivity() const {
  if (!getExecutionContext() || getExecutionContext()->isContextDestroyed())
    return false;
  if (!internalBodyBuffer())
    return false;
  if (internalBodyBuffer()->hasPendingActivity())
    return true;
  return Body::hasPendingActivity();
}

void Response::populateWebServiceWorkerResponse(
    WebServiceWorkerResponse& response) {
  m_response->populateWebServiceWorkerResponse(response);
}

Response::Response(ExecutionContext* context)
    : Response(context, FetchResponseData::create()) {}

Response::Response(ExecutionContext* context, FetchResponseData* response)
    : Response(context, response, Headers::create(response->headerList())) {
  m_headers->setGuard(Headers::ResponseGuard);
}

Response::Response(ExecutionContext* context,
                   FetchResponseData* response,
                   Headers* headers)
    : Body(context), m_response(response), m_headers(headers) {
  installBody();
}

bool Response::hasBody() const {
  return m_response->internalBuffer();
}

bool Response::bodyUsed() {
  return internalBodyBuffer() && internalBodyBuffer()->isStreamDisturbed();
}

String Response::mimeType() const {
  return m_response->mimeType();
}

String Response::internalMIMEType() const {
  return m_response->internalMIMEType();
}

const Vector<KURL>& Response::internalURLList() const {
  return m_response->internalURLList();
}

void Response::installBody() {
  if (!internalBodyBuffer())
    return;
  refreshBody(internalBodyBuffer()->scriptState());
}

void Response::refreshBody(ScriptState* scriptState) {
  v8::Local<v8::Value> bodyBuffer = ToV8(internalBodyBuffer(), scriptState);
  v8::Local<v8::Value> response = ToV8(this, scriptState);
  if (response.IsEmpty()) {
    // |toV8| can return an empty handle when the worker is terminating.
    // We don't want the renderer to crash in such cases.
    // TODO(yhirano): Delete this block after the graceful shutdown
    // mechanism is introduced.
    return;
  }
  DCHECK(response->IsObject());
  V8HiddenValue::setHiddenValue(
      scriptState, response.As<v8::Object>(),
      V8HiddenValue::internalBodyBuffer(scriptState->isolate()), bodyBuffer);
}

DEFINE_TRACE(Response) {
  Body::trace(visitor);
  visitor->trace(m_response);
  visitor->trace(m_headers);
}

}  // namespace blink
