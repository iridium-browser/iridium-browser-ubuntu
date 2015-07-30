// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"

class Profile;

namespace content {
class PageNavigator;
}

class NetworkProfileBubbleView : public views::BubbleDelegateView,
                                 public views::ButtonListener,
                                 public views::LinkListener {
 public:
  NetworkProfileBubbleView(views::View* anchor,
                           content::PageNavigator* navigator,
                           Profile* profile);
 private:
  ~NetworkProfileBubbleView() override;

  // views::BubbleDelegateView:
  void Init() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // Used for loading pages.
  content::PageNavigator* navigator_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NetworkProfileBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NETWORK_PROFILE_BUBBLE_VIEW_H_
