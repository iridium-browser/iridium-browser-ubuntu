// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_manager.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_util.h"
#include "ash/display/screen_ash.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/screen.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/display/display_configurator_animation.h"
#include "base/sys_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace ash {
typedef std::vector<gfx::Display> DisplayList;
typedef std::vector<DisplayInfo> DisplayInfoList;

namespace {

// We need to keep this in order for unittests to tell if
// the object in gfx::Screen::GetScreenByType is for shutdown.
gfx::Screen* screen_for_shutdown = NULL;

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 100;

struct DisplaySortFunctor {
  bool operator()(const gfx::Display& a, const gfx::Display& b) {
    return a.id() < b.id();
  }
};

struct DisplayInfoSortFunctor {
  bool operator()(const DisplayInfo& a, const DisplayInfo& b) {
    return a.id() < b.id();
  }
};

struct DisplayModeMatcher {
  DisplayModeMatcher(const DisplayMode& target_mode)
      : target_mode(target_mode) {}
  bool operator()(const DisplayMode& mode) {
    return target_mode.IsEquivalent(mode);
  }
  DisplayMode target_mode;
};

gfx::Display& GetInvalidDisplay() {
  static gfx::Display* invalid_display = new gfx::Display();
  return *invalid_display;
}

void SetInternalDisplayModeList(DisplayInfo* info) {
  DisplayMode native_mode;
  native_mode.size = info->bounds_in_native().size();
  native_mode.device_scale_factor = info->device_scale_factor();
  native_mode.ui_scale = 1.0f;
  info->SetDisplayModes(CreateInternalDisplayModeList(native_mode));
}

void MaybeInitInternalDisplay(DisplayInfo* info) {
  int64 id = info->id();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshUseFirstDisplayAsInternal)) {
    gfx::Display::SetInternalDisplayId(id);
    SetInternalDisplayModeList(info);
  }
}

}  // namespace

using std::string;
using std::vector;

DisplayManager::DisplayManager()
    : delegate_(NULL),
      screen_ash_(new ScreenAsh),
      screen_(screen_ash_.get()),
      layout_store_(new DisplayLayoutStore),
      first_display_id_(gfx::Display::kInvalidDisplayID),
      num_connected_displays_(0),
      force_bounds_changed_(false),
      change_display_upon_host_resize_(false),
      second_display_mode_(EXTENDED),
      mirrored_display_id_(gfx::Display::kInvalidDisplayID),
      registered_internal_display_rotation_lock_(false),
      registered_internal_display_rotation_(gfx::Display::ROTATE_0),
      weak_ptr_factory_(this) {

#if defined(OS_CHROMEOS)
  // Enable only on the device so that DisplayManagerFontTest passes.
  if (base::SysInfo::IsRunningOnChromeOS())
    DisplayInfo::SetUse125DSFForUIScaling(true);

  change_display_upon_host_resize_ = !base::SysInfo::IsRunningOnChromeOS();
#endif
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_ALTERNATE,
                                 screen_ash_.get());
  gfx::Screen* current_native =
      gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  // If there is no native, or the native was for shutdown,
  // use ash's screen.
  if (!current_native ||
      current_native == screen_for_shutdown) {
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   screen_ash_.get());
  }
}

DisplayManager::~DisplayManager() {
#if defined(OS_CHROMEOS)
  // Reset the font params.
  gfx::SetFontRenderParamsDeviceScaleFactor(1.0f);
#endif
}

bool DisplayManager::InitFromCommandLine() {
  DisplayInfoList info_list;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshHostWindowBounds))
    return false;
  const string size_str =
      command_line->GetSwitchValueASCII(switches::kAshHostWindowBounds);
  vector<string> parts;
  base::SplitString(size_str, ',', &parts);
  for (vector<string>::const_iterator iter = parts.begin();
       iter != parts.end(); ++iter) {
    info_list.push_back(DisplayInfo::CreateFromSpec(*iter));
    info_list.back().set_native(true);
  }
  MaybeInitInternalDisplay(&info_list[0]);
  if (info_list.size() > 1 &&
      command_line->HasSwitch(switches::kAshEnableSoftwareMirroring)) {
    SetSecondDisplayMode(MIRRORING);
  }
  OnNativeDisplaysChanged(info_list);
  return true;
}

void DisplayManager::InitDefaultDisplay() {
  DisplayInfoList info_list;
  info_list.push_back(DisplayInfo::CreateFromSpec(std::string()));
  info_list.back().set_native(true);
  MaybeInitInternalDisplay(&info_list[0]);
  OnNativeDisplaysChanged(info_list);
}

void DisplayManager::RefreshFontParams() {
#if defined(OS_CHROMEOS)
  // Use the largest device scale factor among currently active displays. Non
  // internal display may have bigger scale factor in case the external display
  // is an 4K display.
  float largest_device_scale_factor = 1.0f;
  for (const gfx::Display& display : displays_) {
    const ash::DisplayInfo& info = display_info_[display.id()];
    largest_device_scale_factor = std::max(
        largest_device_scale_factor, info.GetEffectiveDeviceScaleFactor());
  }
  gfx::SetFontRenderParamsDeviceScaleFactor(largest_device_scale_factor);
#endif  // OS_CHROMEOS
}

bool DisplayManager::IsActiveDisplay(const gfx::Display& display) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == display.id())
      return true;
  }
  return false;
}

bool DisplayManager::HasInternalDisplay() const {
  return gfx::Display::InternalDisplayId() != gfx::Display::kInvalidDisplayID;
}

bool DisplayManager::IsInternalDisplayId(int64 id) const {
  return gfx::Display::InternalDisplayId() == id;
}

