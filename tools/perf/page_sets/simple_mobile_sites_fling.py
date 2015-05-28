# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from page_sets import fling_gesture_supported_shared_state

from telemetry.page import shared_page_state
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SimpleFlingPage(page_module.Page):

  def __init__(self, url, page_set):
    super(SimpleFlingPage, self).__init__(
        url=url,
        page_set=page_set,
        credentials_path='data/credentials.json',
        shared_page_state_class=(fling_gesture_supported_shared_state\
            .FlingGestureSupportedSharedState))
    self.archive_data_file = 'data/simple_mobile_sites.json'

  def RunNavigateSteps(self, action_runner):
    super(SimpleFlingPage, self).RunNavigateSteps(action_runner)
    # TODO(epenner): Remove this wait (http://crbug.com/366933)
    action_runner.Wait(5)

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction('FlingAction')
    # Swiping up induces a downward fling, and 500 pixels of touch scrolling
    # runway ensures consistent fling velocities.
    action_runner.SwipePage(direction='up',
                            distance='500',
                            speed_in_pixels_per_second=5000)
    interaction.End()

class SimpleMobileSitesFlingPageSet(page_set_module.PageSet):

  """ Simple mobile sites """

  def __init__(self):
    super(SimpleMobileSitesFlingPageSet, self).__init__(
      user_agent_type='tablet_10_inch',
      archive_data_file='data/simple_mobile_sites.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    fling_page_list = [
      # Why: Scrolls moderately complex pages (up to 60 layers)
      'http://www.ebay.co.uk/',
      'https://www.flickr.com/',
      'http://www.apple.com/mac/',
      'http://www.nyc.gov',
      'http://m.nytimes.com/'
    ]

    for url in fling_page_list:
      self.AddUserStory(SimpleFlingPage(url, self))

