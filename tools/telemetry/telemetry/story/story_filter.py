# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import re

from telemetry.internal.util import command_line


class _StoryMatcher(object):
  def __init__(self, pattern):
    self._regex = None
    self.has_compile_error = False
    if pattern:
      try:
        self._regex = re.compile(pattern)
      except re.error:
        self.has_compile_error = True

  def __nonzero__(self):
    return self._regex is not None

  def HasMatch(self, story):
    return self and bool(
        self._regex.search(story.display_name) or
        (story.name and self._regex.search(story.name)))


class _StoryLabelMatcher(object):
  def __init__(self, labels_str):
    self._labels = labels_str.split(',') if labels_str else None

  def __nonzero__(self):
    return self._labels is not None

  def HasLabelIn(self, story):
    return self and bool(story.labels.intersection(self._labels))


class StoryFilter(command_line.ArgumentHandlerMixIn):
  """Filters stories in the story set based on command-line flags."""

  @classmethod
  def AddCommandLineArgs(cls, parser):
    group = optparse.OptionGroup(parser, 'User story filtering options')
    group.add_option('--story-filter',
        help='Use only stories whose names match the given filter regexp.')
    group.add_option('--story-filter-exclude',
        help='Exclude stories whose names match the given filter regexp.')
    group.add_option('--story-label-filter',
        help='Use only stories that have any of these labels')
    group.add_option('--story-label-filter-exclude',
        help='Exclude stories that have any of these labels')
    parser.add_option_group(group)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    cls._include_regex = _StoryMatcher(args.story_filter)
    cls._exclude_regex = _StoryMatcher(args.story_filter_exclude)
    cls._include_labels = _StoryLabelMatcher(args.story_label_filter)
    cls._exclude_labels = _StoryLabelMatcher(args.story_label_filter_exclude)

    if cls._include_regex.has_compile_error:
      raise parser.error('--story-filter: Invalid regex.')
    if cls._exclude_regex.has_compile_error:
      raise parser.error('--story-filter-exclude: Invalid regex.')

  @classmethod
  def IsSelected(cls, story):
    # Exclude filters take priority.
    if cls._exclude_labels.HasLabelIn(story):
      return False
    if cls._exclude_regex.HasMatch(story):
      return False

    if cls._include_labels and not cls._include_labels.HasLabelIn(story):
      return False
    if cls._include_regex and not cls._include_regex.HasMatch(story):
      return False
    return True
