# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughPathRenderingCasesPage(page_module.Page):

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.Wait(10)


class ChalkboardPage(page_module.Page):

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ClickStart'):
      action_runner.EvaluateJavaScript(
          'document.getElementById("StartButton").click()')
      action_runner.Wait(20)

class ToughPathRenderingCasesPageSet(page_set_module.PageSet):

  """
  Description: Self-driven path rendering examples
  """

  def __init__(self):
    super(ToughPathRenderingCasesPageSet, self).__init__(
      archive_data_file='data/tough_path_rendering_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'http://www.craftymind.com/factory/guimark2/HTML5ChartingTest.html'
    ]

    for url in urls_list:
      self.AddUserStory(ToughPathRenderingCasesPage(url, self))

    self.AddUserStory(ChalkboardPage(
        'http://ie.microsoft.com/testdrive/Performance/Chalkboard/', self))
