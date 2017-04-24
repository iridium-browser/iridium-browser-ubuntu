// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <X11/extensions/Xrandr.h>

#undef Bool
#undef None

#include <algorithm>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/chromeos/x11/display_mode_x11.h"
#include "ui/display/manager/chromeos/x11/display_snapshot_x11.h"
#include "ui/display/manager/chromeos/x11/native_display_delegate_x11.h"
#include "ui/display/manager/chromeos/x11/native_display_event_dispatcher_x11.h"

namespace display {

namespace {

std::unique_ptr<DisplaySnapshotX11> CreateOutput(int64_t id,
                                                 DisplayConnectionType type,
                                                 RROutput output,
                                                 RRCrtc crtc) {
  static const DisplayModeX11 kDefaultDisplayMode(gfx::Size(1, 1), false, 60.0f,
                                                  20);
  std::vector<std::unique_ptr<const DisplayMode>> modes;
  const DisplayMode* mode;

  modes.push_back(kDefaultDisplayMode.Clone());
  mode = modes.front().get();

  return base::MakeUnique<DisplaySnapshotX11>(
      id, gfx::Point(0, 0), gfx::Size(0, 0), type, false, false, std::string(),
      std::move(modes), std::vector<uint8_t>(), mode, nullptr, output, crtc, 0);
}

std::unique_ptr<DisplaySnapshotX11> CreateExternalOutput(RROutput output,
                                                         RRCrtc crtc) {
  return CreateOutput(static_cast<int64_t>(output),
                      DISPLAY_CONNECTION_TYPE_UNKNOWN, output, crtc);
}

std::unique_ptr<DisplaySnapshotX11> CreateInternalOutput(RROutput output,
                                                         RRCrtc crtc) {
  return CreateOutput(0, DISPLAY_CONNECTION_TYPE_INTERNAL, output, crtc);
}

class TestHelperDelegate : public NativeDisplayDelegateX11::HelperDelegate {
 public:
  TestHelperDelegate();
  ~TestHelperDelegate() override;

  int num_calls_update_xrandr_config() const {
    return num_calls_update_xrandr_config_;
  }

  int num_calls_notify_observers() const { return num_calls_notify_observers_; }

  void SetCachedOutputs(
      const std::vector<std::unique_ptr<DisplaySnapshot>>& outputs) {
    cached_outputs_.resize(outputs.size());
    std::transform(outputs.cbegin(), outputs.cend(), cached_outputs_.begin(),
                   [](const std::unique_ptr<DisplaySnapshot>& item) {
                     return item.get();
                   });
  }

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  void UpdateXRandRConfiguration(const base::NativeEvent& event) override;
  std::vector<DisplaySnapshot*> GetCachedDisplays() const override;
  void NotifyDisplayObservers() override;

 private:
  int num_calls_update_xrandr_config_;
  int num_calls_notify_observers_;

  std::vector<DisplaySnapshot*> cached_outputs_;

