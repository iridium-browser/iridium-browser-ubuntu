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

#include "config.h"
#include "core/fetch/CrossOriginAccessControl.h"

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringBuilder.h"
#include <algorithm>

namespace blink {

static PassOwnPtr<HTTPHeaderSet> createAllowedCrossOriginResponseHeadersSet()
{
    OwnPtr<HTTPHeaderSet> headerSet = adoptPtr(new HashSet<String, CaseFoldingHash>);

    headerSet->add("cache-control");
    headerSet->add("content-language");
    headerSet->add("content-type");
    headerSet->add("expires");
    headerSet->add("last-modified");
    headerSet->add("pragma");

    return headerSet.release();
}

bool isOnAccessControlResponseHeaderWhitelist(const String& name)
{
    AtomicallyInitializedStaticReference(HTTPHeaderSet, allowedCrossOriginResponseHeaders, (createAllowedCrossOriginResponseHeadersSet().leakPtr()));

    return allowedCrossOriginResponseHeaders.contains(name);
}

void updateRequestForAccessControl(ResourceRequest& request, SecurityOrigin* securityOrigin, StoredCredentials allowCredentials)
{
    request.removeCredentials();
    request.setAllowStoredCredentials(allowCredentials == AllowStoredCredentials);
    request.setFetchCredentialsMode(allowCredentials == AllowStoredCredentials ? WebURLRequest::FetchCredentialsModeInclude : WebURLRequest::FetchCredentialsModeOmit);

    if (securityOrigin)
        request.setHTTPOrigin(securityOrigin->toAtomicString());
}

ResourceRequest createAccessControlPreflightRequest(const ResourceRequest& request, SecurityOrigin* securityOrigin)
{
    ResourceRequest preflightRequest(request.url());
    updateRequestForAccessControl(preflightRequest, securityOrigin, DoNotAllowStoredCredentials);
    preflightRequest.setHTTPMethod("OPTIONS");
    preflightRequest.setHTTPHeaderField("Access-Control-Request-Method", request.httpMethod());
    preflightRequest.setPriority(request.priority());
    preflightRequest.setRequestContext(request.requestContext());
    preflightRequest.setSkipServiceWorker(true);

    const HTTPHeaderMap& requestHeaderFields = request.httpHeaderFields();

    if (requestHeaderFields.size() > 0) {
        // Sort header names lexicographically: https://crbug.com/452391
        // Fetch API Spec:
        //   https://fetch.spec.whatwg.org/#cors-preflight-fetch-0
        Vector<String> headers;
        for (const auto& header : requestHeaderFields) {
            if (equalIgnoringCase(header.key, "referer")) {
                // When the request is from a Worker, referrer header was added
                // by WorkerThreadableLoader. But it should not be added to
                // Access-Control-Request-Headers header.
                continue;
            }
            headers.append(header.key.lower());
        }
        std::sort(headers.begin(), headers.end(), WTF::codePointCompareLessThan);
        StringBuilder headerBuffer;
        for (const String& header : headers) {
            if (!headerBuffer.isEmpty())
                headerBuffer.appendLiteral(", ");
            headerBuffer.append(header);
        }
        preflightRequest.setHTTPHeaderField("Access-Control-Request-Headers", AtomicString(headerBuffer.toString()));
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

bool passesAccessControlCheck(const ResourceResponse& response, StoredCredentials includeCredentials, SecurityOrigin* securityOrigin, String& errorDescription)
{
    AtomicallyInitializedStaticReference(AtomicString, accessControlAllowOrigin, (new AtomicString("access-control-allow-origin", AtomicString::ConstructFromLiteral)));
    AtomicallyInitializedStaticReference(AtomicString, accessControlAllowCredentials, (new AtomicString("access-control-allow-credentials", AtomicString::ConstructFromLiteral)));

    if (!response.httpStatusCode()) {
        errorDescription = "Received an invalid response. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        return false;
    }

    const AtomicString& accessControlOriginString = response.httpHeaderField(accessControlAllowOrigin);
    if (accessControlOriginString == starAtom) {
        // A wildcard Access-Control-Allow-Origin can not be used if credentials are to be sent,
        // even with Access-Control-Allow-Credentials set to true.
        if (includeCredentials == DoNotAllowStoredCredentials)
            return true;
        if (response.isHTTP()) {
            errorDescription = "A wildcard '*' cannot be used in the 'Access-Control-Allow-Origin' header when the credentials flag is true. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
            return false;
        }
    } else if (accessControlOriginString != securityOrigin->toAtomicString()) {
        if (accessControlOriginString.isNull()) {
            errorDescription = "No 'Access-Control-Allow-Origin' header is present on the requested resource. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";

            if (isInterestingStatusCode(response.httpStatusCode()))
                errorDescription.append(" The response had HTTP status code " + String::number(response.httpStatusCode()) + ".");
        } else if (accessControlOriginString.string().find(isOriginSeparator, 0) != kNotFound) {
            errorDescription = "The 'Access-Control-Allow-Origin' header contains multiple values '" + accessControlOriginString + "', but only one is allowed. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        } else {
            KURL headerOrigin(KURL(), accessControlOriginString);
            if (!headerOrigin.isValid())
                errorDescription = "The 'Access-Control-Allow-Origin' header contains the invalid value '" + accessControlOriginString + "'. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
            else
                errorDescription = "The 'Access-Control-Allow-Origin' header has a value '" + accessControlOriginString + "' that is not equal to the supplied origin. Origin '" + securityOrigin->toString() + "' is therefore not allowed access.";
        }
        return false;
    }

    if (includeCredentials == AllowStoredCredentials) {
        const AtomicString& accessControlCredentialsString = response.httpHeaderField(accessControlAllowCredentials);
        if (accessControlCredentialsString != "true") {
            errorDescription = "Credentials flag is 'true', but the 'Access-Control-Allow-Credentials' header is '" + accessControlCredentialsString + "'. It must be 'true' to allow credentials.";
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
        errorDescription = "Invalid HTTP status code " + String::number(response.httpStatusCode());
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

bool CrossOriginAccessControl::isLegalRedirectLocation(const KURL& requestURL, String& errorDescription)
{
    // CORS restrictions imposed on Location: URL -- http://www.w3.org/TR/cors/#redirect-steps (steps 2 + 3.)
    if (!SchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(requestURL.protocol())) {
        errorDescription = "The request was redirected to a URL ('" + requestURL.string() + "') which has a disallowed scheme for cross-origin requests.";
        return false;
    }

    if (!(requestURL.user().isEmpty() && requestURL.pass().isEmpty())) {
        errorDescription = "The request was redirected to a URL ('" + requestURL.string() + "') containing userinfo, which is disallowed for cross-origin requests.";
        return false;
    }

    return true;
}

bool CrossOriginAccessControl::handleRedirect(SecurityOrigin* securityOrigin, ResourceRequest& request, const ResourceResponse& redirectResponse, StoredCredentials withCredentials, ResourceLoaderOptions& options, String& errorMessage)
{
    // http://www.w3.org/TR/cors/#redirect-steps terminology:
    const KURL& originalURL = redirectResponse.url();
    const KURL& requestURL = request.url();

    bool redirectCrossOrigin = !securityOrigin->canRequest(requestURL);

    // Same-origin request URLs that redirect are allowed without checking access.
    if (!securityOrigin->canRequest(originalURL)) {
        // Follow http://www.w3.org/TR/cors/#redirect-steps
        String errorDescription;

        // Steps 3 & 4 - check if scheme and other URL restrictions hold.
        bool allowRedirect = isLegalRedirectLocation(requestURL, errorDescription);
        if (allowRedirect) {
            // Step 5: perform resource sharing access check.
            allowRedirect = passesAccessControlCheck(redirectResponse, withCredentials, securityOrigin, errorDescription);
            if (allowRedirect) {
                RefPtr<SecurityOrigin> originalOrigin = SecurityOrigin::create(originalURL);
                // Step 6: if the request URL origin is not same origin as the original URL's,
                // set the source origin to a globally unique identifier.
                if (!originalOrigin->canRequest(requestURL)) {
                    options.securityOrigin = SecurityOrigin::createUnique();
                    securityOrigin = options.securityOrigin.get();
                }
            }
        }
        if (!allowRedirect) {
            const String& originalOrigin = SecurityOrigin::create(originalURL)->toString();
            errorMessage = "Redirect at origin '" + originalOrigin + "' has been blocked from loading by Cross-Origin Resource Sharing policy: " + errorDescription;
            return false;
        }
    }
    if (redirectCrossOrigin) {
        // If now to a different origin, update/set Origin:.
        request.clearHTTPOrigin();
        request.setHTTPOrigin(securityOrigin->toAtomicString());
        // If the user didn't request credentials in the first place, update our
        // state so we neither request them nor expect they must be allowed.
        if (options.credentialsRequested == ClientDidNotRequestCredentials)
            options.allowCredentials = DoNotAllowStoredCredentials;
    }
    return true;
}

} // namespace blink