DisplayLayout DisplayManager::GetCurrentDisplayLayout() {
  DCHECK_LE(2U, num_connected_displays());
  // Invert if the primary was swapped.
  if (num_connected_displays() == 2) {
    DisplayIdPair pair = GetCurrentDisplayIdPair();
    return layout_store_->ComputeDisplayLayoutForDisplayIdPair(pair);
  } else if (num_connected_displays() > 2) {
    // Return fixed horizontal layout for >= 3 displays.
    DisplayLayout layout(DisplayLayout::RIGHT, 0);
    return layout;
  }
  NOTREACHED() << "DisplayLayout is requested for single display";
  // On release build, just fallback to default instead of blowing up.
  DisplayLayout layout =
      layout_store_->default_display_layout();
  layout.primary_id = displays_[0].id();
  return layout;
}

DisplayIdPair DisplayManager::GetCurrentDisplayIdPair() const {
  if (IsMirrored()) {
    if (software_mirroring_enabled()) {
      CHECK_EQ(2u, num_connected_displays());
      // This comment is to make it easy to distinguish the crash
      // between two checks.
      CHECK_EQ(1u, displays_.size());
    }
    return std::make_pair(displays_[0].id(), mirrored_display_id_);
  } else {
    CHECK_LE(2u, displays_.size());
    int64 id_at_zero = displays_[0].id();
    if (id_at_zero == gfx::Display::InternalDisplayId() ||
        id_at_zero == first_display_id()) {
      return std::make_pair(id_at_zero, displays_[1].id());
    } else {
      return std::make_pair(displays_[1].id(), id_at_zero);
    }
  }
}

void DisplayManager::SetLayoutForCurrentDisplays(
    const DisplayLayout& layout_relative_to_primary) {
  if (GetNumDisplays() != 2)
    return;
  const gfx::Display& primary = screen_->GetPrimaryDisplay();
  const DisplayIdPair pair = GetCurrentDisplayIdPair();
  // Invert if the primary was swapped.
  DisplayLayout to_set = pair.first == primary.id() ?
      layout_relative_to_primary : layout_relative_to_primary.Invert();

  DisplayLayout current_layout =
      layout_store_->GetRegisteredDisplayLayout(pair);
  if (to_set.position != current_layout.position ||
      to_set.offset != current_layout.offset) {
    to_set.primary_id = primary.id();
    layout_store_->RegisterLayoutForDisplayIdPair(
        pair.first, pair.second, to_set);
    if (delegate_)
      delegate_->PreDisplayConfigurationChange(false);
    // PreDisplayConfigurationChange(false);
    // TODO(oshima): Call UpdateDisplays instead.
    const DisplayLayout layout = GetCurrentDisplayLayout();
    UpdateDisplayBoundsForLayout(
        layout, primary,
        FindDisplayForId(ScreenUtil::GetSecondaryDisplay().id()));

    // Primary's bounds stay the same. Just notify bounds change
    // on the secondary.
    screen_ash_->NotifyMetricsChanged(
        ScreenUtil::GetSecondaryDisplay(),
        gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
        gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
    if (delegate_)
      delegate_->PostDisplayConfigurationChange();
  }
}

const gfx::Display& DisplayManager::GetDisplayForId(int64 id) const {
  gfx::Display* display =
      const_cast<DisplayManager*>(this)->FindDisplayForId(id);
  return display ? *display : GetInvalidDisplay();
}

const gfx::Display& DisplayManager::FindDisplayContainingPoint(
    const gfx::Point& point_in_screen) const {
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    const gfx::Display& display = *iter;
    if (display.bounds().Contains(point_in_screen))
      return display;
  }
  return GetInvalidDisplay();
}

bool DisplayManager::UpdateWorkAreaOfDisplay(int64 display_id,
                                             const gfx::Insets& insets) {
  gfx::Display* display = FindDisplayForId(display_id);
  DCHECK(display);
  gfx::Rect old_work_area = display->work_area();
  display->UpdateWorkAreaFromInsets(insets);
  return old_work_area != display->work_area();
}

void DisplayManager::SetOverscanInsets(int64 display_id,
                                       const gfx::Insets& insets_in_dip) {
  bool update = false;
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (insets_in_dip.empty()) {
        info.set_clear_overscan_insets(true);
      } else {
        info.set_clear_overscan_insets(false);
        info.SetOverscanInsets(insets_in_dip);
      }
      update = true;
    }
    display_info_list.push_back(info);
  }
  if (update) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplays(display_info_list);
  } else {
    display_info_[display_id].SetOverscanInsets(insets_in_dip);
  }
}

void DisplayManager::SetDisplayRotation(int64 display_id,
                                        gfx::Display::Rotation rotation,
                                        gfx::Display::RotationSource source) {
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      if (info.GetRotation(source) == rotation &&
          info.GetActiveRotation() == rotation) {
        return;
      }
      info.SetRotation(rotation, source);
    }
    display_info_list.push_back(info);
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

bool DisplayManager::SetDisplayUIScale(int64 display_id,
                                       float ui_scale) {
  if (!IsDisplayUIScalingEnabled() ||
      gfx::Display::InternalDisplayId() != display_id) {
    return false;
  }
  bool found = false;
  // TODO(mukai): merge this implementation into SetDisplayMode().
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      found = true;
      if (info.configured_ui_scale() == ui_scale)
        return true;
      if (!HasDisplayModeForUIScale(info, ui_scale))
        return false;
      info.set_configured_ui_scale(ui_scale);
    }
    display_info_list.push_back(info);
  }
  if (found) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplays(display_info_list);
    return true;
  }
  return false;
}

