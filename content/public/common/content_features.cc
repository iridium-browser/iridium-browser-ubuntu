// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/content_features.h"

namespace features {

// All features in alphabetical order.

// Enables brotli "Accept-Encoding" advertising and "Content-Encoding" support.
// Brotli format specification: http://www.ietf.org/id/draft-alakuijala-brotli
const base::Feature kBrotliEncoding{"brotli-encoding",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// If Canvas2D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kCanvas2DImageChromium{"Canvas2DImageChromium",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the credential management API:
// https://w3c.github.io/webappsec-credential-management/
const base::Feature kCredentialManagementAPI{"CredentialManagementAPI",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable GPU Rasterization by default. This can still be overridden by
// --force-gpu-rasterization or --disable-gpu-rasterization.
const base::Feature kDefaultEnableGpuRasterization{
    "DefaultEnableGpuRasterization", base::FEATURE_DISABLED_BY_DEFAULT};

// Do not call SharedBuffer::unlock() (https://crbug.com/603791).
const base::Feature kDoNotUnlockSharedBuffer{"DoNotUnlockSharedBuffer",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Speculatively pre-evaluate Javascript which will likely use document.write to
// load an external script. The feature extracts the written markup and sends it
// to the preload scanner.
const base::Feature kDocumentWriteEvaluator{"DocumentWriteEvaluator",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Feature Policy framework for granting and removing access to
// other features through HTTP headers.
const base::Feature kFeaturePolicy{"FeaturePolicy",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a blink::FontCache optimization that reuses a font to serve different
// size of font.
const base::Feature kFontCacheScaling{"FontCacheScaling",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// FeatureList definition for trials to enable the download button on
// MediaDocument.
const base::Feature kMediaDocumentDownloadButton{
    "MediaDocumentDownloadButton",
    base::FEATURE_DISABLED_BY_DEFAULT
};

// Enable the material design playback UI for media elements.  This is always
// on for OS_ANDROID, but may be enabled by experiment for other platforms.
const base::Feature kNewMediaPlaybackUi{"NewMediaPlaybackUi",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Non-validating reload for desktop.
// See https://crbug.com/591245
const base::Feature kNonValidatingReloadOnNormalReload{
    "NonValidatingReloadOnNormalReload", base::FEATURE_ENABLED_BY_DEFAULT};

// Non-validating reload on reload-to-refresh-content (e.g. pull-to-refresh).
// See https://crbug.com/591245
const base::Feature kNonValidatingReloadOnRefreshContent{
    "NonValidatingReloadOnRefreshContentV2",
    base::FEATURE_DISABLED_BY_DEFAULT};

// An experiment to optimize resource loading IPC for small resources.
// http://crbug.com/580928
const base::Feature kOptimizeLoadingIPCForSmallResources{
    "OptimizeLoadingIPCForSmallResources",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Origin Trials for controlling access to feature/API experiments.
const base::Feature kOriginTrials{"OriginTrials",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Paint optimizations including subsequence caching and empty phase skipping.
const base::Feature kPaintOptimizations{"PaintOptimizations",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Whether the lookahead parser in Blink runs on the main thread.
const base::Feature kParseHTMLOnMainThread{"ParseHTMLOnMainThread",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Whether document level event listeners should default 'passive' to true.
const base::Feature kPassiveDocumentEventListeners{
    "PassiveDocumentEventListeners", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether we should force a touchstart and first touchmove per scroll event
// listeners to be 'passive' during fling.
const base::Feature kPassiveEventListenersDueToFling{
    "PassiveEventListenersDueToFling", base::FEATURE_DISABLED_BY_DEFAULT};

// Pointer events support.
const base::Feature kPointerEvents{"PointerEvent",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If Pepper 3D Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kPepper3DImageChromium{"Pepper3DImageChromium",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Throttle Blink's rendering pipeline based on frame visibility.
const base::Feature kRenderingPipelineThrottling{
    "RenderingPipelineThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
const base::Feature kScrollAnchoring{"ScrollAnchoring",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Speculatively launches Service Workers on mouse/touch events.
const base::Feature kSpeculativeLaunchServiceWorker{
    "SpeculativeLaunchServiceWorker", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables implementation of the Cache-Control: stale-while-revalidate directive
// which permits servers to allow the use of stale resources while revalidation
// proceeds in the background. See http://crbug.com/348877
const base::Feature kStaleWhileRevalidate{"StaleWhileRevalidate2",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Throttle Blink timers in out-of-view cross origin frames.
const base::Feature kTimerThrottlingForHiddenFrames{
    "TimerThrottlingForHiddenFrames", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables token binding
// (https://www.ietf.org/id/draft-ietf-tokbind-protocol-04.txt).
const base::Feature kTokenBinding{"token-binding",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enables touchpad and wheel scroll latching.
const base::Feature kTouchpadAndWheelScrollLatching{
    "TouchpadAndWheelScrollLatching", base::FEATURE_DISABLED_BY_DEFAULT};

// Weak MemoryCache (https://crbug.com/603462).
const base::Feature kWeakMemoryCache{"WeakMemoryCache",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If WebGL Image Chromium is allowed, this feature controls whether it is
// enabled.
const base::Feature kWebGLImageChromium{"WebGLImageChromium",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Makes WebRTC use ECDSA certs by default (i.e., when no cert type was
// specified in JS).
const base::Feature kWebRtcEcdsaDefault {"WebRTC-EnableWebRtcEcdsa",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Use GpuMemoryBuffer backed VideoFrames in media streams.
const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames{
    "WebRTC-UseGpuMemoryBufferVideoFrames",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the WebUSB API is enabled:
// https://wicg.github.io/webusb
const base::Feature kWebUsb{"WebUSB", base::FEATURE_ENABLED_BY_DEFAULT};

// Make sendBeacon throw for a Blob with a non simple type.
const base::Feature kSendBeaconThrowForBlobWithNonSimpleType{
    "SendBeaconThrowForBlobWithNonSimpleType",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_ANDROID)
// Allow videos to autoplay without a user gesture if muted.
const base::Feature kAutoplayMutedVideos{"AutoplayMutedVideos",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Use IME's own thread instead of using main UI thread. It also means that
// we will not use replica editor and do a round trip to renderer to synchronize
// with Blink data.
const base::Feature kImeThread{"ImeThread", base::FEATURE_ENABLED_BY_DEFAULT};

// FeatureList definition for the Seccomp field trial.
const base::Feature kSeccompSandboxAndroid{"SeccompSandboxAndroid",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// The JavaScript API for payments on the web.
const base::Feature kWebPayments{"WebPayments",
                                 base::FEATURE_ENABLED_BY_DEFAULT};

#endif

#if defined(OS_WIN)
// Emergency "off switch" for new Windows sandbox security mitigation,
// sandbox::MITIGATION_EXTENSION_POINT_DISABLE.
const base::Feature kWinSboxDisableExtensionPoints{
    "WinSboxDisableExtensionPoint", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

}  // namespace features
