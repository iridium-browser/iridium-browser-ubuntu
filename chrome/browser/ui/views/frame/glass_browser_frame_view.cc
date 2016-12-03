// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_browser_frame_view.h"

#include <dwmapi.h>
#include <utility>

#include "base/win/windows_version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle_win.h"
#include "ui/base/theme_provider.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/win/hwnd_util.h"
#include "ui/views/window/client_view.h"

HICON GlassBrowserFrameView::throbber_icons_[
    GlassBrowserFrameView::kThrobberIconCount];

namespace {
// Thickness of the frame edge between the non-client area and the web content.
const int kClientBorderThickness = 3;
// Besides the frame border, there's empty space atop the window in restored
// mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 11;
// At the window corners the resize area is not actually bigger, but the 16
// pixels at the end of the top and bottom edges trigger diagonal resizing.
const int kResizeCornerWidth = 16;
// How far the profile switcher button is from the left of the minimize button.
const int kProfileSwitcherButtonOffset = 5;
// The content edge images have a shadow built into them.
const int kContentEdgeShadowThickness = 2;
// In restored mode, the New Tab button isn't at the same height as the caption
// buttons, but the space will look cluttered if it actually slides under them,
// so we stop it when the gap between the two is down to 5 px.
const int kNewTabCaptionRestoredSpacing = 5;
// In maximized mode, where the New Tab button and the caption buttons are at
// similar vertical coordinates, we need to reserve a larger, 16 px gap to avoid
// looking too cluttered.
const int kNewTabCaptionMaximizedSpacing = 16;
// Height of the profile switcher button. Same as the height of the Windows 7/8
// caption buttons.
// TODO(bsep): Windows 10 caption buttons look very different and we would like
// the profile switcher button to match on that platform.
const int kProfileSwitcherButtonHeight = 20;
// There is a small one-pixel strip right above the caption buttons in which the
// resize border "peeks" through.
const int kCaptionButtonTopInset = 1;

// Converts the |image| to a Windows icon and returns the corresponding HICON
// handle. |image| is resized to desired |width| and |height| if needed.
base::win::ScopedHICON CreateHICONFromSkBitmapSizedTo(
    const gfx::ImageSkia& image,
    int width,
    int height) {
  return IconUtil::CreateHICONFromSkBitmap(
      width == image.width() && height == image.height()
          ? *image.bitmap()
          : skia::ImageOperations::Resize(*image.bitmap(),
                                          skia::ImageOperations::RESIZE_BEST,
                                          width, height));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, public:

GlassBrowserFrameView::GlassBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      profile_switcher_(this),
      throbber_running_(false),
      throbber_frame_(0) {
  if (browser_view->ShouldShowWindowIcon())
    InitThrobberIcons();
}

GlassBrowserFrameView::~GlassBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // In maximized RTL windows, don't let the tabstrip overlap the caption area,
  // or the alpha-blending it does will make things like the profile switcher
  // button look glitchy.
  const int offset = (ui::MaterialDesignController::IsModeMaterial() ||
                      !CaptionButtonsOnLeadingEdge() || !frame()->IsMaximized())
                         ? GetLayoutInsets(AVATAR_ICON).right()
                         : 0;
  const int x = incognito_bounds_.right() + offset;
  int end_x = width() - ClientBorderThickness(false);
  if (!CaptionButtonsOnLeadingEdge()) {
    end_x = std::min(frame()->GetMinimizeButtonOffset(), end_x) -
        (frame()->IsMaximized() ?
            kNewTabCaptionMaximizedSpacing : kNewTabCaptionRestoredSpacing);

    // The profile switcher button is optionally displayed to the left of the
    // minimize button.
    if (profile_switcher_.view()) {
      const int old_end_x = end_x;
      end_x -= profile_switcher_.view()->width() + kProfileSwitcherButtonOffset;

      // In non-maximized mode, allow the new tab button to slide completely
      // under the profile switcher button.
      if (!frame()->IsMaximized()) {
        end_x = std::min(end_x + GetLayoutSize(NEW_TAB_BUTTON).width() +
                             kNewTabCaptionRestoredSpacing,
                         old_end_x);
      }
    }
  }
  return gfx::Rect(x, TopAreaHeight(false), std::max(0, end_x - x),
                   tabstrip->GetPreferredSize().height());
}

int GlassBrowserFrameView::GetTopInset(bool restored) const {
  return GetClientAreaInsets(restored).top();
}

int GlassBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
}

