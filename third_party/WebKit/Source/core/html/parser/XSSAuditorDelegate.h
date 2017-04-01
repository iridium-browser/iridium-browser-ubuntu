/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XSSAuditorDelegate_h
#define XSSAuditorDelegate_h

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class Document;
class EncodedFormData;

class XSSInfo {
  USING_FAST_MALLOC(XSSInfo);
  WTF_MAKE_NONCOPYABLE(XSSInfo);

 public:
  static std::unique_ptr<XSSInfo> create(const String& originalURL,
                                         bool didBlockEntirePage,
                                         bool didSendXSSProtectionHeader) {
    return WTF::wrapUnique(new XSSInfo(originalURL, didBlockEntirePage,
                                       didSendXSSProtectionHeader));
  }

  String buildConsoleError() const;
  bool isSafeToSendToAnotherThread() const;

  String m_originalURL;
  bool m_didBlockEntirePage;
  bool m_didSendXSSProtectionHeader;
  TextPosition m_textPosition;

 private:
  XSSInfo(const String& originalURL,
          bool didBlockEntirePage,
          bool didSendXSSProtectionHeader)
      : m_originalURL(originalURL.isolatedCopy()),
        m_didBlockEntirePage(didBlockEntirePage),
        m_didSendXSSProtectionHeader(didSendXSSProtectionHeader) {}
};

class XSSAuditorDelegate final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(XSSAuditorDelegate);

 public:
  explicit XSSAuditorDelegate(Document*);
  DECLARE_TRACE();

  void didBlockScript(const XSSInfo&);
  void setReportURL(const KURL& url) { m_reportURL = url; }

 private:
  PassRefPtr<EncodedFormData> generateViolationReport(const XSSInfo&);

  Member<Document> m_document;
  bool m_didSendNotifications;
  KURL m_reportURL;
};

typedef Vector<std::unique_ptr<XSSInfo>> XSSInfoStream;

}  // namespace blink

#endif
