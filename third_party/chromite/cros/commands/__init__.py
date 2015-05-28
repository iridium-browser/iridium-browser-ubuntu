# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This package contains all valid cros commands and their unittests.

All commands can be either imported directly or looked up using this module.
This modules contains a dictionary called commands mapping
command names -> command classes e.g. image->cros_image.ImageCommand.
"""

from __future__ import print_function

import glob
import os

from chromite import cros
from chromite.lib import cros_import


def _FindModules(subdir_path):
  """Returns a list of all the relevant python modules in |sub_dir_path|"""
  # We only load cros_[!unittest] modules.
  modules = []
  for file_path in glob.glob(subdir_path + '/cros_*.py'):
    if not file_path.endswith('_unittest.py'):
      modules.append(file_path)

  return modules


def _ImportCommands():
  """Directly imports all cros_[!unittest] python modules.

  This method imports the cros_[!unittest] modules which may contain
  commands. When these modules are loaded, declared commands (those that use
  the cros.CommandDecorator) will automatically get added to cros._commands.
  """
  subdir_path = os.path.dirname(__file__)
  for file_path in _FindModules(subdir_path):
    file_name = os.path.basename(file_path)
    mod_name = os.path.splitext(file_name)[0]
    cros_import.ImportModule(('chromite', 'cros', 'commands', mod_name))


def ListCommands():
  """Return a dictionary mapping command names to classes."""
  _ImportCommands()
  # pylint: disable=protected-access
  return cros._commands.copy()
