#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions used by Generational and Mutational ClusterFuzz
fuzzers."""

import argparse
import os
import random
import string
import sys
import tempfile

APP_PATH_KEY = 'APP_PATH'
FLAGS_PREFIX = 'flags-'
FUZZ_PREFIX = 'fuzz-'
IPC_FUZZER_APPLICATION = 'ipc_fuzzer'
IPC_REPLAY_APPLICATION = 'ipc_fuzzer_replay'
IPCDUMP_EXTENSION = '.ipcdump'
LAUNCH_PREFIXES = [
  '--gpu-launcher',
  '--plugin-launcher',
  '--ppapi-plugin-launcher',
  '--renderer-cmd-prefix',
  '--utility-cmd-prefix',
]

def application_name_for_platform(application_name):
  """Return application name for current platform."""
  if platform() == 'WINDOWS':
    return application_name + '.exe'
  return application_name

def create_flags_file(ipcdump_testcase_path, ipc_replay_application_path):
  """Create a flags file to add launch prefix to application command line."""
  random_launch_prefix = random.choice(LAUNCH_PREFIXES)
  file_content = '%s=%s' % (random_launch_prefix, ipc_replay_application_path)

  flags_file_path = ipcdump_testcase_path.replace(FUZZ_PREFIX, FLAGS_PREFIX)
  file_handle = open(flags_file_path, 'w')
  file_handle.write(file_content)
  file_handle.close()

def create_temp_file():
  """Create a temporary file."""
  temp_file = tempfile.NamedTemporaryFile(delete=False)
  temp_file.close()
  return temp_file.name

def get_fuzzer_application_name():
  """Get the application name for the fuzzer binary."""
  return application_name_for_platform(IPC_FUZZER_APPLICATION)

def get_replay_application_name():
  """Get the application name for the replay binary."""
  return application_name_for_platform(IPC_REPLAY_APPLICATION)

def parse_arguments():
  """Parse fuzzer arguments."""
  parser = argparse.ArgumentParser()
  parser.add_argument('--input_dir')
  parser.add_argument('--output_dir')
  parser.add_argument('--no_of_files', type=int)
  args = parser.parse_args();
  if (not args.input_dir or
      not args.output_dir or
      not args.no_of_files):
    parser.print_help()
    sys.exit(1)

  return args

def random_id(size=16, chars=string.ascii_lowercase):
  """Return a random id string, default 16 characters long."""
  return ''.join(random.choice(chars) for _ in range(size))

def random_ipcdump_testcase_path(ipcdump_directory):
  """Return a random ipc testcase path."""
  return os.path.join(
      ipcdump_directory,
      '%s%s%s' % (FUZZ_PREFIX, random_id(), IPCDUMP_EXTENSION))

def platform():
  """Return running platform."""
  if sys.platform.startswith('win'):
    return 'WINDOWS'
  if sys.platform.startswith('linux'):
    return 'LINUX'
  if sys.platform == 'darwin':
    return 'MAC'

  assert False, 'Unknown platform'

def get_application_path():
  """Return chrome application path."""
  if APP_PATH_KEY not in os.environ:
    sys.exit(
        'Environment variable %s should be set to chrome path.' % APP_PATH_KEY)

  return os.environ[APP_PATH_KEY]
