# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides fakes for several of Telemetry's internal objects.

These allow code like story_runner and Benchmark to be run and tested
without compiling or starting a browser. Class names prepended with an
underscore are intended to be implementation details, and should not
be subclassed; however, some, like _FakeBrowser, have public APIs that
may need to be called in tests.
"""

from telemetry.internal.browser import browser_options
from telemetry.internal.platform import system_info
from telemetry.page import shared_page_state
from telemetry.testing.internal import fake_gpu_info

# Classes and functions which are intended to be part of the public
# fakes API.

class FakePlatform(object):
  @property
  def is_host_platform(self):
    raise NotImplementedError

  @property
  def network_controller(self):
    return _FakeNetworkController()

  @property
  def tracing_controller(self):
    return None

  def CanMonitorThermalThrottling(self):
    return False

  def IsThermallyThrottled(self):
    return False

  def HasBeenThermallyThrottled(self):
    return False

  def GetDeviceTypeName(self):
    raise NotImplementedError

  def GetArchName(self):
    raise NotImplementedError

  def GetOSName(self):
    raise NotImplementedError

  def GetOSVersionName(self):
    raise NotImplementedError


class FakeLinuxPlatform(FakePlatform):
  @property
  def is_host_platform(self):
    return True

  def GetDeviceTypeName(self):
    return 'Desktop'

  def GetArchName(self):
    return 'x86_64'

  def GetOSName(self):
    return 'linux'

  def GetOSVersionName(self):
    return 'trusty'


class FakePossibleBrowser(object):
  def __init__(self):
    self._returned_browser = _FakeBrowser(FakeLinuxPlatform())

  @property
  def returned_browser(self):
    """The browser object that will be returned through later API calls."""
    return self._returned_browser

  def Create(self, finder_options):
    return self.returned_browser

  @property
  def platform(self):
    """The platform object from the returned browser.

    To change this or set it up, change the returned browser's
    platform.
    """
    return self.returned_browser.platform

  def SetCredentialsPath(self, _):
    pass


class FakeSharedPageState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(FakeSharedPageState, self).__init__(test, finder_options, story_set)

  def _GetPossibleBrowser(self, test, finder_options):
    p = FakePossibleBrowser()
    self.ConfigurePossibleBrowser(p)
    return p

  def ConfigurePossibleBrowser(self, possible_browser):
    """Override this to configure the PossibleBrowser.

    Can make changes to the browser's configuration here via e.g.:
       possible_browser.returned_browser.returned_system_info = ...
    """
    pass


  def DidRunStory(self, results):
    # TODO(kbr): add a test which throws an exception from DidRunStory
    # to verify the fix from https://crrev.com/86984d5fc56ce00e7b37ebe .
    super(FakeSharedPageState, self).DidRunStory(results)


class FakeSystemInfo(system_info.SystemInfo):
  def __init__(self, model_name='', gpu_dict=None):
    if gpu_dict == None:
      gpu_dict = fake_gpu_info.FAKE_GPU_INFO
    super(FakeSystemInfo, self).__init__(model_name, gpu_dict)


def CreateBrowserFinderOptions(browser_type=None):
  """Creates fake browser finder options for discovering a browser."""
  return browser_options.BrowserFinderOptions(browser_type=browser_type)


# Internal classes. Note that end users may still need to both call
# and mock out methods of these classes, but they should not be
# subclassed.

class _FakeBrowser(object):
  def __init__(self, platform):
    self._tabs = _FakeTabList(self)
    self._returned_system_info = FakeSystemInfo()
    self._platform = platform

  @property
  def platform(self):
    return self._platform

  @platform.setter
  def platform(self, incoming):
    """Allows overriding of the fake browser's platform object."""
    assert isinstance(incoming, FakePlatform)
    self._platform = incoming

  @property
  def returned_system_info(self):
    """The object which will be returned from calls to GetSystemInfo."""
    return self._returned_system_info

  @returned_system_info.setter
  def returned_system_info(self, incoming):
    """Allows overriding of the returned SystemInfo object.

    Incoming argument must be an instance of FakeSystemInfo."""
    assert isinstance(incoming, FakeSystemInfo)
    self._returned_system_info = incoming

  @property
  def credentials(self):
    return _FakeCredentials()

  def Close(self):
    pass

  @property
  def supports_system_info(self):
    return True

  def GetSystemInfo(self):
    return self.returned_system_info

  @property
  def supports_tab_control(self):
    return True

  @property
  def tabs(self):
    return self._tabs


class _FakeCredentials(object):
  def WarnIfMissingCredentials(self, _):
    pass


class _FakeNetworkController(object):
  def SetReplayArgs(self, *args, **kwargs):
    pass

  def UpdateReplayForExistingBrowser(self):
    pass


class _FakeTab(object):
  def __init__(self, browser, tab_id):
    self._browser = browser
    self._tab_id = str(tab_id)

  @property
  def id(self):
    return self._tab_id

  @property
  def browser(self):
    return self._browser

  def WaitForDocumentReadyStateToBeComplete(self, timeout=0):
    pass

  def Navigate(self, url, script_to_evaluate_on_commit=None,
               timeout=0):
    pass

  def WaitForDocumentReadyStateToBeInteractiveOrBetter(self, timeout=0):
    pass

  def IsAlive(self):
    return True

  def CloseConnections(self):
    pass

  def Close(self):
    pass


class _FakeTabList(object):
  _current_tab_id = 0

  def __init__(self, browser):
    self._tabs = []
    self._browser = browser

  def New(self, timeout=300):
    type(self)._current_tab_id += 1
    t = _FakeTab(self._browser, type(self)._current_tab_id)
    self._tabs.append(t)
    return t

  def __iter__(self):
    return self._tabs.__iter__()

  def __len__(self):
    return len(self._tabs)

  def __getitem__(self, index):
    return self._tabs[index]

  def GetTabById(self, identifier):
    """The identifier of a tab can be accessed with tab.id."""
    for tab in self._tabs:
      if tab.id == identifier:
        return tab
    return None
