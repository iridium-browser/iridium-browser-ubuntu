// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_

#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/view_click_listener.h"

namespace ash {
// The detailed view for showing IME list.
class ImeListView : public TrayDetailsView, public ViewClickListener {
 public:
  enum SingleImeBehavior {
    // Shows the IME menu if there's only one IME in system.
    SHOW_SINGLE_IME,
    // Hides the IME menu if there's only one IME in system.
    HIDE_SINGLE_IME
  };

  ImeListView(SystemTrayItem* owner,
              bool show_keyboard_toggle,
              SingleImeBehavior single_ime_behavior);

  ~ImeListView() override;

  // Updates the view.
  virtual void Update(const IMEInfoList& list,
                      const IMEPropertyInfoList& property_list,
                      bool show_keyboard_toggle,
                      SingleImeBehavior single_ime_behavior);

 protected:
  // ViewClickListener:
  void OnViewClicked(views::View* sender) override;

 private:
  // To allow the test class to access |ime_map_|.
  friend class ImeMenuTrayTest;

  // Appends the IMEs to the scrollable area of the detailed view.
  void AppendIMEList(const IMEInfoList& list);

  // Appends the IME listed to the scrollable area of the detailed view.
  void AppendIMEProperties(const IMEPropertyInfoList& property_list);

  // Appends the on-screen keyboard status to the last area of the detailed
  // view.
  void AppendKeyboardStatus();

  std::map<views::View*, std::string> ime_map_;
  std::map<views::View*, std::string> property_map_;
  views::View* keyboard_status_;

  DISALLOW_COPY_AND_ASSIGN(ImeListView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_
