/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 *
 */

#include "core/fetch/CrossOriginAccessControl.h"

#include "core/fetch/FetchUtils.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>
#include <memory>

namespace blink {

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(HTTPHeaderSet, allowedCrossOriginResponseHeaders, (new HTTPHeaderSet({
        "cache-control",
        "content-language",
        "content-type",
        "expires",
        "last-modified",
        "pragma",
    })));
    return allowedCrossOriginResponseHeaders.contains(name);
}

void updateRequestForAccessControl(ResourceRequest& request, const SecurityOrigin* securityOrigin, StoredCredentials allowCredentials)
{
    request.removeCredentials();
    request.setAllowStoredCredentials(allowCredentials == AllowStoredCredentials);

    if (securityOrigin)
        request.setHTTPOrigin(securityOrigin);
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, const SecurityOrigin* securityOrigin)
{
    ResourceRequest preflightRequest(request.url());
    updateRequestForAccessControl(preflightRequest, securityOrigin, DoNotAllowStoredCredentials);
    preflightRequest.setHTTPMethod(HTTPNames::OPTIONS);
    preflightRequest.setHTTPHeaderField(HTTPNames::Access_Control_Request_Method, AtomicString(request.httpMethod()));
    preflightRequest.setPriority(request.priority());
    preflightRequest.setRequestContext(request.requestContext());
    preflightRequest.setSkipServiceWorker(WebURLRequest::SkipServiceWorker::All);

    if (request.isExternalRequest())
        preflightRequest.setHTTPHeaderField(HTTPNames::Access_Control_Request_External, "true");

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (requestHeaderFields.size() > 0) {
        // Fetch API Spec:
        //   https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
        Vector<String> headers;
        for (const auto& header : requestHeaderFields) {
            if (FetchUtils::isSimpleHeader(header.key, header.value)) {
                // Exclude simple headers.
                continue;
            }
            if (equalIgnoringCase(header.key, "referer")) {
                // When the request is from a Worker, referrer header was added
                // by WorkerThreadableLoader. But it should not be added to
                // Access-Control-Request-Headers header.
                continue;
            }
            headers.append(header.key.lower());
        }
        // Sort header names lexicographically.
        std::sort(headers.begin(), headers.end(), WTF::codePointCompareLessThan);
        StringBuilder headerBuffer;
        for (const String& header : headers) {
            if (!headerBuffer.isEmpty())
                headerBuffer.append(", ");
            headerBuffer.append(header);
        }
        preflightRequest.setHTTPHeaderField(HTTPNames::Access_Control_Request_Headers, AtomicString(headerBuffer.toString()));
    }

    return preflightRequest;
}

static bool isOriginSeparator(UChar ch)
{
    return isASCIISpace(ch) || ch == ',';
}

static bool isInterestingStatusCode(int statusCode)
{
    // Predicate that gates what status codes should be included in
    // console error messages for responses containing no access
    // control headers.
    return statusCode >= 400;
}

