// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_BUBBLE_REQUEST_IMPL_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_BUBBLE_REQUEST_IMPL_H_

#include "base/callback.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/permission_request_id.h"

class GURL;

class PermissionContextBase;

// Default implementation of PermissionBubbleRequest, it is assumed that the
// caller owns it and that it can be deleted once the |delete_callback|
// is executed.
class PermissionBubbleRequestImpl : public PermissionBubbleRequest {
 public:
  using PermissionDecidedCallback = base::Callback<void(bool, ContentSetting)>;

  PermissionBubbleRequestImpl(
      const GURL& request_origin,
      bool user_gesture,
      ContentSettingsType type,
      const std::string& display_languages,
      const PermissionDecidedCallback& permission_decided_callback,
      const base::Closure delete_callback);

  ~PermissionBubbleRequestImpl() override;

  // PermissionBubbleRequest:
  int GetIconID() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetMessageTextFragment() const override;
  bool HasUserGesture() const override;

  // TODO(miguelg) Change this method to GetOrigin()
  GURL GetRequestingHostname() const override;

  // Remember to call RegisterActionTaken for these methods if you are
  // overriding them.
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;

 protected:
  void RegisterActionTaken() { action_taken_ = true; }

 private:
  GURL request_origin_;
  bool user_gesture_;
  ContentSettingsType type_;
  std::string display_languages_;

  // Called once a decision is made about the permission.
  const PermissionDecidedCallback permission_decided_callback_;

  // Called when the bubble is no longer in use so it can be deleted by
  // the caller.
  const base::Closure delete_callback_;
  bool is_finished_;
  bool action_taken_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleRequestImpl);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_BUBBLE_REQUEST_IMPL_H_
