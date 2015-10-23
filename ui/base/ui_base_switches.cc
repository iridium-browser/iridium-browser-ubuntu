// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Fall back to using CAOpenGLLayers display content, instead of the IOSurface
// based overlay display path.
const char kDisableMacOverlays[] = "disable-mac-overlays";

// Disable use of cross-process CALayers to display content directly from the
// GPU process on Mac.
const char kDisableRemoteCoreAnimation[] = "disable-remote-core-animation";

// Show borders around CALayers corresponding to overlays and partial damage.
const char kShowMacOverlayBorders[] = "show-mac-overlay-borders";
#endif

// Disables use of DWM composition for top level windows.
const char kDisableDwmComposition[] = "disable-dwm-composition";

// Disables large icons on the New Tab page.
const char kDisableIconNtp[] = "disable-icon-ntp";

// Disables touch adjustment.
const char kDisableTouchAdjustment[] = "disable-touch-adjustment";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disables controls that support touch base text editing.
const char kDisableTouchEditing[] = "disable-touch-editing";

// Disables additional visual feedback to touch input.
const char kDisableTouchFeedback[] = "disable-touch-feedback";

// Enables large icons on the New Tab page.
const char kEnableIconNtp[] = "enable-icon-ntp";

// Enables a zoomed popup bubble that allows the user to select a link.
const char kEnableLinkDisambiguationPopup[] =
    "enable-link-disambiguation-popup";

// Enables touch event based drag and drop.
const char kEnableTouchDragDrop[] = "enable-touch-drag-drop";

// Enables controls that support touch base text editing.
const char kEnableTouchEditing[] = "enable-touch-editing";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
const char kLang[] = "lang";

// Defines the Material Design visual feedback shape.
const char kMaterialDesignInkDrop[] = "material-design-ink-drop";

// Defines the Material Design visual feedback as a circle.
const char kMaterialDesignInkDropCircle[] = "circle";

// Defines the Material Design visual feedback as a sqaure.
const char kMaterialDesignInkDropSquare[] = "square";

// Defines the speed of Material Design visual feedback animations.
const char kMaterialDesignInkDropAnimationSpeed[] =
    "material-design-ink-drop-animation-speed";

// Defines that Material Design visual feedback animations should be fast.
const char kMaterialDesignInkDropAnimationSpeedFast[] = "fast";

// Defines that Material Design visual feedback animations should be slow.
const char kMaterialDesignInkDropAnimationSpeedSlow[] = "slow";

#if defined(ENABLE_TOPCHROME_MD)
// Enables top Chrome material design elements.
const char kTopChromeMD[] = "top-chrome-md";

// Material design mode for the |kTopChromeMD| switch.
const char kTopChromeMDMaterial[] = "material";

// Material design hybrid mode for the |kTopChromeMD| switch. Targeted for
// mouse/touch hybrid devices.
const char kTopChromeMDMaterialHybrid[] = "material-hybrid";

// Classic, non-material, mode for the |kTopChromeMD| switch.
const char kTopChromeMDNonMaterial[] = "";
#endif  // defined(ENABLE_TOPCHROME_MD)

// On Windows only: requests that Chrome connect to the running Metro viewer
// process.
const char kViewerConnect[] = "connect-to-metro-viewer";

}  // namespace switches