  DISALLOW_COPY_AND_ASSIGN(TestHelperDelegate);
};

TestHelperDelegate::TestHelperDelegate()
    : num_calls_update_xrandr_config_(0), num_calls_notify_observers_(0) {}

TestHelperDelegate::~TestHelperDelegate() {}

void TestHelperDelegate::UpdateXRandRConfiguration(
    const base::NativeEvent& event) {
  ++num_calls_update_xrandr_config_;
}

std::vector<DisplaySnapshot*> TestHelperDelegate::GetCachedDisplays() const {
  return cached_outputs_;
}

void TestHelperDelegate::NotifyDisplayObservers() {
  ++num_calls_notify_observers_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayEventDispatcherX11Test

class NativeDisplayEventDispatcherX11Test : public testing::Test {
 public:
  NativeDisplayEventDispatcherX11Test();
  ~NativeDisplayEventDispatcherX11Test() override;

 protected:
  void DispatchScreenChangeEvent();
  void DispatchOutputChangeEvent(RROutput output,
                                 RRCrtc crtc,
                                 RRMode mode,
                                 bool connected);

  int xrandr_event_base_;
  std::unique_ptr<TestHelperDelegate> helper_delegate_;
  std::unique_ptr<NativeDisplayEventDispatcherX11> dispatcher_;
  base::SimpleTestTickClock* test_tick_clock_;  // Owned by |dispatcher_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeDisplayEventDispatcherX11Test);
};

NativeDisplayEventDispatcherX11Test::NativeDisplayEventDispatcherX11Test()
    : xrandr_event_base_(10),
      helper_delegate_(new TestHelperDelegate()),
      dispatcher_(new NativeDisplayEventDispatcherX11(helper_delegate_.get(),
                                                      xrandr_event_base_)),
      test_tick_clock_(new base::SimpleTestTickClock) {
  test_tick_clock_->Advance(base::TimeDelta::FromMilliseconds(1));
  dispatcher_->SetTickClockForTest(
      std::unique_ptr<base::TickClock>(test_tick_clock_));
}

NativeDisplayEventDispatcherX11Test::~NativeDisplayEventDispatcherX11Test() {}

void NativeDisplayEventDispatcherX11Test::DispatchScreenChangeEvent() {
  XRRScreenChangeNotifyEvent event = {0};
  event.type = xrandr_event_base_ + RRScreenChangeNotify;

  dispatcher_->DispatchEvent(reinterpret_cast<const ui::PlatformEvent>(&event));
}

void NativeDisplayEventDispatcherX11Test::DispatchOutputChangeEvent(
    RROutput output,
    RRCrtc crtc,
    RRMode mode,
    bool connected) {
  XRROutputChangeNotifyEvent event = {0};
  event.type = xrandr_event_base_ + RRNotify;
  event.subtype = RRNotify_OutputChange;
  event.output = output;
  event.crtc = crtc;
  event.mode = mode;
  event.connection = connected ? RR_Connected : RR_Disconnected;

  dispatcher_->DispatchEvent(reinterpret_cast<const ui::PlatformEvent>(&event));
}

}  // namespace

TEST_F(NativeDisplayEventDispatcherX11Test, OnScreenChangedEvent) {
  DispatchScreenChangeEvent();
  EXPECT_EQ(1, helper_delegate_->num_calls_update_xrandr_config());
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnFirstEvent) {
  DispatchOutputChangeEvent(1, 10, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_update_xrandr_config());
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationAfterSecondEvent) {
  DispatchOutputChangeEvent(1, 10, 20, true);

  // Simulate addition of the first output to the cached output list.
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(2, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDisconnect) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(1, 10, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnModeChange) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(1, 10, 21, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnSecondOutput) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, CheckNotificationOnDifferentCrtc) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(1, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       CheckNotificationOnSecondOutputDisconnect) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  outputs.push_back(CreateExternalOutput(2, 11));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test,
       AvoidDuplicateNotificationOnSecondOutputDisconnect) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  outputs.push_back(CreateExternalOutput(2, 11));
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());

  // Simulate removal of second output from cached output list.
  outputs.erase(outputs.begin() + 1);
  helper_delegate_->SetCachedOutputs(outputs);

  DispatchOutputChangeEvent(2, 11, 20, false);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, ForceUpdateAfterCacheExpiration) {
  // +1 to compenstate a possible rounding error.
  const int kHalfOfExpirationMs =
      NativeDisplayEventDispatcherX11::kUseCacheAfterStartupMs / 2 + 1;

  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateExternalOutput(1, 10));
  outputs.push_back(CreateExternalOutput(2, 11));
  helper_delegate_->SetCachedOutputs(outputs);

  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  // Duplicated event will be ignored during the startup.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));

  // Duplicated event will still be ignored.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  // The startup timeout has been elapsed. Duplicated event
  // should not be ignored.
  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());

  // Sending the same event immediately shoudldn't be ignored.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(2, helper_delegate_->num_calls_notify_observers());

  // Advancing time further should not change the behavior.
  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(3, helper_delegate_->num_calls_notify_observers());

  test_tick_clock_->Advance(
      base::TimeDelta::FromMilliseconds(kHalfOfExpirationMs));
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(4, helper_delegate_->num_calls_notify_observers());
}

TEST_F(NativeDisplayEventDispatcherX11Test, UpdateMissingExternalDisplayId) {
  std::vector<std::unique_ptr<DisplaySnapshot>> outputs;
  outputs.push_back(CreateInternalOutput(1, 10));
  helper_delegate_->SetCachedOutputs(outputs);

  ASSERT_EQ(0, helper_delegate_->num_calls_notify_observers());

  // Internal display's ID can be zero and not updated.
  DispatchOutputChangeEvent(1, 10, 20, true);
  EXPECT_EQ(0, helper_delegate_->num_calls_notify_observers());

  outputs.clear();
  outputs.push_back(CreateOutput(0, DISPLAY_CONNECTION_TYPE_UNKNOWN, 2, 11));
  helper_delegate_->SetCachedOutputs(outputs);

  // External display should be updated if the id is zero.
  DispatchOutputChangeEvent(2, 11, 20, true);
  EXPECT_EQ(1, helper_delegate_->num_calls_notify_observers());
}

}  // namespace display
