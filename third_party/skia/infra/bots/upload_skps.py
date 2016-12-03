# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create a CL to update the SKP version."""


import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.getcwd(), 'common'))
# pylint:disable=F0401
from py.utils import git_utils



CHROMIUM_SKIA = 'https://chromium.googlesource.com/skia.git'
COMMIT_MSG = '''Update SKP version

Automatic commit by the RecreateSKPs bot.

TBR=
NO_MERGE_BUILDS
'''
SKIA_COMMITTER_EMAIL = 'skia.buildbots@gmail.com'
SKIA_COMMITTER_NAME = 'skia.buildbots'
SKIA_REPO = 'https://skia.googlesource.com/skia.git'


def main(target_dir):
  subprocess.check_call(['git', 'config', '--local', 'user.name',
                         SKIA_COMMITTER_NAME])
  subprocess.check_call(['git', 'config', '--local', 'user.email',
                         SKIA_COMMITTER_EMAIL])
  if CHROMIUM_SKIA in subprocess.check_output(['git', 'remote', '-v']):
    subprocess.check_call(['git', 'remote', 'set-url', 'origin', SKIA_REPO,
                           CHROMIUM_SKIA])

  # Download CIPD.
  cipd_sha1 = os.path.join(os.getcwd(), 'infra', 'bots', 'tools', 'luci-go',
                           'linux64', 'cipd.sha1')
  subprocess.check_call(['download_from_google_storage', '-s', cipd_sha1,
                         '--bucket', 'chromium-luci'])

  with git_utils.GitBranch(branch_name='update_skp_version',
                           commit_msg=COMMIT_MSG,
                           commit_queue=True):
    upload_script = os.path.join(
        os.getcwd(), 'infra', 'bots', 'assets', 'skp', 'upload.py')
    subprocess.check_call(['python', upload_script, '-t', target_dir])


if '__main__' == __name__:
  parser = argparse.ArgumentParser()
  parser.add_argument("--target_dir")
  args = parser.parse_args()
  main(args.target_dir)
