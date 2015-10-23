# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry import story


class LoFiPage(page_module.Page):
  """
  A test page for the chrome proxy Lo-Fi tests.
  Checks that the compressed image is below a certain threshold.
  """

  def __init__(self, url, page_set):
    super(LoFiPage, self).__init__(url=url, page_set=page_set)


class LoFiStorySet(story.StorySet):
  """ Chrome proxy test sites """

  def __init__(self):
    super(LoFiStorySet, self).__init__()

    urls_list = [
      'http://check.googlezip.net/lofi.png',
    ]

    for url in urls_list:
      self.AddStory(LoFiPage(url, self))
