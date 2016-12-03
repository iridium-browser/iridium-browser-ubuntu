// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

class WindowControlsTest : public extensions::PlatformAppBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(extensions::switches::kEnableAppWindowControls);
  }
  content::WebContents* GetWebContentsForExtensionWindow(
      const extensions::Extension* extension);
};

content::WebContents* WindowControlsTest::GetWebContentsForExtensionWindow(
    const extensions::Extension* extension) {
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile());

  // Lookup render view host for background page.
  const extensions::ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension->id());

  // Go through all active views, looking for the first window of the extension.
  for (content::RenderFrameHost* host : process_manager->GetAllFrames()) {
    // Filter out views not part of this extension
    if (process_manager->GetExtensionForRenderFrameHost(host) == extension) {
      // Filter out the background page view
      content::WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(host);
      if (web_contents != extension_host->web_contents())
        return web_contents;
    }
  }
  return nullptr;
}

IN_PROC_BROWSER_TEST_F(WindowControlsTest, CloseControlWorks) {
  // Launch app and wait for window to show up
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("window_controls/buttons", "window-opened");

  // Find WebContents of window
  content::WebContents* web_contents =
      GetWebContentsForExtensionWindow(extension);
  ASSERT_TRUE(web_contents != NULL);

  // Send a left click on the "Close" button and wait for the close action
  // to happen.
  ExtensionTestMessageListener window_closed("window-closed", false);

  // Send mouse click somewhere inside the [x] button
  const int controlOffset = 25;
  int x = web_contents->GetContainerBounds().size().width() - controlOffset;
  int y = controlOffset;
  content::SimulateMouseClickAt(web_contents,
                                0,
                                blink::WebMouseEvent::Button::Left,
                                gfx::Point(x, y));

  ASSERT_TRUE(window_closed.WaitUntilSatisfied());
}
