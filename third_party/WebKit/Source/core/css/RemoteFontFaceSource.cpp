// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/RemoteFontFaceSource.h"

#include "core/css/CSSCustomFontData.h"
#include "core/css/CSSFontFace.h"
#include "core/css/CSSFontSelector.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrameClient.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/page/NetworkStateNotifier.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontDescription.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/ResourceLoadPriority.h"
#include "public/platform/WebEffectiveConnectionType.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

bool isEffectiveConnectionTypeSlowFor(Document* document) {
  WebEffectiveConnectionType type =
      document->frame()->loader().client()->getEffectiveConnectionType();

  WebEffectiveConnectionType thresholdType =
      WebEffectiveConnectionType::TypeUnknown;
  if (RuntimeEnabledFeatures::webFontsInterventionV2With2GEnabled()) {
    thresholdType = WebEffectiveConnectionType::Type2G;
  } else if (RuntimeEnabledFeatures::webFontsInterventionV2With3GEnabled()) {
    thresholdType = WebEffectiveConnectionType::Type3G;
  } else if (RuntimeEnabledFeatures::
                 webFontsInterventionV2WithSlow2GEnabled()) {
    thresholdType = WebEffectiveConnectionType::TypeSlow2G;
  }
  DCHECK_NE(WebEffectiveConnectionType::TypeUnknown, thresholdType);

  return WebEffectiveConnectionType::TypeOffline <= type &&
         type <= thresholdType;
}

bool isConnectionTypeSlow() {
  return networkStateNotifier().connectionType() == WebConnectionTypeCellular2G;
}

bool isInterventionV2Enabled() {
  return RuntimeEnabledFeatures::webFontsInterventionV2With2GEnabled() ||
         RuntimeEnabledFeatures::webFontsInterventionV2With3GEnabled() ||
         RuntimeEnabledFeatures::webFontsInterventionV2WithSlow2GEnabled();
}

}  // namespace

RemoteFontFaceSource::RemoteFontFaceSource(FontResource* font,
                                           CSSFontSelector* fontSelector,
                                           FontDisplay display)
    : m_font(font),
      m_fontSelector(fontSelector),
      m_display(display),
      m_period(display == FontDisplaySwap ? SwapPeriod : BlockPeriod),
      m_histograms(font->url().protocolIsData()
                       ? FontLoadHistograms::FromDataURL
                       : font->isLoaded() ? FontLoadHistograms::FromMemoryCache
                                          : FontLoadHistograms::FromUnknown,
                   m_display),
      m_isInterventionTriggered(false) {
  m_font->addClient(this);

  if (shouldTriggerWebFontsIntervention()) {
    m_isInterventionTriggered = true;
    m_period = SwapPeriod;
    m_fontSelector->document()->addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, InfoMessageLevel,
        "Slow network is detected. Fallback font will be used while loading: " +
            m_font->url().elidedString()));
  }
}

RemoteFontFaceSource::~RemoteFontFaceSource() {}

void RemoteFontFaceSource::dispose() {
  m_font->removeClient(this);
  m_font = nullptr;
  pruneTable();
}

void RemoteFontFaceSource::pruneTable() {
  if (m_fontDataTable.isEmpty())
    return;

  for (const auto& item : m_fontDataTable) {
    SimpleFontData* fontData = item.value.get();
    if (fontData && fontData->customFontData())
      fontData->customFontData()->clearFontFaceSource();
  }
  m_fontDataTable.clear();
}

bool RemoteFontFaceSource::isLoading() const {
  return m_font->isLoading();
}

bool RemoteFontFaceSource::isLoaded() const {
  return m_font->isLoaded();
}

bool RemoteFontFaceSource::isValid() const {
  return !m_font->errorOccurred();
}

void RemoteFontFaceSource::notifyFinished(Resource*) {
  m_histograms.maySetDataSource(m_font->response().wasCached()
                                    ? FontLoadHistograms::FromDiskCache
                                    : FontLoadHistograms::FromNetwork);
  m_histograms.recordRemoteFont(m_font.get(), m_isInterventionTriggered);
  m_histograms.fontLoaded(m_font->isCORSFailed(),
                          m_font->getStatus() == ResourceStatus::LoadError,
                          m_isInterventionTriggered);

  m_font->ensureCustomFontData();
  // FIXME: Provide more useful message such as OTS rejection reason.
  // See crbug.com/97467
  if (m_font->getStatus() == ResourceStatus::DecodeError &&
      m_fontSelector->document()) {
    m_fontSelector->document()->addConsoleMessage(ConsoleMessage::create(
        OtherMessageSource, WarningMessageLevel,
        "Failed to decode downloaded font: " + m_font->url().elidedString()));
    if (m_font->otsParsingMessage().length() > 1)
      m_fontSelector->document()->addConsoleMessage(ConsoleMessage::create(
          OtherMessageSource, WarningMessageLevel,
          "OTS parsing error: " + m_font->otsParsingMessage()));
  }

  pruneTable();
  if (m_face) {
    m_fontSelector->fontFaceInvalidated();
    m_face->fontLoaded(this);
  }
}

