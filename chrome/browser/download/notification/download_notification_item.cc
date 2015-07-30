// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"

namespace {

const char kDownloadNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

}  // anonymous namespace

// static
const char DownloadNotificationItem::kDownloadNotificationOrigin[] =
    "chrome://downloads";

// static
StubNotificationUIManager*
    DownloadNotificationItem::stub_notification_ui_manager_for_testing_ =
        nullptr;

DownloadNotificationItem::NotificationWatcher::NotificationWatcher(
    DownloadNotificationItem* item)
    : item_(item) {
}

DownloadNotificationItem::NotificationWatcher::~NotificationWatcher() {
}

void DownloadNotificationItem::NotificationWatcher::Close(bool by_user) {
  // Do nothing.
}

void DownloadNotificationItem::NotificationWatcher::Click() {
  item_->OnNotificationClick();
}

bool DownloadNotificationItem::NotificationWatcher::HasClickedListener() {
  return true;
}

void DownloadNotificationItem::NotificationWatcher::ButtonClick(
    int button_index) {
  item_->OnNotificationButtonClick(button_index);
}

std::string DownloadNotificationItem::NotificationWatcher::id() const {
  return base::UintToString(item_->item_->GetId());
}

DownloadNotificationItem::DownloadNotificationItem(content::DownloadItem* item,
                                                   Profile* profile,
                                                   Delegate* delegate)
    : profile_(profile),
      watcher_(new NotificationWatcher(this)),
      item_(item),
      delegate_(delegate) {
  item->AddObserver(this);

  // Notify that the instance is just created.
  delegate_->OnCreated(this);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  message_center::RichNotificationData data;
  // Creates the notification instance. |title| and |body| will be overridden
  // by UpdateNotificationData() below.
  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_PROGRESS,
      GURL(kDownloadNotificationOrigin),  // origin_url
      base::string16(),                   // title
      base::string16(),                   // body
      bundle.GetImageNamed(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kDownloadNotificationNotifierId),
      base::string16(),                    // display_source
      base::UintToString(item_->GetId()),  // tag
      data, watcher_.get()));

  notification_->set_progress(0);
  notification_->set_never_timeout(false);

  UpdateNotificationData(ADD_NEW);
}

DownloadNotificationItem::~DownloadNotificationItem() {
  if (item_)
    item_->RemoveObserver(this);
}

void DownloadNotificationItem::OnNotificationClick() {
  if (openable_) {
    if (item_->IsDone()) {
      item_->OpenDownload();
      CloseNotificationByUser();
    } else {
      item_->SetOpenWhenComplete(!item_->GetOpenWhenComplete());  // Toggle
    }
  } else if (item_->GetState() == content::DownloadItem::INTERRUPTED ||
             item_->GetState() == content::DownloadItem::CANCELLED) {
    GetBrowser()->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUIDownloadsURL), content::Referrer(),
        NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
        false /* is_renderer_initiated */));
    CloseNotificationByUser();
  } else if (item_->IsDone()) {
    CloseNotificationByUser();
  }
}

void DownloadNotificationItem::OnNotificationButtonClick(int button_index) {
  if (button_index < 0 ||
      static_cast<size_t>(button_index) >= button_actions_->size()) {
    // Out of boundary.
    NOTREACHED();
    return;
  }

  DownloadCommands::Command command = button_actions_->at(button_index);
  if (command != DownloadCommands::PAUSE &&
      command != DownloadCommands::RESUME) {
    CloseNotificationByUser();
  }

  DownloadCommands(item_).ExecuteCommand(command);

  // Shows the notification again after clicking "Keep" on dangerous download.
  if (command == DownloadCommands::KEEP)
    UpdateNotificationData(ADD_NEW);
}

// DownloadItem::Observer methods
void DownloadNotificationItem::OnDownloadUpdated(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  UpdateNotificationData(UPDATE_EXISTING);
}

void DownloadNotificationItem::CloseNotificationByNonUser() {
  const std::string& notification_id = watcher_->id();
  const ProfileID profile_id = NotificationUIManager::GetProfileID(profile_);

  notification_ui_manager()->CancelById(notification_id, profile_id);
}

