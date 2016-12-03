// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CERTIFICATE_VIEWER_MAC_H_

#define CHROME_BROWSER_UI_COCOA_CERTIFICATE_VIEWER_MAC_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet.h"

class SSLCertificateViewerCocoaBridge;
@class SFCertificatePanel;

namespace net {
  class X509Certificate;
}

@interface SSLCertificateViewerCocoa : NSObject<ConstrainedWindowSheet> {
 @private
  // The corresponding list of certificates.
  base::scoped_nsobject<NSArray> certificates_;
  std::unique_ptr<SSLCertificateViewerCocoaBridge> observer_;
  base::scoped_nsobject<SFCertificatePanel> panel_;
  std::unique_ptr<ConstrainedWindowMac> constrainedWindow_;
  base::scoped_nsobject<NSWindow> overlayWindow_;
  BOOL closePending_;
  // A copy of the overlay window's size used to restore on show.
  NSSize oldOverlaySize_;
  // A copy of the sheet's |autoresizesSubviews| flag to restore on show.
  BOOL oldResizesSubviews_;
}

- (id)initWithCertificate:(net::X509Certificate*)certificate;

- (void)displayForWebContents:(content::WebContents*)webContents;

- (NSWindow*)overlayWindow;

@end

#endif // CHROME_BROWSER_UI_COCOA_CERTIFICATE_VIEWER_MAC_H_
