/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceResponse_h
#define ResourceResponse_h

#include "platform/PlatformExport.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceLoadInfo.h"
#include "platform/network/ResourceLoadTiming.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponseType.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"

namespace blink {

struct CrossThreadResourceResponseData;

class PLATFORM_EXPORT ResourceResponse final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum HTTPVersion {
    HTTPVersionUnknown,
    HTTPVersion_0_9,
    HTTPVersion_1_0,
    HTTPVersion_1_1,
    HTTPVersion_2_0
  };
  enum SecurityStyle {
    SecurityStyleUnknown,
    SecurityStyleUnauthenticated,
    SecurityStyleAuthenticationBroken,
    SecurityStyleWarning,
    SecurityStyleAuthenticated
  };

  class PLATFORM_EXPORT SignedCertificateTimestamp final {
   public:
    SignedCertificateTimestamp(String status,
                               String origin,
                               String logDescription,
                               String logId,
                               int64_t timestamp,
                               String hashAlgorithm,
                               String signatureAlgorithm,
                               String signatureData)
        : m_status(status),
          m_origin(origin),
          m_logDescription(logDescription),
          m_logId(logId),
          m_timestamp(timestamp),
          m_hashAlgorithm(hashAlgorithm),
          m_signatureAlgorithm(signatureAlgorithm),
          m_signatureData(signatureData) {}
    explicit SignedCertificateTimestamp(
        const struct blink::WebURLResponse::SignedCertificateTimestamp&);
    SignedCertificateTimestamp isolatedCopy() const;

    String m_status;
    String m_origin;
    String m_logDescription;
    String m_logId;
    int64_t m_timestamp;
    String m_hashAlgorithm;
    String m_signatureAlgorithm;
    String m_signatureData;
  };

  using SignedCertificateTimestampList =
      WTF::Vector<SignedCertificateTimestamp>;

  struct SecurityDetails {
    DISALLOW_NEW();
    SecurityDetails() : validFrom(0), validTo(0) {}
    // All strings are human-readable values.
    String protocol;
    // keyExchange is the empty string if not applicable for the connection's
    // protocol.
    String keyExchange;
    // keyExchangeGroup is the empty string if not applicable for the
    // connection's key exchange.
    String keyExchangeGroup;
    String cipher;
    // mac is the empty string when the connection cipher suite does not
    // have a separate MAC value (i.e. if the cipher suite is AEAD).
    String mac;
    String subjectName;
    Vector<String> sanList;
    String issuer;
    time_t validFrom;
    time_t validTo;
    // DER-encoded X509Certificate certificate chain.
    Vector<AtomicString> certificate;
    SignedCertificateTimestampList sctList;
  };

  class ExtraData : public RefCounted<ExtraData> {
   public:
    virtual ~ExtraData() {}
  };

  explicit ResourceResponse(CrossThreadResourceResponseData*);

  // Gets a copy of the data suitable for passing to another thread.
  std::unique_ptr<CrossThreadResourceResponseData> copyData() const;

  ResourceResponse();
  ResourceResponse(const KURL&,
                   const AtomicString& mimeType,
                   long long expectedLength,
                   const AtomicString& textEncodingName);
  ResourceResponse(const ResourceResponse&);
  ResourceResponse& operator=(const ResourceResponse&);

  bool isNull() const { return m_isNull; }
  bool isHTTP() const;

  // The URL of the resource. Note that if a service worker responded to the
  // request for this resource, it may have fetched an entirely different URL
  // and responded with that resource. wasFetchedViaServiceWorker() and
  // originalURLViaServiceWorker() can be used to determine whether and how a
  // service worker responded to the request. Example service worker code:
  //
  // onfetch = (event => {
  //   if (event.request.url == 'https://abc.com')
  //     event.respondWith(fetch('https://def.com'));
  // });
  //
  // If this service worker responds to an "https://abc.com" request, then for
  // the resulting ResourceResponse, url() is "https://abc.com",
  // wasFetchedViaServiceWorker() is true, and originalURLViaServiceWorker() is
  // "https://def.com".
  const KURL& url() const;
  void setURL(const KURL&);

  const AtomicString& mimeType() const;
  void setMimeType(const AtomicString&);

  long long expectedContentLength() const;
  void setExpectedContentLength(long long);

  const AtomicString& textEncodingName() const;
  void setTextEncodingName(const AtomicString&);

  int httpStatusCode() const;
  void setHTTPStatusCode(int);

  const AtomicString& httpStatusText() const;
  void setHTTPStatusText(const AtomicString&);

