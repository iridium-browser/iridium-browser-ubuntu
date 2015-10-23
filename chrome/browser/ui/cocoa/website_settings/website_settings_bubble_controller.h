// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

class WebsiteSettingsUIBridge;

namespace content {
class WebContents;
}

// This NSWindowController subclass manages the InfoBubbleWindow and view that
// are displayed when the user clicks the favicon or security lock icon.
//
// TODO(palmer, sashab): Normalize all WebsiteSettings*, SiteSettings*,
// PageInfo*, et c. to OriginInfo*.
@interface WebsiteSettingsBubbleController : BaseBubbleController {
 @private
  content::WebContents* webContents_;

  base::scoped_nsobject<NSView> contentView_;
  base::scoped_nsobject<NSSegmentedControl> segmentedControl_;
  base::scoped_nsobject<NSTabView> tabView_;

  // Displays the web site identity.
  NSTextField* identityField_;

  // Display the identity status (e.g. verified, not verified).
  NSTextField* identityStatusField_;

  // The main content view for the Permissions tab.
  NSView* permissionsTabContentView_;

  // The main content view for the Connection tab.
  NSView* connectionTabContentView_;

  // Container for cookies info on the Permissions tab.
  NSView* cookiesView_;

  // The link button for showing cookies and site data info.
  NSButton* cookiesButton_;

  // The link button for showing site settings.
  NSButton* siteSettingsButton_;

  // The link button for showing certificate information.
  NSButton* certificateInfoButton_;

  // The link button for revoking certificate decisions.
  NSButton* resetDecisionsButton_;

  // The ID of the server certificate from the identity info. This should
  // always be non-zero on a secure connection, and 0 otherwise.
  int certificateId_;

  // Container for permission info on the Permissions tab.
  NSView* permissionsView_;

  NSImageView* identityStatusIcon_;
  NSTextField* identityStatusDescriptionField_;
  NSView* separatorAfterIdentity_;

  NSImageView* connectionStatusIcon_;
  NSTextField* connectionStatusDescriptionField_;
  NSView* separatorAfterConnection_;

  // The link button to launch the Help Center article explaining the
  // connection info.
  NSButton* helpButton_;

  // The UI translates user actions to specific events and forwards them to the
  // |presenter_|. The |presenter_| handles these events and updates the UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // Bridge which implements the WebsiteSettingsUI interface and forwards
  // methods on to this class.
  scoped_ptr<WebsiteSettingsUIBridge> bridge_;
}

// Designated initializer. The controller will release itself when the bubble
// is closed. |parentWindow| cannot be nil. |webContents| may be nil for
// testing purposes.
- (id)initWithParentWindow:(NSWindow*)parentWindow
   websiteSettingsUIBridge:(WebsiteSettingsUIBridge*)bridge
               webContents:(content::WebContents*)webContents
            isInternalPage:(BOOL)isInternalPage;

// Return the default width of the window. It may be wider to fit the content.
// This may be overriden by a subclass for testing purposes.
- (CGFloat)defaultWindowWidth;

@end

// Provides a bridge between the WebSettingsUI C++ interface and the Cocoa
// implementation in WebsiteSettingsBubbleController.
class WebsiteSettingsUIBridge : public WebsiteSettingsUI {
 public:
  WebsiteSettingsUIBridge();
  ~WebsiteSettingsUIBridge() override;

  // Creates a |WebsiteSettingsBubbleController| and displays the UI. |parent|
  // is the currently active window. |profile| points to the currently active
  // profile. |web_contents| points to the WebContents that wraps the currently
  // active tab. |url| is the GURL of the currently active tab. |ssl| is the
  // |SSLStatus| of the connection to the website in the currently active tab.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   content::WebContents* web_contents,
                   const GURL& url,
                   const content::SSLStatus& ssl);

  void set_bubble_controller(
      WebsiteSettingsBubbleController* bubble_controller);

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetSelectedTab(TabId tab_id) override;

 private:
  // The Cocoa controller for the bubble UI.
  WebsiteSettingsBubbleController* bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsUIBridge);
};
