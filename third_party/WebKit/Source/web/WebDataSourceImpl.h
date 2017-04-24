/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDataSourceImpl_h
#define WebDataSourceImpl_h

#include "core/frame/FrameTypes.h"
#include "core/loader/DocumentLoader.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/web/WebDataSource.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class WebDataSourceImpl final : public DocumentLoader, public WebDataSource {
 public:
  static WebDataSourceImpl* create(LocalFrame*,
                                   const ResourceRequest&,
                                   const SubstituteData&,
                                   ClientRedirectPolicy);

  static WebDataSourceImpl* fromDocumentLoader(DocumentLoader* loader) {
    return static_cast<WebDataSourceImpl*>(loader);
  }

  // WebDataSource methods:
  const WebURLRequest& originalRequest() const override;
  const WebURLRequest& getRequest() const override;
  const WebURLResponse& response() const override;
  bool hasUnreachableURL() const override;
  WebURL unreachableURL() const override;
  void appendRedirect(const WebURL&) override;
  void redirectChain(WebVector<WebURL>&) const override;
  bool isClientRedirect() const override;
  bool replacesCurrentHistoryItem() const override;
  WebNavigationType navigationType() const override;
  ExtraData* getExtraData() const override;
  void setExtraData(ExtraData*) override;
  void setNavigationStartTime(double) override;
  void updateNavigation(double redirectStartTime,
                        double redirectEndTime,
                        double fetchStartTime,
                        bool hasRedirect) override;
  void setSubresourceFilter(WebDocumentSubresourceFilter*) override;

  static WebNavigationType toWebNavigationType(NavigationType);

  DECLARE_VIRTUAL_TRACE();

 private:
  WebDataSourceImpl(LocalFrame*,
                    const ResourceRequest&,
                    const SubstituteData&,
                    ClientRedirectPolicy);
  ~WebDataSourceImpl() override;
  void detachFromFrame() override;
  String debugName() const override { return "WebDataSourceImpl"; }

  // Mutable because the const getters will magically sync these to the
  // latest version from WebKit.
  mutable WrappedResourceRequest m_originalRequestWrapper;
  mutable WrappedResourceRequest m_requestWrapper;
  mutable WrappedResourceResponse m_responseWrapper;

  std::unique_ptr<ExtraData> m_extraData;
};

}  // namespace blink

#endif  // WebDataSourceImpl_h
