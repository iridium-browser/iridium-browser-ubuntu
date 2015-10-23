// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"

class NavigationDetails;
class PermissionQueueController;

// Base class for permission infobars, it implements the default behavior
// so that the accept/deny buttons grant/deny the relevant permission.
// A basic implementor only needs to implement the methods that
// provide an icon and a message text to the infobar.
class PermissionInfobarDelegate : public ConfirmInfoBarDelegate {

 public:
  ContentSettingsType content_setting() const { return type_; }

 protected:
  PermissionInfobarDelegate(PermissionQueueController* controller,
                            const PermissionRequestID& id,
                            const GURL& requesting_origin,
                            ContentSettingsType type);
  ~PermissionInfobarDelegate() override;

 private:
  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  void InfoBarDismissed() override;
  PermissionInfobarDelegate* AsPermissionInfobarDelegate() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  void SetPermission(bool update_content_setting, bool allowed);

  PermissionQueueController* controller_; // not owned by us
  const PermissionRequestID id_;
  GURL requesting_origin_;
  bool action_taken_;
  ContentSettingsType type_;

  DISALLOW_COPY_AND_ASSIGN(PermissionInfobarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_INFOBAR_DELEGATE_H_
