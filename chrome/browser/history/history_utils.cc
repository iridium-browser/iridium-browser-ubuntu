// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_utils.h"

#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "url/gurl.h"

bool CanAddURLToHistory(const GURL& url) {
  if (!url.is_valid())
    return false;

  // TODO: We should allow kChromeUIScheme URLs if they have been explicitly
  // typed.  Right now, however, these are marked as typed even when triggered
  // by a shortcut or menu action.
  if (url.SchemeIs(url::kJavaScriptScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kViewSourceScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(chrome::kChromeSearchScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme))
    return false;

  // Allow all about: and chrome: URLs except about:blank, since the user may
  // like to see "chrome://memory/", etc. in their history and autocomplete.
  if (url == GURL(url::kAboutBlankURL))
    return false;

  return true;
}
