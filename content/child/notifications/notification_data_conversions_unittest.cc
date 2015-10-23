// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_data_conversions.h"

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/platform_notification_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"

namespace content {

const char kNotificationTitle[] = "My Notification";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world!";
const char kNotificationTag[] = "my_tag";
const char kNotificationIconUrl[] = "https://example.com/icon.png";
const int kNotificationVibrationPattern[] = { 100, 200, 300 };
const unsigned char kNotificationData[] = { 0xdf, 0xff, 0x0, 0x0, 0xff, 0xdf };
const char kAction1Name[] = "btn1";
const char kAction1Title[] = "Button 1";
const char kAction2Name[] = "btn2";
const char kAction2Title[] = "Button 2";

TEST(NotificationDataConversionsTest, ToPlatformNotificationData) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  std::vector<char> developer_data(
      kNotificationData, kNotificationData + arraysize(kNotificationData));

  blink::WebVector<blink::WebNotificationAction> web_actions(
      static_cast<size_t>(2));
  web_actions[0].action = blink::WebString::fromUTF8(kAction1Name);
  web_actions[0].title = blink::WebString::fromUTF8(kAction1Title);
  web_actions[1].action = blink::WebString::fromUTF8(kAction2Name);
  web_actions[1].title = blink::WebString::fromUTF8(kAction2Title);

  blink::WebNotificationData web_data(
      blink::WebString::fromUTF8(kNotificationTitle),
      blink::WebNotificationData::DirectionLeftToRight,
      blink::WebString::fromUTF8(kNotificationLang),
      blink::WebString::fromUTF8(kNotificationBody),
      blink::WebString::fromUTF8(kNotificationTag),
      blink::WebURL(GURL(kNotificationIconUrl)),
      blink::WebVector<int>(vibration_pattern),
      true /* silent */,
      blink::WebVector<char>(developer_data),
      web_actions);

  PlatformNotificationData platform_data = ToPlatformNotificationData(web_data);
  EXPECT_EQ(base::ASCIIToUTF16(kNotificationTitle), platform_data.title);
  EXPECT_EQ(PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT,
            platform_data.direction);
  EXPECT_EQ(kNotificationLang, platform_data.lang);
  EXPECT_EQ(base::ASCIIToUTF16(kNotificationBody), platform_data.body);
  EXPECT_EQ(kNotificationTag, platform_data.tag);
  EXPECT_EQ(kNotificationIconUrl, platform_data.icon.spec());
  EXPECT_TRUE(platform_data.silent);

  EXPECT_THAT(platform_data.vibration_pattern,
      testing::ElementsAreArray(kNotificationVibrationPattern));

  ASSERT_EQ(developer_data.size(), platform_data.data.size());
  for (size_t i = 0; i < developer_data.size(); ++i)
    EXPECT_EQ(developer_data[i], platform_data.data[i]);
  ASSERT_EQ(web_actions.size(), platform_data.actions.size());
  EXPECT_EQ(kAction1Name, platform_data.actions[0].action);
  EXPECT_EQ(base::ASCIIToUTF16(kAction1Title), platform_data.actions[0].title);
  EXPECT_EQ(kAction2Name, platform_data.actions[1].action);
  EXPECT_EQ(base::ASCIIToUTF16(kAction2Title), platform_data.actions[1].title);
}

TEST(NotificationDataConversionsTest, ToWebNotificationData) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  std::vector<char> developer_data(
      kNotificationData, kNotificationData + arraysize(kNotificationData));

  PlatformNotificationData platform_data;
  platform_data.title = base::ASCIIToUTF16(kNotificationTitle);
  platform_data.direction =
      PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
  platform_data.lang = kNotificationLang;
  platform_data.body = base::ASCIIToUTF16(kNotificationBody);
  platform_data.tag = kNotificationTag;
  platform_data.icon = GURL(kNotificationIconUrl);
  platform_data.vibration_pattern = vibration_pattern;
  platform_data.silent = true;
  platform_data.data = developer_data;
  platform_data.actions.resize(2);
  platform_data.actions[0].action = kAction1Name;
  platform_data.actions[0].title = base::ASCIIToUTF16(kAction1Title);
  platform_data.actions[1].action = kAction2Name;
  platform_data.actions[1].title = base::ASCIIToUTF16(kAction2Title);

  blink::WebNotificationData web_data = ToWebNotificationData(platform_data);
  EXPECT_EQ(kNotificationTitle, web_data.title);
  EXPECT_EQ(blink::WebNotificationData::DirectionLeftToRight,
            web_data.direction);
  EXPECT_EQ(kNotificationLang, web_data.lang);
  EXPECT_EQ(kNotificationBody, web_data.body);
  EXPECT_EQ(kNotificationTag, web_data.tag);
  EXPECT_EQ(kNotificationIconUrl, web_data.icon.string());

  ASSERT_EQ(vibration_pattern.size(), web_data.vibrate.size());
  for (size_t i = 0; i < vibration_pattern.size(); ++i)
    EXPECT_EQ(vibration_pattern[i], web_data.vibrate[i]);

  EXPECT_TRUE(web_data.silent);

  ASSERT_EQ(developer_data.size(), web_data.data.size());
  for (size_t i = 0; i < developer_data.size(); ++i)
    EXPECT_EQ(developer_data[i], web_data.data[i]);

  ASSERT_EQ(platform_data.actions.size(), web_data.actions.size());
  EXPECT_EQ(kAction1Name, web_data.actions[0].action);
  EXPECT_EQ(kAction1Title, web_data.actions[0].title);
  EXPECT_EQ(kAction2Name, web_data.actions[1].action);
  EXPECT_EQ(kAction2Title, web_data.actions[1].title);
}

TEST(NotificationDataConversionsTest, NotificationDataDirectionality) {
  std::map<blink::WebNotificationData::Direction,
           PlatformNotificationData::Direction> mappings;

  mappings[blink::WebNotificationData::DirectionLeftToRight] =
      PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
  mappings[blink::WebNotificationData::DirectionRightToLeft] =
      PlatformNotificationData::DIRECTION_RIGHT_TO_LEFT;
  mappings[blink::WebNotificationData::DirectionAuto] =
      PlatformNotificationData::DIRECTION_AUTO;

  for (const auto& pair : mappings) {
    {
      blink::WebNotificationData web_data;
      web_data.direction = pair.first;

      PlatformNotificationData platform_data =
          ToPlatformNotificationData(web_data);
      EXPECT_EQ(pair.second, platform_data.direction);
    }
    {
      PlatformNotificationData platform_data;
      platform_data.direction = pair.second;

      blink::WebNotificationData web_data =
          ToWebNotificationData(platform_data);
      EXPECT_EQ(pair.first, web_data.direction);
    }
  }
}

}  // namespace content
