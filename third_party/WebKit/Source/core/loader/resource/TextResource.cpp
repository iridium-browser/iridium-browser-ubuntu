// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/TextResource.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "platform/SharedBuffer.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

TextResource::TextResource(const ResourceRequest& resourceRequest,
                           Resource::Type type,
                           const ResourceLoaderOptions& options,
                           const String& mimeType,
                           const String& charset)
    : Resource(resourceRequest, type, options),
      m_decoder(TextResourceDecoder::create(mimeType, charset)) {}

TextResource::~TextResource() {}

void TextResource::setEncoding(const String& chs) {
  m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String TextResource::encoding() const {
  return m_decoder->encoding().name();
}

String TextResource::decodedText() const {
  DCHECK(data());

  StringBuilder builder;
  const char* segment;
  size_t position = 0;
  while (size_t length = data()->getSomeData(segment, position)) {
    builder.append(m_decoder->decode(segment, length));
    position += length;
  }
  builder.append(m_decoder->flush());
  return builder.toString();
}

}  // namespace blink