void DisplayManager::SetDisplayResolution(int64 display_id,
                                          const gfx::Size& resolution) {
  DCHECK_NE(gfx::Display::InternalDisplayId(), display_id);
  if (gfx::Display::InternalDisplayId() == display_id)
    return;
  const DisplayInfo& display_info = GetDisplayInfo(display_id);
  const std::vector<DisplayMode>& modes = display_info.display_modes();
  DCHECK_NE(0u, modes.size());
  DisplayMode target_mode;
  target_mode.size = resolution;
  std::vector<DisplayMode>::const_iterator iter =
      std::find_if(modes.begin(), modes.end(), DisplayModeMatcher(target_mode));
  if (iter == modes.end()) {
    LOG(WARNING) << "Unsupported resolution was requested:"
                 << resolution.ToString();
    return;
  }
  display_modes_[display_id] = *iter;
#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS())
    Shell::GetInstance()->display_configurator()->OnConfigurationChanged();
#endif
}

bool DisplayManager::SetDisplayMode(int64 display_id,
                                    const DisplayMode& display_mode) {
  if (IsInternalDisplayId(display_id)) {
    SetDisplayUIScale(display_id, display_mode.ui_scale);
    return false;
  }

  DisplayInfoList display_info_list;
  bool display_property_changed = false;
  bool resolution_changed = false;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo info = GetDisplayInfo(iter->id());
    if (info.id() == display_id) {
      const std::vector<DisplayMode>& modes = info.display_modes();
      std::vector<DisplayMode>::const_iterator iter =
          std::find_if(modes.begin(),
                       modes.end(),
                       DisplayModeMatcher(display_mode));
      if (iter == modes.end()) {
        LOG(WARNING) << "Unsupported resolution was requested:"
                     << display_mode.size.ToString();
        return false;
      }
      display_modes_[display_id] = *iter;
      if (info.bounds_in_native().size() != display_mode.size)
        resolution_changed = true;
      if (info.device_scale_factor() != display_mode.device_scale_factor) {
        info.set_device_scale_factor(display_mode.device_scale_factor);
        display_property_changed = true;
      }
    }
    display_info_list.push_back(info);
  }
  if (display_property_changed) {
    AddMirrorDisplayInfoIfAny(&display_info_list);
    UpdateDisplays(display_info_list);
  }
#if defined(OS_CHROMEOS)
  if (resolution_changed && base::SysInfo::IsRunningOnChromeOS())
    Shell::GetInstance()->display_configurator()->OnConfigurationChanged();
#endif
  return resolution_changed;
}

void DisplayManager::RegisterDisplayProperty(
    int64 display_id,
    gfx::Display::Rotation rotation,
    float ui_scale,
    const gfx::Insets* overscan_insets,
    const gfx::Size& resolution_in_pixels,
    float device_scale_factor,
    ui::ColorCalibrationProfile color_profile) {
  if (display_info_.find(display_id) == display_info_.end())
    display_info_[display_id] = DisplayInfo(display_id, std::string(), false);

  display_info_[display_id].SetRotation(rotation,
                                        gfx::Display::ROTATION_SOURCE_ACTIVE);
  display_info_[display_id].SetColorProfile(color_profile);
  // Just in case the preference file was corrupted.
  // TODO(mukai): register |display_modes_| here as well, so the lookup for the
  // default mode in GetActiveModeForDisplayId() gets much simpler.
  if (0.5f <= ui_scale && ui_scale <= 2.0f)
    display_info_[display_id].set_configured_ui_scale(ui_scale);
  if (overscan_insets)
    display_info_[display_id].SetOverscanInsets(*overscan_insets);
  if (!resolution_in_pixels.IsEmpty()) {
    DCHECK(!IsInternalDisplayId(display_id));
    // Default refresh rate, until OnNativeDisplaysChanged() updates us with the
    // actual display info, is 60 Hz.
    DisplayMode mode(resolution_in_pixels, 60.0f, false, false);
    mode.device_scale_factor = device_scale_factor;
    display_modes_[display_id] = mode;
  }
}

DisplayMode DisplayManager::GetActiveModeForDisplayId(int64 display_id) const {
  DisplayMode selected_mode;
  if (GetSelectedModeForDisplayId(display_id, &selected_mode))
    return selected_mode;

  // If 'selected' mode is empty, it should return the default mode. This means
  // the native mode for the external display. Unfortunately this is not true
  // for the internal display because restoring UI-scale doesn't register the
  // restored mode to |display_mode_|, so it needs to look up the mode whose
  // UI-scale value matches. See the TODO in RegisterDisplayProperty().
  const DisplayInfo& info = GetDisplayInfo(display_id);
  const std::vector<DisplayMode>& display_modes = info.display_modes();

  if (IsInternalDisplayId(display_id)) {
    for (size_t i = 0; i < display_modes.size(); ++i) {
      if (info.configured_ui_scale() == display_modes[i].ui_scale)
        return display_modes[i];
    }
  } else {
    for (size_t i = 0; i < display_modes.size(); ++i) {
      if (display_modes[i].native)
        return display_modes[i];
    }
  }
  return selected_mode;
}

void DisplayManager::RegisterDisplayRotationProperties(bool rotation_lock,
    gfx::Display::Rotation rotation) {
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  registered_internal_display_rotation_lock_ = rotation_lock;
  registered_internal_display_rotation_ = rotation;
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
}

bool DisplayManager::GetSelectedModeForDisplayId(int64 id,
                                                 DisplayMode* mode_out) const {
  std::map<int64, DisplayMode>::const_iterator iter = display_modes_.find(id);
  if (iter == display_modes_.end())
    return false;
  *mode_out = iter->second;
  return true;
}

