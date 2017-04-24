// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/bluetooth/tray_bluetooth.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/throbber_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "device/bluetooth/bluetooth_common.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace tray {
namespace {

// Updates bluetooth device |device| in the |list|. If it is new, append to the
// end of the |list|; otherwise, keep it at the same place, but update the data
// with new device info provided by |device|.
void UpdateBluetoothDeviceListHelper(BluetoothDeviceList* list,
                                     const BluetoothDeviceInfo& device) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if ((*it).address == device.address) {
      *it = device;
      return;
    }
  }

  list->push_back(device);
}

// Removes the obsolete BluetoothDevices from |list|, if they are not in the
// |new_list|.
void RemoveObsoleteBluetoothDevicesFromList(
    BluetoothDeviceList* list,
    const std::set<std::string>& new_list) {
  for (BluetoothDeviceList::iterator it = list->begin(); it != list->end();
       ++it) {
    if (new_list.find((*it).address) == new_list.end()) {
      it = list->erase(it);
      if (it == list->end())
        return;
    }
  }
}

// Returns corresponding device type icons for given Bluetooth device types and
// connection states.
const gfx::VectorIcon& GetBluetoothDeviceIcon(
    device::BluetoothDeviceType device_type,
    bool connected) {
  switch (device_type) {
    case device::BluetoothDeviceType::COMPUTER:
      return ash::kSystemMenuComputerIcon;
    case device::BluetoothDeviceType::PHONE:
      return ash::kSystemMenuPhoneIcon;
    case device::BluetoothDeviceType::AUDIO:
    case device::BluetoothDeviceType::CAR_AUDIO:
      return ash::kSystemMenuHeadsetIcon;
    case device::BluetoothDeviceType::VIDEO:
      return ash::kSystemMenuVideocamIcon;
    case device::BluetoothDeviceType::JOYSTICK:
    case device::BluetoothDeviceType::GAMEPAD:
      return ash::kSystemMenuGamepadIcon;
    case device::BluetoothDeviceType::KEYBOARD:
    case device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return ash::kSystemMenuKeyboardIcon;
    case device::BluetoothDeviceType::TABLET:
      return ash::kSystemMenuTabletIcon;
    case device::BluetoothDeviceType::MOUSE:
      return ash::kSystemMenuMouseIcon;
    case device::BluetoothDeviceType::MODEM:
    case device::BluetoothDeviceType::PERIPHERAL:
      return ash::kSystemMenuBluetoothIcon;
    case device::BluetoothDeviceType::UNKNOWN:
      LOG(WARNING) << "Unknown device type icon for Bluetooth was requested.";
      break;
  }
  return connected ? ash::kSystemMenuBluetoothConnectedIcon
                   : ash::kSystemMenuBluetoothIcon;
}

const int kDisabledPanelLabelBaselineY = 20;

}  // namespace

class BluetoothDefaultView : public TrayItemMore {
 public:
  explicit BluetoothDefaultView(SystemTrayItem* owner) : TrayItemMore(owner) {}
  ~BluetoothDefaultView() override {}

  void Update() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    const bool enabled = delegate->GetBluetoothEnabled();
    if (delegate->GetBluetoothAvailable()) {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      const base::string16 label = rb.GetLocalizedString(
          enabled ? IDS_ASH_STATUS_TRAY_BLUETOOTH_ENABLED
                  : IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED);
      SetLabel(label);
      SetAccessibleName(label);
      SetVisible(true);
    } else {
      SetVisible(false);
    }
    UpdateStyle();
  }

 protected:
  // TrayItemMore:
  std::unique_ptr<TrayPopupItemStyle> HandleCreateStyle() const override {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    std::unique_ptr<TrayPopupItemStyle> style =
        TrayItemMore::HandleCreateStyle();
    style->set_color_style(
        delegate->GetBluetoothEnabled()
            ? TrayPopupItemStyle::ColorStyle::ACTIVE
            : delegate->GetBluetoothAvailable()
                  ? TrayPopupItemStyle::ColorStyle::INACTIVE
                  : TrayPopupItemStyle::ColorStyle::DISABLED);

    return style;
  }

  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();
    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(gfx::CreateVectorIcon(GetCurrentIcon(), style->GetIconColor()));
  }

 private:
  const gfx::VectorIcon& GetCurrentIcon() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (!delegate->GetBluetoothEnabled())
      return kSystemMenuBluetoothDisabledIcon;

    bool has_connected_device = false;
    BluetoothDeviceList list;
    delegate->GetAvailableBluetoothDevices(&list);
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].connected) {
        has_connected_device = true;
        break;
      }
    }
    return has_connected_device ? kSystemMenuBluetoothConnectedIcon
                                : kSystemMenuBluetoothIcon;
  }

  DISALLOW_COPY_AND_ASSIGN(BluetoothDefaultView);
};

