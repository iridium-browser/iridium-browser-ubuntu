// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/audio/tray_audio.h"

#include <cmath>
#include <utility>

#include "ash/common/ash_constants.h"
#include "ash/common/display/display_info.h"
#include "ash/common/system/audio/tray_audio_delegate.h"
#include "ash/common/system/audio/volume_view.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/fixed_sized_scroll_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/volume_control_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

TrayAudio::TrayAudio(SystemTray* system_tray,
                     std::unique_ptr<system::TrayAudioDelegate> audio_delegate)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_VOLUME_MUTE, UMA_AUDIO),
      audio_delegate_(std::move(audio_delegate)),
      volume_view_(NULL),
      pop_up_volume_view_(false) {
  WmShell::Get()->system_tray_notifier()->AddAudioObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

TrayAudio::~TrayAudio() {
  display::Screen::GetScreen()->RemoveObserver(this);
  WmShell::Get()->system_tray_notifier()->RemoveAudioObserver(this);
}

// static
bool TrayAudio::ShowAudioDeviceMenu() {
#if defined(OS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

bool TrayAudio::GetInitialVisibility() {
  return audio_delegate_->IsOutputAudioMuted();
}

views::View* TrayAudio::CreateDefaultView(LoginStatus status) {
  volume_view_ = new tray::VolumeView(this, audio_delegate_.get(), true);
  return volume_view_;
}

views::View* TrayAudio::CreateDetailedView(LoginStatus status) {
  volume_view_ = new tray::VolumeView(this, audio_delegate_.get(), false);
  return volume_view_;
}

void TrayAudio::DestroyDefaultView() {
  volume_view_ = NULL;
}

void TrayAudio::DestroyDetailedView() {
  if (volume_view_) {
    volume_view_ = NULL;
    pop_up_volume_view_ = false;
  }
}

bool TrayAudio::ShouldHideArrow() const {
  return true;
}

bool TrayAudio::ShouldShowShelf() const {
  return TrayAudio::ShowAudioDeviceMenu() && !pop_up_volume_view_;
}

void TrayAudio::OnOutputNodeVolumeChanged(uint64_t /* node_id */,
                                          double /* volume */) {
  float percent =
      static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f;
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  pop_up_volume_view_ = true;
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

void TrayAudio::OnOutputMuteChanged(bool /* mute_on */, bool system_adjust) {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->Update();
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  } else if (!system_adjust) {
    pop_up_volume_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
  }
}

void TrayAudio::OnAudioNodesChanged() {
  Update();
}

void TrayAudio::OnActiveOutputNodeChanged() {
  Update();
}

void TrayAudio::OnActiveInputNodeChanged() {
  Update();
}

void TrayAudio::ChangeInternalSpeakerChannelMode() {
  // Swap left/right channel only if it is in Yoga mode.
  system::TrayAudioDelegate::AudioChannelMode channel_mode =
      system::TrayAudioDelegate::NORMAL;
  if (display::Display::HasInternalDisplay()) {
    const DisplayInfo& display_info =
        WmShell::Get()->GetDisplayInfo(display::Display::InternalDisplayId());
    if (display_info.GetActiveRotation() == display::Display::ROTATE_180)
      channel_mode = system::TrayAudioDelegate::LEFT_RIGHT_SWAPPED;
  }

  audio_delegate_->SetInternalSpeakerChannelMode(channel_mode);
}

void TrayAudio::OnDisplayAdded(const display::Display& new_display) {
  if (!new_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();
}

void TrayAudio::OnDisplayRemoved(const display::Display& old_display) {
  if (!old_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();
}

void TrayAudio::OnDisplayMetricsChanged(const display::Display& display,
                                        uint32_t changed_metrics) {
  if (!display.IsInternal())
    return;

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
    ChangeInternalSpeakerChannelMode();
}

void TrayAudio::Update() {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());
  if (volume_view_) {
    volume_view_->SetVolumeLevel(
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
    volume_view_->Update();
  }
}

}  // namespace ash