bool DisplayManager::IsDisplayUIScalingEnabled() const {
  return GetDisplayIdForUIScaling() != gfx::Display::kInvalidDisplayID;
}

gfx::Insets DisplayManager::GetOverscanInsets(int64 display_id) const {
  std::map<int64, DisplayInfo>::const_iterator it =
      display_info_.find(display_id);
  return (it != display_info_.end()) ?
      it->second.overscan_insets_in_dip() : gfx::Insets();
}

void DisplayManager::SetColorCalibrationProfile(
    int64 display_id,
    ui::ColorCalibrationProfile profile) {
#if defined(OS_CHROMEOS)
  if (!display_info_[display_id].IsColorProfileAvailable(profile))
    return;

  if (delegate_)
    delegate_->PreDisplayConfigurationChange(false);
  // Just sets color profile if it's not running on ChromeOS (like tests).
  if (!base::SysInfo::IsRunningOnChromeOS() ||
      Shell::GetInstance()->display_configurator()->SetColorCalibrationProfile(
          display_id, profile)) {
    display_info_[display_id].SetColorProfile(profile);
    UMA_HISTOGRAM_ENUMERATION(
        "ChromeOS.Display.ColorProfile", profile, ui::NUM_COLOR_PROFILES);
  }
  if (delegate_)
    delegate_->PostDisplayConfigurationChange();
#endif
}

void DisplayManager::OnNativeDisplaysChanged(
    const std::vector<DisplayInfo>& updated_displays) {
  if (updated_displays.empty()) {
    VLOG(1) << "OnNativeDisplaysChanged(0): # of current displays="
            << displays_.size();
    // If the device is booted without display, or chrome is started
    // without --ash-host-window-bounds on linux desktop, use the
    // default display.
    if (displays_.empty()) {
      std::vector<DisplayInfo> init_displays;
      init_displays.push_back(DisplayInfo::CreateFromSpec(std::string()));
      MaybeInitInternalDisplay(&init_displays[0]);
      OnNativeDisplaysChanged(init_displays);
    } else {
      // Otherwise don't update the displays when all displays are disconnected.
      // This happens when:
      // - the device is idle and powerd requested to turn off all displays.
      // - the device is suspended. (kernel turns off all displays)
      // - the internal display's brightness is set to 0 and no external
      //   display is connected.
      // - the internal display's brightness is 0 and external display is
      //   disconnected.
      // The display will be updated when one of displays is turned on, and the
      // display list will be updated correctly.
    }
    return;
  }
  first_display_id_ = updated_displays[0].id();
  std::set<gfx::Point> origins;

  if (updated_displays.size() == 1) {
    VLOG(1) << "OnNativeDisplaysChanged(1):" << updated_displays[0].ToString();
  } else {
    VLOG(1) << "OnNativeDisplaysChanged(" << updated_displays.size()
            << ") [0]=" << updated_displays[0].ToString()
            << ", [1]=" << updated_displays[1].ToString();
  }

  bool internal_display_connected = false;
  num_connected_displays_ = updated_displays.size();
  mirrored_display_id_ = gfx::Display::kInvalidDisplayID;
  mirroring_display_ = gfx::Display();
  DisplayInfoList new_display_info_list;
  for (DisplayInfoList::const_iterator iter = updated_displays.begin();
       iter != updated_displays.end();
       ++iter) {
    if (!internal_display_connected)
      internal_display_connected = IsInternalDisplayId(iter->id());
    // Mirrored monitors have the same origins.
    gfx::Point origin = iter->bounds_in_native().origin();
    if (origins.find(origin) != origins.end()) {
      InsertAndUpdateDisplayInfo(*iter);
      mirrored_display_id_ = iter->id();
    } else {
      origins.insert(origin);
      new_display_info_list.push_back(*iter);
    }

    DisplayMode new_mode;
    new_mode.size = iter->bounds_in_native().size();
    new_mode.device_scale_factor = iter->device_scale_factor();
    new_mode.ui_scale = iter->configured_ui_scale();
    const std::vector<DisplayMode>& display_modes = iter->display_modes();
    // This is empty the displays are initialized from InitFromCommandLine.
    if (!display_modes.size())
      continue;
    std::vector<DisplayMode>::const_iterator display_modes_iter =
        std::find_if(display_modes.begin(),
                     display_modes.end(),
                     DisplayModeMatcher(new_mode));
    // Update the actual resolution selected as the resolution request may fail.
    if (display_modes_iter == display_modes.end())
      display_modes_.erase(iter->id());
    else if (display_modes_.find(iter->id()) != display_modes_.end())
      display_modes_[iter->id()] = *display_modes_iter;
  }
  if (HasInternalDisplay() &&
      !internal_display_connected &&
      display_info_.find(gfx::Display::InternalDisplayId()) ==
      display_info_.end()) {
    DisplayInfo internal_display_info(
        gfx::Display::InternalDisplayId(),
        l10n_util::GetStringUTF8(IDS_ASH_INTERNAL_DISPLAY_NAME),
        false  /*Internal display must not have overscan */);
    internal_display_info.SetBounds(gfx::Rect(0, 0, 800, 600));
    display_info_[gfx::Display::InternalDisplayId()] = internal_display_info;
  }
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::UpdateDisplays() {
  DisplayInfoList display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    display_info_list.push_back(GetDisplayInfo(iter->id()));
  }
  AddMirrorDisplayInfoIfAny(&display_info_list);
  UpdateDisplays(display_info_list);
}

