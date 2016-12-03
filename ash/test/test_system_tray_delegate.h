// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
#define ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_

#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/common/system/tray/ime_info.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace ash {
namespace test {

class TestSystemTrayDelegate : public DefaultSystemTrayDelegate {
 public:
  TestSystemTrayDelegate();
  ~TestSystemTrayDelegate() override;

  // Sets whether a system update is required. Defaults to false. Static so
  // tests can set the value before the system tray is constructed. Reset in
  // AshTestHelper::TearDown.
  static void SetSystemUpdateRequired(bool required);

  // Changes the login status when initially the delegate is created. This will
  // be called before AshTestBase::SetUp() to test the case when chrome is
  // restarted right after the login (such like a flag is set).
  // This value will be reset in AshTestHelper::TearDown,  most test fixtures
  // don't need to care its lifecycle.
  static void SetInitialLoginStatus(LoginStatus login_status);

  // Changes the current login status in the test. This also invokes
  // UpdateAfterLoginStatusChange(). Usually this is called in the test code to
  // set up a login status. This will fit to most of the test cases, but this
  // cannot be set during the initialization. To test the initialization,
  // consider using SetInitialLoginStatus() instead.
  void SetLoginStatus(LoginStatus login_status);

  void set_should_show_display_notification(bool should_show) {
    should_show_display_notification_ = should_show;
  }

  // Updates the session length limit so that the limit will come from now in
  // |new_limit|.
  void SetSessionLengthLimitForTest(const base::TimeDelta& new_limit);

  // Clears the session length limit.
  void ClearSessionLengthLimit();

  // Sets the IME info.
  void SetCurrentIME(const IMEInfo& info);

  // Sets the list of available IMEs.
  void SetAvailableIMEList(const IMEInfoList& list);

  // Overridden from SystemTrayDelegate:
  LoginStatus GetUserLoginStatus() const override;
  bool IsUserSupervised() const override;
  void GetSystemUpdateInfo(UpdateInfo* info) const override;
  bool ShouldShowDisplayNotification() override;
  bool GetSessionStartTime(base::TimeTicks* session_start_time) override;
  bool GetSessionLengthLimit(base::TimeDelta* session_length_limit) override;
  void SignOut() override;
  std::unique_ptr<SystemTrayItem> CreateDisplayTrayItem(
      SystemTray* tray) override;
  std::unique_ptr<SystemTrayItem> CreateRotationLockTrayItem(
      SystemTray* tray) override;
  void GetCurrentIME(IMEInfo* info) override;
  void GetAvailableIMEList(IMEInfoList* list) override;

 private:
  bool should_show_display_notification_;
  LoginStatus login_status_;
  base::TimeDelta session_length_limit_;
  bool session_length_limit_set_;
  IMEInfo current_ime_;
  IMEInfoList ime_list_;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SYSTEM_TRAY_DELEGATE_H_
