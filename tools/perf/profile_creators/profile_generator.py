# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handles generating profiles and transferring them to/from mobile devices."""

import logging
import optparse
import os
import shutil
import stat
import sys
import tempfile

from profile_creators import profile_extender
from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.core import util
from telemetry.internal import story_runner


def _DiscoverProfileExtenderClasses():
  profile_extenders_dir = os.path.abspath(os.path.join(util.GetBaseDir(),
      os.pardir, 'perf', 'profile_creators'))
  base_dir = os.path.abspath(os.path.join(profile_extenders_dir, os.pardir))

  profile_extenders_unfiltered = discover.DiscoverClasses(
      profile_extenders_dir, base_dir, profile_extender.ProfileExtender)

  # Remove 'extender' suffix from keys.
  profile_extenders = {}
  for test_name, test_class in profile_extenders_unfiltered.iteritems():
    assert test_name.endswith('_extender')
    test_name = test_name[:-len('_extender')]
    profile_extenders[test_name] = test_class
  return profile_extenders


def _IsPseudoFile(directory, paths):
  """Filter function for shutil.copytree() to reject socket files and symlinks
  since those can't be copied around on bots."""
  def IsSocket(full_path):
    """Check if a file at a given path is a socket."""
    try:
      if stat.S_ISSOCK(os.stat(full_path).st_mode):
        return True
    except OSError:
      # Thrown if we encounter a broken symlink.
      pass
    return False

  ignore_list = []
  for path in paths:
    full_path = os.path.join(directory, path)

    if os.path.isdir(full_path):
      continue
    if not IsSocket(full_path) and not os.path.islink(full_path):
      continue

    logging.warning('Ignoring pseudo file: %s' % full_path)
    ignore_list.append(path)

  return ignore_list


def GenerateProfiles(profile_extender_class, profile_creator_name, options):
  """Generate a profile"""

  temp_output_directory = tempfile.mkdtemp()
  options.output_profile_path = temp_output_directory

  profile_creator_instance = profile_extender_class(options)
  try:
    profile_creator_instance.Run()
  except Exception as e:
    logging.exception('Profile creation failed.')
    shutil.rmtree(temp_output_directory)
    raise e

  # Everything is a-ok, move results to final destination.
  generated_profiles_dir = os.path.abspath(options.output_dir)
  if not os.path.exists(generated_profiles_dir):
    os.makedirs(generated_profiles_dir)
  out_path = os.path.join(generated_profiles_dir, profile_creator_name)
  if os.path.exists(out_path):
    shutil.rmtree(out_path)

  shutil.copytree(temp_output_directory, out_path, ignore=_IsPseudoFile)
  shutil.rmtree(temp_output_directory)
  sys.stderr.write("SUCCESS: Generated profile copied to: '%s'.\n" % out_path)

  return 0


def AddCommandLineArgs(parser):
  story_runner.AddCommandLineArgs(parser)

  profile_extenders = _DiscoverProfileExtenderClasses().keys()
  legal_profile_creators = '|'.join(profile_extenders)
  group = optparse.OptionGroup(parser, 'Profile generation options')
  group.add_option('--profile-type-to-generate',
      dest='profile_type_to_generate',
      default=None,
      help='Type of profile to generate. '
           'Supported values: %s' % legal_profile_creators)
  parser.add_option_group(group)


def ProcessCommandLineArgs(parser, args):
  story_runner.ProcessCommandLineArgs(parser, args)

  if not args.profile_type_to_generate:
    parser.error("Must specify --profile-type-to-generate option.")

  profile_extenders = _DiscoverProfileExtenderClasses().keys()
  if args.profile_type_to_generate not in profile_extenders:
    legal_profile_creators = '|'.join(profile_extenders)
    parser.error("Invalid profile type, legal values are: %s." %
        legal_profile_creators)

  if not args.browser_type:
    parser.error("Must specify --browser option.")

  if not args.output_dir:
    parser.error("Must specify --output-dir option.")

  if args.browser_options.dont_override_profile:
    parser.error("Can't use existing profile when generating profile.")


def Main():
  options = browser_options.BrowserFinderOptions()
  parser = options.CreateParser(
      "%%prog <--profile-type-to-generate=...> <--browser=...> <--output-dir>")
  AddCommandLineArgs(parser)
  _, _ = parser.parse_args()
  ProcessCommandLineArgs(parser, options)

  # Generate profile.
  profile_extenders = _DiscoverProfileExtenderClasses()
  profile_extender_class = profile_extenders[options.profile_type_to_generate]
  return GenerateProfiles(profile_extender_class,
      options.profile_type_to_generate, options)