void DownloadNotificationItem::CloseNotificationByUser() {
  const std::string& notification_id = watcher_->id();
  const ProfileID profile_id = NotificationUIManager::GetProfileID(profile_);
  const std::string notification_id_in_message_center =
      ProfileNotification::GetProfileNotificationId(notification_id,
                                                    profile_id);

  notification_ui_manager()->CancelById(notification_id, profile_id);

  // When the message center is visible, |NotificationUIManager::CancelByID()|
  // delays the close hence the notification is not closed at this time. But
  // from the viewpoint of UX of MessageCenter, we should close it immediately
  // because it's by user action. So, we request closing of it directlly to
  // MessageCenter instance.
  // Note that: this calling has no side-effect even when the message center
  // is not opened.
  g_browser_process->message_center()->RemoveNotification(
      notification_id_in_message_center, true /* by_user */);
}

void DownloadNotificationItem::UpdateNotificationData(
    NotificationUpdateType type) {
  DownloadItemModel model(item_);
  DownloadCommands command(item_);

  if (previous_download_state_ != content::DownloadItem::IN_PROGRESS) {
    if (item_->GetState() == content::DownloadItem::IN_PROGRESS)
      delegate_->OnDownloadStarted(this);
  } else {
    if (item_->GetState() != content::DownloadItem::IN_PROGRESS)
      delegate_->OnDownloadStopped(this);
  }

  if (item_->IsDangerous()) {
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    notification_->set_title(GetTitle());
    notification_->set_message(GetWarningText());

    // Show icon.
    SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_MALICIOUS);
  } else {
    notification_->set_title(GetTitle());
    notification_->set_message(model.GetStatusText());

    bool is_off_the_record = item_->GetBrowserContext() &&
                             item_->GetBrowserContext()->IsOffTheRecord();

    switch (item_->GetState()) {
      case content::DownloadItem::IN_PROGRESS:
        notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        notification_->set_progress(item_->PercentComplete());
        if (is_off_the_record) {
          // TODO(yoshiki): Replace the tentative image.
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }
        break;
      case content::DownloadItem::COMPLETE:
        DCHECK(item_->IsDone());

        // Shows a notifiation as progress type once so the visible content will
        // be updated.
        // Note: only progress-type notification's content will be updated
        // immediately when the message center is visible.
        notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        notification_->set_progress(100);

        if (is_off_the_record) {
          // TODO(yoshiki): Replace the tentative image.
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }
        break;
      case content::DownloadItem::CANCELLED:
        // Confgirms that a download is cancelled by user action.
        DCHECK(item_->GetLastReason() ==
                   content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED ||
               item_->GetLastReason() ==
                   content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN);

        CloseNotificationByUser();

        previous_download_state_ = item_->GetState();
        return;  // Skips the remaining since the notification has closed.
      case content::DownloadItem::INTERRUPTED:
        // Shows a notifiation as progress type once so the visible content will
        // be updated. (same as the case of type = COMPLETE)
        notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        notification_->set_progress(0);
        SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_WARNING);
        break;
      case content::DownloadItem::MAX_DOWNLOAD_STATE:  // sentinel
        NOTREACHED();
    }
  }

  std::vector<message_center::ButtonInfo> notification_actions;
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      GetPossibleActions().Pass());

  openable_ = false;
  button_actions_.reset(new std::vector<DownloadCommands::Command>);
  for (auto it = actions->begin(); it != actions->end(); it++) {
    if (*it == DownloadCommands::OPEN_WHEN_COMPLETE) {
      openable_ = true;
    } else {
      button_actions_->push_back(*it);
      message_center::ButtonInfo button_info =
          message_center::ButtonInfo(GetCommandLabel(*it));
      button_info.icon = command.GetCommandIcon(*it);
      notification_actions.push_back(button_info);
    }
  }
  notification_->set_buttons(notification_actions);

  if (item_->IsDone()) {
    // TODO(yoshiki): If the downloaded file is an image, show the thumbnail.
  }

  if (type == ADD_NEW) {
    notification_ui_manager()->Add(*notification_, profile_);
  } else if (type == UPDATE_EXISTING) {
    notification_ui_manager()->Update(*notification_, profile_);

    // When the download is just completed (or interrupted), close the
    // notification once and re-show it immediately so it'll pop up.
    if ((item_->GetState() == content::DownloadItem::COMPLETE &&
         previous_download_state_ != content::DownloadItem::COMPLETE) ||
        (item_->GetState() == content::DownloadItem::INTERRUPTED &&
         previous_download_state_ != content::DownloadItem::INTERRUPTED)) {
      CloseNotificationByNonUser();
      // Changes the type from PROGRESS to SIMPLE.
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      notification_ui_manager()->Add(*notification_, profile_);
    }
  } else {
    NOTREACHED();
  }

  previous_download_state_ = item_->GetState();
}

