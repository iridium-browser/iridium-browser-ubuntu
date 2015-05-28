// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/gfx/geometry/rect.h"


// Helpers --------------------------------------------------------------------

namespace chrome {

namespace {

// Handles destroying a TestBrowserWindow when the Browser it is attached to is
// destroyed.
class TestBrowserWindowOwner : public chrome::BrowserListObserver {
 public:
  explicit TestBrowserWindowOwner(TestBrowserWindow* window) : window_(window) {
    BrowserList::AddObserver(this);
  }
  ~TestBrowserWindowOwner() override { BrowserList::RemoveObserver(this); }

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    if (browser->window() == window_.get())
      delete this;
  }

  scoped_ptr<TestBrowserWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowOwner);
};

}  // namespace

Browser* CreateBrowserWithTestWindowForParams(Browser::CreateParams* params) {
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  params->window = window;
  return new Browser(*params);
}

}  // namespace chrome


// TestBrowserWindow::TestLocationBar -----------------------------------------

GURL TestBrowserWindow::TestLocationBar::GetDestinationURL() const {
  return GURL();
}

WindowOpenDisposition
    TestBrowserWindow::TestLocationBar::GetWindowOpenDisposition() const {
  return CURRENT_TAB;
}

ui::PageTransition
    TestBrowserWindow::TestLocationBar::GetPageTransition() const {
  return ui::PAGE_TRANSITION_LINK;
}

bool TestBrowserWindow::TestLocationBar::ShowPageActionPopup(
    const extensions::Extension* extension, bool grant_active_tab) {
  return false;
}

const OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() const {
  return NULL;
}

OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() {
  return NULL;
}

LocationBarTesting*
    TestBrowserWindow::TestLocationBar::GetLocationBarForTesting() {
  return NULL;
}


// TestBrowserWindow ----------------------------------------------------------

TestBrowserWindow::TestBrowserWindow() {}

TestBrowserWindow::~TestBrowserWindow() {}

bool TestBrowserWindow::IsActive() const {
  return false;
}

bool TestBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

gfx::NativeWindow TestBrowserWindow::GetNativeWindow() const {
  return NULL;
}

StatusBubble* TestBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState TestBrowserWindow::GetRestoredState() const {
  return ui::SHOW_STATE_DEFAULT;
}

gfx::Rect TestBrowserWindow::GetBounds() const {
  return gfx::Rect();
}

bool TestBrowserWindow::IsMaximized() const {
  return false;
}

bool TestBrowserWindow::IsMinimized() const {
  return false;
}

bool TestBrowserWindow::ShouldHideUIForFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

bool TestBrowserWindow::SupportsFullscreenWithToolbar() const {
  return false;
}

void TestBrowserWindow::UpdateFullscreenWithToolbar(bool with_toolbar) {
}

bool TestBrowserWindow::IsFullscreenWithToolbar() const {
  return false;
}

#if defined(OS_WIN)
bool TestBrowserWindow::IsInMetroSnapMode() const {
  return false;
}
#endif

LocationBar* TestBrowserWindow::GetLocationBar() const {
  return const_cast<TestLocationBar*>(&location_bar_);
}

bool TestBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

bool TestBrowserWindow::IsTabStripEditable() const {
  return false;
}

bool TestBrowserWindow::IsToolbarVisible() const {
  return false;
}

gfx::Rect TestBrowserWindow::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool TestBrowserWindow::ShowSessionCrashedBubble() {
  return false;
}

bool TestBrowserWindow::IsProfileResetBubbleSupported() const {
  return false;
}

GlobalErrorBubbleViewBase* TestBrowserWindow::ShowProfileResetBubble(
    const base::WeakPtr<ProfileResetGlobalError>& global_error) {
  return nullptr;
}

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return &download_shelf_;
}

WindowOpenDisposition TestBrowserWindow::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return NEW_POPUP;
}

FindBar* TestBrowserWindow::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
    TestBrowserWindow::GetWebContentsModalDialogHost() {
  return NULL;
}

int
TestBrowserWindow::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  return 0;
}

void TestBrowserWindow::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {}

ExclusiveAccessContext* TestBrowserWindow::GetExclusiveAccessContext() {
  return nullptr;
}