void DisplayManager::UpdateDisplays(
    const std::vector<DisplayInfo>& updated_display_info_list) {
#if defined(OS_WIN)
  DCHECK_EQ(1u, updated_display_info_list.size()) <<
      ": Multiple display test does not work on Windows bots. Please "
      "skip (don't disable) the test using SupportsMultipleDisplays()";
#endif

  DisplayInfoList new_display_info_list = updated_display_info_list;
  std::sort(displays_.begin(), displays_.end(), DisplaySortFunctor());
  std::sort(new_display_info_list.begin(),
            new_display_info_list.end(),
            DisplayInfoSortFunctor());
  DisplayList removed_displays;
  std::map<size_t, uint32_t> display_changes;
  std::vector<size_t> added_display_indices;

  DisplayList::iterator curr_iter = displays_.begin();
  DisplayInfoList::const_iterator new_info_iter = new_display_info_list.begin();

  DisplayList new_displays;

  // Use the internal display or 1st as the mirror source, then scale
  // the root window so that it matches the external display's
  // resolution. This is necessary in order for scaling to work while
  // mirrored.
  int64 mirroing_display_id = gfx::Display::kInvalidDisplayID;

  if (second_display_mode_ != EXTENDED && new_display_info_list.size() == 2) {
    bool zero_is_source =
        first_display_id_ == new_display_info_list[0].id() ||
        gfx::Display::InternalDisplayId() == new_display_info_list[0].id();
    DCHECK_EQ(MIRRORING, second_display_mode_);
    mirrored_display_id_ = new_display_info_list[zero_is_source ? 1 : 0].id();
    mirroing_display_id = mirrored_display_id_;
  }

  while (curr_iter != displays_.end() ||
         new_info_iter != new_display_info_list.end()) {
    if (new_info_iter != new_display_info_list.end() &&
        mirroing_display_id == new_info_iter->id()) {
      DisplayInfo info = *new_info_iter;
      info.SetOverscanInsets(gfx::Insets());
      InsertAndUpdateDisplayInfo(info);
      mirroring_display_ =
          CreateDisplayFromDisplayInfoById(mirroing_display_id);
      ++new_info_iter;
      // Remove existing external display if it is going to be used as
      // mirroring display.
      if (curr_iter != displays_.end() &&
          curr_iter->id() == mirroing_display_id) {
        removed_displays.push_back(*curr_iter);
        ++curr_iter;
      }
      continue;
    }

    if (curr_iter == displays_.end()) {
      // more displays in new list.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    } else if (new_info_iter == new_display_info_list.end()) {
      // more displays in current list.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else if (curr_iter->id() == new_info_iter->id()) {
      const gfx::Display& current_display = *curr_iter;
      // Copy the info because |CreateDisplayFromInfo| updates the instance.
      const DisplayInfo current_display_info =
          GetDisplayInfo(current_display.id());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      gfx::Display new_display =
          CreateDisplayFromDisplayInfoById(new_info_iter->id());
      const DisplayInfo& new_display_info = GetDisplayInfo(new_display.id());

      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_NONE;

      // At that point the new Display objects we have are not entirely updated,
      // they are missing the translation related to the Display disposition in
      // the layout.
      // Using display.bounds() and display.work_area() would fail most of the
      // time.
      if (force_bounds_changed_ ||
          (current_display_info.bounds_in_native() !=
           new_display_info.bounds_in_native()) ||
          (current_display_info.GetOverscanInsetsInPixel() !=
           new_display_info.GetOverscanInsetsInPixel()) ||
          current_display.size() != new_display.size()) {
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
            gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
      }

      if (current_display.device_scale_factor() !=
          new_display.device_scale_factor()) {
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
      }

      if (current_display.rotation() != new_display.rotation())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_ROTATION;

      if (metrics != gfx::DisplayObserver::DISPLAY_METRIC_NONE) {
        display_changes.insert(
            std::pair<size_t, uint32_t>(new_displays.size(), metrics));
      }

      new_display.UpdateWorkAreaFromInsets(current_display.GetWorkAreaInsets());
      new_displays.push_back(new_display);
      ++curr_iter;
      ++new_info_iter;
    } else if (curr_iter->id() < new_info_iter->id()) {
      // more displays in current list between ids, which means it is deleted.
      removed_displays.push_back(*curr_iter);
      ++curr_iter;
    } else {
      // more displays in new list between ids, which means it is added.
      added_display_indices.push_back(new_displays.size());
      InsertAndUpdateDisplayInfo(*new_info_iter);
      new_displays.push_back(
          CreateDisplayFromDisplayInfoById(new_info_iter->id()));
      ++new_info_iter;
    }
  }
  gfx::Display old_primary;
  if (delegate_)
    old_primary = screen_ash_->GetPrimaryDisplay();

  // Clear focus if the display has been removed, but don't clear focus if
  // the destkop has been moved from one display to another
  // (mirror -> docked, docked -> single internal).
  bool clear_focus =
      !removed_displays.empty() &&
      !(removed_displays.size() == 1 && added_display_indices.size() == 1);
  if (delegate_)
    delegate_->PreDisplayConfigurationChange(clear_focus);

  // Do not update |displays_| if there's nothing to be updated. Without this,
  // it will not update the display layout, which causes the bug
  // http://crbug.com/155948.
  if (display_changes.empty() && added_display_indices.empty() &&
      removed_displays.empty()) {
    // When changing from software mirroring mode to sinlge display
    // mode, it is possible there is no need to update |displays_| and
    // we early out here. But we still need to update the mirroring
    // window and call the PostDisplayConfigurationChange() cause
    // there are some clients need to act on this,
    // e.g. TouchTransformerController needs to adjust the
    // TouchTransformer when/ switching from dual displays to single
    // display.
    if (delegate_) {
      if (HasSoftwareMirroringDisplay())
        CreateMirrorWindowAsyncIfAny();
      else
        delegate_->CloseMirroringDisplay();
      delegate_->PostDisplayConfigurationChange();
    }
    return;
  }

  std::vector<size_t> updated_indices;
  if (UpdateNonPrimaryDisplayBoundsForLayout(&new_displays, &updated_indices)) {
    for (std::vector<size_t>::iterator it = updated_indices.begin();
         it != updated_indices.end(); ++it) {
      size_t updated_index = *it;
      if (std::find(added_display_indices.begin(),
                    added_display_indices.end(),
                    updated_index) == added_display_indices.end()) {
        uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                           gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
        if (display_changes.find(updated_index) != display_changes.end())
          metrics |= display_changes[updated_index];

        display_changes[updated_index] = metrics;
      }
    }
  }

  displays_ = new_displays;

  RefreshFontParams();
  base::AutoReset<bool> resetter(&change_display_upon_host_resize_, false);

  // Temporarily add displays to be removed because display object
  // being removed are accessed during shutting down the root.
  displays_.insert(displays_.end(), removed_displays.begin(),
                   removed_displays.end());

  for (DisplayList::const_reverse_iterator iter = removed_displays.rbegin();
       iter != removed_displays.rend(); ++iter) {
    screen_ash_->NotifyDisplayRemoved(displays_.back());
    displays_.pop_back();
  }

  bool has_mirroring_display = HasSoftwareMirroringDisplay();
  // Close the mirroring window here to avoid creating two compositor on
  // one display.
  if (!has_mirroring_display && delegate_)
    delegate_->CloseMirroringDisplay();

  for (std::vector<size_t>::iterator iter = added_display_indices.begin();
       iter != added_display_indices.end(); ++iter) {
    screen_ash_->NotifyDisplayAdded(displays_[*iter]);
  }

  bool notify_primary_change =
      delegate_ ? old_primary.id() != screen_->GetPrimaryDisplay().id() : false;

  for (std::map<size_t, uint32_t>::iterator iter = display_changes.begin();
       iter != display_changes.end();
       ++iter) {
    uint32_t metrics = iter->second;
    const gfx::Display& updated_display = displays_[iter->first];

    if (notify_primary_change &&
        updated_display.id() == screen_->GetPrimaryDisplay().id()) {
      metrics |= gfx::DisplayObserver::DISPLAY_METRIC_PRIMARY;
      notify_primary_change = false;
    }
    screen_ash_->NotifyMetricsChanged(updated_display, metrics);
  }

  if (notify_primary_change) {
    // This happens when a primary display has moved to anther display without
    // bounds change.
    const gfx::Display& primary = screen_->GetPrimaryDisplay();
    if (primary.id() != old_primary.id()) {
      uint32_t metrics = gfx::DisplayObserver::DISPLAY_METRIC_PRIMARY;
      if (primary.size() != old_primary.size()) {
        metrics |= (gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS |
                    gfx::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      }
      if (primary.device_scale_factor() != old_primary.device_scale_factor())
        metrics |= gfx::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;

      screen_ash_->NotifyMetricsChanged(primary, metrics);
    }
  }

  if (delegate_)
    delegate_->PostDisplayConfigurationChange();

#if defined(USE_X11) && defined(OS_CHROMEOS)
  if (!display_changes.empty() && base::SysInfo::IsRunningOnChromeOS())
    ui::ClearX11DefaultRootWindow();
#endif

  // Create the mirroring window asynchronously after all displays
  // are added so that it can mirror the display newly added. This can
  // happen when switching from dock mode to software mirror mode.
  CreateMirrorWindowAsyncIfAny();
}

const gfx::Display& DisplayManager::GetDisplayAt(size_t index) const {
  DCHECK_LT(index, displays_.size());
  return displays_[index];
}

const gfx::Display& DisplayManager::GetPrimaryDisplayCandidate() const {
  if (GetNumDisplays() != 2)
    return displays_[0];
  DisplayLayout layout = layout_store_->GetRegisteredDisplayLayout(
      GetCurrentDisplayIdPair());
  return GetDisplayForId(layout.primary_id);
}

size_t DisplayManager::GetNumDisplays() const {
  return displays_.size();
}

bool DisplayManager::IsMirrored() const {
  return mirrored_display_id_ != gfx::Display::kInvalidDisplayID;
}

const DisplayInfo& DisplayManager::GetDisplayInfo(int64 display_id) const {
  DCHECK_NE(gfx::Display::kInvalidDisplayID, display_id);

  std::map<int64, DisplayInfo>::const_iterator iter =
      display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

std::string DisplayManager::GetDisplayNameForId(int64 id) {
  if (id == gfx::Display::kInvalidDisplayID)
    return l10n_util::GetStringUTF8(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  std::map<int64, DisplayInfo>::const_iterator iter = display_info_.find(id);
  if (iter != display_info_.end() && !iter->second.name().empty())
    return iter->second.name();

  return base::StringPrintf("Display %d", static_cast<int>(id));
}

int64 DisplayManager::GetDisplayIdForUIScaling() const {
  // UI Scaling is effective only on internal display.
  int64 display_id = gfx::Display::InternalDisplayId();
#if defined(OS_WIN)
  display_id = first_display_id();
#endif
  return display_id;
}

void DisplayManager::SetMirrorMode(bool mirrored) {
  if (num_connected_displays() <= 1)
    return;

#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    ui::MultipleDisplayState new_state =
        mirrored ? ui::MULTIPLE_DISPLAY_STATE_DUAL_MIRROR :
                   ui::MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
    Shell::GetInstance()->display_configurator()->SetDisplayMode(new_state);
    return;
  }
#endif
  // This is fallback path to emulate mirroroing on desktop.
  SetSecondDisplayMode(mirrored ? MIRRORING : EXTENDED);
  DisplayInfoList display_info_list;
  int count = 0;
  for (std::map<int64, DisplayInfo>::const_iterator iter =
           display_info_.begin();
       count < 2; ++iter, ++count) {
    display_info_list.push_back(GetDisplayInfo(iter->second.id()));
  }
  UpdateDisplays(display_info_list);
#if defined(OS_CHROMEOS)
  if (Shell::GetInstance()->display_configurator_animation()) {
    Shell::GetInstance()->display_configurator_animation()->
        StartFadeInAnimation();
  }
#endif
}

void DisplayManager::AddRemoveDisplay() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  const DisplayInfo& first_display = GetDisplayInfo(displays_[0].id());
  new_display_info_list.push_back(first_display);
  // Add if there is only one display connected.
  if (num_connected_displays() == 1) {
    // Layout the 2nd display below the primary as with the real device.
    gfx::Rect host_bounds = first_display.bounds_in_native();
    new_display_info_list.push_back(DisplayInfo::CreateFromSpec(
        base::StringPrintf(
            "%d+%d-500x400", host_bounds.x(), host_bounds.bottom())));
  }
  num_connected_displays_ = new_display_info_list.size();
  mirrored_display_id_ = gfx::Display::kInvalidDisplayID;
  mirroring_display_ = gfx::Display();
  UpdateDisplays(new_display_info_list);
}

void DisplayManager::ToggleDisplayScaleFactor() {
  DCHECK(!displays_.empty());
  std::vector<DisplayInfo> new_display_info_list;
  for (DisplayList::const_iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    DisplayInfo display_info = GetDisplayInfo(iter->id());
    display_info.set_device_scale_factor(
        display_info.device_scale_factor() == 1.0f ? 2.0f : 1.0f);
    new_display_info_list.push_back(display_info);
  }
  AddMirrorDisplayInfoIfAny(&new_display_info_list);
  UpdateDisplays(new_display_info_list);
}

#if defined(OS_CHROMEOS)
void DisplayManager::SetSoftwareMirroring(bool enabled) {
  SetSecondDisplayMode(enabled ? MIRRORING : EXTENDED);
}

bool DisplayManager::SoftwareMirroringEnabled() const {
  return software_mirroring_enabled();
}
#endif

void DisplayManager::SetSecondDisplayMode(SecondDisplayMode mode) {
  second_display_mode_ = mode;
  mirrored_display_id_ = gfx::Display::kInvalidDisplayID;
  mirroring_display_ = gfx::Display();
}

bool DisplayManager::UpdateDisplayBounds(int64 display_id,
                                         const gfx::Rect& new_bounds) {
  if (change_display_upon_host_resize_) {
    display_info_[display_id].SetBounds(new_bounds);
    // Don't notify observers if the mirrored window has changed.
    if (software_mirroring_enabled() && mirrored_display_id_ == display_id)
      return false;
    gfx::Display* display = FindDisplayForId(display_id);
    display->SetSize(display_info_[display_id].size_in_pixel());
    screen_ash_->NotifyMetricsChanged(
        *display, gfx::DisplayObserver::DISPLAY_METRIC_BOUNDS);
    return true;
  }
  return false;
}

void DisplayManager::CreateMirrorWindowAsyncIfAny() {
  // Do not post a task if the software mirroring doesn't exist, or
  // during initialization when compositor's init task isn't posted yet.
  // ash::Shell::Init() will call this after the compositor is initialized.
  if (!HasSoftwareMirroringDisplay() || !delegate_)
    return;
  base::MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&DisplayManager::CreateMirrorWindowIfAny,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DisplayManager::CreateScreenForShutdown() const {
  bool native_is_ash =
      gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE) ==
      screen_ash_.get();
  delete screen_for_shutdown;
  screen_for_shutdown = screen_ash_->CloneForShutdown();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_ALTERNATE,
                                 screen_for_shutdown);
  if (native_is_ash) {
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   screen_for_shutdown);
  }
}