void DownloadNotificationItem::OnDownloadOpened(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);
  // Do nothing.
}

void DownloadNotificationItem::OnDownloadRemoved(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  // Removing the notification causes calling |NotificationDelegate::Close()|.
  notification_ui_manager()->CancelById(
      watcher_->id(), NotificationUIManager::GetProfileID(profile_));
  delegate_->OnDownloadRemoved(this);
}

void DownloadNotificationItem::OnDownloadDestroyed(
    content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  item_ = nullptr;
}

void DownloadNotificationItem::SetNotificationImage(int resource_id) {
  if (image_resource_id_ == resource_id)
    return;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  image_resource_id_ = resource_id;
  notification_->set_icon(bundle.GetImageNamed(image_resource_id_));
}

NotificationUIManager* DownloadNotificationItem::notification_ui_manager()
    const {
  if (stub_notification_ui_manager_for_testing_) {
    return stub_notification_ui_manager_for_testing_;
  }
  return g_browser_process->notification_ui_manager();
}

scoped_ptr<std::vector<DownloadCommands::Command>>
DownloadNotificationItem::GetPossibleActions() const {
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      new std::vector<DownloadCommands::Command>());

  if (item_->IsDangerous()) {
    actions->push_back(DownloadCommands::DISCARD);
    actions->push_back(DownloadCommands::KEEP);
    return actions.Pass();
  }

  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      actions->push_back(DownloadCommands::OPEN_WHEN_COMPLETE);
      if (!item_->IsPaused())
        actions->push_back(DownloadCommands::PAUSE);
      else
        actions->push_back(DownloadCommands::RESUME);
      actions->push_back(DownloadCommands::CANCEL);
      break;
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
      if (item_->CanResume())
        actions->push_back(DownloadCommands::RESUME);
      break;
    case content::DownloadItem::COMPLETE:
      actions->push_back(DownloadCommands::OPEN_WHEN_COMPLETE);
      actions->push_back(DownloadCommands::SHOW_IN_FOLDER);
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
  return actions.Pass();
}

base::string16 DownloadNotificationItem::GetTitle() const {
  base::string16 title_text;
  base::string16 file_name =
      item_->GetFileNameToReportUser().LossyDisplayName();
  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE, file_name);
      break;
    case content::DownloadItem::COMPLETE:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE, file_name);
      break;
    case content::DownloadItem::INTERRUPTED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
      break;
    case content::DownloadItem::CANCELLED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
  return title_text;
}

base::string16 DownloadNotificationItem::GetCommandLabel(
    DownloadCommands::Command command) const {
  int id = -1;
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      if (item_ && !item_->IsDone())
        id = IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE;
      else
        id = IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE;
      break;
    case DownloadCommands::PAUSE:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_PAUSE;
      break;
    case DownloadCommands::RESUME:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_RESUME;
      break;
    case DownloadCommands::SHOW_IN_FOLDER:
      id = IDS_DOWNLOAD_LINK_SHOW;
      break;
    case DownloadCommands::DISCARD:
      id = IDS_DISCARD_DOWNLOAD;
      break;
    case DownloadCommands::KEEP:
      id = IDS_CONFIRM_DOWNLOAD;
      break;
    case DownloadCommands::CANCEL:
      id = IDS_DOWNLOAD_LINK_CANCEL;
      break;
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      // Only for menu.
      NOTREACHED();
      return base::string16();
  }
  CHECK(id != -1);
  return l10n_util::GetStringUTF16(id);
}

base::string16 DownloadNotificationItem::GetWarningText() const {
  // Should only be called if IsDangerous().
  DCHECK(item_->IsDangerous());
  base::string16 elided_filename =
      item_->GetFileNameToReportUser().LossyDisplayName();
  switch (item_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL: {
      return l10n_util::GetStringUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
    }
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
      if (download_crx_util::IsExtensionDownload(*item_)) {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
      } else {
        return l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                                          elided_filename);
      }
    }
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case content::DOWNLOAD_DANGER_TYPE_MAX: {
      break;
    }
  }
  NOTREACHED();
  return base::string16();
}

Browser* DownloadNotificationItem::GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      profile_, chrome::GetActiveDesktop());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}
