// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_

#include <map>

#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/view_click_listener.h"
#include "base/macros.h"
#include "chromeos/audio/audio_device.h"
#include "ui/gfx/font.h"

namespace views {
class View;
}

namespace ash {
class HoverHighlightView;

namespace tray {

class AudioDetailedView : public TrayDetailsView, public ViewClickListener {
 public:
  explicit AudioDetailedView(SystemTrayItem* owner);

  ~AudioDetailedView() override;

  void Update();

 private:
  void AddScrollListInfoItem(const base::string16& text);

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        bool highlight,
                                        bool checked);

  void CreateHeaderEntry();
  void CreateItems();

  void UpdateScrollableList();
  void UpdateAudioDevices();

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override;

  typedef std::map<views::View*, chromeos::AudioDevice> AudioDeviceMap;

  chromeos::AudioDeviceList output_devices_;
  chromeos::AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
