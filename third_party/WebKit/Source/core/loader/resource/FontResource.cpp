/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "core/loader/resource/FontResource.h"

#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCustomPlatformData.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/ResourceClientWalker.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "wtf/CurrentTime.h"

namespace blink {

// Durations of font-display periods.
// https://tabatkins.github.io/specs/css-font-display/#font-display-desc
// TODO(toyoshim): Revisit short limit value once cache-aware font display is
// launched. crbug.com/570205
static const double fontLoadWaitShortLimitSec = 0.1;
static const double fontLoadWaitLongLimitSec = 3.0;

enum FontPackageFormat {
  PackageFormatUnknown,
  PackageFormatSFNT,
  PackageFormatWOFF,
  PackageFormatWOFF2,
  PackageFormatSVG,
  PackageFormatEnumMax
};

static FontPackageFormat packageFormatOf(SharedBuffer* buffer) {
  if (buffer->size() < 4)
    return PackageFormatUnknown;

  const char* data = buffer->data();
  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F')
    return PackageFormatWOFF;
  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2')
    return PackageFormatWOFF2;
  return PackageFormatSFNT;
}

static void recordPackageFormatHistogram(FontPackageFormat format) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, packageFormatHistogram,
      new EnumerationHistogram("WebFont.PackageFormat", PackageFormatEnumMax));
  packageFormatHistogram.count(format);
}

FontResource* FontResource::fetch(FetchRequest& request,
                                  ResourceFetcher* fetcher) {
  DCHECK_EQ(request.resourceRequest().frameType(),
            WebURLRequest::FrameTypeNone);
  request.mutableResourceRequest().setRequestContext(
      WebURLRequest::RequestContextFont);
  return toFontResource(
      fetcher->requestResource(request, FontResourceFactory()));
}

FontResource::FontResource(const ResourceRequest& resourceRequest,
                           const ResourceLoaderOptions& options)
    : Resource(resourceRequest, Font, options),
      m_loadLimitState(LoadNotStarted),
      m_corsFailed(false),
      m_fontLoadShortLimitTimer(this,
                                &FontResource::fontLoadShortLimitCallback),
      m_fontLoadLongLimitTimer(this, &FontResource::fontLoadLongLimitCallback) {
}

FontResource::~FontResource() {}

void FontResource::didAddClient(ResourceClient* c) {
  DCHECK(FontResourceClient::isExpectedType(c));
  Resource::didAddClient(c);

  // Block client callbacks if currently loading from cache.
  if (isLoading() && loader()->isCacheAwareLoadingActivated())
    return;

  ProhibitAddRemoveClientInScope prohibitAddRemoveClient(this);
  if (m_loadLimitState == ShortLimitExceeded ||
      m_loadLimitState == LongLimitExceeded)
    static_cast<FontResourceClient*>(c)->fontLoadShortLimitExceeded(this);
  if (m_loadLimitState == LongLimitExceeded)
    static_cast<FontResourceClient*>(c)->fontLoadLongLimitExceeded(this);
}

void FontResource::setRevalidatingRequest(const ResourceRequest& request) {
  // Reload will use the same object, and needs to reset |m_loadLimitState|
  // before any didAddClient() is called again.
  DCHECK(isLoaded());
  DCHECK(!m_fontLoadShortLimitTimer.isActive());
  DCHECK(!m_fontLoadLongLimitTimer.isActive());
  m_loadLimitState = LoadNotStarted;
  Resource::setRevalidatingRequest(request);
}

void FontResource::startLoadLimitTimers() {
  DCHECK(isLoading());
  DCHECK_EQ(m_loadLimitState, LoadNotStarted);
  m_loadLimitState = UnderLimit;
  m_fontLoadShortLimitTimer.startOneShot(fontLoadWaitShortLimitSec,
                                         BLINK_FROM_HERE);
  m_fontLoadLongLimitTimer.startOneShot(fontLoadWaitLongLimitSec,
                                        BLINK_FROM_HERE);
}

