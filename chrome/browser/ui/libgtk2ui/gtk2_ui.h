// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal.h"
#include "chrome/browser/ui/libgtk2ui/gtk2_signal_registrar.h"
#include "chrome/browser/ui/libgtk2ui/libgtk2ui_export.h"
#include "chrome/browser/ui/libgtk2ui/owned_widget_gtk2.h"
#include "ui/events/linux/text_edit_key_bindings_delegate_auralinux.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/window/frame_buttons.h"

typedef struct _GdkColor GdkColor;
typedef struct _GtkBorder GtkBorder;
typedef struct _GtkStyle GtkStyle;
typedef struct _GtkWidget GtkWidget;
typedef struct _PangoFontDescription PangoFontDescription;

class SkBitmap;

namespace gfx {
class Image;
}

namespace libgtk2ui {
class Gtk2Border;
class Gtk2KeyBindingsHandler;
class Gtk2SignalRegistrar;
class GConfListener;

// Interface to GTK2 desktop features.
//
class Gtk2UI : public views::LinuxUI {
 public:
  Gtk2UI();
  ~Gtk2UI() override;

  typedef base::Callback<ui::NativeTheme*(aura::Window* window)>
      NativeThemeGetter;

  // Setters used by GConfListener:
  void SetWindowButtonOrdering(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);
  void SetNonClientMiddleClickAction(NonClientMiddleClickAction action);

  // Draws the GTK button border for state |gtk_state| onto a bitmap.
  SkBitmap DrawGtkButtonBorder(int gtk_state,
                               bool focused,
                               bool call_to_action,
                               int width,
                               int height) const;

  // ui::LinuxInputMethodContextFactory:
  scoped_ptr<ui::LinuxInputMethodContext> CreateInputMethodContext(
      ui::LinuxInputMethodContextDelegate* delegate,
      bool is_simple) const override;

  // gfx::LinuxFontDelegate:
  gfx::FontRenderParams GetDefaultFontRenderParams() const override;
  void GetDefaultFontDescription(
      std::string* family_out,
      int* size_pixels_out,
      int* style_out,
      gfx::FontRenderParams* params_out) const override;

  // ui::LinuxShellDialog:
  ui::SelectFileDialog* CreateSelectFileDialog(
      ui::SelectFileDialog::Listener* listener,
      ui::SelectFilePolicy* policy) const override;

  // ui::LinuxUI:
  void Initialize() override;
  gfx::Image GetThemeImageNamed(int id) const override;
  bool GetColor(int id, SkColor* color) const override;
  bool HasCustomImage(int id) const override;
  SkColor GetFocusRingColor() const override;
  SkColor GetThumbActiveColor() const override;
  SkColor GetThumbInactiveColor() const override;
  SkColor GetTrackColor() const override;
  SkColor GetActiveSelectionBgColor() const override;
  SkColor GetActiveSelectionFgColor() const override;
  SkColor GetInactiveSelectionBgColor() const override;
  SkColor GetInactiveSelectionFgColor() const override;
  double GetCursorBlinkInterval() const override;
  ui::NativeTheme* GetNativeTheme(aura::Window* window) const override;
  void SetNativeThemeOverride(const NativeThemeGetter& callback) override;
  bool GetDefaultUsesSystemTheme() const override;
  void SetDownloadCount(int count) const override;
  void SetProgressFraction(float percentage) const override;
  bool IsStatusIconSupported() const override;
  scoped_ptr<views::StatusIconLinux> CreateLinuxStatusIcon(
      const gfx::ImageSkia& image,
      const base::string16& tool_tip) const override;
  gfx::Image GetIconForContentType(const std::string& content_type,
                                   int size) const override;
  scoped_ptr<views::Border> CreateNativeBorder(
      views::LabelButton* owning_button,
      scoped_ptr<views::LabelButtonBorder> border) override;
  void AddWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  void RemoveWindowButtonOrderObserver(
      views::WindowButtonOrderObserver* observer) override;
  bool UnityIsRunning() override;
  NonClientMiddleClickAction GetNonClientMiddleClickAction() override;
  void NotifyWindowManagerStartupComplete() override;

  // ui::TextEditKeybindingDelegate:
  bool MatchEvent(const ui::Event& event,
                  std::vector<ui::TextEditCommandAuraLinux>* commands) override;

  // ui::Views::LinuxUI:
  void UpdateDeviceScaleFactor(float device_scale_factor) override;
  float GetDeviceScaleFactor() const override;

 private:
  typedef std::map<int, SkColor> ColorMap;
  typedef std::map<int, color_utils::HSL> TintMap;
  typedef std::map<int, gfx::Image> ImageCache;

  // This method returns the colors webkit will use for the scrollbars. When no
  // colors are specified by the GTK+ theme, this function averages of the
  // thumb part and of the track colors.
  void GetScrollbarColors(GdkColor* thumb_active_color,
                          GdkColor* thumb_inactive_color,
                          GdkColor* track_color);

  // Extracts colors and tints from the GTK theme, both for the
  // ThemeService interface and the colors we send to webkit.
  void LoadGtkValues();

  // Reads in explicit theme frame colors from the ChromeGtkFrame style class
  // or generates them per our fallback algorithm.
  GdkColor BuildFrameColors(GtkStyle* frame_style);

