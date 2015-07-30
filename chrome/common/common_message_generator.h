// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, hence no include guard.

#include "chrome/common/benchmarking_messages.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/mac/app_shim_messages.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/tts_messages.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/cast_messages.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#endif

#if defined(ENABLE_MDNS)
#include "chrome/common/local_discovery/local_discovery_messages.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/common/service_messages.h"
#endif

#if defined(ENABLE_PRINTING)
#include "chrome/common/chrome_utility_printing_messages.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/common/spellcheck_messages.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/common/media/webrtc_logging_messages.h"
#endif

#if defined(SAFE_BROWSING_SERVICE)
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#endif
