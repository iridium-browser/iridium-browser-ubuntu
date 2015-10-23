// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_H_

#include "chrome/browser/ssl/connection_security.h"
#include "components/toolbar/toolbar_model.h"

// This class is a //chrome-specific extension of the ToolbarModel interface.
// TODO(blundell): If connection_security::SecurityLevel gets componentized,
// GetSecurityLevel() can be folded into ToolbarModel and this class can go
// away. crbug.com/515071
class ChromeToolbarModel : public ToolbarModel {
 public:
  ~ChromeToolbarModel() override;

  // Returns the security level that the toolbar should display.  If
  // |ignore_editing| is true, the result reflects the underlying state of the
  // page without regard to any user edits that may be in progress in the
  // omnibox.
  virtual connection_security::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const = 0;

 protected:
  ChromeToolbarModel();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_H_