void RemoteFontFaceSource::fontLoadShortLimitExceeded(FontResource*) {
  if (m_font->isLoaded())
    return;

  if (m_display == FontDisplayFallback)
    switchToSwapPeriod();
  else if (m_display == FontDisplayOptional)
    switchToFailurePeriod();
}

void RemoteFontFaceSource::fontLoadLongLimitExceeded(FontResource*) {
  if (m_font->isLoaded())
    return;

  if (m_display == FontDisplayBlock ||
      (!m_isInterventionTriggered && m_display == FontDisplayAuto))
    switchToSwapPeriod();
  else if (m_display == FontDisplayFallback)
    switchToFailurePeriod();

  m_histograms.longLimitExceeded(m_isInterventionTriggered);
}

void RemoteFontFaceSource::switchToSwapPeriod() {
  ASSERT(m_period == BlockPeriod);
  m_period = SwapPeriod;

  pruneTable();
  if (m_face) {
    m_fontSelector->fontFaceInvalidated();
    m_face->didBecomeVisibleFallback(this);
  }

  m_histograms.recordFallbackTime(m_font.get());
}

void RemoteFontFaceSource::switchToFailurePeriod() {
  if (m_period == BlockPeriod)
    switchToSwapPeriod();
  ASSERT(m_period == SwapPeriod);
  m_period = FailurePeriod;
}

bool RemoteFontFaceSource::shouldTriggerWebFontsIntervention() {
  if (RuntimeEnabledFeatures::webFontsInterventionTriggerEnabled())
    return true;
  if (m_histograms.dataSource() == FontLoadHistograms::FromMemoryCache ||
      m_histograms.dataSource() == FontLoadHistograms::FromDataURL)
    return false;

  bool networkIsSlow =
      isInterventionV2Enabled()
          ? isEffectiveConnectionTypeSlowFor(m_fontSelector->document())
          : isConnectionTypeSlow();

  return networkIsSlow && m_display == FontDisplayAuto;
}

bool RemoteFontFaceSource::isLowPriorityLoadingAllowedForRemoteFont() const {
  return m_isInterventionTriggered && isInterventionV2Enabled();
}

PassRefPtr<SimpleFontData> RemoteFontFaceSource::createFontData(
    const FontDescription& fontDescription) {
  if (!isLoaded())
    return createLoadingFallbackFontData(fontDescription);

  if (!m_font->ensureCustomFontData() || m_period == FailurePeriod)
    return nullptr;

  m_histograms.recordFallbackTime(m_font.get());

  return SimpleFontData::create(
      m_font->platformDataFromCustomData(fontDescription.effectiveFontSize(),
                                         fontDescription.isSyntheticBold(),
                                         fontDescription.isSyntheticItalic(),
                                         fontDescription.orientation(),
                                         fontDescription.variationSettings()),
      CustomFontData::create());
}

PassRefPtr<SimpleFontData> RemoteFontFaceSource::createLoadingFallbackFontData(
    const FontDescription& fontDescription) {
  // This temporary font is not retained and should not be returned.
  FontCachePurgePreventer fontCachePurgePreventer;
  SimpleFontData* temporaryFont =
      FontCache::fontCache()->getNonRetainedLastResortFallbackFont(
          fontDescription);
  if (!temporaryFont) {
    ASSERT_NOT_REACHED();
    return nullptr;
  }
  RefPtr<CSSCustomFontData> cssFontData = CSSCustomFontData::create(
      this, m_period == BlockPeriod ? CSSCustomFontData::InvisibleFallback
                                    : CSSCustomFontData::VisibleFallback);
  return SimpleFontData::create(temporaryFont->platformData(), cssFontData);
}

