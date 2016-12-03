// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/sad_tab.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
class LabelButton;
}

///////////////////////////////////////////////////////////////////////////////
//
// SadTabView
//
//  A views::View subclass used to render the presentation of the crashed
//  "sad tab" in the browser window when a renderer is destroyed unnaturally.
//
///////////////////////////////////////////////////////////////////////////////
class SadTabView : public chrome::SadTab,
                   public views::View,
                   public views::LinkListener,
                   public views::ButtonListener {
 public:
  // Tag to denote which type of action button is displayed.
  enum ButtonTag {
    SAD_TAB_BUTTON_FEEDBACK,
    SAD_TAB_BUTTON_RELOAD,
  };

  SadTabView(content::WebContents* web_contents, chrome::SadTabKind kind);
  ~SadTabView() override;

  // Overridden from views::View:
  void Layout() override;

  // Overridden from views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

 protected:
  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // Overridden from chrome::SadTab:
  void Show() override;
  void Close() override;

  views::Label* CreateLabel(const base::string16& text);
  views::Link* CreateLink(const base::string16& text, const SkColor& color);

  content::WebContents* web_contents_;
  chrome::SadTabKind kind_;
  bool painted_;
  views::Label* message_;
  views::Link* help_link_;
  views::LabelButton* action_button_;
  views::Label* title_;
  views::StyledLabel* help_message_;
  static int total_crashes_;

  DISALLOW_COPY_AND_ASSIGN(SadTabView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SAD_TAB_VIEW_H__