  const AtomicString& httpHeaderField(const AtomicString& name) const;
  void setHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void addHTTPHeaderField(const AtomicString& name, const AtomicString& value);
  void clearHTTPHeaderField(const AtomicString& name);
  const HTTPHeaderMap& httpHeaderFields() const;

  bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

  bool isAttachment() const;

  AtomicString httpContentType() const;

  // These functions return parsed values of the corresponding response headers.
  // NaN means that the header was not present or had invalid value.
  bool cacheControlContainsNoCache() const;
  bool cacheControlContainsNoStore() const;
  bool cacheControlContainsMustRevalidate() const;
  bool hasCacheValidatorFields() const;
  double cacheControlMaxAge() const;
  double cacheControlStaleWhileRevalidate() const;
  double date() const;
  double age() const;
  double expires() const;
  double lastModified() const;

  unsigned connectionID() const;
  void setConnectionID(unsigned);

  bool connectionReused() const;
  void setConnectionReused(bool);

  bool wasCached() const;
  void setWasCached(bool);

  ResourceLoadTiming* resourceLoadTiming() const;
  void setResourceLoadTiming(PassRefPtr<ResourceLoadTiming>);

  PassRefPtr<ResourceLoadInfo> resourceLoadInfo() const;
  void setResourceLoadInfo(PassRefPtr<ResourceLoadInfo>);

  HTTPVersion httpVersion() const { return m_httpVersion; }
  void setHTTPVersion(HTTPVersion version) { m_httpVersion = version; }

  bool hasMajorCertificateErrors() const { return m_hasMajorCertificateErrors; }
  void setHasMajorCertificateErrors(bool hasMajorCertificateErrors) {
    m_hasMajorCertificateErrors = hasMajorCertificateErrors;
  }

  SecurityStyle getSecurityStyle() const { return m_securityStyle; }
  void setSecurityStyle(SecurityStyle securityStyle) {
    m_securityStyle = securityStyle;
  }

  const SecurityDetails* getSecurityDetails() const {
    return &m_securityDetails;
  }
  void setSecurityDetails(const String& protocol,
                          const String& keyExchange,
                          const String& keyExchangeGroup,
                          const String& cipher,
                          const String& mac,
                          const String& subjectName,
                          const Vector<String>& sanList,
                          const String& issuer,
                          time_t validFrom,
                          time_t validTo,
                          const Vector<AtomicString>& certificate,
                          const SignedCertificateTimestampList& sctList);

  long long appCacheID() const { return m_appCacheID; }
  void setAppCacheID(long long id) { m_appCacheID = id; }

  const KURL& appCacheManifestURL() const { return m_appCacheManifestURL; }
  void setAppCacheManifestURL(const KURL& url) { m_appCacheManifestURL = url; }

  bool wasFetchedViaSPDY() const { return m_wasFetchedViaSPDY; }
  void setWasFetchedViaSPDY(bool value) { m_wasFetchedViaSPDY = value; }

  // See ServiceWorkerResponseInfo::was_fetched_via_service_worker.
  bool wasFetchedViaServiceWorker() const {
    return m_wasFetchedViaServiceWorker;
  }
  void setWasFetchedViaServiceWorker(bool value) {
    m_wasFetchedViaServiceWorker = value;
  }

  bool wasFetchedViaForeignFetch() const { return m_wasFetchedViaForeignFetch; }
  void setWasFetchedViaForeignFetch(bool value) {
    m_wasFetchedViaForeignFetch = value;
  }

  // See ServiceWorkerResponseInfo::was_fallback_required.
  bool wasFallbackRequiredByServiceWorker() const {
    return m_wasFallbackRequiredByServiceWorker;
  }
  void setWasFallbackRequiredByServiceWorker(bool value) {
    m_wasFallbackRequiredByServiceWorker = value;
  }

  WebServiceWorkerResponseType serviceWorkerResponseType() const {
    return m_serviceWorkerResponseType;
  }
  void setServiceWorkerResponseType(WebServiceWorkerResponseType value) {
    m_serviceWorkerResponseType = value;
  }

  // See ServiceWorkerResponseInfo::url_list_via_service_worker.
  const Vector<KURL>& urlListViaServiceWorker() const {
    return m_urlListViaServiceWorker;
  }
  void setURLListViaServiceWorker(const Vector<KURL>& urlList) {
    m_urlListViaServiceWorker = urlList;
  }

  // Returns the last URL of urlListViaServiceWorker if exists. Otherwise
  // returns an empty URL.
  KURL originalURLViaServiceWorker() const;