void RemoteFontFaceSource::beginLoadIfNeeded() {
  if (m_fontSelector->document() && m_font->stillNeedsLoad()) {
    if (!m_font->url().protocolIsData() && !m_font->isLoaded() &&
        m_display == FontDisplayAuto &&
        m_font->isLowPriorityLoadingAllowedForRemoteFont()) {
      // Set the loading priority to VeryLow since this font is not required
      // for painting the text.
      m_font->didChangePriority(ResourceLoadPriorityVeryLow, 0);
    }
    if (m_fontSelector->document()->fetcher()->startLoad(m_font)) {
      // Start timers only when load is actually started asynchronously.
      if (!m_font->isLoaded())
        m_font->startLoadLimitTimers();
      m_histograms.loadStarted();
    }
  }

  if (m_face)
    m_face->didBeginLoad();
}

DEFINE_TRACE(RemoteFontFaceSource) {
  visitor->trace(m_font);
  visitor->trace(m_fontSelector);
  CSSFontFaceSource::trace(visitor);
  FontResourceClient::trace(visitor);
}

void RemoteFontFaceSource::FontLoadHistograms::loadStarted() {
  if (!m_loadStartTime)
    m_loadStartTime = currentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::fallbackFontPainted(
    DisplayPeriod period) {
  if (period == BlockPeriod && !m_blankPaintTime)
    m_blankPaintTime = currentTimeMS();
}

void RemoteFontFaceSource::FontLoadHistograms::fontLoaded(
    bool isCorsFailed,
    bool loadError,
    bool isInterventionTriggered) {
  if (!m_isLongLimitExceeded && m_fontDisplay == FontDisplayAuto &&
      !isCorsFailed && !loadError) {
    recordInterventionResult(isInterventionTriggered);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::longLimitExceeded(
    bool isInterventionTriggered) {
  m_isLongLimitExceeded = true;
  maySetDataSource(FromNetwork);
  if (m_fontDisplay == FontDisplayAuto)
    recordInterventionResult(isInterventionTriggered);
}

void RemoteFontFaceSource::FontLoadHistograms::recordFallbackTime(
    const FontResource* font) {
  if (m_blankPaintTime <= 0)
    return;
  int duration = static_cast<int>(currentTimeMS() - m_blankPaintTime);
  DEFINE_STATIC_LOCAL(CustomCountHistogram, blankTextShownTimeHistogram,
                      ("WebFont.BlankTextShownTime", 0, 10000, 50));
  blankTextShownTimeHistogram.count(duration);
  m_blankPaintTime = -1;
}

void RemoteFontFaceSource::FontLoadHistograms::recordRemoteFont(
    const FontResource* font,
    bool isInterventionTriggered) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, cacheHitHistogram,
                      ("WebFont.CacheHit", CacheHitEnumMax));
  cacheHitHistogram.count(dataSourceMetricsValue());

  if (m_dataSource == FromDiskCache || m_dataSource == FromNetwork) {
    DCHECK_NE(m_loadStartTime, 0);
    int duration = static_cast<int>(currentTimeMS() - m_loadStartTime);
    recordLoadTimeHistogram(font, duration, isInterventionTriggered);

    enum { CORSFail, CORSSuccess, CORSEnumMax };
    int corsValue = font->isCORSFailed() ? CORSFail : CORSSuccess;
    DEFINE_STATIC_LOCAL(EnumerationHistogram, corsHistogram,
                        ("WebFont.CORSSuccess", CORSEnumMax));
    corsHistogram.count(corsValue);
  }
}

void RemoteFontFaceSource::FontLoadHistograms::maySetDataSource(
    DataSource dataSource) {
  if (m_dataSource != FromUnknown)
    return;
  // Classify as memory cache hit if |m_loadStartTime| is not set, i.e.
  // this RemoteFontFaceSource instance didn't trigger FontResource
  // loading.
  if (m_loadStartTime == 0)
    m_dataSource = FromMemoryCache;
  else
    m_dataSource = dataSource;
}

void RemoteFontFaceSource::FontLoadHistograms::recordLoadTimeHistogram(
    const FontResource* font,
    int duration,
    bool isInterventionTriggered) {
  CHECK_NE(FromUnknown, m_dataSource);

  if (font->errorOccurred()) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, loadErrorHistogram,
                        ("WebFont.DownloadTime.LoadError", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missedCacheLoadErrorHistogram,
        ("WebFont.MissedCache.DownloadTime.LoadError", 0, 10000, 50));
    loadErrorHistogram.count(duration);
    if (m_dataSource == FromNetwork)
      missedCacheLoadErrorHistogram.count(duration);
    return;
  }

  unsigned size = font->encodedSize();
  if (size < 10 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under10kHistogram,
                        ("WebFont.DownloadTime.0.Under10KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missedCacheUnder10kHistogram,
        ("WebFont.MissedCache.DownloadTime.0.Under10KB", 0, 10000, 50));
    under10kHistogram.count(duration);
    if (m_dataSource == FromNetwork)
      missedCacheUnder10kHistogram.count(duration);
    return;
  }
  if (size < 50 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under50kHistogram,
                        ("WebFont.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missedCacheUnder50kHistogram,
        ("WebFont.MissedCache.DownloadTime.1.10KBTo50KB", 0, 10000, 50));
    // Breakdowns metrics to understand WebFonts intervention.
    // Now we only cover this 10KBto50KB range because 70% of requests are
    // covered in this range, and having metrics for all size cases cost.
    DEFINE_STATIC_LOCAL(CustomCountHistogram,
                        missedCacheAndInterventionTriggeredUnder50kHistogram,
                        ("WebFont.MissedCacheAndInterventionTriggered."
                         "DownloadTime.1.10KBTo50KB",
                         0, 10000, 50));
    DEFINE_STATIC_LOCAL(CustomCountHistogram,
                        missedCacheAndInterventionNotTriggeredUnder50kHistogram,
                        ("WebFont.MissedCacheAndInterventionNotTriggered."
                         "DownloadTime.1.10KBTo50KB",
                         0, 10000, 50));
    under50kHistogram.count(duration);
    if (m_dataSource == FromNetwork) {
      missedCacheUnder50kHistogram.count(duration);
      if (isInterventionTriggered)
        missedCacheAndInterventionTriggeredUnder50kHistogram.count(duration);
      else
        missedCacheAndInterventionNotTriggeredUnder50kHistogram.count(duration);
    }
    return;
  }
  if (size < 100 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under100kHistogram,
                        ("WebFont.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missedCacheUnder100kHistogram,
        ("WebFont.MissedCache.DownloadTime.2.50KBTo100KB", 0, 10000, 50));
    under100kHistogram.count(duration);
    if (m_dataSource == FromNetwork)
      missedCacheUnder100kHistogram.count(duration);
    return;
  }
  if (size < 1024 * 1024) {
    DEFINE_STATIC_LOCAL(CustomCountHistogram, under1mbHistogram,
                        ("WebFont.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, missedCacheUnder1mbHistogram,
        ("WebFont.MissedCache.DownloadTime.3.100KBTo1MB", 0, 10000, 50));
    under1mbHistogram.count(duration);
    if (m_dataSource == FromNetwork)
      missedCacheUnder1mbHistogram.count(duration);
    return;
  }
  DEFINE_STATIC_LOCAL(CustomCountHistogram, over1mbHistogram,
                      ("WebFont.DownloadTime.4.Over1MB", 0, 10000, 50));
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, missedCacheOver1mbHistogram,
      ("WebFont.MissedCache.DownloadTime.4.Over1MB", 0, 10000, 50));
  over1mbHistogram.count(duration);
  if (m_dataSource == FromNetwork)
    missedCacheOver1mbHistogram.count(duration);
}

