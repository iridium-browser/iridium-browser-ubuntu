// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_custom_notification_item.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/arc/notification/arc_custom_notification_view.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/custom_notification_content_view_delegate.h"

namespace arc {

namespace {

constexpr char kNotifierId[] = "ARC_NOTIFICATION";

class ArcNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit ArcNotificationDelegate(ArcCustomNotificationItem* item)
      : item_(item) {
    DCHECK(item_);
  }

  std::unique_ptr<message_center::CustomContent> CreateCustomContent()
      override {
    auto view = base::MakeUnique<ArcCustomNotificationView>(item_);
    auto content_view_delegate = view->CreateContentViewDelegate();
    return base::MakeUnique<message_center::CustomContent>(
        std::move(view), std::move(content_view_delegate));
  }

  void Close(bool by_user) override { item_->Close(by_user); }

  void Click() override { item_->Click(); }

 private:
  // The destructor is private since this class is ref-counted.
  ~ArcNotificationDelegate() override {}

  ArcCustomNotificationItem* const item_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationDelegate);
};

}  // namespace

ArcCustomNotificationItem::ArcCustomNotificationItem(
    ArcNotificationManager* manager,
    message_center::MessageCenter* message_center,
    const std::string& notification_key,
    const AccountId& profile_id)
    : ArcNotificationItem(manager,
                          message_center,
                          notification_key,
                          profile_id) {
}

ArcCustomNotificationItem::~ArcCustomNotificationItem() {
  for (auto& observer : observers_)
    observer.OnItemDestroying();
}

void ArcCustomNotificationItem::UpdateWithArcNotificationData(
    mojom::ArcNotificationDataPtr data) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(notification_key(), data->key);

  if (HasPendingNotification()) {
    CacheArcNotificationData(std::move(data));
    return;
  }

  message_center::RichNotificationData rich_data;
  rich_data.pinned = (data->no_clear || data->ongoing_event);
  rich_data.priority = ConvertAndroidPriority(data->priority);
  if (data->small_icon)
    rich_data.small_image = gfx::Image::CreateFrom1xBitmap(*data->small_icon);
  if (data->accessible_name.has_value())
    rich_data.accessible_name = base::UTF8ToUTF16(*data->accessible_name);

  message_center::NotifierId notifier_id(
      message_center::NotifierId::SYSTEM_COMPONENT, kNotifierId);
  notifier_id.profile_id = profile_id().GetUserEmail();

  auto notification = base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, notification_id(),
      base::UTF8ToUTF16(data->title), base::UTF8ToUTF16(data->message),
      gfx::Image(),
      base::UTF8ToUTF16("arc"),  // display source
      GURL(),                    // empty origin url, for system component
      notifier_id, rich_data, new ArcNotificationDelegate(this));
  notification->set_timestamp(base::Time::FromJavaTime(data->time));
  SetNotification(std::move(notification));

  pinned_ = rich_data.pinned;
  expand_state_ = data->expand_state;

  if (!data->snapshot_image || data->snapshot_image->isNull()) {
    snapshot_ = gfx::ImageSkia();
  } else {
    snapshot_ = gfx::ImageSkia(gfx::ImageSkiaRep(
        *data->snapshot_image, data->snapshot_image_scale));
  }

  for (auto& observer : observers_)
    observer.OnItemUpdated();

  AddToMessageCenter();
}

void ArcCustomNotificationItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcCustomNotificationItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ArcCustomNotificationItem::IncrementWindowRefCount() {
  ++window_ref_count_;
  if (window_ref_count_ == 1)
    manager()->CreateNotificationWindow(notification_key());
}

void ArcCustomNotificationItem::DecrementWindowRefCount() {
  DCHECK_GT(window_ref_count_, 0);
  --window_ref_count_;
  if (window_ref_count_ == 0)
    manager()->CloseNotificationWindow(notification_key());
}

}  // namespace arc
