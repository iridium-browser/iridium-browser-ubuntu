/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "platform/mhtml/MHTMLParser.h"

#include "platform/mhtml/ArchiveResource.h"
#include "platform/network/ParsedContentType.h"
#include "platform/text/QuotedPrintable.h"
#include "wtf/HashMap.h"
#include "wtf/text/Base64.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace blink {

// This class is a limited MIME parser used to parse the MIME headers of MHTML
// files.
class MIMEHeader : public GarbageCollectedFinalized<MIMEHeader> {
 public:
  static MIMEHeader* create() { return new MIMEHeader; }

  enum Encoding {
    QuotedPrintable,
    Base64,
    EightBit,
    SevenBit,
    Binary,
    Unknown
  };

  static MIMEHeader* parseHeader(SharedBufferChunkReader* crLFLineReader);

  bool isMultipart() const {
    return m_contentType.startsWith("multipart/", TextCaseASCIIInsensitive);
  }

  String contentType() const { return m_contentType; }
  String charset() const { return m_charset; }
  Encoding contentTransferEncoding() const { return m_contentTransferEncoding; }
  String contentLocation() const { return m_contentLocation; }
  String contentID() const { return m_contentID; }

  // Multi-part type and boundaries are only valid for multipart MIME headers.
  String multiPartType() const { return m_multipartType; }
  String endOfPartBoundary() const { return m_endOfPartBoundary; }
  String endOfDocumentBoundary() const { return m_endOfDocumentBoundary; }

  DEFINE_INLINE_TRACE() {}

 private:
  MIMEHeader();

  static Encoding parseContentTransferEncoding(const String&);

  String m_contentType;
  String m_charset;
  Encoding m_contentTransferEncoding;
  String m_contentLocation;
  String m_contentID;
  String m_multipartType;
  String m_endOfPartBoundary;
  String m_endOfDocumentBoundary;
};

typedef HashMap<String, String> KeyValueMap;

static KeyValueMap retrieveKeyValuePairs(SharedBufferChunkReader* buffer) {
  KeyValueMap keyValuePairs;
  String line;
  String key;
  StringBuilder value;
  while (!(line = buffer->nextChunkAsUTF8StringWithLatin1Fallback()).isNull()) {
    if (line.isEmpty())
      break;  // Empty line means end of key/value section.
    if (line[0] == '\t') {
      value.append(line.substring(1));
      continue;
    }
    // New key/value, store the previous one if any.
    if (!key.isEmpty()) {
      if (keyValuePairs.find(key) != keyValuePairs.end())
        DVLOG(1) << "Key duplicate found in MIME header. Key is '" << key
                 << "', previous value replaced.";
      keyValuePairs.add(key, value.toString().stripWhiteSpace());
      key = String();
      value.clear();
    }
    size_t semiColonIndex = line.find(':');
    if (semiColonIndex == kNotFound) {
      // This is not a key value pair, ignore.
      continue;
    }
    key = line.substring(0, semiColonIndex).lower().stripWhiteSpace();
    value.append(line.substring(semiColonIndex + 1));
  }
  // Store the last property if there is one.
  if (!key.isEmpty())
    keyValuePairs.set(key, value.toString().stripWhiteSpace());
  return keyValuePairs;
}

MIMEHeader* MIMEHeader::parseHeader(SharedBufferChunkReader* buffer) {
  MIMEHeader* mimeHeader = MIMEHeader::create();
  KeyValueMap keyValuePairs = retrieveKeyValuePairs(buffer);
  KeyValueMap::iterator mimeParametersIterator =
      keyValuePairs.find("content-type");
  if (mimeParametersIterator != keyValuePairs.end()) {
    ParsedContentType parsedContentType(mimeParametersIterator->value);
    mimeHeader->m_contentType = parsedContentType.mimeType();
    if (!mimeHeader->isMultipart()) {
      mimeHeader->m_charset = parsedContentType.charset().stripWhiteSpace();
    } else {
      mimeHeader->m_multipartType =
          parsedContentType.parameterValueForName("type");
      mimeHeader->m_endOfPartBoundary =
          parsedContentType.parameterValueForName("boundary");
      if (mimeHeader->m_endOfPartBoundary.isNull()) {
        DVLOG(1) << "No boundary found in multipart MIME header.";
        return nullptr;
      }
      mimeHeader->m_endOfPartBoundary.insert("--", 0);
      mimeHeader->m_endOfDocumentBoundary = mimeHeader->m_endOfPartBoundary;
      mimeHeader->m_endOfDocumentBoundary.append("--");
    }
  }

  mimeParametersIterator = keyValuePairs.find("content-transfer-encoding");
  if (mimeParametersIterator != keyValuePairs.end())
    mimeHeader->m_contentTransferEncoding =
        parseContentTransferEncoding(mimeParametersIterator->value);

  mimeParametersIterator = keyValuePairs.find("content-location");
  if (mimeParametersIterator != keyValuePairs.end())
    mimeHeader->m_contentLocation = mimeParametersIterator->value;

  // See rfc2557 - section 8.3 - Use of the Content-ID header and CID URLs.
  mimeParametersIterator = keyValuePairs.find("content-id");
  if (mimeParametersIterator != keyValuePairs.end())
    mimeHeader->m_contentID = mimeParametersIterator->value;

  return mimeHeader;
}

