# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import util


# TODO(dtu): Move these functions from core.util to here.
GetBaseDir = util.GetBaseDir
GetTelemetryDir = util.GetTelemetryDir
GetUnittestDataDir = util.GetUnittestDataDir
GetChromiumSrcDir = util.GetChromiumSrcDir
AddDirToPythonPath = util.AddDirToPythonPath
GetBuildDirectories = util.GetBuildDirectories


def IsExecutable(path):
  return os.path.isfile(path) and os.access(path, os.X_OK)


def FindInstalledWindowsApplication(application_path):
  """Search common Windows installation directories for an application.

  Args:
    application_path: Path to application relative from installation location.
  Returns:
    A string representing the full path, or None if not found.
  """
  search_paths = [os.getenv('PROGRAMFILES(X86)'),
                  os.getenv('PROGRAMFILES'),
                  os.getenv('LOCALAPPDATA')]
  search_paths += os.getenv('PATH', '').split(os.pathsep)

  for search_path in search_paths:
    if not search_path:
      continue
    path = os.path.join(search_path, application_path)
    if IsExecutable(path):
      return path

  return None


def IsSubpath(subpath, superpath):
  """Returns True iff subpath is or is in superpath."""
  subpath = os.path.realpath(subpath)
  superpath = os.path.realpath(superpath)

  while len(subpath) >= len(superpath):
    if subpath == superpath:
      return True
    subpath = os.path.split(subpath)[0]
  return False


def ListFiles(base_directory, should_include_dir=lambda _: True,
              should_include_file=lambda _: True):
  matching_files = []
  for root, dirs, files in os.walk(base_directory):
    dirs[:] = [dir_name for dir_name in dirs if should_include_dir(dir_name)]
    matching_files += [os.path.join(root, file_name)
                       for file_name in files if should_include_file(file_name)]
  return sorted(matching_files)