  const Vector<char>& multipartBoundary() const { return m_multipartBoundary; }
  void setMultipartBoundary(const char* bytes, size_t size) {
    m_multipartBoundary.clear();
    m_multipartBoundary.append(bytes, size);
  }

  const String& cacheStorageCacheName() const {
    return m_cacheStorageCacheName;
  }
  void setCacheStorageCacheName(const String& cacheStorageCacheName) {
    m_cacheStorageCacheName = cacheStorageCacheName;
  }

  const Vector<String>& corsExposedHeaderNames() const {
    return m_corsExposedHeaderNames;
  }
  void setCorsExposedHeaderNames(const Vector<String>& headerNames) {
    m_corsExposedHeaderNames = headerNames;
  }

  bool didServiceWorkerNavigationPreload() const {
    return m_didServiceWorkerNavigationPreload;
  }
  void setDidServiceWorkerNavigationPreload(bool value) {
    m_didServiceWorkerNavigationPreload = value;
  }

  int64_t responseTime() const { return m_responseTime; }
  void setResponseTime(int64_t responseTime) { m_responseTime = responseTime; }

  const AtomicString& remoteIPAddress() const { return m_remoteIPAddress; }
  void setRemoteIPAddress(const AtomicString& value) {
    m_remoteIPAddress = value;
  }

  unsigned short remotePort() const { return m_remotePort; }
  void setRemotePort(unsigned short value) { m_remotePort = value; }

  long long encodedDataLength() const { return m_encodedDataLength; }
  void setEncodedDataLength(long long value);

  long long encodedBodyLength() const { return m_encodedBodyLength; }
  void addToEncodedBodyLength(long long value);

  long long decodedBodyLength() const { return m_decodedBodyLength; }
  void addToDecodedBodyLength(long long value);

  const String& downloadedFilePath() const { return m_downloadedFilePath; }
  void setDownloadedFilePath(const String&);

  // Extra data associated with this response.
  ExtraData* getExtraData() const { return m_extraData.get(); }
  void setExtraData(PassRefPtr<ExtraData> extraData) {
    m_extraData = extraData;
  }

  unsigned memoryUsage() const {
    // average size, mostly due to URL and Header Map strings
    return 1280;
  }

  // PlzNavigate: Even if there is redirections, only one
  // ResourceResponse is built: the final response.
  // The redirect response chain can be accessed by this function.
  const Vector<ResourceResponse>& redirectResponses() const {
    return m_redirectResponses;
  }
  void appendRedirectResponse(const ResourceResponse&);

  // This method doesn't compare the all members.
  static bool compare(const ResourceResponse&, const ResourceResponse&);

 private:
  void updateHeaderParsedState(const AtomicString& name);

  KURL m_url;
  AtomicString m_mimeType;
  long long m_expectedContentLength;
  AtomicString m_textEncodingName;
  int m_httpStatusCode;
  AtomicString m_httpStatusText;
  HTTPHeaderMap m_httpHeaderFields;
  bool m_wasCached : 1;
  unsigned m_connectionID;
  bool m_connectionReused : 1;
  RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
  RefPtr<ResourceLoadInfo> m_resourceLoadInfo;

  bool m_isNull : 1;

  mutable CacheControlHeader m_cacheControlHeader;

  mutable bool m_haveParsedAgeHeader : 1;
  mutable bool m_haveParsedDateHeader : 1;
  mutable bool m_haveParsedExpiresHeader : 1;
  mutable bool m_haveParsedLastModifiedHeader : 1;

  mutable double m_age;
  mutable double m_date;
  mutable double m_expires;
  mutable double m_lastModified;

  // True if the resource was retrieved by the embedder in spite of
  // certificate errors.
  bool m_hasMajorCertificateErrors;

  // The security style of the resource.
  // This only contains a valid value when the DevTools Network domain is
  // enabled. (Otherwise, it contains a default value of Unknown.)
  SecurityStyle m_securityStyle;

  // Security details of this request's connection.
  // If m_securityStyle is Unknown or Unauthenticated, this does not contain
  // valid data.
  SecurityDetails m_securityDetails;

  // HTTP version used in the response, if known.
  HTTPVersion m_httpVersion;

  // The id of the appcache this response was retrieved from, or zero if
  // the response was not retrieved from an appcache.
  long long m_appCacheID;

  // The manifest url of the appcache this response was retrieved from, if any.
  // Note: only valid for main resource responses.
  KURL m_appCacheManifestURL;

  // The multipart boundary of this response.
  Vector<char> m_multipartBoundary;

