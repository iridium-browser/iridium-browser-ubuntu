// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/FetchUtils.h"

#include "platform/HTTPNames.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "wtf/HashSet.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

bool isHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

class ForbiddenHeaderNames {
  WTF_MAKE_NONCOPYABLE(ForbiddenHeaderNames);
  USING_FAST_MALLOC(ForbiddenHeaderNames);

 public:
  bool has(const String& name) const {
    return m_fixedNames.contains(name) ||
           name.startsWith(m_proxyHeaderPrefix, TextCaseASCIIInsensitive) ||
           name.startsWith(m_secHeaderPrefix, TextCaseASCIIInsensitive);
  }

  static const ForbiddenHeaderNames& get();

 private:
  ForbiddenHeaderNames();

  String m_proxyHeaderPrefix;
  String m_secHeaderPrefix;
  HashSet<String, CaseFoldingHash> m_fixedNames;
};

ForbiddenHeaderNames::ForbiddenHeaderNames()
    : m_proxyHeaderPrefix("proxy-"), m_secHeaderPrefix("sec-") {
  m_fixedNames = {
      "accept-charset",
      "accept-encoding",
      "access-control-request-headers",
      "access-control-request-method",
      "connection",
      "content-length",
      "cookie",
      "cookie2",
      "date",
      "dnt",
      "expect",
      "host",
      "keep-alive",
      "origin",
      "referer",
      "te",
      "trailer",
      "transfer-encoding",
      "upgrade",
      "user-agent",
      "via",
  };
}

const ForbiddenHeaderNames& ForbiddenHeaderNames::get() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const ForbiddenHeaderNames, instance,
                                  new ForbiddenHeaderNames);
  return instance;
}

}  // namespace

bool FetchUtils::isSimpleMethod(const String& method) {
  // http://fetch.spec.whatwg.org/#simple-method
  // "A simple method is a method that is `GET`, `HEAD`, or `POST`."
  return method == "GET" || method == "HEAD" || method == "POST";
}

bool FetchUtils::isSimpleHeader(const AtomicString& name,
                                const AtomicString& value) {
  // http://fetch.spec.whatwg.org/#simple-header
  // "A simple header is a header whose name is either one of `Accept`,
  // `Accept-Language`, and `Content-Language`, or whose name is
  // `Content-Type` and value, once parsed, is one of
  // `application/x-www-form-urlencoded`, `multipart/form-data`, and
  // `text/plain`."
  // Treat 'Save-Data' as a simple header, since it is added by Chrome when
  // Data Saver feature is enabled.
  // Treat inspector headers as a simple headers, since they are added by blink
  // when the inspector is open.

  if (equalIgnoringCase(name, "accept") ||
      equalIgnoringCase(name, "accept-language") ||
      equalIgnoringCase(name, "content-language") ||
      equalIgnoringCase(
          name, HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id) ||
      equalIgnoringCase(name, HTTPNames::X_DevTools_Request_Id) ||
      equalIgnoringCase(name, "save-data"))
    return true;

  if (equalIgnoringCase(name, "content-type"))
    return isSimpleContentType(value);

  return false;
}

bool FetchUtils::isSimpleContentType(const AtomicString& mediaType) {
  AtomicString mimeType = extractMIMETypeFromMediaType(mediaType);
  return equalIgnoringCase(mimeType, "application/x-www-form-urlencoded") ||
         equalIgnoringCase(mimeType, "multipart/form-data") ||
         equalIgnoringCase(mimeType, "text/plain");
}

bool FetchUtils::isSimpleRequest(const String& method,
                                 const HTTPHeaderMap& headerMap) {
  if (!isSimpleMethod(method))
    return false;

  for (const auto& header : headerMap) {
    // Preflight is required for MIME types that can not be sent via form
    // submission.
    if (!isSimpleHeader(header.key, header.value))
      return false;
  }

  return true;
}

bool FetchUtils::isForbiddenMethod(const String& method) {
  // http://fetch.spec.whatwg.org/#forbidden-method
  // "A forbidden method is a method that is a byte case-insensitive match"
  //  for one of `CONNECT`, `TRACE`, and `TRACK`."
  return equalIgnoringCase(method, "TRACE") ||
         equalIgnoringCase(method, "TRACK") ||
         equalIgnoringCase(method, "CONNECT");
}

bool FetchUtils::isForbiddenHeaderName(const String& name) {
  // http://fetch.spec.whatwg.org/#forbidden-header-name
  // "A forbidden header name is a header names that is one of:
  //   `Accept-Charset`, `Accept-Encoding`, `Access-Control-Request-Headers`,
  //   `Access-Control-Request-Method`, `Connection`,
  //   `Content-Length, Cookie`, `Cookie2`, `Date`, `DNT`, `Expect`, `Host`,
  //   `Keep-Alive`, `Origin`, `Referer`, `TE`, `Trailer`,
  //   `Transfer-Encoding`, `Upgrade`, `User-Agent`, `Via`
  // or starts with `Proxy-` or `Sec-` (including when it is just `Proxy-` or
  // `Sec-`)."

  return ForbiddenHeaderNames::get().has(name);
}

bool FetchUtils::isForbiddenResponseHeaderName(const String& name) {
  // http://fetch.spec.whatwg.org/#forbidden-response-header-name
  // "A forbidden response header name is a header name that is one of:
  // `Set-Cookie`, `Set-Cookie2`"

  return equalIgnoringCase(name, "set-cookie") ||
         equalIgnoringCase(name, "set-cookie2");
}

bool FetchUtils::isSimpleOrForbiddenRequest(const String& method,
                                            const HTTPHeaderMap& headerMap) {
  if (!isSimpleMethod(method))
    return false;

  for (const auto& header : headerMap) {
    if (!isSimpleHeader(header.key, header.value) &&
        !isForbiddenHeaderName(header.key))
      return false;
  }

  return true;
}

AtomicString FetchUtils::normalizeMethod(const AtomicString& method) {
  // https://fetch.spec.whatwg.org/#concept-method-normalize

  // We place GET and POST first because they are more commonly used than
  // others.
  const char* const methods[] = {
      "GET", "POST", "DELETE", "HEAD", "OPTIONS", "PUT",
  };

  for (const auto& known : methods) {
    if (equalIgnoringCase(method, known)) {
      // Don't bother allocating a new string if it's already all
      // uppercase.
      return method == known ? method : known;
    }
  }
  return method;
}

String FetchUtils::normalizeHeaderValue(const String& value) {
  // https://fetch.spec.whatwg.org/#concept-header-value-normalize
  // Strip leading and trailing whitespace from header value.
  // HTTP whitespace bytes are 0x09, 0x0A, 0x0D, and 0x20.

  return value.stripWhiteSpace(isHTTPWhitespace);
}

}  // namespace blink