  // Sets the underlying theme colors/tints from a GTK color.
  void SetThemeColorFromGtk(int id, const GdkColor* color);
  void SetThemeTintFromGtk(int id, const GdkColor* color);

  // Creates and returns a frame color, either using |gtk_base| verbatim if
  // non-NULL, or tinting |base| with |tint|. Also sets |color_id| and
  // |tint_id| to the returned color.
  GdkColor BuildAndSetFrameColor(const GdkColor* base,
                                 const GdkColor* gtk_base,
                                 const color_utils::HSL& tint,
                                 int color_id,
                                 int tint_id);

  // Lazily generates each bitmap used in the gtk theme.
  SkBitmap GenerateGtkThemeBitmap(int id) const;

  // Creates a GTK+ version of IDR_THEME_FRAME. Instead of tinting, this
  // creates a theme configurable gradient ending with |color_id| at the
  // bottom, and |gradient_name| at the top if that color is specified in the
  // theme.
  SkBitmap GenerateFrameImage(int color_id,
                              const char* gradient_name) const;

  // Takes the base frame image |base_id| and tints it with |tint_id|.
  SkBitmap GenerateTabImage(int base_id) const;

  // Tints an icon based on tint.
  SkBitmap GenerateTintedIcon(int base_id,
                              const color_utils::HSL& tint) const;

  // Renders a GTK icon as a SkBitmap, with prelight/active border if
  // appropriate.
  SkBitmap GenerateGTKIcon(int base_id) const;

  // Renders a GTK button border the size of the image |sizing_idr| in
  // |gtk_state|.
  SkBitmap GenerateToolbarBezel(int gtk_state, int sizing_idr) const;

  // Returns the tint for buttons that contrasts with the normal window
  // background color.
  void GetNormalButtonTintHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current normal text in an entry.
  void GetNormalEntryForegroundHSL(color_utils::HSL* tint) const;

  // Returns a tint that's the color of the current highlighted text in an
  // entry.
  void GetSelectedEntryForegroundHSL(color_utils::HSL* tint) const;

  // Gets a color for the background of the call to action button.
  SkColor CallToActionBgColor(int gtk_state) const;

  // Frees all calculated images and color data.
  void ClearAllThemeData();

  // Updates |default_font_*| based on |desc|.
  void UpdateDefaultFont(const PangoFontDescription* desc);

  // Handles signal from GTK that our theme has been changed.
  CHROMEGTK_CALLBACK_1(Gtk2UI, void, OnStyleSet, GtkStyle*);

  GtkWidget* fake_window_;
  GtkWidget* fake_frame_;
  OwnedWidgetGtk fake_label_;
  OwnedWidgetGtk fake_entry_;

  // Tracks all the signals we have connected to on various widgets.
  scoped_ptr<Gtk2SignalRegistrar> signals_;

  // Tints and colors calculated by LoadGtkValues() that are given to the
  // caller while |use_gtk_| is true.
  ColorMap colors_;
  TintMap tints_;

  // Colors used to tint certain icons.
  color_utils::HSL button_tint_;
  color_utils::HSL entry_tint_;
  color_utils::HSL selected_entry_tint_;

  // Colors that we pass to WebKit. These are generated each time the theme
  // changes.
  SkColor focus_ring_color_;
  SkColor thumb_active_color_;
  SkColor thumb_inactive_color_;
  SkColor track_color_;
  SkColor active_selection_bg_color_;
  SkColor active_selection_fg_color_;
  SkColor inactive_selection_bg_color_;
  SkColor inactive_selection_fg_color_;

  // Details about the default UI font.
  std::string default_font_family_;
  int default_font_size_pixels_;
  int default_font_style_;  // Bitfield of gfx::Font::Style values.
  gfx::FontRenderParams default_font_render_params_;

#if defined(USE_GCONF)
  // Currently, the only source of window button configuration. This will
  // change if we ever have to support XFCE's configuration system or KDE's.
  scoped_ptr<GConfListener> gconf_listener_;
#endif  // defined(USE_GCONF)

  // If either of these vectors are non-empty, they represent the current
  // window button configuration.
  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_;

  scoped_ptr<Gtk2KeyBindingsHandler> key_bindings_handler_;

  // Objects to notify when the window frame button order changes.
  base::ObserverList<views::WindowButtonOrderObserver> observer_list_;

  // Whether we should lower the window on a middle click to the non client
  // area.
  NonClientMiddleClickAction middle_click_action_;

  // Image cache of lazily created images.
  mutable ImageCache gtk_images_;

  // Used to override the native theme for a window. If no override is provided
  // or the callback returns NULL, Gtk2UI will default to a NativeThemeGtk2
  // instance.
  NativeThemeGetter native_theme_overrider_;

  float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(Gtk2UI);
};

}  // namespace libgtk2ui

// Access point to the GTK2 desktop system. This should be the only symbol that
// is exported in the library; everything else should be used through the
// interface, because eventually this .so will be loaded through dlopen at
// runtime so our main binary can conditionally load GTK2 or GTK3 or EFL or
// QT or whatever.
LIBGTK2UI_EXPORT views::LinuxUI* BuildGtk2UI();

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_UI_H_
