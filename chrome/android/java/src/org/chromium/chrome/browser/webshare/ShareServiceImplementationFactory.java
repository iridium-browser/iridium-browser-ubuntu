// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import org.chromium.content.browser.InterfaceRegistry.ImplementationFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.mojom.webshare.ShareService;

/**
 * Factory that creates instances of ShareService.
 */
public class ShareServiceImplementationFactory implements ImplementationFactory<ShareService> {
    private final WebContents mWebContents;

    public ShareServiceImplementationFactory(WebContents webContents) {
        mWebContents = webContents;
    }

    @Override
    public ShareService createImpl() {
        return new ShareServiceImpl(mWebContents);
    }
}