bool FontResource::ensureCustomFontData() {
  if (!m_fontData && !errorOccurred() && !isLoading()) {
    if (data())
      m_fontData = FontCustomPlatformData::create(data(), m_otsParsingMessage);

    if (m_fontData) {
      recordPackageFormatHistogram(packageFormatOf(data()));
    } else {
      setStatus(ResourceStatus::DecodeError);
      recordPackageFormatHistogram(PackageFormatUnknown);
    }
  }
  return m_fontData.get();
}

FontPlatformData FontResource::platformDataFromCustomData(
    float size,
    bool bold,
    bool italic,
    FontOrientation orientation,
    FontVariationSettings* fontVariationSettings) {
  DCHECK(m_fontData);
  return m_fontData->fontPlatformData(size, bold, italic, orientation,
                                      fontVariationSettings);
}

void FontResource::willReloadAfterDiskCacheMiss() {
  DCHECK(isLoading());
  DCHECK(loader()->isCacheAwareLoadingActivated());
  if (m_loadLimitState == ShortLimitExceeded ||
      m_loadLimitState == LongLimitExceeded) {
    notifyClientsShortLimitExceeded();
  }
  if (m_loadLimitState == LongLimitExceeded)
    notifyClientsLongLimitExceeded();

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, loadLimitHistogram,
      ("WebFont.LoadLimitOnDiskCacheMiss", LoadLimitStateEnumMax));
  loadLimitHistogram.count(m_loadLimitState);
}

void FontResource::fontLoadShortLimitCallback(TimerBase*) {
  DCHECK(isLoading());
  DCHECK_EQ(m_loadLimitState, UnderLimit);
  m_loadLimitState = ShortLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (loader()->isCacheAwareLoadingActivated())
    return;
  notifyClientsShortLimitExceeded();
}

void FontResource::fontLoadLongLimitCallback(TimerBase*) {
  DCHECK(isLoading());
  DCHECK_EQ(m_loadLimitState, ShortLimitExceeded);
  m_loadLimitState = LongLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (loader()->isCacheAwareLoadingActivated())
    return;
  notifyClientsLongLimitExceeded();
}

void FontResource::notifyClientsShortLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibitAddRemoveClient(this);
  ResourceClientWalker<FontResourceClient> walker(clients());
  while (FontResourceClient* client = walker.next())
    client->fontLoadShortLimitExceeded(this);
}

void FontResource::notifyClientsLongLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibitAddRemoveClient(this);
  ResourceClientWalker<FontResourceClient> walker(clients());
  while (FontResourceClient* client = walker.next())
    client->fontLoadLongLimitExceeded(this);
}

void FontResource::allClientsAndObserversRemoved() {
  m_fontData.reset();
  Resource::allClientsAndObserversRemoved();
}

void FontResource::checkNotify() {
  m_fontLoadShortLimitTimer.stop();
  m_fontLoadLongLimitTimer.stop();

  Resource::checkNotify();
}

bool FontResource::isLowPriorityLoadingAllowedForRemoteFont() const {
  DCHECK(!url().protocolIsData());
  DCHECK(!isLoaded());
  ResourceClientWalker<FontResourceClient> walker(clients());
  while (FontResourceClient* client = walker.next()) {
    if (!client->isLowPriorityLoadingAllowedForRemoteFont()) {
      return false;
    }
  }
  return true;
}

void FontResource::onMemoryDump(WebMemoryDumpLevelOfDetail level,
                                WebProcessMemoryDump* memoryDump) const {
  Resource::onMemoryDump(level, memoryDump);
  if (!m_fontData)
    return;
  const String name = getMemoryDumpName() + "/decoded_webfont";
  WebMemoryAllocatorDump* dump = memoryDump->createMemoryAllocatorDump(name);
  dump->addScalar("size", "bytes", m_fontData->dataSize());
  memoryDump->addSuballocation(dump->guid(), "malloc");
}

}  // namespace blink