void DisplayManager::UpdateInternalDisplayModeListForTest() {
  if (display_info_.count(gfx::Display::InternalDisplayId()) == 0)
    return;
  DisplayInfo* info = &display_info_[gfx::Display::InternalDisplayId()];
  SetInternalDisplayModeList(info);
}

gfx::Display* DisplayManager::FindDisplayForId(int64 id) {
  for (DisplayList::iterator iter = displays_.begin();
       iter != displays_.end(); ++iter) {
    if ((*iter).id() == id)
      return &(*iter);
  }
  DLOG(WARNING) << "Could not find display:" << id;
  return NULL;
}

void DisplayManager::AddMirrorDisplayInfoIfAny(
    std::vector<DisplayInfo>* display_info_list) {
  if (software_mirroring_enabled() && IsMirrored())
    display_info_list->push_back(GetDisplayInfo(mirrored_display_id_));
}

void DisplayManager::InsertAndUpdateDisplayInfo(const DisplayInfo& new_info) {
  std::map<int64, DisplayInfo>::iterator info =
      display_info_.find(new_info.id());
  if (info != display_info_.end()) {
    info->second.Copy(new_info);
  } else {
    display_info_[new_info.id()] = new_info;
    display_info_[new_info.id()].set_native(false);
  }
  display_info_[new_info.id()].UpdateDisplaySize();
  OnDisplayInfoUpdated(display_info_[new_info.id()]);
}

