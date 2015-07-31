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

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

#include "public/web/WebSettings.h"

namespace blink {

class DevToolsEmulator;
class Settings;

class WebSettingsImpl final : public WebSettings {
public:
    WebSettingsImpl(Settings*, DevToolsEmulator*);
    virtual ~WebSettingsImpl() { }

    virtual void setFromStrings(const WebString& name, const WebString& value) override;

    virtual bool mainFrameResizesAreOrientationChanges() const override;
    virtual bool shrinksViewportContentToFit() const override;
    virtual int availablePointerTypes() const override;
    virtual PointerType primaryPointerType() const override;
    virtual int availableHoverTypes() const override;
    virtual HoverType primaryHoverType() const override;
    virtual bool viewportEnabled() const override;
    virtual void setAccelerated2dCanvasEnabled(bool) override;
    virtual void setAccelerated2dCanvasMSAASampleCount(int) override;
    virtual void setAcceleratedCompositingEnabled(bool) override;
    virtual void setPreferCompositingToLCDTextEnabled(bool) override;
    virtual void setAccessibilityEnabled(bool) override;
    virtual void setAccessibilityPasswordValuesEnabled(bool) override;
    virtual void setAllowDisplayOfInsecureContent(bool) override;
    virtual void setAllowFileAccessFromFileURLs(bool) override;
    virtual void setAllowCustomScrollbarInMainFrame(bool) override;
    virtual void setAllowRunningOfInsecureContent(bool) override;
    virtual void setAllowScriptsToCloseWindows(bool) override;
    virtual void setAllowUniversalAccessFromFileURLs(bool) override;
    virtual void setAntialiased2dCanvasEnabled(bool) override;
    virtual void setAntialiasedClips2dCanvasEnabled(bool) override;
    virtual void setAsynchronousSpellCheckingEnabled(bool) override;
    virtual void setAutoZoomFocusedNodeToLegibleScale(bool) override;
    virtual void setCaretBrowsingEnabled(bool) override;
    virtual void setClobberUserAgentInitialScaleQuirk(bool) override;
    virtual void setCookieEnabled(bool) override;
    virtual void setNavigateOnDragDrop(bool) override;
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setDNSPrefetchingEnabled(bool) override;
    virtual void setDOMPasteAllowed(bool) override;
    virtual void setDefaultFixedFontSize(int) override;
    virtual void setDefaultFontSize(int) override;
    virtual void setDefaultTextEncodingName(const WebString&) override;
    virtual void setDefaultVideoPosterURL(const WebString&) override;
    virtual void setDeferredImageDecodingEnabled(bool) override;
    virtual void setDeviceScaleAdjustment(float) override;

    // FIXME: Replace these two with pointer/hover queries? crbug.com/441813
    virtual void setDeviceSupportsMouse(bool) override;
    virtual void setDeviceSupportsTouch(bool) override;