  // Was the resource fetched over SPDY.  See http://dev.chromium.org/spdy
  bool m_wasFetchedViaSPDY;

  // Was the resource fetched over an explicit proxy (HTTP, SOCKS, etc).
  bool m_wasFetchedViaProxy;

  // Was the resource fetched over a ServiceWorker.
  bool m_wasFetchedViaServiceWorker;

  // Was the resource fetched using a foreign fetch service worker.
  bool m_wasFetchedViaForeignFetch;

  // Was the fallback request with skip service worker flag required.
  bool m_wasFallbackRequiredByServiceWorker;

  // The type of the response which was fetched by the ServiceWorker.
  WebServiceWorkerResponseType m_serviceWorkerResponseType;

  // The URL list of the response which was fetched by the ServiceWorker.
  // This is empty if the response was created inside the ServiceWorker.
  Vector<KURL> m_urlListViaServiceWorker;

  // The cache name of the CacheStorage from where the response is served via
  // the ServiceWorker. Null if the response isn't from the CacheStorage.
  String m_cacheStorageCacheName;

  // The headers that should be exposed according to CORS. Only guaranteed
  // to be set if the response was fetched by a ServiceWorker.
  Vector<String> m_corsExposedHeaderNames;

  // True if service worker navigation preload was performed due to
  // the request for this resource.
  bool m_didServiceWorkerNavigationPreload;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  int64_t m_responseTime;

  // Remote IP address of the socket which fetched this resource.
  AtomicString m_remoteIPAddress;

  // Remote port number of the socket which fetched this resource.
  unsigned short m_remotePort;

  // Size of the response in bytes prior to decompression.
  long long m_encodedDataLength;

  // Size of the response body in bytes prior to decompression.
  long long m_encodedBodyLength;

  // Sizes of the response body in bytes after any content-encoding is
  // removed.
  long long m_decodedBodyLength;

  // The downloaded file path if the load streamed to a file.
  String m_downloadedFilePath;

  // The handle to the downloaded file to ensure the underlying file will not
  // be deleted.
  RefPtr<BlobDataHandle> m_downloadedFileHandle;

  // ExtraData associated with the response.
  RefPtr<ExtraData> m_extraData;

  // PlzNavigate: the redirect responses are transmitted
  // inside the final response.
  Vector<ResourceResponse> m_redirectResponses;
};

inline bool operator==(const ResourceResponse& a, const ResourceResponse& b) {
  return ResourceResponse::compare(a, b);
}
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) {
  return !(a == b);
}

struct CrossThreadResourceResponseData {
  WTF_MAKE_NONCOPYABLE(CrossThreadResourceResponseData);
  USING_FAST_MALLOC(CrossThreadResourceResponseData);

 public:
  CrossThreadResourceResponseData() {}
  KURL m_url;
  String m_mimeType;
  long long m_expectedContentLength;
  String m_textEncodingName;
  int m_httpStatusCode;
  String m_httpStatusText;
  std::unique_ptr<CrossThreadHTTPHeaderMapData> m_httpHeaders;
  RefPtr<ResourceLoadTiming> m_resourceLoadTiming;
  bool m_hasMajorCertificateErrors;
  ResourceResponse::SecurityStyle m_securityStyle;
  ResourceResponse::SecurityDetails m_securityDetails;
  // This is |certificate| from SecurityDetails since that structure should
  // use an AtomicString but this temporary structure is sent across threads.
  Vector<String> m_certificate;
  ResourceResponse::HTTPVersion m_httpVersion;
  long long m_appCacheID;
  KURL m_appCacheManifestURL;
  Vector<char> m_multipartBoundary;
  bool m_wasFetchedViaSPDY;
  bool m_wasFetchedViaProxy;
  bool m_wasFetchedViaServiceWorker;
  bool m_wasFetchedViaForeignFetch;
  bool m_wasFallbackRequiredByServiceWorker;
  WebServiceWorkerResponseType m_serviceWorkerResponseType;
  Vector<KURL> m_urlListViaServiceWorker;
  String m_cacheStorageCacheName;
  bool m_didServiceWorkerNavigationPreload;
  int64_t m_responseTime;
  String m_remoteIPAddress;
  unsigned short m_remotePort;
  long long m_encodedDataLength;
  long long m_encodedBodyLength;
  long long m_decodedBodyLength;
  String m_downloadedFilePath;
  RefPtr<BlobDataHandle> m_downloadedFileHandle;
};

}  // namespace blink

#endif  // ResourceResponse_h
