# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughFiltersCasesPage(page_module.Page):

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Filter'):
      action_runner.Wait(10)


class PirateMarkPage(page_module.Page):

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Filter'):
      action_runner.EvaluateJavaScript(
          'document.getElementById("benchmarkButtonText").click()')
      action_runner.Wait(10)

class ToughFiltersCasesPageSet(page_set_module.PageSet):

  """
  Description: Self-driven filters animation examples
  """

  def __init__(self):
    super(ToughFiltersCasesPageSet, self).__init__(
      archive_data_file='data/tough_filters_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'http://letmespellitoutforyou.com/samples/svg/filter_terrain.svg',
      'http://static.bobdo.net/Analog_Clock.svg',
    ]

    for url in urls_list:
      self.AddUserStory(ToughFiltersCasesPage(url, self))

    self.AddUserStory(PirateMarkPage(
        'http://ie.microsoft.com/testdrive/Performance/Pirates/', self))