static String buildAccessControlFailureMessage(const String& detail, const SecurityOrigin* securityOrigin)
{
    return detail + " Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
}

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentials includeCredentials, const SecurityOrigin* securityOrigin, String& errorDescription, WebURLRequest::RequestContext context)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allowOriginHeaderName, (new AtomicString("access-control-allow-origin")));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allowCredentialsHeaderName, (new AtomicString("access-control-allow-credentials")));
    DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, allowSuboriginHeaderName, (new AtomicString("access-control-allow-suborigin")));

    // TODO(esprehn): This code is using String::append extremely inefficiently
    // causing tons of copies. It should pass around a StringBuilder instead.

    int statusCode = response.httpStatusCode();

    if (!statusCode) {
        errorDescription = buildAccessControlFailureMessage("Invalid response.", securityOrigin);
        return false;
    }

    const AtomicString& allowOriginHeaderValue = response.httpHeaderField(allowOriginHeaderName);

    // Check Suborigins, unless the Access-Control-Allow-Origin is '*',
    // which implies that all Suborigins are okay as well.
    if (securityOrigin->hasSuborigin() && allowOriginHeaderValue != starAtom) {
        const AtomicString& allowSuboriginHeaderValue = response.httpHeaderField(allowSuboriginHeaderName);
        AtomicString atomicSuboriginName(securityOrigin->suborigin()->name());
        if (allowSuboriginHeaderValue != starAtom && allowSuboriginHeaderValue != atomicSuboriginName) {
            errorDescription = buildAccessControlFailureMessage("The 'Access-Control-Allow-Suborigin' header has a value '" + allowSuboriginHeaderValue + "' that is not equal to the supplied suborigin.", securityOrigin);
            return false;
        }
    }

    if (allowOriginHeaderValue == starAtom) {
        // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
        // even with Access-Control-Allow-Credentials set to true.
        if (includeCredentials == DoNotAllowStoredCredentials)
            return true;
        if (response.isHTTP()) {
            errorDescription = buildAccessControlFailureMessage("A wildcard '*' cannot be used in the 'Access-Control-Allow-Origin' header when the credentials flag is true.", securityOrigin);

            if (context == WebURLRequest::RequestContextXMLHttpRequest)
                errorDescription.append(" The credentials mode of an XMLHttpRequest is controlled by the withCredentials attribute.");

            return false;
        }
    } else if (allowOriginHeaderValue != securityOrigin->toAtomicString()) {
        if (allowOriginHeaderValue.isNull()) {
            errorDescription = buildAccessControlFailureMessage("No 'Access-Control-Allow-Origin' header is present on the requested resource.", securityOrigin);

            if (isInterestingStatusCode(statusCode)) {
                errorDescription.append(" The response had HTTP status code ");
                errorDescription.append(String::number(statusCode));
                errorDescription.append('.');
            }

            if (context == WebURLRequest::RequestContextFetch)
                errorDescription.append(" If an opaque response serves your needs, set the request's mode to 'no-cors' to fetch the resource with CORS disabled.");

            return false;
        }

        String detail;
        if (allowOriginHeaderValue.getString().find(isOriginSeparator, 0) != kNotFound) {
            detail = "The 'Access-Control-Allow-Origin' header contains multiple values '" + allowOriginHeaderValue + "', but only one is allowed.";
        } else {
            KURL headerOrigin(KURL(), allowOriginHeaderValue);
            if (!headerOrigin.isValid())
                detail = "The 'Access-Control-Allow-Origin' header contains the invalid value '" + allowOriginHeaderValue + "'.";
            else
                detail = "The 'Access-Control-Allow-Origin' header has a value '" + allowOriginHeaderValue + "' that is not equal to the supplied origin.";
        }
        errorDescription = buildAccessControlFailureMessage(detail, securityOrigin);
        if (context == WebURLRequest::RequestContextFetch)
            errorDescription.append(" Have the server send the header with a valid value, or, if an opaque response serves your needs, set the request's mode to 'no-cors' to fetch the resource with CORS disabled.");
        return false;
    }

    if (includeCredentials == AllowStoredCredentials) {
        const AtomicString& allowCredentialsHeaderValue = response.httpHeaderField(allowCredentialsHeaderName);
        if (allowCredentialsHeaderValue != "true") {
            errorDescription = buildAccessControlFailureMessage("Credentials flag is 'true', but the 'Access-Control-Allow-Credentials' header is '" + allowCredentialsHeaderValue + "'. It must be 'true' to allow credentials.", securityOrigin);
            return false;
        }
    }

    return true;
}

bool passesPreflightStatusCheck(const ResourceResponse& response, String& errorDescription)
{
    // CORS preflight with 3XX is considered network error in
    // Fetch API Spec:
    //   https://fetch.spec.whatwg.org/#cors-preflight-fetch
    // CORS Spec:
    //   http://www.w3.org/TR/cors/#cross-origin-request-with-preflight-0
    // https://crbug.com/452394
    if (response.httpStatusCode() < 200 || response.httpStatusCode() >= 300) {
        errorDescription = "Response for preflight has invalid HTTP status code " + String::number(response.httpStatusCode());
        return false;
    }

    return true;
}

bool passesExternalPreflightCheck(const ResourceResponse& response, String& errorDescription)
{
    AtomicString result = response.httpHeaderField(HTTPNames::Access_Control_Allow_External);
    if (result.isNull()) {
        errorDescription = "No 'Access-Control-Allow-External' header was present in the preflight response for this external request (This is an experimental header which is defined in 'https://mikewest.github.io/cors-rfc1918/').";
        return false;
    }
    if (!equalIgnoringCase(result, "true")) {
        errorDescription = "The 'Access-Control-Allow-External' header in the preflight response for this external request had a value of '" + result + "',  not 'true' (This is an experimental header which is defined in 'https://mikewest.github.io/cors-rfc1918/').";
        return false;
    }
    return true;
}

