// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/update_display_configuration_task.h"

#include "ui/display/chromeos/configure_displays_task.h"
#include "ui/display/chromeos/display_layout_manager.h"
#include "ui/display/chromeos/display_util.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"

namespace ui {

UpdateDisplayConfigurationTask::UpdateDisplayConfigurationTask(
    NativeDisplayDelegate* delegate,
    DisplayLayoutManager* layout_manager,
    MultipleDisplayState new_display_state,
    chromeos::DisplayPowerState new_power_state,
    int power_flags,
    uint32_t background_color_argb,
    bool force_configure,
    const ResponseCallback& callback)
    : delegate_(delegate),
      layout_manager_(layout_manager),
      new_display_state_(new_display_state),
      new_power_state_(new_power_state),
      power_flags_(power_flags),
      background_color_argb_(background_color_argb),
      force_configure_(force_configure),
      callback_(callback),
      force_dpms_(false),
      weak_ptr_factory_(this) {
  delegate_->GrabServer();
}

UpdateDisplayConfigurationTask::~UpdateDisplayConfigurationTask() {
  delegate_->UngrabServer();
}

void UpdateDisplayConfigurationTask::Run() {
  delegate_->GetDisplays(
      base::Bind(&UpdateDisplayConfigurationTask::OnDisplaysUpdated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void UpdateDisplayConfigurationTask::OnDisplaysUpdated(
    const std::vector<DisplaySnapshot*>& displays) {
  cached_displays_ = displays;

  if (cached_displays_.size() > 1 && background_color_argb_)
    delegate_->SetBackgroundColor(background_color_argb_);

  // If the user hasn't requested a display state, update it using the requested
  // power state.
  if (new_display_state_ == MULTIPLE_DISPLAY_STATE_INVALID)
    new_display_state_ = ChooseDisplayState();

  VLOG(1) << "OnDisplaysUpdated: new_display_state="
          << MultipleDisplayStateToString(new_display_state_)
          << " new_power_state=" << DisplayPowerStateToString(new_power_state_)
          << " flags=" << power_flags_
          << " force_configure=" << force_configure_
          << " display_count=" << cached_displays_.size();
  // If there has been any change in the requested power state and the displays
  // aren't being turned off force a change in DPMS state.
  force_dpms_ = ShouldForceDpms() && ShouldConfigure();

  if (ShouldConfigure()) {
    EnterState(base::Bind(&UpdateDisplayConfigurationTask::OnStateEntered,
                          weak_ptr_factory_.GetWeakPtr()));
  } else {
    // If we don't have to configure then we're sticking with the old
    // configuration. Update it such that it reflects in the reported value.
    new_power_state_ = layout_manager_->GetPowerState();
    FinishConfiguration(true);
  }
}

void UpdateDisplayConfigurationTask::EnterState(
    const ConfigureDisplaysTask::ResponseCallback& callback) {
  VLOG(2) << "EnterState";
  std::vector<DisplayConfigureRequest> requests;
  if (!layout_manager_->GetDisplayLayout(cached_displays_, new_display_state_,
                                         new_power_state_, &requests,
                                         &framebuffer_size_)) {
    callback.Run(ConfigureDisplaysTask::ERROR);
    return;
  }
  if (!requests.empty()) {
    DCHECK(!framebuffer_size_.IsEmpty());
    delegate_->CreateFrameBuffer(framebuffer_size_);
    configure_task_.reset(
        new ConfigureDisplaysTask(delegate_, requests, callback));
    configure_task_->Run();
  } else {
    VLOG(2) << "No displays";
    callback.Run(ConfigureDisplaysTask::SUCCESS);
  }
}

void UpdateDisplayConfigurationTask::OnStateEntered(
    ConfigureDisplaysTask::Status status) {
  bool success = status != ConfigureDisplaysTask::ERROR;
  if (new_display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR &&
      status == ConfigureDisplaysTask::PARTIAL_SUCCESS)
    success = false;

  if (layout_manager_->GetSoftwareMirroringController()) {
    bool enable_software_mirroring = false;
    if (!success && new_display_state_ == MULTIPLE_DISPLAY_STATE_DUAL_MIRROR) {
      if (layout_manager_->GetDisplayState() !=
              MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED ||
          layout_manager_->GetPowerState() != new_power_state_ ||
          force_configure_) {
        new_display_state_ = MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
        EnterState(base::Bind(
            &UpdateDisplayConfigurationTask::OnEnableSoftwareMirroring,
            weak_ptr_factory_.GetWeakPtr()));
        return;
      }

      success = layout_manager_->GetDisplayState() ==
                MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
      enable_software_mirroring = success;
      if (success)
        new_display_state_ = MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
    }

    layout_manager_->GetSoftwareMirroringController()->SetSoftwareMirroring(
        enable_software_mirroring);
  }

  FinishConfiguration(success);
}

void UpdateDisplayConfigurationTask::OnEnableSoftwareMirroring(
    ConfigureDisplaysTask::Status status) {
  bool success = status != ConfigureDisplaysTask::ERROR;
  layout_manager_->GetSoftwareMirroringController()->SetSoftwareMirroring(
      success);
  FinishConfiguration(success);
}

void UpdateDisplayConfigurationTask::FinishConfiguration(bool success) {
  if (success && force_dpms_)
    delegate_->ForceDPMSOn();

  callback_.Run(success, cached_displays_, framebuffer_size_,
                new_display_state_, new_power_state_);
}

bool UpdateDisplayConfigurationTask::ShouldForceDpms() const {
  return new_power_state_ != chromeos::DISPLAY_POWER_ALL_OFF &&
         (layout_manager_->GetPowerState() != new_power_state_ ||
          (power_flags_ & DisplayConfigurator::kSetDisplayPowerForceProbe));
}

bool UpdateDisplayConfigurationTask::ShouldConfigure() const {
  if (force_configure_)
    return true;

  if (cached_displays_.size() == 1 &&
      cached_displays_[0]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL)
    return true;

  if (!(power_flags_ &
        DisplayConfigurator::kSetDisplayPowerOnlyIfSingleInternalDisplay))
    return true;

  if (new_display_state_ != layout_manager_->GetDisplayState())
    return true;

  return false;
}

MultipleDisplayState UpdateDisplayConfigurationTask::ChooseDisplayState()
    const {
  int num_on_displays =
      GetDisplayPower(cached_displays_, new_power_state_, NULL);
  switch (cached_displays_.size()) {
    case 0:
      return MULTIPLE_DISPLAY_STATE_HEADLESS;
    case 1:
      return MULTIPLE_DISPLAY_STATE_SINGLE;
    default: {
      if (num_on_displays == 1) {
        // If only one display is currently turned on, return the "single"
        // state so that its native mode will be used.
        return MULTIPLE_DISPLAY_STATE_SINGLE;
      }
      if (num_on_displays >= 3) {
        return MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED;
      } else if (cached_displays_.size() == 2) {
        if (!layout_manager_->GetStateController())
          return MULTIPLE_DISPLAY_STATE_DUAL_EXTENDED;
        // With either both displays on or both displays off, use one of the
        // dual modes.
        std::vector<int64_t> display_ids;
        for (size_t i = 0; i < cached_displays_.size(); ++i)
          display_ids.push_back(cached_displays_[i]->display_id());

        return layout_manager_->GetStateController()->GetStateForDisplayIds(
            display_ids);
      }
      NOTREACHED();
    }
  }
  return MULTIPLE_DISPLAY_STATE_INVALID;
}

}  // namespace ui