void GlassBrowserFrameView::UpdateThrobber(bool running) {
  if (throbber_running_) {
    if (running) {
      DisplayNextThrobberFrame();
    } else {
      StopThrobber();
    }
  } else if (running) {
    StartThrobber();
  }
}

gfx::Size GlassBrowserFrameView::GetMinimumSize() const {
  gfx::Size min_size(browser_view()->GetMinimumSize());

  // Account for the client area insets.
  gfx::Insets insets = GetClientAreaInsets(false);
  min_size.Enlarge(insets.width(), insets.height());
  // Client area insets do not include the shadow thickness.
  min_size.Enlarge(2 * kContentEdgeShadowThickness, 0);

  // Ensure that the minimum width is enough to hold a tab strip with minimum
  // width at its usual insets.
  if (browser_view()->IsTabStripVisible()) {
    TabStrip* tabstrip = browser_view()->tabstrip();
    int min_tabstrip_width = tabstrip->GetMinimumSize().width();
    int min_tabstrip_area_width =
        width() - GetBoundsForTabStrip(tabstrip).width() + min_tabstrip_width;
    min_size.set_width(std::max(min_tabstrip_area_width, min_size.width()));
  }

  return min_size;
}

views::View* GlassBrowserFrameView::GetProfileSwitcherView() const {
  return profile_switcher_.view();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect GlassBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect GlassBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  HWND hwnd = views::HWNDForWidget(frame());
  if (!browser_view()->IsTabStripVisible() && hwnd) {
    // If we don't have a tabstrip, we're either a popup or an app window, in
    // which case we have a standard size non-client area and can just use
    // AdjustWindowRectEx to obtain it. We check for a non-null window handle in
    // case this gets called before the window is actually created.
    RECT rect = client_bounds.ToRECT();
    AdjustWindowRectEx(&rect, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                       GetWindowLong(hwnd, GWL_EXSTYLE));
    return gfx::Rect(rect);
  }

  gfx::Insets insets = GetClientAreaInsets(false);
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - insets.top()),
                   client_bounds.width() + insets.width(),
                   client_bounds.height() + insets.height());
}

int GlassBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  // For app windows and popups without a custom titlebar we haven't customized
  // the frame at all so Windows can figure it out.
  if (!frame()->CustomDrawSystemTitlebar() &&
      !browser_view()->IsBrowserTypeNormal())
    return HTNOWHERE;

  // If the point isn't within our bounds, then it's in the native portion of
  // the frame so again Windows can figure it out.
  if (!bounds().Contains(point))
    return HTNOWHERE;

  // See if the point is within the incognito icon or the profile switcher menu.
  if ((profile_indicator_icon() &&
       profile_indicator_icon()->GetMirroredBounds().Contains(point)) ||
      (profile_switcher_.view() &&
       profile_switcher_.view()->GetMirroredBounds().Contains(point)))
    return HTCLIENT;

  int frame_component = frame()->client_view()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  We still have to check the tabstrip
  // first so that clicks in a tab don't get treated as sysmenu clicks.
  int client_border_thickness = ClientBorderThickness(false);
  gfx::Rect sys_menu_region(
      client_border_thickness,
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME),
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSMICON),
      display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSMICON));
  if (sys_menu_region.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // On Windows 8+, the caption buttons are almost butted up to the top right
  // corner of the window. This code ensures the mouse isn't set to a size
  // cursor while hovering over the caption buttons, thus giving the incorrect
  // impression that the user can resize the window.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    RECT button_bounds = {0};
    if (SUCCEEDED(DwmGetWindowAttribute(views::HWNDForWidget(frame()),
                                        DWMWA_CAPTION_BUTTON_BOUNDS,
                                        &button_bounds,
                                        sizeof(button_bounds)))) {
      gfx::Rect buttons = gfx::ConvertRectToDIP(display::win::GetDPIScale(),
                                                gfx::Rect(button_bounds));
      // The sizing region at the window edge above the caption buttons is
      // 1 px regardless of scale factor. If we inset by 1 before converting
      // to DIPs, the precision loss might eliminate this region entirely. The
      // best we can do is to inset after conversion. This guarantees we'll
      // show the resize cursor when resizing is possible. The cost of which
      // is also maybe showing it over the portion of the DIP that isn't the
      // outermost pixel.
      buttons.Inset(0, kCaptionButtonTopInset, 0, 0);
      if (buttons.Contains(point))
        return HTNOWHERE;
    }
  }

  int top_border_thickness = FrameTopBorderThickness(false);
  // We want the resize corner behavior to apply to the kResizeCornerWidth
  // pixels at each end of the top and bottom edges.  Because |point|'s x
  // coordinate is based on the DWM-inset portion of the window (so, it's 0 at
  // the first pixel inside the left DWM margin), we need to subtract the DWM
  // margin thickness, which we calculate as the total frame border thickness
  // minus the nonclient border thickness.
  const int dwm_margin = FrameBorderThickness() - client_border_thickness;
  int window_component = GetHTComponentForFrame(
      point, top_border_thickness, client_border_thickness,
      top_border_thickness, kResizeCornerWidth - dwm_margin,
      frame()->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, views::View overrides:

void GlassBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  if (frame()->CustomDrawSystemTitlebar())
    PaintTitlebar(canvas);
  if (!browser_view()->IsTabStripVisible())
    return;
  if (IsToolbarVisible())
    PaintToolbarBackground(canvas);
  if (ClientBorderThickness(false) > 0)
    PaintClientEdge(canvas);
}

void GlassBrowserFrameView::Layout() {
  if (browser_view()->IsRegularOrGuestSession())
    LayoutProfileSwitcher();
  LayoutIncognitoIcon();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, protected:

// BrowserNonClientFrameView:
void GlassBrowserFrameView::UpdateProfileIcons() {
  if (browser_view()->IsRegularOrGuestSession())
    profile_switcher_.Update(AvatarButtonStyle::NATIVE);
  else
    UpdateProfileIndicatorIcon();
}

///////////////////////////////////////////////////////////////////////////////
// GlassBrowserFrameView, private:

// views::NonClientFrameView:
bool GlassBrowserFrameView::DoesIntersectRect(const views::View* target,
                                              const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  bool hit_incognito_icon =
      profile_indicator_icon() &&
      profile_indicator_icon()->GetMirroredBounds().Intersects(rect);
  bool hit_profile_switcher_button =
      profile_switcher_.view() &&
      profile_switcher_.view()->GetMirroredBounds().Intersects(rect);
  return hit_incognito_icon || hit_profile_switcher_button ||
         !frame()->client_view()->bounds().Intersects(rect);
}

int GlassBrowserFrameView::ClientBorderThickness(bool restored) const {
  // The frame ends abruptly at the 1 pixel window border drawn by Windows 10.
  if (!browser_view()->HasClientEdge())
    return 0;

  if ((frame()->IsMaximized() || frame()->IsFullscreen()) && !restored)
    return 0;

  return kClientBorderThickness;
}

int GlassBrowserFrameView::FrameBorderThickness() const {
  return (frame()->IsMaximized() || frame()->IsFullscreen()) ?
      0 : display::win::ScreenWin::GetSystemMetricsInDIP(SM_CXSIZEFRAME);
}

int GlassBrowserFrameView::FrameTopBorderThickness(bool restored) const {
  // Distinct from FrameBorderThickness() because Windows gives maximized
  // windows an offscreen CYSIZEFRAME-thick region around the edges. The
  // left/right/bottom edges don't worry about this because we cancel them out
  // in BrowserDesktopWindowTreeHostWin::GetClientAreaInsets() so the offscreen
  // area is non-client as far as Windows is concerned. However we can't do this
  // with the top inset because otherwise Windows will give us a standard
  // titlebar. Thus we must compensate here to avoid having UI elements drift
  // off the top of the screen.
  if (frame()->IsFullscreen() && !restored)
    return 0;
  // Mouse and touch locations are floored but GetSystemMetricsInDIP is rounded,
  // so we need to floor instead or else the difference will cause the hittest
  // to fail when it ought to succeed.
  // TODO(robliao): Resolve this GetSystemMetrics call.
  return std::floor(GetSystemMetrics(SM_CYSIZEFRAME) /
                    display::win::GetDPIScale());
}

int GlassBrowserFrameView::TopAreaHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored)
    return 0;

  const int top = FrameTopBorderThickness(restored);
  // The tab top inset is equal to the height of any shadow region above the
  // tabs, plus a 1 px top stroke.  In maximized mode, we want to push the
  // shadow region off the top of the screen but leave the top stroke.
  // Annoyingly, the pre-MD layout uses different heights for the hit-test
  // exclusion region (which we want here, since we're trying to size the border
  // so that the region above the tab's hit-test zone matches) versus the shadow
  // thickness.
  const int exclusion = GetLayoutConstant(TAB_TOP_EXCLUSION_HEIGHT);
  return (frame()->IsMaximized() && !restored) ?
      (top - GetLayoutInsets(TAB).top() + 1) :
      (top + kNonClientRestoredExtraThickness - exclusion);
}