void parseAccessControlExposeHeadersAllowList(const String& headerValue, HTTPHeaderSet& headerSet)
{
    Vector<String> headers;
    headerValue.split(',', false, headers);
    for (unsigned headerCount = 0; headerCount < headers.size(); headerCount++) {
        String strippedHeader = headers[headerCount].stripWhiteSpace();
        if (!strippedHeader.isEmpty())
            headerSet.add(strippedHeader);
    }
}

void extractCorsExposedHeaderNamesList(const ResourceResponse& response, HTTPHeaderSet& headerSet)
{
    // If a response was fetched via a service worker, it will always have
    // corsExposedHeaderNames set, either from the Access-Control-Expose-Headers
    // header, or explicitly via foreign fetch. For requests that didn't come
    // from a service worker, foreign fetch doesn't apply so just parse the CORS
    // header.
    if (response.wasFetchedViaServiceWorker()) {
        for (const auto& header : response.corsExposedHeaderNames())
            headerSet.add(header);
        return;
    }
    parseAccessControlExposeHeadersAllowList(response.httpHeaderField(HTTPNames::Access_Control_Expose_Headers), headerSet);
}

bool CrossOriginAccessControl::isLegalRedirectLocation(const KURL& requestURL, String& errorDescription)
{
    // Block non HTTP(S) schemes as specified in the step 4 in
    // https://fetch.spec.whatwg.org/#http-redirect-fetch. Chromium also allows
    // the data scheme.
    //
    // TODO(tyoshino): This check should be performed regardless of the CORS
    // flag and request's mode.
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(requestURL.protocol())) {
        errorDescription = "Redirect location '" + requestURL.getString() + "' has a disallowed scheme for cross-origin requests.";
        return false;
    }

    // Block URLs including credentials as specified in the step 9 in
    // https://fetch.spec.whatwg.org/#http-redirect-fetch.
    //
    // TODO(tyoshino): This check should be performed also when request's
    // origin is not same origin with the redirect destination's origin.
    if (!(requestURL.user().isEmpty() && requestURL.pass().isEmpty())) {
        errorDescription = "Redirect location '" + requestURL.getString() + "' contains userinfo, which is disallowed for cross-origin requests.";
        return false;
    }

    return true;
}

bool CrossOriginAccessControl::handleRedirect(PassRefPtr<SecurityOrigin> securityOrigin, ResourceRequest& newRequest, const ResourceResponse& redirectResponse, StoredCredentials withCredentials, ResourceLoaderOptions& options, String& errorMessage)
{
    // http://www.w3.org/TR/cors/#redirect-steps terminology:
    const KURL& lastURL = redirectResponse.url();
    const KURL& newURL = newRequest.url();

    RefPtr<SecurityOrigin> currentSecurityOrigin = securityOrigin;

    RefPtr<SecurityOrigin> newSecurityOrigin = currentSecurityOrigin;

    // TODO(tyoshino): This should be fixed to check not only the last one but
    // all redirect responses.
    if (!currentSecurityOrigin->canRequest(lastURL)) {
        // Follow http://www.w3.org/TR/cors/#redirect-steps
        String errorDescription;

        if (!isLegalRedirectLocation(newURL, errorDescription)) {
            errorMessage = "Redirect from '" + lastURL.getString() + "' has been blocked by CORS policy: " + errorDescription;
            return false;
        }

        // Step 5: perform resource sharing access check.
        if (!passesAccessControlCheck(redirectResponse, withCredentials, currentSecurityOrigin.get(), errorDescription, newRequest.requestContext())) {
            errorMessage = "Redirect from '" + lastURL.getString() + "' has been blocked by CORS policy: " + errorDescription;
            return false;
        }

        RefPtr<SecurityOrigin> lastOrigin = SecurityOrigin::create(lastURL);
        // Set request's origin to a globally unique identifier as specified in
        // the step 10 in https://fetch.spec.whatwg.org/#http-redirect-fetch.
        if (!lastOrigin->canRequest(newURL)) {
            options.securityOrigin = SecurityOrigin::createUnique();
            newSecurityOrigin = options.securityOrigin;
        }
    }

    if (!currentSecurityOrigin->canRequest(newURL)) {
        newRequest.clearHTTPOrigin();
        newRequest.setHTTPOrigin(newSecurityOrigin.get());

        // Unset credentials flag if request's credentials mode is
        // "same-origin" as request's response tainting becomes "cors".
        //
        // This is equivalent to the step 2 in
        // https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
        if (options.credentialsRequested == ClientDidNotRequestCredentials)
            options.allowCredentials = DoNotAllowStoredCredentials;
    }
    return true;
}

} // namespace blink
