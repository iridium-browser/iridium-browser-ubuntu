// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_member.h"
#include "components/zoom/zoom_event_manager_observer.h"

@class AutocompleteTextField;
class CommandUpdater;
class ContentSettingDecoration;
class EVBubbleDecoration;
class KeywordHintDecoration;
class LocationBarDecoration;
class LocationIconDecoration;
class ManagePasswordsDecoration;
class PageActionDecoration;
class Profile;
class SaveCreditCardDecoration;
class SelectedKeywordDecoration;
class StarDecoration;
class TranslateDecoration;
class ZoomDecoration;
class ZoomDecorationTest;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an OmniboxViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public LocationBar,
                           public LocationBarTesting,
                           public ChromeOmniboxEditController,
                           public zoom::ZoomEventManagerObserver {
 public:
  LocationBarViewMac(AutocompleteTextField* field,
                     CommandUpdater* command_updater,
                     Profile* profile,
                     Browser* browser);
  ~LocationBarViewMac() override;

  // Overridden from LocationBar:
  void ShowFirstRunBubble() override;
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  void AcceptInput() override;
  void FocusLocation(bool select_all) override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override;
  void UpdatePageActions() override;
  void UpdateBookmarkStarVisibility() override;
  void UpdateLocationBarVisibility(bool visible, bool animate) override;
  bool ShowPageActionPopup(const extensions::Extension* extension,
                           bool grant_active_tab) override;
  void UpdateOpenPDFInReaderPrompt() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  const OmniboxView* GetOmniboxView() const override;
  OmniboxView* GetOmniboxView() override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // Overridden from LocationBarTesting:
  int PageActionCount() override;
  int PageActionVisibleCount() override;
  ExtensionAction* GetPageAction(size_t index) override;
  ExtensionAction* GetVisiblePageAction(size_t index) override;
  void TestPageActionPressed(size_t index) override;
  bool GetBookmarkStarVisibility() override;

  // Set/Get the editable state of the field.
  void SetEditable(bool editable);
  bool IsEditable();

  // Set the starred state of the bookmark star.
  void SetStarred(bool starred);

  // Set whether or not the translate icon is lit.
  void SetTranslateIconLit(bool on);

  // Happens when the zoom changes for the active tab. |can_show_bubble| is
  // false when the change in zoom for the active tab wasn't an explicit user
  // action (e.g. switching tabs, creating a new tab, creating a new browser).
  // Additionally, |can_show_bubble| will only be true when the bubble wouldn't
  // be obscured by other UI (app menu) or redundant (+/- from app menu).
  void ZoomChangedForActiveTab(bool can_show_bubble);

  // Checks if the bookmark star should be enabled or not.
  bool IsStarEnabled() const;

  // Get the point in window coordinates on the star for the bookmark bubble to
  // aim at. Only works if IsStarEnabled returns YES.
  NSPoint GetBookmarkBubblePoint() const;

  // Get the point in window coordinates in the save credit card icon for the
  //  save credit card bubble to aim at.
  NSPoint GetSaveCreditCardBubblePoint() const;

  // Get the point in window coordinates on the star for the Translate bubble to
  // aim at.
  NSPoint GetTranslateBubblePoint() const;

  // Get the point in window coordinates in the lock icon for the Manage
  // Passwords bubble to aim at.
  NSPoint GetManagePasswordsBubblePoint() const;

  // Get the point in window coordinates in the security icon at which the page
  // info bubble aims.
  NSPoint GetPageInfoBubblePoint() const;

  // When any image decorations change, call this to ensure everything is
  // redrawn and laid out if necessary.
  void OnDecorationsChanged();

  // Layout the various decorations which live in the field.
  void Layout();

  // Re-draws |decoration| if it's already being displayed.
  void RedrawDecoration(LocationBarDecoration* decoration);

  // Sets preview_enabled_ for the PageActionImageView associated with this
  // |page_action|. If |preview_enabled|, the location bar will display the
  // PageAction icon even if it has not been activated by the extension.
  // This is used by the ExtensionInstalledBubble to preview what the icon
  // will look like for the user upon installation of the extension.
  void SetPreviewEnabledPageAction(ExtensionAction* page_action,
                                   bool preview_enabled);

  // Retrieve the frame for the given |page_action|.
  NSRect GetPageActionFrame(ExtensionAction* page_action);

  // Return |page_action|'s info-bubble point in window coordinates.
  // This function should always be called with a visible page action.
  // If |page_action| is not a page action or not visible, NOTREACHED()
  // is called and this function returns |NSZeroPoint|.
  NSPoint GetPageActionBubblePoint(ExtensionAction* page_action);

  // Updates the controller, and, if |contents| is non-null, restores saved
  // state that the tab holds.
  void Update(const content::WebContents* contents);

  // Clears any location bar state stored for |contents|.
  void ResetTabState(content::WebContents* contents);

  // Set the location bar's icon to the correct image for the current URL.
  void UpdateLocationIcon();

  // Set the location bar's controls to visibly match the current theme.
  void UpdateColorsToMatchTheme();

  // Notify the location bar that it was added to the browser window. Provides
  // an update point for interface objects that need to set their appearance
  // based on the window's theme.
  void OnAddedToWindow();

  // Notify the location bar that the browser window theme has changed. Provides
  // an update point for interface objects that need to set their appearance
  // based on the window's theme.
  void OnThemeChanged();

  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override;
  void OnChanged() override;
  void ShowURL() override;
  ToolbarModel* GetToolbarModel() override;
  const ToolbarModel* GetToolbarModel() const override;
  content::WebContents* GetWebContents() override;

  bool ShouldShowEVBubble() const;
  NSImage* GetKeywordImage(const base::string16& keyword);

  AutocompleteTextField* GetAutocompleteTextField() { return field_; }

  // Returns true if the location bar is dark.
  bool IsLocationBarDark() const;

  ManagePasswordsDecoration* manage_passwords_decoration() {
    return manage_passwords_decoration_.get();
  }

  Browser* browser() const { return browser_; }

  // ZoomManagerObserver:
  // Updates the view for the zoom icon when default zoom levels change.
  void OnDefaultZoomLevelChanged() override;

  // Returns the decoration accessibility views for all of this
  // LocationBarViewMac's decorations. The returned NSViews may not have been
  // positioned yet.
  std::vector<NSView*> GetDecorationAccessibilityViews();

 private:
  friend ZoomDecorationTest;

  // Posts |notification| to the default notification center.
  void PostNotification(NSString* notification);

  // Return the decoration for |page_action|.
  PageActionDecoration* GetPageActionDecoration(ExtensionAction* page_action);

  // Clear the page-action decorations.
  void DeletePageActionDecorations();

  void OnEditBookmarksEnabledChanged();

  // Re-generate the page-action decorations from the profile's
  // extension service.
  void RefreshPageActionDecorations();

  // Whether the page actions represented by |page_action_decorations_| differ
  // in ordering or value from |page_actions|.
  bool PageActionsDiffer(
      const std::vector<ExtensionAction*>& page_actions) const;

  // Updates visibility of the content settings icons based on the current
  // tab contents state.
  bool RefreshContentSettingsDecorations();

  void ShowFirstRunBubbleInternal();

  // Updates the translate decoration in the omnibox with the current translate
  // state.
  void UpdateTranslateDecoration();

  // Updates the zoom decoration in the omnibox with the current zoom level.
  // Returns whether any updates were made.
  bool UpdateZoomDecoration(bool default_zoom_changed);

  // Returns pointers to all of the LocationBarDecorations owned by this
  // LocationBarViewMac. This helper function is used for positioning and
  // re-positioning accessibility views.
  std::vector<LocationBarDecoration*> GetDecorations();

  // Updates |decoration|'s accessibility view's position to match the computed
  // position the decoration will be drawn at.
  void UpdateAccessibilityViewPosition(LocationBarDecoration* decoration);

  std::unique_ptr<OmniboxViewMac> omnibox_view_;

  AutocompleteTextField* field_;  // owned by tab controller

  // A decoration that shows an icon to the left of the address.
  std::unique_ptr<LocationIconDecoration> location_icon_decoration_;

  // A decoration that shows the keyword-search bubble on the left.
  std::unique_ptr<SelectedKeywordDecoration> selected_keyword_decoration_;

  // A decoration that shows a lock icon and ev-cert label in a bubble
  // on the left.
  std::unique_ptr<EVBubbleDecoration> ev_bubble_decoration_;

  // Save credit card icon on the right side of the omnibox.
  std::unique_ptr<SaveCreditCardDecoration> save_credit_card_decoration_;

  // Bookmark star right of page actions.
  std::unique_ptr<StarDecoration> star_decoration_;

  // Translate icon at the end of the ominibox.
  std::unique_ptr<TranslateDecoration> translate_decoration_;

  // A zoom icon at the end of the omnibox, which shows at non-standard zoom
  // levels.
  std::unique_ptr<ZoomDecoration> zoom_decoration_;

  // Decorations for the installed Page Actions.
  ScopedVector<PageActionDecoration> page_action_decorations_;

  // The content blocked decorations.
  ScopedVector<ContentSettingDecoration> content_setting_decorations_;

  // Keyword hint decoration displayed on the right-hand side.
  std::unique_ptr<KeywordHintDecoration> keyword_hint_decoration_;

  // The right-hand-side button to manage passwords associated with a page.
  std::unique_ptr<ManagePasswordsDecoration> manage_passwords_decoration_;

  Browser* browser_;

  // Used to change the visibility of the star decoration.
  BooleanPrefMember edit_bookmarks_enabled_;

  // Indicates whether or not the location bar is currently visible.
  bool location_bar_visible_;

  // Used to schedule a task for the first run info bubble.
  base::WeakPtrFactory<LocationBarViewMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
