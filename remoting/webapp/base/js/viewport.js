// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides shared view port management utilities.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/** @type {Object} */
remoting.Viewport = {};

/**
 * Helper function accepting client and host dimensions, and returning a chosen
 * size for the plugin element, in DIPs.
 *
 * @param {{width: number, height: number}} clientSizeDips Available client
 *     dimensions, in DIPs.
 * @param {number} clientPixelRatio Number of physical pixels per client DIP.
 * @param {{width: number, height: number}} desktopSize Size of the host desktop
 *     in physical pixels.
 * @param {{x: number, y: number}} desktopDpi DPI of the host desktop in both
 *     dimensions.
 * @param {number} desktopScale The scale factor configured for the host.
 * @param {boolean} isFullscreen True if full-screen mode is active.
 * @param {boolean} shrinkToFit True if shrink-to-fit should be applied.
 * @return {{width: number, height: number}} Chosen plugin dimensions, in DIPs.
 */
remoting.Viewport.choosePluginSize = function(
    clientSizeDips, clientPixelRatio, desktopSize, desktopDpi, desktopScale,
    isFullscreen, shrinkToFit) {
  base.debug.assert(clientSizeDips.width > 0);
  base.debug.assert(clientSizeDips.height > 0);
  base.debug.assert(clientPixelRatio >= 1.0);
  base.debug.assert(desktopSize.width > 0);
  base.debug.assert(desktopSize.height > 0);
  base.debug.assert(desktopDpi.x > 0);
  base.debug.assert(desktopDpi.y > 0);
  base.debug.assert(desktopScale > 0);

  // We have the following goals in sizing the desktop display at the client:
  //  1. Avoid losing detail by down-scaling beyond 1:1 host:device pixels.
  //  2. Avoid up-scaling if that will cause the client to need scrollbars.
  //  3. Avoid introducing blurriness with non-integer up-scaling factors.
  //  4. Avoid having huge "letterboxes" around the desktop, if it's really
  //     small.
  //  5. Compensate for mismatched DPIs, so that the behaviour of features like
  //     shrink-to-fit matches their "natural" rather than their pixel size.
  //     e.g. with shrink-to-fit active a 1024x768 low-DPI host on a 640x480
  //     high-DPI client will be up-scaled to 1280x960, rather than displayed
  //     at 1:1 host:physical client pixels.
  //
  // To determine the ideal size we follow a four-stage process:
  //  1. Determine the "natural" size at which to display the desktop.
  //    a. Initially assume 1:1 mapping of desktop to client device pixels.
  //    b. If host DPI is less than the client's then up-scale accordingly.
  //    c. If desktopScale is configured for the host then allow that to
  //       reduce the amount of up-scaling from (b). e.g. if the client:host
  //       DPIs are 2:1 then a desktopScale of 1.5 would reduce the up-scale
  //       to 4:3, while a desktopScale of 3.0 would result in no up-scaling.
  //  2. If the natural size of the desktop is smaller than the client device
  //     then apply up-scaling by an integer scale factor to avoid excessive
  //     letterboxing.
  //  3. If shrink-to-fit is configured then:
  //     a. If the natural size exceeds the client size then apply down-scaling
  //        by an arbitrary scale factor.
  //     b. If we're in full-screen mode and the client & host aspect-ratios
  //        are radically different (e.g. the host is actually multi-monitor)
  //        then shrink-to-fit to the shorter dimension, rather than leaving
  //        huge letterboxes; the user can then bump-scroll around the desktop.
  //  4. If the overall scale factor is fractionally over an integer factor
  //     then reduce it to that integer factor, to avoid blurring.

  // All calculations are performed in device pixels.
  var clientWidth = clientSizeDips.width * clientPixelRatio;
  var clientHeight = clientSizeDips.height * clientPixelRatio;

  // 1. Determine a "natural" size at which to display the desktop.
  var scale = 1.0;

  // Determine the effective host device pixel ratio.
  // Note that we round up or down to the closest integer pixel ratio.
  var hostPixelRatioX = Math.round(desktopDpi.x / 96);
  var hostPixelRatioY = Math.round(desktopDpi.y / 96);
  var hostPixelRatio = Math.min(hostPixelRatioX, hostPixelRatioY);

  // Allow up-scaling to account for DPI.
  scale = Math.max(scale, clientPixelRatio / hostPixelRatio);

  // Allow some or all of the up-scaling to be cancelled by the desktopScale.
  if (desktopScale > 1.0) {
    scale = Math.max(1.0, scale / desktopScale);
  }

  // 2. If the host is still much smaller than the client, then up-scale to
  //    avoid wasting space, but only by an integer factor, to avoid blurring.
  if (desktopSize.width * scale <= clientWidth &&
      desktopSize.height * scale <= clientHeight) {
    var scaleX = Math.floor(clientWidth / desktopSize.width);
    var scaleY = Math.floor(clientHeight / desktopSize.height);
    scale = Math.min(scaleX, scaleY);
    base.debug.assert(scale >= 1.0);
  }

  // 3. Apply shrink-to-fit, if configured.
  if (shrinkToFit) {
    var scaleFitWidth = Math.min(scale, clientWidth / desktopSize.width);
    var scaleFitHeight = Math.min(scale, clientHeight / desktopSize.height);
    scale = Math.min(scaleFitHeight, scaleFitWidth);

    // If we're running full-screen then try to handle common side-by-side
    // multi-monitor combinations more intelligently.
    if (isFullscreen) {
      // If the host has two monitors each the same size as the client then
      // scale-to-fit will have the desktop occupy only 50% of the client area,
      // in which case it would be preferable to down-scale less and let the
      // user bump-scroll around ("scale-and-pan").
      // Triggering scale-and-pan if less than 65% of the client area would be
      // used adds enough fuzz to cope with e.g. 1280x800 client connecting to
      // a (2x1280)x1024 host nicely.
      // Note that we don't need to account for scrollbars while fullscreen.
      if (scale <= scaleFitHeight * 0.65) {
        scale = scaleFitHeight;
      }
      if (scale <= scaleFitWidth * 0.65) {
        scale = scaleFitWidth;
      }
    }
  }

  // 4. Avoid blurring for close-to-integer up-scaling factors.
  if (scale > 1.0) {
    var scaleBlurriness = scale / Math.floor(scale);
    if (scaleBlurriness < 1.1) {
      scale = Math.floor(scale);
    }
  }

  // Return the necessary plugin dimensions in DIPs.
  scale = scale / clientPixelRatio;
  var pluginWidth = Math.round(desktopSize.width * scale);
  var pluginHeight = Math.round(desktopSize.height * scale);
  return { width: pluginWidth, height: pluginHeight };
};

}());