    virtual void setDisableReadingFromCanvas(bool) override;
    virtual void setDoubleTapToZoomEnabled(bool) override;
    virtual void setDownloadableBinaryFontsEnabled(bool) override;
    virtual void setEditingBehavior(EditingBehavior) override;
    virtual void setEnableScrollAnimator(bool) override;
    virtual void setEnableTouchAdjustment(bool) override;
    virtual bool multiTargetTapNotificationEnabled() override;
    virtual void setMultiTargetTapNotificationEnabled(bool) override;
    virtual void setRegionBasedColumnsEnabled(bool) override;
    virtual void setExperimentalWebGLEnabled(bool) override;
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setForceZeroLayoutHeight(bool) override;
    virtual void setFullscreenSupported(bool) override;
    virtual void setHyperlinkAuditingEnabled(bool) override;
    virtual void setIgnoreMainFrameOverflowHiddenQuirk(bool) override;
    virtual void setImageAnimationPolicy(ImageAnimationPolicy) override;
    virtual void setImagesEnabled(bool) override;
    virtual void setInlineTextBoxAccessibilityEnabled(bool) override;
    virtual void setJavaEnabled(bool) override;
    virtual void setJavaScriptCanAccessClipboard(bool) override;
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool) override;
    virtual void setJavaScriptEnabled(bool) override;
    virtual void setLoadsImagesAutomatically(bool) override;
    virtual void setLoadWithOverviewMode(bool) override;
    virtual void setLocalStorageEnabled(bool) override;
    virtual void setMainFrameClipsContent(bool) override;
    virtual void setMainFrameResizesAreOrientationChanges(bool) override;
    virtual void setMaxTouchPoints(int) override;
    virtual void setMediaControlsOverlayPlayButtonEnabled(bool) override;
    virtual void setMediaPlaybackRequiresUserGesture(bool) override;
    virtual void setMinimumAccelerated2dCanvasSize(int) override;
    virtual void setMinimumFontSize(int) override;
    virtual void setMinimumLogicalFontSize(int) override;
    virtual void setMockScrollbarsEnabled(bool) override;
    virtual void setOfflineWebApplicationCacheEnabled(bool) override;
    virtual void setOpenGLMultisamplingEnabled(bool) override;
    virtual void setPasswordEchoDurationInSeconds(double) override;
    virtual void setPasswordEchoEnabled(bool) override;
    virtual void setPerTilePaintingEnabled(bool) override;
    virtual void setPictographFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setPinchOverlayScrollbarThickness(int) override;
    virtual void setPluginsEnabled(bool) override;
    virtual void setAvailablePointerTypes(int) override;
    virtual void setPrimaryPointerType(PointerType) override;
    virtual void setAvailableHoverTypes(int) override;
    virtual void setPrimaryHoverType(HoverType) override;
    virtual void setRenderVSyncNotificationEnabled(bool) override;
    virtual void setReportScreenSizeInPhysicalPixelsQuirk(bool) override;
    virtual void setRootLayerScrolls(bool) override;
    virtual void setRubberBandingOnCompositorThread(bool) override;
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setSelectTrailingWhitespaceEnabled(bool override);
    virtual void setSelectionIncludesAltImageText(bool) override;
    virtual void setSelectionStrategy(SelectionStrategyType) override;
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setShouldPrintBackgrounds(bool) override;
    virtual void setShouldClearDocumentBackground(bool) override;
    virtual void setShouldRespectImageOrientation(bool) override;
    virtual void setShowContextMenuOnMouseUp(bool) override;
    virtual void setShowFPSCounter(bool) override;
    virtual void setShowPaintRects(bool) override;
    virtual void setShrinksViewportContentToFit(bool) override;
    virtual void setSmartInsertDeleteEnabled(bool) override;
    virtual void setSpatialNavigationEnabled(bool) override;
    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) override;
    virtual void setStrictMixedContentChecking(bool) override;
    virtual void setStrictPowerfulFeatureRestrictions(bool) override;
    virtual void setSupportDeprecatedTargetDensityDPI(bool) override;
    virtual void setSupportsMultipleWindows(bool) override;
    virtual void setSyncXHRInDocumentsEnabled(bool) override;
    virtual void setTextAreasAreResizable(bool) override;
    virtual void setTextAutosizingEnabled(bool) override;
    virtual void setAccessibilityFontScaleFactor(float) override;
    virtual void setTextTrackBackgroundColor(const WebString&);
    virtual void setTextTrackFontFamily(const WebString&);
    virtual void setTextTrackFontStyle(const WebString&);
    virtual void setTextTrackFontVariant(const WebString&);
    virtual void setTextTrackTextColor(const WebString&);
    virtual void setTextTrackTextShadow(const WebString&);
    virtual void setTextTrackTextSize(const WebString&);
    virtual void setThreadedScrollingEnabled(bool) override;
    virtual void setTouchDragDropEnabled(bool) override;
    virtual void setTouchEditingEnabled(bool) override;
    virtual void setUnifiedTextCheckerEnabled(bool) override;
    virtual void setUnsafePluginPastingEnabled(bool) override;
    virtual void setUsesEncodingDetector(bool) override;
    virtual void setUseLegacyBackgroundSizeShorthandBehavior(bool) override;
    virtual void setUseMobileViewportStyle(bool) override;
    virtual void setUseSolidColorScrollbars(bool) override;
    virtual void setUseWideViewport(bool) override;
    virtual void setV8CacheOptions(V8CacheOptions) override;
    virtual void setValidationMessageTimerMagnification(int) override;
    virtual void setViewportEnabled(bool) override;
    virtual void setViewportMetaEnabled(bool) override;
    virtual void setViewportMetaLayoutSizeQuirk(bool) override;
    virtual void setViewportMetaMergeContentQuirk(bool) override;
    virtual void setViewportMetaNonUserScalableQuirk(bool) override;
    virtual void setViewportMetaZeroValuesQuirk(bool) override;
    virtual void setWebAudioEnabled(bool) override;
    virtual void setWebGLErrorsToConsoleEnabled(bool) override;
    virtual void setWebSecurityEnabled(bool) override;
    virtual void setWideViewportQuirkEnabled(bool) override;
    virtual void setXSSAuditorEnabled(bool) override;

    bool showFPSCounter() const { return m_showFPSCounter; }
    bool showPaintRects() const { return m_showPaintRects; }
    bool renderVSyncNotificationEnabled() const { return m_renderVSyncNotificationEnabled; }
    bool autoZoomFocusedNodeToLegibleScale() const { return m_autoZoomFocusedNodeToLegibleScale; }
    bool doubleTapToZoomEnabled() const;
    bool perTilePaintingEnabled() const { return m_perTilePaintingEnabled; }
    bool supportDeprecatedTargetDensityDPI() const { return m_supportDeprecatedTargetDensityDPI; }
    bool viewportMetaEnabled() const;
    bool viewportMetaLayoutSizeQuirk() const { return m_viewportMetaLayoutSizeQuirk; }
    bool viewportMetaNonUserScalableQuirk() const { return m_viewportMetaNonUserScalableQuirk; }
    bool clobberUserAgentInitialScaleQuirk() const { return m_clobberUserAgentInitialScaleQuirk; }

    void setMockGestureTapHighlightsEnabled(bool);
    bool mockGestureTapHighlightsEnabled() const;

private:
    Settings* m_settings;
    DevToolsEmulator* m_devToolsEmulator;
    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_renderVSyncNotificationEnabled;
    bool m_autoZoomFocusedNodeToLegibleScale;
    bool m_deferredImageDecodingEnabled;
    bool m_perTilePaintingEnabled;
    bool m_supportDeprecatedTargetDensityDPI;
    bool m_shrinksViewportContentToFit;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/277369.
    bool m_viewportMetaLayoutSizeQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/312691.
    bool m_viewportMetaNonUserScalableQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/313754.
    bool m_clobberUserAgentInitialScaleQuirk;
    bool m_mainFrameResizesAreOrientationChanges;
};

} // namespace blink

#endif