void RemoteFontFaceSource::FontLoadHistograms::recordInterventionResult(
    bool isTriggered) {
  CHECK_NE(FromUnknown, m_dataSource);

  // interventionResult takes 0-3 values.
  int interventionResult = 0;
  if (m_isLongLimitExceeded)
    interventionResult |= 1 << 0;
  if (isTriggered)
    interventionResult |= 1 << 1;
  const int boundary = 1 << 2;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, interventionHistogram,
                      ("WebFont.InterventionResult", boundary));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, missedCacheInterventionHistogram,
                      ("WebFont.InterventionResult.MissedCache", boundary));
  interventionHistogram.count(interventionResult);
  if (m_dataSource == FromNetwork)
    missedCacheInterventionHistogram.count(interventionResult);
}

RemoteFontFaceSource::FontLoadHistograms::CacheHitMetrics
RemoteFontFaceSource::FontLoadHistograms::dataSourceMetricsValue() {
  switch (m_dataSource) {
    case FromDataURL:
      return DataUrl;
    case FromMemoryCache:
      return MemoryHit;
    case FromDiskCache:
      return DiskHit;
    case FromNetwork:
      return Miss;
    case FromUnknown:
    // Fall through.
    default:
      NOTREACHED();
  }
  return Miss;
}

}  // namespace blink
