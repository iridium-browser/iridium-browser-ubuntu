# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class ToughLayoutCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughLayoutCasesPage, self).__init__(
        url=url, page_set=page_set, credentials_path = 'data/credentials.json',
        shared_page_state_class=shared_page_state.SharedDesktopPageState)
    self.archive_data_file = 'data/tough_layout_cases.json'


class ToughLayoutCasesPageSet(story.StorySet):

  """
  The slowest layouts observed in the alexa top 1 million sites in  July 2013.
  """

  def __init__(self):
    super(ToughLayoutCasesPageSet, self).__init__(
      archive_data_file='data/tough_layout_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    urls_list = [
      'http://oilevent.com',
      'http://www.muzoboss.ru',
      'http://natunkantha.com',
      'http://www.mossiella.com',
      'http://bookish.com',
      'http://mydiyclub.com',
      'http://amarchoti.blogspot.com',
      'http://picarisimo.es',
      'http://chinaapache.com',
      'http://indoritel.com'
    ]

    for url in urls_list:
      self.AddStory(ToughLayoutCasesPage(url, self))