class BluetoothDetailedView : public TrayDetailsView {
 public:
  BluetoothDetailedView(SystemTrayItem* owner, LoginStatus login)
      : TrayDetailsView(owner),
        login_(login),
        toggle_(nullptr),
        settings_(nullptr),
        disabled_panel_(nullptr) {
    CreateItems();
  }

  ~BluetoothDetailedView() override {
    // Stop discovering bluetooth devices when exiting BT detailed view.
    BluetoothStopDiscovering();
  }

  void Update() {
    BluetoothStartDiscovering();
    UpdateBluetoothDeviceList();

    // Update UI.
    UpdateDeviceScrollList();
    UpdateHeaderEntry();
    Layout();
  }

 private:
  void CreateItems() {
    CreateScrollableList();
    CreateTitleRow(IDS_ASH_STATUS_TRAY_BLUETOOTH);
  }

  void BluetoothStartDiscovering() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (delegate->GetBluetoothDiscovering()) {
      ShowLoadingIndicator();
      return;
    }
    HideLoadingIndicator();
    if (delegate->GetBluetoothEnabled())
      delegate->BluetoothStartDiscovering();
  }

  void BluetoothStopDiscovering() {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (delegate && delegate->GetBluetoothDiscovering()) {
      delegate->BluetoothStopDiscovering();
      HideLoadingIndicator();
    }
  }

  void UpdateBluetoothDeviceList() {
    std::set<std::string> new_connecting_devices;
    std::set<std::string> new_connected_devices;
    std::set<std::string> new_paired_not_connected_devices;
    std::set<std::string> new_discovered_not_paired_devices;

    BluetoothDeviceList list;
    WmShell::Get()->system_tray_delegate()->GetAvailableBluetoothDevices(&list);
    for (size_t i = 0; i < list.size(); ++i) {
      if (list[i].connecting) {
        new_connecting_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&connecting_devices_, list[i]);
      } else if (list[i].connected && list[i].paired) {
        new_connected_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&connected_devices_, list[i]);
      } else if (list[i].paired) {
        new_paired_not_connected_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&paired_not_connected_devices_,
                                        list[i]);
      } else {
        new_discovered_not_paired_devices.insert(list[i].address);
        UpdateBluetoothDeviceListHelper(&discovered_not_paired_devices_,
                                        list[i]);
      }
    }
    RemoveObsoleteBluetoothDevicesFromList(&connecting_devices_,
                                           new_connecting_devices);
    RemoveObsoleteBluetoothDevicesFromList(&connected_devices_,
                                           new_connected_devices);
    RemoveObsoleteBluetoothDevicesFromList(&paired_not_connected_devices_,
                                           new_paired_not_connected_devices);
    RemoveObsoleteBluetoothDevicesFromList(&discovered_not_paired_devices_,
                                           new_discovered_not_paired_devices);
  }

  void UpdateHeaderEntry() {
    bool is_bluetooth_enabled =
        WmShell::Get()->system_tray_delegate()->GetBluetoothEnabled();
    if (toggle_)
      toggle_->SetIsOn(is_bluetooth_enabled, true);
  }

  void UpdateDeviceScrollList() {
    device_map_.clear();
    scroll_content()->RemoveAllChildViews(true);

    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    bool bluetooth_enabled = delegate->GetBluetoothEnabled();
    bool bluetooth_available = delegate->GetBluetoothAvailable();

    // If Bluetooth is disabled, show a panel which only indicates that it is
    // disabled, instead of the scroller with Bluetooth devices.
    if (bluetooth_enabled) {
      HideDisabledPanel();
    } else {
      ShowDisabledPanel();
      return;
    }

    // Add paired devices (and their section header in MD) in the list.
    size_t num_paired_devices = connected_devices_.size() +
                                connecting_devices_.size() +
                                paired_not_connected_devices_.size();
    if (num_paired_devices > 0) {
      AddSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED_DEVICES);
      AppendSameTypeDevicesToScrollList(connected_devices_, true, true,
                                        bluetooth_enabled);
      AppendSameTypeDevicesToScrollList(connecting_devices_, true, false,
                                        bluetooth_enabled);
      AppendSameTypeDevicesToScrollList(paired_not_connected_devices_, false,
                                        false, bluetooth_enabled);
    }

    // Add paired devices (and their section header in MD) in the list.
    if (discovered_not_paired_devices_.size() > 0) {
      if (num_paired_devices > 0)
        AddSubHeader(IDS_ASH_STATUS_TRAY_BLUETOOTH_UNPAIRED_DEVICES);
      AppendSameTypeDevicesToScrollList(discovered_not_paired_devices_, false,
                                        false, bluetooth_enabled);
    }

    // Show user Bluetooth state if there is no bluetooth devices in list.
    if (device_map_.size() == 0) {
      if (bluetooth_available && bluetooth_enabled) {
        HoverHighlightView* container = new HoverHighlightView(this);
        container->AddLabel(l10n_util::GetStringUTF16(
                                IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERING),
                            gfx::ALIGN_LEFT, false);
        scroll_content()->AddChildView(container);
      }
    }

    scroll_content()->InvalidateLayout();
  }

  void AppendSameTypeDevicesToScrollList(const BluetoothDeviceList& list,
                                         bool highlight,
                                         bool checked,
                                         bool enabled) {
    for (size_t i = 0; i < list.size(); ++i) {
      HoverHighlightView* container = nullptr;
      gfx::ImageSkia icon_image = CreateVectorIcon(
          GetBluetoothDeviceIcon(list[i].device_type, list[i].connected),
          kMenuIconColor);
      container = AddScrollListItem(list[i].display_name, icon_image,
                                    list[i].connected, list[i].connecting);
      device_map_[container] = list[i].address;
    }
  }

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        const gfx::ImageSkia& image,
                                        bool connected,
                                        bool connecting) {
    HoverHighlightView* container = new HoverHighlightView(this);
    if (connected) {
      SetupConnectedItem(container, text, image);
    } else if (connecting) {
      SetupConnectingItem(container, text, image);
    } else {
      container->AddIconAndLabel(image, text, false);
    }
    scroll_content()->AddChildView(container);
    return container;
  }

  void AddSubHeader(int message_id) {
    TriView* header = TrayPopupUtils::CreateSubHeaderRowView();
    TrayPopupUtils::ConfigureAsStickyHeader(header);

    views::Label* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(l10n_util::GetStringUTF16(message_id));
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
    style.SetupLabel(label);
    header->AddView(TriView::Container::CENTER, label);

    scroll_content()->AddChildView(header);
  }

  void SetupConnectedItem(HoverHighlightView* container,
                          const base::string16& text,
                          const gfx::ImageSkia& image) {
    container->AddIconAndLabels(
        image, text, l10n_util::GetStringUTF16(
                         IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
    TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::CAPTION);
    style.set_color_style(TrayPopupItemStyle::ColorStyle::CONNECTED);
    style.SetupLabel(container->sub_text_label());
  }

  void SetupConnectingItem(HoverHighlightView* container,
                           const base::string16& text,
                           const gfx::ImageSkia& image) {
    container->AddIconAndLabels(
        image, text, l10n_util::GetStringUTF16(
                         IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING));
    ThrobberView* throbber = new ThrobberView;
    throbber->Start();
    container->AddRightView(throbber);
  }

  // Returns true if the device with |device_id| is found in |device_list|,
  // and the display_name of the device will be returned in |display_name| if
  // it's not NULL.
  bool FoundDevice(const std::string& device_id,
                   const BluetoothDeviceList& device_list,
                   base::string16* display_name,
                   device::BluetoothDeviceType* device_type) {
    for (size_t i = 0; i < device_list.size(); ++i) {
      if (device_list[i].address == device_id) {
        if (display_name)
          *display_name = device_list[i].display_name;
        if (device_type)
          *device_type = device_list[i].device_type;
        return true;
      }
    }
    return false;
  }

  // Updates UI of the clicked bluetooth device to show it is being connected
  // or disconnected if such an operation is going to be performed underway.
  void UpdateClickedDevice(const std::string& device_id,
                           views::View* item_container) {
    base::string16 display_name;
    device::BluetoothDeviceType device_type;
    if (FoundDevice(device_id, paired_not_connected_devices_, &display_name,
                    &device_type)) {
      item_container->RemoveAllChildViews(true);
      HoverHighlightView* container =
          static_cast<HoverHighlightView*>(item_container);
      TrayPopupItemStyle style(
          TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
      gfx::ImageSkia icon_image = CreateVectorIcon(
          GetBluetoothDeviceIcon(device_type, false), style.GetIconColor());
      SetupConnectingItem(container, display_name, icon_image);
      scroll_content()->SizeToPreferredSize();
      scroller()->Layout();
    }
  }

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    if (!delegate->GetBluetoothEnabled())
      return;

    std::map<views::View*, std::string>::iterator find;
    find = device_map_.find(view);
    if (find == device_map_.end())
      return;

    const std::string device_id = find->second;
    if (FoundDevice(device_id, connecting_devices_, nullptr, nullptr))
      return;

    UpdateClickedDevice(device_id, view);
    delegate->ConnectToBluetoothDevice(device_id);
  }

  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override {
    if (sender == toggle_) {
      SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
      WmShell::Get()->RecordUserMetricsAction(
          delegate->GetBluetoothEnabled() ? UMA_STATUS_AREA_BLUETOOTH_DISABLED
                                          : UMA_STATUS_AREA_BLUETOOTH_ENABLED);
      delegate->ToggleBluetooth();
    } else if (sender == settings_) {
      ShowSettings();
    } else {
      NOTREACHED();
    }
  }

  void CreateExtraTitleRowButtons() override {
    if (login_ == LoginStatus::LOCKED)
      return;

    DCHECK(!toggle_);
    DCHECK(!settings_);

    tri_view()->SetContainerVisible(TriView::Container::END, true);

    toggle_ =
        TrayPopupUtils::CreateToggleButton(this, IDS_ASH_STATUS_TRAY_BLUETOOTH);
    tri_view()->AddView(TriView::Container::END, toggle_);

    settings_ =
        CreateSettingsButton(login_, IDS_ASH_STATUS_TRAY_BLUETOOTH_SETTINGS);
    tri_view()->AddView(TriView::Container::END, settings_);
  }

  void ShowSettings() {
    if (TrayPopupUtils::CanOpenWebUISettings(login_)) {
      WmShell::Get()->system_tray_delegate()->ManageBluetoothDevices();
      owner()->system_tray()->CloseSystemBubble();
    }
  }

  void ShowLoadingIndicator() {
    // Setting a value of -1 gives progress_bar an infinite-loading behavior.
    ShowProgress(-1, true);
  }

  void HideLoadingIndicator() { ShowProgress(0, false); }

  void ShowDisabledPanel() {
    DCHECK(scroller());
    if (!disabled_panel_) {
      disabled_panel_ = CreateDisabledPanel();
      // Insert |disabled_panel_| before the scroller, since the scroller will
      // have unnecessary bottom border when it is not the last child.
      AddChildViewAt(disabled_panel_, GetIndexOf(scroller()));
      // |disabled_panel_| need to fill the remaining space below the title row
      // so that the inner contents of |disabled_panel_| are placed properly.
      box_layout()->SetFlexForView(disabled_panel_, 1);
    }
    disabled_panel_->SetVisible(true);
    scroller()->SetVisible(false);
  }

  void HideDisabledPanel() {
    DCHECK(scroller());
    if (disabled_panel_)
      disabled_panel_->SetVisible(false);
    scroller()->SetVisible(true);
  }

  views::View* CreateDisabledPanel() {
    views::View* container = new views::View;
    views::BoxLayout* box_layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    container->SetLayoutManager(box_layout);

    TrayPopupItemStyle style(
        TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.set_color_style(TrayPopupItemStyle::ColorStyle::DISABLED);

    views::ImageView* image_view = new views::ImageView;
    image_view->SetImage(gfx::CreateVectorIcon(kSystemMenuBluetoothDisabledIcon,
                                               style.GetIconColor()));
    image_view->SetVerticalAlignment(views::ImageView::TRAILING);
    container->AddChildView(image_view);

    views::Label* label = new views::Label(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED));
    style.SetupLabel(label);
    label->SetBorder(views::CreateEmptyBorder(
        kDisabledPanelLabelBaselineY - label->GetBaseline(), 0, 0, 0));
    container->AddChildView(label);

    // Make top padding of the icon equal to the height of the label so that the
    // icon is vertically aligned to center of the container.
    image_view->SetBorder(
        views::CreateEmptyBorder(label->GetPreferredSize().height(), 0, 0, 0));
    return container;
  }

  LoginStatus login_;

  std::map<views::View*, std::string> device_map_;

  BluetoothDeviceList connected_devices_;
  BluetoothDeviceList connecting_devices_;
  BluetoothDeviceList paired_not_connected_devices_;
  BluetoothDeviceList discovered_not_paired_devices_;

  views::ToggleButton* toggle_;
  views::Button* settings_;

  // The container of the message "Bluetooth is disabled" and an icon. It should
  // be shown instead of Bluetooth device list when Bluetooth is disabled.
  views::View* disabled_panel_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetailedView);
};

}  // namespace tray