void DisplayManager::OnDisplayInfoUpdated(const DisplayInfo& display_info) {
#if defined(OS_CHROMEOS)
  ui::ColorCalibrationProfile color_profile = display_info.color_profile();
  if (color_profile != ui::COLOR_PROFILE_STANDARD) {
    Shell::GetInstance()->display_configurator()->SetColorCalibrationProfile(
        display_info.id(), color_profile);
  }
#endif
}

gfx::Display DisplayManager::CreateDisplayFromDisplayInfoById(int64 id) {
  DCHECK(display_info_.find(id) != display_info_.end());
  const DisplayInfo& display_info = display_info_[id];

  gfx::Display new_display(display_info.id());
  gfx::Rect bounds_in_native(display_info.size_in_pixel());
  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();

  // Simply set the origin to (0,0).  The primary display's origin is
  // always (0,0) and the bounds of non-primary display(s) will be updated
  // in |UpdateNonPrimaryDisplayBoundsForLayout| called in |UpdateDisplay|.
  new_display.SetScaleAndBounds(
      device_scale_factor, gfx::Rect(bounds_in_native.size()));
  new_display.set_rotation(display_info.GetActiveRotation());
  new_display.set_touch_support(display_info.touch_support());
  return new_display;
}

bool DisplayManager::UpdateNonPrimaryDisplayBoundsForLayout(
    DisplayList* displays,
    std::vector<size_t>* updated_indices) const {

  if (displays->size() < 2U)
    return false;

  if (displays->size() > 2U) {
    // For more than 2 displays, always use horizontal layout.
    int x_offset = displays->at(0).bounds().width();
    for (size_t i = 1; i < displays->size(); ++i) {
      gfx::Display& display = displays->at(i);
      const gfx::Rect& bounds = display.bounds();
      gfx::Point origin = gfx::Point(x_offset, 0);
      gfx::Insets insets = display.GetWorkAreaInsets();
      display.set_bounds(gfx::Rect(origin, bounds.size()));
      display.UpdateWorkAreaFromInsets(insets);
      x_offset += bounds.width();
      updated_indices->push_back(i);
    }
    return true;
  }

  int64 id_at_zero = displays->at(0).id();
  DisplayIdPair pair =
      (id_at_zero == first_display_id_ ||
       id_at_zero == gfx::Display::InternalDisplayId()) ?
      std::make_pair(id_at_zero, displays->at(1).id()) :
      std::make_pair(displays->at(1).id(), id_at_zero);
  DisplayLayout layout =
      layout_store_->ComputeDisplayLayoutForDisplayIdPair(pair);

  // Ignore if a user has a old format (should be extremely rare)
  // and this will be replaced with DCHECK.
  if (layout.primary_id != gfx::Display::kInvalidDisplayID) {
    size_t primary_index, secondary_index;
    if (displays->at(0).id() == layout.primary_id) {
      primary_index = 0;
      secondary_index = 1;
    } else {
      primary_index = 1;
      secondary_index = 0;
    }
    // This function may be called before the secondary display is
    // registered. The bounds is empty in that case and will
    // return true.
    gfx::Rect bounds =
        GetDisplayForId(displays->at(secondary_index).id()).bounds();
    UpdateDisplayBoundsForLayout(
        layout, displays->at(primary_index), &displays->at(secondary_index));
    updated_indices->push_back(secondary_index);
    return bounds != displays->at(secondary_index).bounds();
  }
  return false;
}

