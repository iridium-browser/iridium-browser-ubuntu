// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/notifications/NotificationData.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/UnionTypesModules.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationOptions.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

const char kNotificationTitle[] = "My Notification";

const char kNotificationDir[] = "rtl";
const char kNotificationLang[] = "nl";
const char kNotificationBody[] = "Hello, world";
const char kNotificationTag[] = "my_tag";
const char kNotificationIcon[] = "https://example.com/icon.png";
const unsigned kNotificationVibration[] = { 42, 10, 20, 30, 40 };
const bool kNotificationSilent = false;

const char kNotificationActionAction[] = "my_action";
const char kNotificationActionTitle[] = "My Action";

const unsigned kNotificationVibrationUnnormalized[] = { 10, 1000000, 50, 42 };
const int kNotificationVibrationNormalized[] = { 10, 10000, 50 };

class NotificationDataTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_executionContext = adoptRefWillBeNoop(new NullExecutionContext());
    }

    ExecutionContext* executionContext() { return m_executionContext.get(); }

private:
    RefPtrWillBePersistent<ExecutionContext> m_executionContext;
};

TEST_F(NotificationDataTest, ReflectProperties)
{
    Vector<unsigned> vibrationPattern;
    for (size_t i = 0; i < arraysize(kNotificationVibration); ++i)
        vibrationPattern.append(kNotificationVibration[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(vibrationPattern);

    HeapVector<NotificationAction> actions;
    for (size_t i = 0; i < Notification::maxActions(); ++i) {
        NotificationAction action;
        action.setAction(kNotificationActionAction);
        action.setTitle(kNotificationActionTitle);

        actions.append(action);
    }

    NotificationOptions options;
    options.setDir(kNotificationDir);
    options.setLang(kNotificationLang);
    options.setBody(kNotificationBody);
    options.setTag(kNotificationTag);
    options.setIcon(kNotificationIcon);
    options.setVibrate(vibrationSequence);
    options.setSilent(kNotificationSilent);
    options.setActions(actions);

    // TODO(peter): Test |options.data| and |notificationData.data|.

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    EXPECT_EQ(kNotificationTitle, notificationData.title);

    EXPECT_EQ(WebNotificationData::DirectionRightToLeft, notificationData.direction);
    EXPECT_EQ(kNotificationLang, notificationData.lang);
    EXPECT_EQ(kNotificationBody, notificationData.body);
    EXPECT_EQ(kNotificationTag, notificationData.tag);

    // TODO(peter): Test notificationData.icon when ExecutionContext::completeURL() works in this test.

    ASSERT_EQ(vibrationPattern.size(), notificationData.vibrate.size());
    for (size_t i = 0; i < vibrationPattern.size(); ++i)
        EXPECT_EQ(vibrationPattern[i], static_cast<unsigned>(notificationData.vibrate[i]));

    EXPECT_EQ(kNotificationSilent, notificationData.silent);
    EXPECT_EQ(actions.size(), notificationData.actions.size());
}

TEST_F(NotificationDataTest, SilentNotificationWithVibration)
{
    Vector<unsigned> vibrationPattern;
    for (size_t i = 0; i < arraysize(kNotificationVibration); ++i)
        vibrationPattern.append(kNotificationVibration[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(vibrationPattern);

    NotificationOptions options;
    options.setVibrate(vibrationSequence);
    options.setSilent(true);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_TRUE(exceptionState.hadException());

    EXPECT_EQ("Silent notifications must not specify vibration patterns.", exceptionState.message());
}

TEST_F(NotificationDataTest, InvalidIconUrl)
{
    NotificationOptions options;
    options.setIcon("https://invalid:icon:url");

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    EXPECT_TRUE(notificationData.icon.isEmpty());
}

TEST_F(NotificationDataTest, VibrationNormalization)
{
    Vector<unsigned> unnormalizedPattern;
    for (size_t i = 0; i < arraysize(kNotificationVibrationUnnormalized); ++i)
        unnormalizedPattern.append(kNotificationVibrationUnnormalized[i]);

    UnsignedLongOrUnsignedLongSequence vibrationSequence;
    vibrationSequence.setUnsignedLongSequence(unnormalizedPattern);

    NotificationOptions options;
    options.setVibrate(vibrationSequence);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    Vector<int> normalizedPattern;
    for (size_t i = 0; i < arraysize(kNotificationVibrationNormalized); ++i)
        normalizedPattern.append(kNotificationVibrationNormalized[i]);

    ASSERT_EQ(normalizedPattern.size(), notificationData.vibrate.size());
    for (size_t i = 0; i < normalizedPattern.size(); ++i)
        EXPECT_EQ(normalizedPattern[i], notificationData.vibrate[i]);
}

TEST_F(NotificationDataTest, DirectionValues)
{
    WTF::HashMap<String, WebNotificationData::Direction> mappings;
    mappings.add("ltr", WebNotificationData::DirectionLeftToRight);
    mappings.add("rtl", WebNotificationData::DirectionRightToLeft);
    mappings.add("auto", WebNotificationData::DirectionAuto);

    // Invalid values should default to "auto".
    mappings.add("peter", WebNotificationData::DirectionAuto);

    for (const String& direction : mappings.keys()) {
        NotificationOptions options;
        options.setDir(direction);

        TrackExceptionState exceptionState;
        WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
        ASSERT_FALSE(exceptionState.hadException());

        EXPECT_EQ(mappings.get(direction), notificationData.direction);
    }
}

TEST_F(NotificationDataTest, RequiredActionProperties)
{
    NotificationOptions options;

    // The NotificationAction.action property is required.
    {
        NotificationAction action;
        action.setTitle(kNotificationActionTitle);

        HeapVector<NotificationAction> actions;
        actions.append(action);

        options.setActions(actions);

        TrackExceptionState exceptionState;
        WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
        ASSERT_TRUE(exceptionState.hadException());
        EXPECT_EQ("NotificationAction `action` must not be empty.", exceptionState.message());
    }

    // The NotificationAction.title property is required.
    {
        NotificationAction action;
        action.setAction(kNotificationActionAction);

        HeapVector<NotificationAction> actions;
        actions.append(action);

        options.setActions(actions);

        TrackExceptionState exceptionState;
        WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
        ASSERT_TRUE(exceptionState.hadException());
        EXPECT_EQ("NotificationAction `title` must not be empty.", exceptionState.message());
    }
}

TEST_F(NotificationDataTest, MaximumActionCount)
{
    HeapVector<NotificationAction> actions;
    for (size_t i = 0; i < Notification::maxActions() + 2; ++i) {
        NotificationAction action;
        action.setAction(String::number(i));
        action.setTitle(kNotificationActionTitle);

        actions.append(action);
    }

    NotificationOptions options;
    options.setActions(actions);

    TrackExceptionState exceptionState;
    WebNotificationData notificationData = createWebNotificationData(executionContext(), kNotificationTitle, options, exceptionState);
    ASSERT_FALSE(exceptionState.hadException());

    // The stored actions will be capped to |maxActions| entries.
    ASSERT_EQ(Notification::maxActions(), notificationData.actions.size());

    for (size_t i = 0; i < Notification::maxActions(); ++i) {
        WebString expectedAction = String::number(i);
        EXPECT_EQ(expectedAction, notificationData.actions[i].action);
    }
}

} // namespace
} // namespace blink
