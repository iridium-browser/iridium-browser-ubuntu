// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/web/WebPageImportanceSignals.h"

#include "public/platform/Platform.h"

namespace blink {

void WebPageImportanceSignals::reset()
{
    m_hadFormInteraction = false;
}

void WebPageImportanceSignals::onCommitLoad()
{
    Platform::current()->histogramEnumeration("PageImportanceSignals.HadFormInteraction.OnCommitLoad", m_hadFormInteraction, 2);

    reset();
}

} // namespace blink