MIMEHeader::Encoding MIMEHeader::parseContentTransferEncoding(
    const String& text) {
  String encoding = text.stripWhiteSpace().lower();
  if (encoding == "base64")
    return Base64;
  if (encoding == "quoted-printable")
    return QuotedPrintable;
  if (encoding == "8bit")
    return EightBit;
  if (encoding == "7bit")
    return SevenBit;
  if (encoding == "binary")
    return Binary;
  DVLOG(1) << "Unknown encoding '" << text << "' found in MIME header.";
  return Unknown;
}

MIMEHeader::MIMEHeader() : m_contentTransferEncoding(Unknown) {}

static bool skipLinesUntilBoundaryFound(SharedBufferChunkReader& lineReader,
                                        const String& boundary) {
  String line;
  while (
      !(line = lineReader.nextChunkAsUTF8StringWithLatin1Fallback()).isNull()) {
    if (line == boundary)
      return true;
  }
  return false;
}

MHTMLParser::MHTMLParser(PassRefPtr<const SharedBuffer> data)
    : m_lineReader(std::move(data), "\r\n") {}

HeapVector<Member<ArchiveResource>> MHTMLParser::parseArchive() {
  MIMEHeader* header = MIMEHeader::parseHeader(&m_lineReader);
  HeapVector<Member<ArchiveResource>> resources;
  if (!parseArchiveWithHeader(header, resources))
    resources.clear();
  return resources;
}

bool MHTMLParser::parseArchiveWithHeader(
    MIMEHeader* header,
    HeapVector<Member<ArchiveResource>>& resources) {
  if (!header) {
    DVLOG(1) << "Failed to parse MHTML part: no header.";
    return false;
  }

  if (!header->isMultipart()) {
    // With IE a page with no resource is not multi-part.
    bool endOfArchiveReached = false;
    ArchiveResource* resource =
        parseNextPart(*header, String(), String(), endOfArchiveReached);
    if (!resource)
      return false;
    resources.push_back(resource);
    return true;
  }

  // Skip the message content (it's a generic browser specific message).
  skipLinesUntilBoundaryFound(m_lineReader, header->endOfPartBoundary());

  bool endOfArchive = false;
  while (!endOfArchive) {
    MIMEHeader* resourceHeader = MIMEHeader::parseHeader(&m_lineReader);
    if (!resourceHeader) {
      DVLOG(1) << "Failed to parse MHTML, invalid MIME header.";
      return false;
    }
    if (resourceHeader->contentType() == "multipart/alternative") {
      // Ignore IE nesting which makes little sense (IE seems to nest only some
      // of the frames).
      if (!parseArchiveWithHeader(resourceHeader, resources)) {
        DVLOG(1) << "Failed to parse MHTML subframe.";
        return false;
      }
      skipLinesUntilBoundaryFound(m_lineReader, header->endOfPartBoundary());
      continue;
    }

    ArchiveResource* resource =
        parseNextPart(*resourceHeader, header->endOfPartBoundary(),
                      header->endOfDocumentBoundary(), endOfArchive);
    if (!resource) {
      DVLOG(1) << "Failed to parse MHTML part.";
      return false;
    }
    resources.push_back(resource);
  }
  return true;
}