void DisplayManager::CreateMirrorWindowIfAny() {
  if (HasSoftwareMirroringDisplay() && delegate_) {
    DisplayInfo display_info = GetDisplayInfo(mirroring_display_.id());
    delegate_->CreateOrUpdateMirroringDisplay(display_info);
  }
}

bool DisplayManager::HasSoftwareMirroringDisplay() {
  return second_display_mode_ != DisplayManager::EXTENDED &&
      mirroring_display_.is_valid();
}

// static
void DisplayManager::UpdateDisplayBoundsForLayout(
    const DisplayLayout& layout,
    const gfx::Display& primary_display,
    gfx::Display* secondary_display) {
  DCHECK_EQ("0,0", primary_display.bounds().origin().ToString());

  const gfx::Rect& primary_bounds = primary_display.bounds();
  const gfx::Rect& secondary_bounds = secondary_display->bounds();
  gfx::Point new_secondary_origin = primary_bounds.origin();

  DisplayLayout::Position position = layout.position;

  // Ignore the offset in case the secondary display doesn't share edges with
  // the primary display.
  int offset = layout.offset;
  if (position == DisplayLayout::TOP || position == DisplayLayout::BOTTOM) {
    offset = std::min(
        offset, primary_bounds.width() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.width() + kMinimumOverlapForInvalidOffset);
  } else {
    offset = std::min(
        offset, primary_bounds.height() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.height() + kMinimumOverlapForInvalidOffset);
  }
  switch (position) {
    case DisplayLayout::TOP:
      new_secondary_origin.Offset(offset, -secondary_bounds.height());
      break;
    case DisplayLayout::RIGHT:
      new_secondary_origin.Offset(primary_bounds.width(), offset);
      break;
    case DisplayLayout::BOTTOM:
      new_secondary_origin.Offset(offset, primary_bounds.height());
      break;
    case DisplayLayout::LEFT:
      new_secondary_origin.Offset(-secondary_bounds.width(), offset);
      break;
  }
  gfx::Insets insets = secondary_display->GetWorkAreaInsets();
  secondary_display->set_bounds(
      gfx::Rect(new_secondary_origin, secondary_bounds.size()));
  secondary_display->UpdateWorkAreaFromInsets(insets);
}

}  // namespace ash