int GlassBrowserFrameView::TitlebarHeight(bool restored) const {
  if (frame()->IsFullscreen() && !restored)
    return 0;
  return display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYCAPTION) +
         display::win::ScreenWin::GetSystemMetricsInDIP(SM_CYSIZEFRAME);
}

int GlassBrowserFrameView::WindowTopY() const {
  return frame()->IsMaximized() ? FrameTopBorderThickness(false) : 1;
}

bool GlassBrowserFrameView::IsToolbarVisible() const {
  return browser_view()->IsToolbarVisible() &&
      !browser_view()->toolbar()->GetPreferredSize().IsEmpty();
}

bool GlassBrowserFrameView::CaptionButtonsOnLeadingEdge() const {
  // Because we don't set WS_EX_LAYOUTRTL (which would conflict with Chrome's
  // own RTL layout logic), Windows always draws the caption buttons on the
  // right, even when we want to be RTL. See crbug.com/560619.
  return base::i18n::IsRTL();
}

void GlassBrowserFrameView::PaintTitlebar(gfx::Canvas* canvas) const {
  SkColor frame_color = 0xFFCCCCCC;
  gfx::Rect tabstrip_bounds = GetBoundsForTabStrip(browser_view()->tabstrip());
  const int y = WindowTopY();
  canvas->FillRect(gfx::Rect(0, y, width(), tabstrip_bounds.bottom() - y),
                   frame_color);
  // The 1 pixel line at the top is drawn by Windows when we leave that section
  // of the window blank because we have called DwmExtendFrameIntoClientArea()
  // inside BrowserDesktopWindowTreeHostWin::UpdateDWMFrame().
}