TrayBluetooth::TrayBluetooth(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_BLUETOOTH),
      default_(nullptr),
      detailed_(nullptr) {
  WmShell::Get()->system_tray_notifier()->AddBluetoothObserver(this);
}

TrayBluetooth::~TrayBluetooth() {
  WmShell::Get()->system_tray_notifier()->RemoveBluetoothObserver(this);
}

views::View* TrayBluetooth::CreateTrayView(LoginStatus status) {
  return NULL;
}

views::View* TrayBluetooth::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == NULL);
  default_ = new tray::BluetoothDefaultView(this);
  default_->SetEnabled(status != LoginStatus::LOCKED);
  default_->Update();
  return default_;
}

views::View* TrayBluetooth::CreateDetailedView(LoginStatus status) {
  if (!WmShell::Get()->system_tray_delegate()->GetBluetoothAvailable())
    return NULL;
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW);
  CHECK(detailed_ == NULL);
  detailed_ = new tray::BluetoothDetailedView(this, status);
  detailed_->Update();
  return detailed_;
}

void TrayBluetooth::DestroyTrayView() {}

void TrayBluetooth::DestroyDefaultView() {
  default_ = NULL;
}

void TrayBluetooth::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayBluetooth::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayBluetooth::OnBluetoothRefresh() {
  if (default_)
    default_->Update();
  else if (detailed_)
    detailed_->Update();
}

void TrayBluetooth::OnBluetoothDiscoveringChanged() {
  if (!detailed_)
    return;
  detailed_->Update();
}

}  // namespace ash
