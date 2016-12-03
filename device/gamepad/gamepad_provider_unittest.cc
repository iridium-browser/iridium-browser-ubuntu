// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_provider.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using blink::WebGamepads;

// Helper class to generate and record user gesture callbacks.
class UserGestureListener {
 public:
  UserGestureListener() : has_user_gesture_(false), weak_factory_(this) {}

  base::Closure GetClosure() {
    return base::Bind(&UserGestureListener::GotUserGesture,
                      weak_factory_.GetWeakPtr());
  }

  bool has_user_gesture() const { return has_user_gesture_; }

 private:
  void GotUserGesture() { has_user_gesture_ = true; }

  bool has_user_gesture_;
  base::WeakPtrFactory<UserGestureListener> weak_factory_;
};

// Main test fixture
class GamepadProviderTest : public testing::Test, public GamepadTestHelper {
 public:
  GamepadProvider* CreateProvider(const WebGamepads& test_data) {
    mock_data_fetcher_ = new MockGamepadDataFetcher(test_data);
    provider_.reset(new GamepadProvider(
        std::unique_ptr<GamepadSharedBuffer>(new MockGamepadSharedBuffer()),
        nullptr, std::unique_ptr<GamepadDataFetcher>(mock_data_fetcher_)));
    return provider_.get();
  }

 protected:
  GamepadProviderTest() {}

  std::unique_ptr<GamepadProvider> provider_;

  // Pointer owned by the provider.
  MockGamepadDataFetcher* mock_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProviderTest);
};

// Crashes. http://crbug.com/106163
// crbug.com/147549
#if defined(OS_ANDROID)
#define MAYBE_PollingAccess DISABLED_PollingAccess
#else
#define MAYBE_PollingAccess PollingAccess
#endif
TEST_F(GamepadProviderTest, MAYBE_PollingAccess) {
  WebGamepads test_data;
  test_data.length = 1;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttonsLength = 1;
  test_data.items[0].axesLength = 2;
  test_data.items[0].buttons[0].value = 1.f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();

  base::RunLoop().RunUntilIdle();

  mock_data_fetcher_->WaitForDataRead();

  // Renderer-side, pull data out of poll buffer.
  base::SharedMemoryHandle handle = provider->GetSharedMemoryHandleForProcess(
      base::GetCurrentProcessHandle());
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, true));
  EXPECT_TRUE(shared_memory->Map(sizeof(WebGamepads)));
  void* mem = shared_memory->memory();

  WebGamepads* output = static_cast<WebGamepads*>(mem);

  EXPECT_EQ(1u, output->length);
  EXPECT_EQ(1u, output->items[0].buttonsLength);
  EXPECT_EQ(1.f, output->items[0].buttons[0].value);
  EXPECT_EQ(true, output->items[0].buttons[0].pressed);
  EXPECT_EQ(2u, output->items[0].axesLength);
  EXPECT_EQ(-1.f, output->items[0].axes[0]);
  EXPECT_EQ(0.5f, output->items[0].axes[1]);
}

// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, UserGesture) {
  WebGamepads no_button_data;
  no_button_data.length = 1;
  no_button_data.items[0].connected = true;
  no_button_data.items[0].timestamp = 0;
  no_button_data.items[0].buttonsLength = 1;
  no_button_data.items[0].axesLength = 2;
  no_button_data.items[0].buttons[0].value = 0.f;
  no_button_data.items[0].buttons[0].pressed = false;
  no_button_data.items[0].axes[0] = 0.f;
  no_button_data.items[0].axes[1] = .4f;

  WebGamepads button_down_data = no_button_data;
  button_down_data.items[0].buttons[0].value = 1.f;
  button_down_data.items[0].buttons[0].pressed = true;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(no_button_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();

  provider->RegisterForUserGesture(listener.GetClosure());
  mock_data_fetcher_->WaitForDataReadAndCallbacksIssued();

  // It should not have issued our callback.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(listener.has_user_gesture());

  // Set a button down and wait for it to be read twice.
  mock_data_fetcher_->SetTestData(button_down_data);
  mock_data_fetcher_->WaitForDataReadAndCallbacksIssued();

  // It should have issued our callback.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener.has_user_gesture());
}

// Crashes. http://crbug.com/106163
// crbug.com/147549
// Flaky on MSAN: http://crbug.com/640086
#if defined(OS_ANDROID)
#define MAYBE_Sanitization DISABLED_Sanitization
#elif defined(MEMORY_SANITIZER)
#define MAYBE_Sanitization DISABLED_Sanitization
#else
#define MAYBE_Sanitization Sanitization
#endif
// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, MAYBE_Sanitization) {
  WebGamepads active_data;
  active_data.length = 1;
  active_data.items[0].connected = true;
  active_data.items[0].timestamp = 0;
  active_data.items[0].buttonsLength = 1;
  active_data.items[0].axesLength = 1;
  active_data.items[0].buttons[0].value = 1.f;
  active_data.items[0].buttons[0].pressed = true;
  active_data.items[0].axes[0] = -1.f;

  WebGamepads zero_data;
  zero_data.length = 1;
  zero_data.items[0].connected = true;
  zero_data.items[0].timestamp = 0;
  zero_data.items[0].buttonsLength = 1;
  zero_data.items[0].axesLength = 1;
  zero_data.items[0].buttons[0].value = 0.f;
  zero_data.items[0].buttons[0].pressed = false;
  zero_data.items[0].axes[0] = 0.f;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(active_data);
  provider->SetSanitizationEnabled(true);
  provider->Resume();

  base::RunLoop().RunUntilIdle();

  mock_data_fetcher_->WaitForDataRead();

  // Renderer-side, pull data out of poll buffer.
  base::SharedMemoryHandle handle = provider->GetSharedMemoryHandleForProcess(
      base::GetCurrentProcessHandle());
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, true));
  EXPECT_TRUE(shared_memory->Map(sizeof(WebGamepads)));
  void* mem = shared_memory->memory();

  WebGamepads* output = static_cast<WebGamepads*>(mem);

  // Initial data should all be zeroed out due to sanitization, even though the
  // gamepad reported input
  EXPECT_EQ(1u, output->length);
  EXPECT_EQ(1u, output->items[0].buttonsLength);
  EXPECT_EQ(0.f, output->items[0].buttons[0].value);
  EXPECT_FALSE(output->items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output->items[0].axesLength);
  EXPECT_EQ(0.f, output->items[0].axes[0]);

  // Zero out the inputs
  mock_data_fetcher_->SetTestData(zero_data);
  mock_data_fetcher_->WaitForDataReadAndCallbacksIssued();

  // Should still read zero, which is now an accurate reflection of the data
  EXPECT_EQ(1u, output->length);
  EXPECT_EQ(1u, output->items[0].buttonsLength);
  EXPECT_EQ(0.f, output->items[0].buttons[0].value);
  EXPECT_FALSE(output->items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output->items[0].axesLength);
  EXPECT_EQ(0.f, output->items[0].axes[0]);

  // Re-set the active inputs
  mock_data_fetcher_->SetTestData(active_data);
  mock_data_fetcher_->WaitForDataReadAndCallbacksIssued();

  // Should now accurately reflect the reported data.
  EXPECT_EQ(1u, output->length);
  EXPECT_EQ(1u, output->items[0].buttonsLength);
  EXPECT_EQ(1.f, output->items[0].buttons[0].value);
  EXPECT_TRUE(output->items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output->items[0].axesLength);
  EXPECT_EQ(-1.f, output->items[0].axes[0]);
}

}  // namespace

}  // namespace device
