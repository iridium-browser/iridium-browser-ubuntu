// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/printer_service_provider.h"

#include <stdint.h>

#include <limits>

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "device/usb/usb_ids.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kPrinterAdded[] = "PrinterAdded";

const char kPrinterProviderFoundNotificationID[] =
    "chrome://settings/printer/printer_app_found";

const char kNoPrinterProviderNotificationID[] =
    "chrome://settings/printer/no_printer_app";

enum PrinterServiceEvent {
  PRINTER_ADDED,
  PAGE_DISPLAYED,
  PRINTER_SERVICE_EVENT_MAX,
};

// TODO(vitalybuka): update URL with more relevant information.
const char kCloudPrintLearnUrl[] =
    "https://www.google.com/landing/cloudprint/index.html";

void ActivateContents(Browser* browser, content::WebContents* contents) {
  browser->tab_strip_model()->ActivateTabAt(
      browser->tab_strip_model()->GetIndexOfWebContents(contents), false);
}

Browser* ActivateAndGetBrowserForUrl(GURL url) {
  for (TabContentsIterator it; !it.done(); it.Next()) {
    if (it->GetLastCommittedURL() == url) {
      ActivateContents(it.browser(), *it);
      return it.browser();
    }
  }
  return nullptr;
}

bool HexStringToUInt16(const std::string& input, uint16* output) {
  uint32 output_uint = 0;
  if (!base::HexStringToUInt(input, &output_uint) ||
      output_uint > std::numeric_limits<uint16>::max())
    return false;
  *output = static_cast<uint16>(output_uint);
  return true;
}

void FindOrOpenCloudPrintPage(const std::string& /* vendor */,
                              const std::string& /* product */) {
  UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent", PRINTER_ADDED,
                            PRINTER_SERVICE_EVENT_MAX);
  if (!ash::Shell::GetInstance()->session_state_delegate()->
          IsActiveUserSessionStarted() ||
      ash::Shell::GetInstance()->session_state_delegate()->IsScreenLocked()) {
    return;
  }

  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;

  GURL url(kCloudPrintLearnUrl);

  if (!ActivateAndGetBrowserForUrl(url)) {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        profile, chrome::HOST_DESKTOP_TYPE_ASH);
    UMA_HISTOGRAM_ENUMERATION("PrinterService.PrinterServiceEvent",
                              PAGE_DISPLAYED, PRINTER_SERVICE_EVENT_MAX);
    chrome::AddSelectedTabWithURL(displayer.browser(), url,
                                  ui::PAGE_TRANSITION_LINK);
  }
}

base::string16 GetNotificationTitle(uint16 vendor_id, uint16 product_id) {
  const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
  if (vendor_name) {
    return l10n_util::GetStringFUTF16(IDS_PRINTER_DETECTED_NOTIFICATION_TITLE,
                                      base::UTF8ToUTF16(vendor_name));
  } else {
    return l10n_util::GetStringUTF16(
        IDS_PRINTER_DETECTED_NOTIFICATION_TITLE_UNKNOWN_VENDOR);
  }
}

std::string GetNotificationTag(const std::string& vendor_id,
                               const std::string& product_id) {
  return vendor_id + ":" + product_id;
}

// Checks if there is an enabled extension with printerProvider permission and
// usbDevices persmission for the USB (vendor_id, product_id) pair.
bool HasAppThatSupportsPrinter(Profile* profile,
                               uint16 vendor_id,
                               uint16 product_id) {
  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (const auto& extension : enabled_extensions) {
    if (!extension->permissions_data() ||
        !extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kPrinterProvider) ||
        !extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kUsb)) {
      continue;
    }

    extensions::UsbDevicePermission::CheckParam param(
        vendor_id, product_id,
        extensions::UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
    if (extension->permissions_data()->CheckAPIPermissionWithParam(
            extensions::APIPermission::kUsbDevice, &param)) {
      return true;
    }
  }
  return false;
}

// Delegate for notification shown when a printer provider app for the plugged
// in printer is found.
class PrinterProviderExistsNotificationDelegate : public NotificationDelegate {
 public:
  PrinterProviderExistsNotificationDelegate(const std::string& vendor_id,
                                            const std::string& product_id)
      : vendor_id_(vendor_id), product_id_(product_id) {}

  std::string id() const override {
    return "system.printer.printer_provider_exists/" +
           GetNotificationTag(vendor_id_, product_id_);
  }

 private:
  ~PrinterProviderExistsNotificationDelegate() override = default;

  const std::string vendor_id_;
  const std::string product_id_;

  DISALLOW_COPY_AND_ASSIGN(PrinterProviderExistsNotificationDelegate);
};

// Delegate for notification shown when there are no printer provider apps that
// support the plugged in printer found.
// The notification is clickable, and clicking it is supposed to launch
// Chrome Web Store widget listing apps that can support the plugged in printer.
// (not implemented yet).
class SearchPrinterAppNotificationDelegate : public NotificationDelegate {
 public:
  SearchPrinterAppNotificationDelegate(const std::string& vendor_id,
                                       const std::string& product_id)
      : vendor_id_(vendor_id), product_id_(product_id) {}

  std::string id() const override {
    return "system.printer.no_printer_provider_found/" +
           GetNotificationTag(vendor_id_, product_id_);
  }
  bool HasClickedListener() override { return true; }

