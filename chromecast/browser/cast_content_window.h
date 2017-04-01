// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/event.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace chromecast {
namespace shell {

// Class that represents the "window" a WebContents is displayed in cast_shell.
// For Linux, this represents an Aura window. For Android, this is a Activity.
// See CastContentWindowLinux and CastContentWindowAndroid.
class CastContentWindow {
 public:
  class Delegate {
   public:
    virtual void OnWindowDestroyed() = 0;
    virtual void OnKeyEvent(const ui::KeyEvent& key_event) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates the platform specific CastContentWindow. |delegate| should outlive
  // the created CastContentWindow.
  static std::unique_ptr<CastContentWindow> Create(
      CastContentWindow::Delegate* delegate);

  virtual ~CastContentWindow() {}

  // Sets the window's background to be transparent (call before
  // CreateWindowTree).
  virtual void SetTransparent() = 0;

  // Creates a full-screen window for |web_contents| and display it.
  // |web_contents| should outlive this CastContentWindow.
  virtual void ShowWebContents(content::WebContents* web_contents) = 0;

  // Creates a WebContents.
  // TODO(derekjchow): remove this function from this class, since it doesn't
  // have anything to do with displaying web_contents.
  virtual std::unique_ptr<content::WebContents> CreateWebContents(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_H_
