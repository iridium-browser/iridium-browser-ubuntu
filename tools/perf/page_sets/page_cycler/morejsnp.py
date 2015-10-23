# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story

class MorejsnpPage(page_module.Page):

  def __init__(self, url, page_set):
    super(MorejsnpPage, self).__init__(url=url, page_set=page_set)


class MorejsnpPageSet(story.StorySet):

  """ Morejsnp page_cycler benchmark """

  def __init__(self):
    super(MorejsnpPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/morejsnp']),
      cloud_storage_bucket=story.PARTNER_BUCKET)

    urls_list = [
      'file://../../../../data/page_cycler/morejsnp/blog.chromium.org/',
      'file://../../../../data/page_cycler/morejsnp/dev.chromium.org/',
      # pylint: disable=C0301
      'file://../../../../data/page_cycler/morejsnp/googleblog.blogspot.com1/',
      # pylint: disable=C0301
      'file://../../../../data/page_cycler/morejsnp/googleblog.blogspot.com2/',
      'file://../../../../data/page_cycler/morejsnp/test.blogspot.com/',
      'file://../../../../data/page_cycler/morejsnp/www.igoogle.com/',
      'file://../../../../data/page_cycler/morejsnp/www.techcrunch.com/',
      'file://../../../../data/page_cycler/morejsnp/www.webkit.org/',
      'file://../../../../data/page_cycler/morejsnp/www.yahoo.com/'
    ]

    for url in urls_list:
      self.AddStory(MorejsnpPage(url, self))