  void Click() override {
    // TODO(tbarzic): Implement this (http://crbug.com/439448).
  }

 private:
  ~SearchPrinterAppNotificationDelegate() override = default;

  std::string vendor_id_;
  std::string product_id_;

  DISALLOW_COPY_AND_ASSIGN(SearchPrinterAppNotificationDelegate);
};

// Shows a notification for a plugged in printer.
// If there is a printerProvider app that handles the printer's USB (vendor_id,
// product_id) pair, the notification informs the user that the printer is ready
// to be used, otherwise it offers the user to search the Chrome Web Store for
// an app that can handle the printer.
void ShowPrinterPluggedNotification(
    NotificationUIManager* notification_ui_manager,
    const std::string& vendor_id_str,
    const std::string& product_id_str) {
  uint16 vendor_id = 0;
  uint16 product_id = 0;
  if (!HexStringToUInt16(vendor_id_str, &vendor_id) ||
      !HexStringToUInt16(product_id_str, &product_id)) {
    LOG(WARNING) << "Invalid USB ID " << vendor_id_str << ":" << product_id_str;
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()
          ? user_manager::UserManager::Get()->GetActiveUser()
          : nullptr;
  if (!user || !user->HasGaiaAccount())
    return;

  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_ptr<Notification> notification;

  if (HasAppThatSupportsPrinter(profile, vendor_id, product_id)) {
    notification.reset(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GURL(kPrinterProviderFoundNotificationID),
        GetNotificationTitle(vendor_id, product_id),
        l10n_util::GetStringUTF16(
            IDS_PRINTER_DETECTED_NOTIFICATION_PRINT_APP_FOUND_BODY),
        bundle.GetImageNamed(IDR_PRINTER_NOTIFICATION),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   kPrinterProviderFoundNotificationID),
        base::string16(), GetNotificationTag(vendor_id_str, product_id_str),
        message_center::RichNotificationData(),
        new PrinterProviderExistsNotificationDelegate(vendor_id_str,
                                                      product_id_str)));
  } else {
    message_center::RichNotificationData options;
    options.clickable = true;
    notification.reset(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        GURL(kNoPrinterProviderNotificationID),
        GetNotificationTitle(vendor_id, product_id),
        l10n_util::GetStringUTF16(
            IDS_PRINTER_DETECTED_NOTIFICATION_NO_PRINT_APP_BODY),
        bundle.GetImageNamed(IDR_PRINTER_NOTIFICATION),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   kNoPrinterProviderNotificationID),
        base::string16(), GetNotificationTag(vendor_id_str, product_id_str),
        options, new SearchPrinterAppNotificationDelegate(vendor_id_str,
                                                          product_id_str)));
  }

  notification->SetSystemPriority();
  notification_ui_manager->Add(*notification, profile);
}

}  // namespace

namespace chromeos {

PrinterServiceProvider::PrinterServiceProvider()
    : notification_ui_manager_(nullptr), weak_ptr_factory_(this) {
}

PrinterServiceProvider::~PrinterServiceProvider() {
}

void PrinterServiceProvider::Start(
    scoped_refptr<dbus::ExportedObject> exported_object) {
  exported_object_ = exported_object;

  DVLOG(1) << "PrinterServiceProvider started";
  exported_object_->ExportMethod(
      kLibCrosServiceInterface,
      kPrinterAdded,
      base::Bind(&PrinterServiceProvider::PrinterAdded,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&PrinterServiceProvider::OnExported,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PrinterServiceProvider::SetNotificationUIManagerForTesting(
    NotificationUIManager* manager) {
  notification_ui_manager_ = manager;
}

void PrinterServiceProvider::OnExported(
    const std::string& interface_name,
    const std::string& method_name,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to export " << interface_name << "."
               << method_name;
  }
  DVLOG(1) << "Method exported: " << interface_name << "." << method_name;
}

void PrinterServiceProvider::ShowCloudPrintHelp(const std::string& vendor,
                                                const std::string& product) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(&FindOrOpenCloudPrintPage, vendor,
                                              product));
}

void PrinterServiceProvider::PrinterAdded(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(1) << "PrinterAdded " << method_call->ToString();

  dbus::MessageReader reader(method_call);

  std::string vendor_id;
  reader.PopString(&vendor_id);
  StringToUpperASCII(&vendor_id);

  std::string product_id;
  reader.PopString(&product_id);
  StringToUpperASCII(&product_id);

  // Send an empty response.
  response_sender.Run(dbus::Response::FromMethodCall(method_call));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePrinterAppSearch)) {
    ShowPrinterPluggedNotification(
        notification_ui_manager_ ? notification_ui_manager_
                                 : g_browser_process->notification_ui_manager(),
        vendor_id, product_id);
    return;
  }

  // Disable showing Cloudprint help on canary and dev channel, as these have
  // support for printerProvider API.
  // TODO(tbarzic): Remove this and offer the user to search for an extension
  // that can act as a print driver (using printerProvider API) for USB printers
  // detected by this service. http://crbug.com/439448
  if (base::SysInfo::IsRunningOnChromeOS() &&
      chrome::VersionInfo::GetChannel() <= chrome::VersionInfo::CHANNEL_DEV)
    return;

  ShowCloudPrintHelp(vendor_id, product_id);
}

}  // namespace chromeos

