# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

from telemetry.core import discover
from telemetry.story import story_set


# Import all submodules' StorySet classes.
start_dir = os.path.dirname(os.path.abspath(__file__))
top_level_dir = os.path.dirname(start_dir)
base_class = story_set.StorySet
for cls in discover.DiscoverClasses(
    start_dir, top_level_dir, base_class).values():
  setattr(sys.modules[__name__], cls.__name__, cls)