void GlassBrowserFrameView::PaintToolbarBackground(gfx::Canvas* canvas) const {
  gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  if (toolbar_bounds.IsEmpty())
    return;
  gfx::Point toolbar_origin(toolbar_bounds.origin());
  ConvertPointToTarget(browser_view(), this, &toolbar_origin);
  toolbar_bounds.set_origin(toolbar_origin);

  const ui::ThemeProvider* tp = GetThemeProvider();
  const gfx::ImageSkia* const bg = tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR);
  int x = toolbar_bounds.x();
  const int y = toolbar_bounds.y();
  const int bg_y = GetTopInset(false) + Tab::GetYInsetForActiveTabBackground();
  int w = toolbar_bounds.width();
  const int h = toolbar_bounds.height();
  const SkColor separator_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BOTTOM_SEPARATOR);
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Background.  The top stroke is drawn above the toolbar bounds, so unlike
    // in the non-Material Design code below, we don't need to exclude any
    // region from having the background image drawn over it.
    if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
      canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), y - bg_y, x, y,
                           w, h);
    } else {
      canvas->FillRect(toolbar_bounds,
                       tp->GetColor(ThemeProperties::COLOR_TOOLBAR));
    }

    // Material Design has no corners to mask out.

    // Top stroke.  For Material Design, the toolbar has no side strokes.
    gfx::Rect separator_rect(x, y, w, 0);
    gfx::ScopedCanvas scoped_canvas(canvas);
    gfx::Rect tabstrip_bounds(GetBoundsForTabStrip(browser_view()->tabstrip()));
    tabstrip_bounds.set_x(GetMirroredXForRect(tabstrip_bounds));
    canvas->sk_canvas()->clipRect(gfx::RectToSkRect(tabstrip_bounds),
                                  SkRegion::kDifference_Op);
    separator_rect.set_y(tabstrip_bounds.bottom());
    BrowserView::Paint1pxHorizontalLine(canvas, GetToolbarTopSeparatorColor(),
                                        separator_rect, true);

    // Toolbar/content separator.
    BrowserView::Paint1pxHorizontalLine(canvas, separator_color, toolbar_bounds,
                                        true);
  } else {
    // Background.  The top stroke is drawn using the IDR_CONTENT_TOP_XXX
    // images, which overlay the toolbar.  The top 2 px of these images is the
    // actual top stroke + shadow, and is partly transparent, so the toolbar
    // background shouldn't be drawn over it.
    const int bg_dest_y = y + kContentEdgeShadowThickness;
    canvas->TileImageInt(*bg, x + GetThemeBackgroundXInset(), bg_dest_y - bg_y,
                         x, bg_dest_y, w, h - kContentEdgeShadowThickness);

    // On Windows 10+ where we don't draw our own window border but rather go
    // right to the system border, the toolbar has no corners or side strokes.
    if (base::win::GetVersion() < base::win::VERSION_WIN10) {
      // Mask out the corners.
      const gfx::ImageSkia* const left =
          tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER);
      const int img_w = left->width();
      x -= kContentEdgeShadowThickness;
      SkPaint paint;
      paint.setXfermodeMode(SkXfermode::kDstIn_Mode);
      canvas->DrawImageInt(
          *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_LEFT_CORNER_MASK), 0, 0, img_w,
          h, x, y, img_w, h, false, paint);
      const int right_x =
          toolbar_bounds.right() + kContentEdgeShadowThickness - img_w;
      canvas->DrawImageInt(
          *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER_MASK), 0, 0,
          img_w, h, right_x, y, img_w, h, false, paint);

      // Corner and side strokes.
      canvas->DrawImageInt(*left, 0, 0, img_w, h, x, y, img_w, h, false);
      canvas->DrawImageInt(
          *tp->GetImageSkiaNamed(IDR_CONTENT_TOP_RIGHT_CORNER), 0, 0, img_w, h,
          right_x, y, img_w, h, false);

      x += img_w;
      w = right_x - x;
    }

    // Top stroke.
    canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_TOP_CENTER), x, y,
                         w, kContentEdgeShadowThickness);

    // Toolbar/content separator.
    toolbar_bounds.Inset(kClientEdgeThickness, h - kClientEdgeThickness,
                         kClientEdgeThickness, 0);
    canvas->FillRect(toolbar_bounds, separator_color);
  }
}

void GlassBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) const {
  // Pre-Material Design, the client edge images start below the toolbar.  In MD
  // the client edge images start at the top of the toolbar.
  gfx::Rect client_bounds = CalculateClientAreaBounds();
  const int x = client_bounds.x();
  const bool md = ui::MaterialDesignController::IsModeMaterial();
  const gfx::Rect toolbar_bounds(browser_view()->GetToolbarBounds());
  const int y =
      client_bounds.y() + (md ? toolbar_bounds.y() : toolbar_bounds.bottom());
  const int right = client_bounds.right();
  const int bottom = std::max(y, height() - ClientBorderThickness(false));

  // Draw the client edge images.  For non-MD, we fill the toolbar color
  // underneath these images so they will lighten/darken it appropriately to
  // create a "3D shaded" effect.  For MD, where we want a flatter appearance,
  // we do the filling afterwards so the user sees the unmodified toolbar color.
  const ui::ThemeProvider* tp = GetThemeProvider();
  const SkColor toolbar_color = tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  if (!md)
    FillClientEdgeRects(x, y, right, bottom, toolbar_color, canvas);
  if (!md || (base::win::GetVersion() < base::win::VERSION_WIN10)) {
    const gfx::ImageSkia* const right_image =
        tp->GetImageSkiaNamed(IDR_CONTENT_RIGHT_SIDE);
    const int img_w = right_image->width();
    const int height = bottom - y;
    canvas->TileImageInt(*right_image, right, y, img_w, height);
    canvas->DrawImageInt(
        *tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER), right, bottom);
    const gfx::ImageSkia* const bottom_image =
        tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_CENTER);
    canvas->TileImageInt(*bottom_image, x, bottom, client_bounds.width(),
                         bottom_image->height());
    canvas->DrawImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER),
                         x - img_w, bottom);
    canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_CONTENT_LEFT_SIDE),
                         x - img_w, y, img_w, height);
  }
  if (md)
    FillClientEdgeRects(x, y, right, bottom, toolbar_color, canvas);
}

void GlassBrowserFrameView::FillClientEdgeRects(int x,
                                                int y,
                                                int right,
                                                int bottom,
                                                SkColor color,
                                                gfx::Canvas* canvas) const {
  gfx::Rect side(x - kClientEdgeThickness, y, kClientEdgeThickness,
                 bottom + kClientEdgeThickness - y);
  canvas->FillRect(side, color);
  canvas->FillRect(gfx::Rect(x, bottom, right - x, kClientEdgeThickness),
                   color);
  side.set_x(right);
  canvas->FillRect(side, color);
}

void GlassBrowserFrameView::LayoutProfileSwitcher() {
  DCHECK(browser_view()->IsRegularOrGuestSession());
  if (!profile_switcher_.view())
    return;

  gfx::Size label_size = profile_switcher_.view()->GetPreferredSize();

  int button_x = frame()->GetMinimizeButtonOffset() -
                 kProfileSwitcherButtonOffset - label_size.width();
  if (CaptionButtonsOnLeadingEdge())
    button_x = width() - frame()->GetMinimizeButtonOffset() +
               kProfileSwitcherButtonOffset;

  int button_y = WindowTopY();
  if (frame()->IsMaximized()) {
    // In maximized mode the caption buttons appear only 19 pixels high, but
    // their contents are aligned as if they were 20 pixels high and extended
    // 1 pixel off the top of the screen. We position the profile switcher
    // button the same way to match.
    button_y -= 1;
  }
  profile_switcher_.view()->SetBounds(button_x, button_y, label_size.width(),
                                      kProfileSwitcherButtonHeight);
}

void GlassBrowserFrameView::LayoutIncognitoIcon() {
  const bool md = ui::MaterialDesignController::IsModeMaterial();
  const gfx::Insets insets(GetLayoutInsets(AVATAR_ICON));
  const gfx::Size size(GetIncognitoAvatarIcon().size());
  int x = ClientBorderThickness(false);
  // In RTL, the icon needs to start after the caption buttons.
  if (CaptionButtonsOnLeadingEdge()) {
    x = width() - frame()->GetMinimizeButtonOffset() +
        (profile_switcher_.view() ? (profile_switcher_.view()->width() +
                                     kProfileSwitcherButtonOffset)
                                  : 0);
  } else if (!md && !profile_indicator_icon() && IsToolbarVisible() &&
             (base::win::GetVersion() < base::win::VERSION_WIN10)) {
    // In non-MD before Win 10, the toolbar has a rounded corner that we don't
    // want the tabstrip to overlap.
    x += browser_view()->GetToolbarBounds().x() - kContentEdgeShadowThickness +
        GetThemeProvider()->GetImageSkiaNamed(
            IDR_CONTENT_TOP_LEFT_CORNER)->width();
  }
  const int bottom = GetTopInset(false) + browser_view()->GetTabStripHeight() -
      insets.bottom();
  const int y = (md || !frame()->IsMaximized())
                    ? (bottom - size.height())
                    : FrameTopBorderThickness(false);
  incognito_bounds_.SetRect(x + (profile_indicator_icon() ? insets.left() : 0),
                            y, profile_indicator_icon() ? size.width() : 0,
                            bottom - y);
  if (profile_indicator_icon())
    profile_indicator_icon()->SetBoundsRect(incognito_bounds_);
}

void GlassBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds();
}

gfx::Insets GlassBrowserFrameView::GetClientAreaInsets(bool restored) const {
  if (!browser_view()->IsTabStripVisible()) {
    const int top =
        frame()->CustomDrawSystemTitlebar() ? TitlebarHeight(restored) : 0;
    return gfx::Insets(top, 0, 0, 0);
  }

  const int top_height = TopAreaHeight(restored);
  const int border_thickness = ClientBorderThickness(restored);
  return gfx::Insets(top_height,
                     border_thickness,
                     border_thickness,
                     border_thickness);
}

gfx::Rect GlassBrowserFrameView::CalculateClientAreaBounds() const {
  gfx::Rect bounds(GetLocalBounds());
  bounds.Inset(GetClientAreaInsets(false));
  return bounds;
}

void GlassBrowserFrameView::StartThrobber() {
  if (!throbber_running_) {
    throbber_running_ = true;
    throbber_frame_ = 0;
    InitThrobberIcons();
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
  }
}

void GlassBrowserFrameView::StopThrobber() {
  if (throbber_running_) {
    throbber_running_ = false;

    base::win::ScopedHICON previous_small_icon;
    base::win::ScopedHICON previous_big_icon;
    HICON small_icon = nullptr;
    HICON big_icon = nullptr;

    // Check if hosted BrowserView has a window icon to use.
    if (browser_view()->ShouldShowWindowIcon()) {
      gfx::ImageSkia icon = browser_view()->GetWindowIcon();
      if (!icon.isNull()) {
        // Keep previous icons alive as long as they are referenced by the HWND.
        previous_small_icon = std::move(small_window_icon_);
        previous_big_icon = std::move(big_window_icon_);

        // Take responsibility for eventually destroying the created icons.
        small_window_icon_ = CreateHICONFromSkBitmapSizedTo(
            icon, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
        big_window_icon_ = CreateHICONFromSkBitmapSizedTo(
            icon, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));

        small_icon = small_window_icon_.get();
        big_icon = big_window_icon_.get();
      }
    }

    // Fallback to class icon.
    if (!small_icon) {
      small_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICONSM));
    }
    if (!big_icon) {
      big_icon = reinterpret_cast<HICON>(
          GetClassLongPtr(views::HWNDForWidget(frame()), GCLP_HICON));
    }

    // This will reset the icon which we set in the throbber code.
    // WM_SETICON with null icon restores the icon for title bar but not
    // for taskbar. See http://crbug.com/29996
    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_SMALL),
                reinterpret_cast<LPARAM>(small_icon));

    SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
                static_cast<WPARAM>(ICON_BIG),
                reinterpret_cast<LPARAM>(big_icon));
  }
}

void GlassBrowserFrameView::DisplayNextThrobberFrame() {
  throbber_frame_ = (throbber_frame_ + 1) % kThrobberIconCount;
  SendMessage(views::HWNDForWidget(frame()), WM_SETICON,
              static_cast<WPARAM>(ICON_SMALL),
              reinterpret_cast<LPARAM>(throbber_icons_[throbber_frame_]));
}

// static
void GlassBrowserFrameView::InitThrobberIcons() {
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < kThrobberIconCount; ++i) {
      throbber_icons_[i] =
          ui::LoadThemeIconFromResourcesDataDLL(IDI_THROBBER_01 + i);
      DCHECK(throbber_icons_[i]);
    }
    initialized = true;
  }
}
