// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/push_subscription_options.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

namespace content {

namespace {

// NIST P-256 public key made available to layout tests. Must be an uncompressed
// point in accordance with SEC1 2.3.3.
const uint8_t kTestP256Key[] = {
  0x04, 0x55, 0x52, 0x6A, 0xA5, 0x6E, 0x8E, 0xAA, 0x47, 0x97, 0x36, 0x10, 0xC1,
  0x66, 0x3C, 0x1E, 0x65, 0xBF, 0xA1, 0x7B, 0xEE, 0x48, 0xC9, 0xC6, 0xBB, 0xBF,
  0x02, 0x18, 0x53, 0x72, 0x1D, 0x0C, 0x7B, 0xA9, 0xE3, 0x11, 0xB7, 0x03, 0x52,
  0x21, 0xD3, 0x71, 0x90, 0x13, 0xA8, 0xC1, 0xCF, 0xED, 0x20, 0xF7, 0x1F, 0xD1,
  0x7F, 0xF2, 0x76, 0xB6, 0x01, 0x20, 0xD8, 0x35, 0xA5, 0xD9, 0x3C, 0x43, 0xFD
};

static_assert(sizeof(kTestP256Key) == 65,
              "The fake public key must be a valid P-256 uncompressed point.");

// 92-bit (12 byte) authentication key associated with a subscription.
const uint8_t kAuthentication[] = {
  0xA5, 0xD9, 0x3C, 0x43, 0x0C, 0x00, 0xA9, 0xE3, 0x1E, 0x65, 0xBF, 0xA1
};

static_assert(sizeof(kAuthentication) == 12,
              "The fake authentication key must be at least 12 bytes in size.");

blink::WebPushPermissionStatus ToWebPushPermissionStatus(
    blink::mojom::PermissionStatus status) {
  switch (status) {
    case blink::mojom::PermissionStatus::GRANTED:
      return blink::WebPushPermissionStatusGranted;
    case blink::mojom::PermissionStatus::DENIED:
      return blink::WebPushPermissionStatusDenied;
    case blink::mojom::PermissionStatus::ASK:
      return blink::WebPushPermissionStatusPrompt;
  }

  NOTREACHED();
  return blink::WebPushPermissionStatusLast;
}

}  // anonymous namespace

LayoutTestPushMessagingService::LayoutTestPushMessagingService() {
}

LayoutTestPushMessagingService::~LayoutTestPushMessagingService() {
}

GURL LayoutTestPushMessagingService::GetEndpoint(bool standard_protocol) const {
  return GURL(standard_protocol ? "https://example.com/StandardizedEndpoint/"
                                : "https://example.com/LayoutTestEndpoint/");
}

void LayoutTestPushMessagingService::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int renderer_id,
    int render_frame_id,
    const PushSubscriptionOptions& options,
    const PushMessagingService::RegisterCallback& callback) {
  SubscribeFromWorker(requesting_origin, service_worker_registration_id,
                      options, callback);
}

void LayoutTestPushMessagingService::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const PushSubscriptionOptions& options,
    const PushMessagingService::RegisterCallback& callback) {
  if (GetPermissionStatus(requesting_origin, options.user_visible_only) ==
      blink::WebPushPermissionStatusGranted) {
    std::vector<uint8_t> p256dh(
        kTestP256Key, kTestP256Key + arraysize(kTestP256Key));
    std::vector<uint8_t> auth(
        kAuthentication, kAuthentication + arraysize(kAuthentication));

    callback.Run("layoutTestRegistrationId", p256dh, auth,
                 PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE);
  } else {
    callback.Run("registration_id", std::vector<uint8_t>() /* p256dh */,
                 std::vector<uint8_t>() /* auth */,
                 PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
  }
}

void LayoutTestPushMessagingService::GetEncryptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const EncryptionInfoCallback& callback) {
  std::vector<uint8_t> p256dh(
        kTestP256Key, kTestP256Key + arraysize(kTestP256Key));
  std::vector<uint8_t> auth(
        kAuthentication, kAuthentication + arraysize(kAuthentication));

  callback.Run(true /* success */, p256dh, auth);
}

blink::WebPushPermissionStatus
LayoutTestPushMessagingService::GetPermissionStatus(const GURL& origin,
                                                    bool user_visible) {
  return ToWebPushPermissionStatus(LayoutTestContentBrowserClient::Get()
      ->browser_context()
      ->GetPermissionManager()
      ->GetPermissionStatus(PermissionType::PUSH_MESSAGING, origin, origin));
}

bool LayoutTestPushMessagingService::SupportNonVisibleMessages() {
  return false;
}

void LayoutTestPushMessagingService::Unsubscribe(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const UnregisterCallback& callback) {
  callback.Run(PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
}

}  // namespace content
