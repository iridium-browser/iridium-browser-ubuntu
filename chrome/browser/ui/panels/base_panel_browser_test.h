// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/ui/panels/display_settings_provider.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/gfx/geometry/rect.h"

class NativePanelTesting;
class StackedPanelCollection;

class BasePanelBrowserTest : public InProcessBrowserTest {
 public:
  class MockDisplaySettingsProvider : public DisplaySettingsProvider {
   public:
    MockDisplaySettingsProvider() { }
    ~MockDisplaySettingsProvider() override {}

    virtual void SetPrimaryDisplay(
        const gfx::Rect& display_area, const gfx::Rect& work_area) = 0;
    virtual void SetSecondaryDisplay(
        const gfx::Rect& display_area, const gfx::Rect& work_area) = 0;
    virtual void EnableAutoHidingDesktopBar(DesktopBarAlignment alignment,
                                            bool enabled,
                                            int thickness) = 0;
    virtual void SetDesktopBarVisibility(DesktopBarAlignment alignment,
                                         DesktopBarVisibility visibility) = 0;
    virtual void SetDesktopBarThickness(DesktopBarAlignment alignment,
                                        int thickness) = 0;
    virtual void EnableFullScreenMode(bool enabled) = 0;
  };

  BasePanelBrowserTest();
  ~BasePanelBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  enum ActiveState { SHOW_AS_ACTIVE, SHOW_AS_INACTIVE };

  struct CreatePanelParams {
    std::string name;
    gfx::Rect bounds;
    ActiveState show_flag;
    GURL url;
    bool wait_for_fully_created;
    ActiveState expected_active_state;
    PanelManager::CreateMode create_mode;
    Profile* profile;

    CreatePanelParams(const std::string& name,
                      const gfx::Rect& bounds,
                      ActiveState show_flag);
  };

  Panel* CreatePanelWithParams(const CreatePanelParams& params);
  Panel* CreatePanelWithBounds(const std::string& panel_name,
                               const gfx::Rect& bounds);
  Panel* CreatePanel(const std::string& panel_name);

  Panel* CreateDockedPanel(const std::string& name, const gfx::Rect& bounds);
  Panel* CreateDetachedPanel(const std::string& name, const gfx::Rect& bounds);
  Panel* CreateStackedPanel(const std::string& name,
                            const gfx::Rect& bounds,
                            StackedPanelCollection* stack);

  Panel* CreateInactivePanel(const std::string& name);
  Panel* CreateInactiveDockedPanel(const std::string& name,
                                   const gfx::Rect& bounds);
  Panel* CreateInactiveDetachedPanel(const std::string& name,
                                     const gfx::Rect& bounds);

  void ActivatePanel(Panel* panel);
  void DeactivatePanel(Panel* panel);

  static NativePanelTesting* CreateNativePanelTesting(Panel* panel);

  void WaitForPanelActiveState(Panel* panel, ActiveState state);
  void WaitForBoundsAnimationFinished(Panel* panel);

  scoped_refptr<extensions::Extension> CreateExtension(
      const base::FilePath::StringType& path,
      extensions::Manifest::Location location,
      const base::DictionaryValue& extra_value);

  void MoveMouseAndWaitForExpansionStateChange(Panel* panel,
                                               const gfx::Point& position);
  static void MoveMouse(const gfx::Point& position);
  void CloseWindowAndWait(Panel* panel);
  static std::string MakePanelName(int index);

  // Checks if the WM supports activation. This may not be true sometimes on
  // buildbots for example when the wm has crashed.
  static bool WmSupportWindowActivation();

  MockDisplaySettingsProvider* mock_display_settings_provider() const {
    return mock_display_settings_provider_;
  }

  // Some tests might not want to use the mock version.
  void disable_display_settings_mock() {
    mock_display_settings_enabled_ = false;
  }

  static const base::FilePath::CharType* kTestDir;

 private:
  // Passed to and owned by PanelManager.
  MockDisplaySettingsProvider* mock_display_settings_provider_;

  bool mock_display_settings_enabled_;
};

#endif  // CHROME_BROWSER_UI_PANELS_BASE_PANEL_BROWSER_TEST_H_
