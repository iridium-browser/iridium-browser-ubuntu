#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module fetches and prints the dependencies given a benchmark."""

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, 'telemetry'))

from catapult_base import cloud_storage
from telemetry import benchmark_runner


def _GetPerfDir(*subdirs):
  perf_dir = os.path.realpath(os.path.dirname(__file__))
  return os.path.join(perf_dir, *subdirs)


def GetChromiumDir():
  return _GetPerfDir(os.path.pardir, os.path.pardir, os.path.pardir)


def _FetchDependenciesIfNeeded(story_set):
  """ Download files needed by a user story set. """
  # Download files in serving_dirs.
  serving_dirs = story_set.serving_dirs
  for directory in serving_dirs:
    cloud_storage.GetFilesInDirectoryIfChanged(directory, story_set.bucket)

  # Download WPR files.
  if any(not story.is_local for story in story_set):
    story_set.wpr_archive_info.DownloadArchivesIfNeeded()


def _EnumerateDependencies(story_set):
  """ Enumerate pathes of files needed by a user story set. """
  deps = set()
  # Enumerate WPRs
  for story in story_set:
    deps.add(story_set.WprFilePathForStory(story))

  # Enumerate files in serving_dirs
  for directory in story_set.serving_dirs:
    if not os.path.isdir(directory):
      raise ValueError('Must provide a valid directory.')
    # Don't allow the root directory to be a serving_dir.
    if directory == os.path.abspath(os.sep):
      raise ValueError('Trying to serve root directory from HTTP server.')
    for dirpath, _, filenames in os.walk(directory):
      for filename in filenames:
        path_name, extension = os.path.splitext(
            os.path.join(dirpath, filename))
        if extension == '.sha1':
          deps.add(path_name)

  # Return relative pathes.
  prefix_len = len(os.path.realpath(GetChromiumDir())) + 1
  return [dep[prefix_len:] for dep in deps if dep]


def _show_usage():
  print ('Usage: %s benchmark_name\n'
         'Fetch the dependencies of benchmark_name.' % sys.argv[0])


def main(output=sys.stdout):
  config = benchmark_runner.ProjectConfig(
      top_level_dir=_GetPerfDir(),
      benchmark_dirs=[_GetPerfDir('benchmarks')])

  name = sys.argv[1]
  benchmark = benchmark_runner.GetBenchmarkByName(name, config)
  if not benchmark:
    raise ValueError('No such benchmark: %s' % name)

  # Download files according to specified benchmark.
  story_set = benchmark().CreateStorySet(None)

  _FetchDependenciesIfNeeded(story_set)

  # Print files downloaded.
  deps = _EnumerateDependencies(story_set)
  for dep in deps:
    print >> output, dep


if __name__ == '__main__':
  if len(sys.argv) != 2 or sys.argv[1][0] == '-':
    _show_usage()
  else:
    main()
