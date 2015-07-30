# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ExpDirectivePage(page_module.Page):
  """A test page for the experiment Chrome-Proxy directive tests."""

  def __init__(self, url, page_set):
    super(ExpDirectivePage, self).__init__(url=url, page_set=page_set)


class ExpDirectivePageSet(page_set_module.PageSet):
  """ Chrome proxy test sites """

  def __init__(self):
    super(ExpDirectivePageSet, self).__init__()

    urls_list = [
      'http://aws1.mdw.la/exptest/',
    ]

    for url in urls_list:
      self.AddUserStory(ExpDirectivePage(url, self))