ArchiveResource* MHTMLParser::parseNextPart(const MIMEHeader& mimeHeader,
                                            const String& endOfPartBoundary,
                                            const String& endOfDocumentBoundary,
                                            bool& endOfArchiveReached) {
  ASSERT(endOfPartBoundary.isEmpty() == endOfDocumentBoundary.isEmpty());

  // If no content transfer encoding is specified, default to binary encoding.
  MIMEHeader::Encoding contentTransferEncoding =
      mimeHeader.contentTransferEncoding();
  if (contentTransferEncoding == MIMEHeader::Unknown)
    contentTransferEncoding = MIMEHeader::Binary;

  RefPtr<SharedBuffer> content = SharedBuffer::create();
  const bool checkBoundary = !endOfPartBoundary.isEmpty();
  bool endOfPartReached = false;
  if (contentTransferEncoding == MIMEHeader::Binary) {
    if (!checkBoundary) {
      DVLOG(1) << "Binary contents requires end of part";
      return nullptr;
    }
    m_lineReader.setSeparator(endOfPartBoundary.utf8().data());
    Vector<char> part;
    if (!m_lineReader.nextChunk(part)) {
      DVLOG(1) << "Binary contents requires end of part";
      return nullptr;
    }
    content->append(part);
    m_lineReader.setSeparator("\r\n");
    Vector<char> nextChars;
    if (m_lineReader.peek(nextChars, 2) != 2) {
      DVLOG(1) << "Invalid seperator.";
      return nullptr;
    }
    endOfPartReached = true;
    ASSERT(nextChars.size() == 2);
    endOfArchiveReached = (nextChars[0] == '-' && nextChars[1] == '-');
    if (!endOfArchiveReached) {
      String line = m_lineReader.nextChunkAsUTF8StringWithLatin1Fallback();
      if (!line.isEmpty()) {
        DVLOG(1) << "No CRLF at end of binary section.";
        return nullptr;
      }
    }
  } else {
    String line;
    while (!(line = m_lineReader.nextChunkAsUTF8StringWithLatin1Fallback())
                .isNull()) {
      endOfArchiveReached = (line == endOfDocumentBoundary);
      if (checkBoundary && (line == endOfPartBoundary || endOfArchiveReached)) {
        endOfPartReached = true;
        break;
      }
      // Note that we use line.utf8() and not line.ascii() as ascii turns
      // special characters (such as tab, line-feed...) into '?'.
      content->append(line.utf8().data(), line.length());
      if (contentTransferEncoding == MIMEHeader::QuotedPrintable) {
        // The line reader removes the \r\n, but we need them for the content in
        // this case as the QuotedPrintable decoder expects CR-LF terminated
        // lines.
        content->append("\r\n", 2u);
      }
    }
  }
  if (!endOfPartReached && checkBoundary) {
    DVLOG(1) << "No boundary found for MHTML part.";
    return nullptr;
  }

  Vector<char> data;
  switch (contentTransferEncoding) {
    case MIMEHeader::Base64:
      if (!base64Decode(content->data(), content->size(), data)) {
        DVLOG(1) << "Invalid base64 content for MHTML part.";
        return nullptr;
      }
      break;
    case MIMEHeader::QuotedPrintable:
      quotedPrintableDecode(content->data(), content->size(), data);
      break;
    case MIMEHeader::EightBit:
    case MIMEHeader::SevenBit:
    case MIMEHeader::Binary:
      data.append(content->data(), content->size());
      break;
    default:
      DVLOG(1) << "Invalid encoding for MHTML part.";
      return nullptr;
  }
  RefPtr<SharedBuffer> contentBuffer = SharedBuffer::adoptVector(data);
  // FIXME: the URL in the MIME header could be relative, we should resolve it
  // if it is.  The specs mentions 5 ways to resolve a URL:
  // http://tools.ietf.org/html/rfc2557#section-5
  // IE and Firefox (UNMht) seem to generate only absolute URLs.
  KURL location = KURL(KURL(), mimeHeader.contentLocation());
  return ArchiveResource::create(contentBuffer, location,
                                 mimeHeader.contentID(),
                                 AtomicString(mimeHeader.contentType()),
                                 AtomicString(mimeHeader.charset()));
}

// static
KURL MHTMLParser::convertContentIDToURI(const String& contentID) {
  // This function is based primarily on an example from rfc2557 in section
  // 9.5, but also based on more normative parts of specs like:
  // - rfc2557 - MHTML - section 8.3 - "Use of the Content-ID header and CID
  //                                    URLs"
  // - rfc1738 - URL - section 4 (reserved scheme names;  includes "cid")
  // - rfc2387 - multipart/related - section 3.4 - "Syntax" (cid := msg-id)
  // - rfc0822 - msg-id = "<" addr-spec ">"; addr-spec = local-part "@" domain

  if (contentID.length() <= 2)
    return KURL();

  if (!contentID.startsWith('<') || !contentID.endsWith('>'))
    return KURL();

  StringBuilder uriBuilder;
  uriBuilder.append("cid:");
  uriBuilder.append(contentID, 1, contentID.length() - 2);
  return KURL(KURL(), uriBuilder.toString());
}

}  // namespace blink
