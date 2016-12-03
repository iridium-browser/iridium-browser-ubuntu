# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Recipe for the Skia RecreateSKPs Bot."""


DEPS = [
  'build/file',
  'core',
  'depot_tools/gclient',
  'recipe_engine/path',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/step',
  'vars',
]


TEST_BUILDERS = {
  'client.skia.compile': {
    'skiabot-linux-swarm-000': [
      'Housekeeper-Nightly-RecreateSKPs_Canary',
      'Housekeeper-Weekly-RecreateSKPs',
    ],
  },
}


DEPOT_TOOLS_AUTH_TOKEN_FILE = '.depot_tools_oauth2_tokens'
DEPOT_TOOLS_AUTH_TOKEN_FILE_BACKUP = '.depot_tools_oauth2_tokens.old'
UPDATE_SKPS_KEY = 'depot_tools_auth_update_skps'


class depot_tools_auth(object):
  """Temporarily authenticate to depot_tools via GCE metadata."""
  def __init__(self, api, metadata_key):
    self.m = api
    self._key = metadata_key

  def __enter__(self):
    return self.m.python.inline(
        'depot-tools-auth login',
        """
import os
import urllib2

TOKEN_FILE = '%s'
TOKEN_FILE_BACKUP = '%s'
TOKEN_URL = 'http://metadata/computeMetadata/v1/project/attributes/%s'

req = urllib2.Request(TOKEN_URL, headers={'Metadata-Flavor': 'Google'})
contents = urllib2.urlopen(req).read()

home = os.path.expanduser('~')
token_file = os.path.join(home, TOKEN_FILE)
if os.path.isfile(token_file):
  os.rename(token_file, os.path.join(home, TOKEN_FILE_BACKUP))

with open(token_file, 'w') as f:
  f.write(contents)
        """ % (DEPOT_TOOLS_AUTH_TOKEN_FILE,
               DEPOT_TOOLS_AUTH_TOKEN_FILE_BACKUP,
               self._key),
    )

  def __exit__(self, t, v, tb):
    return self.m.python.inline(
        'depot-tools-auth logout',
        """
import os


TOKEN_FILE = '%s'
TOKEN_FILE_BACKUP = '%s'


home = os.path.expanduser('~')
token_file = os.path.join(home, TOKEN_FILE)
if os.path.isfile(token_file):
  os.remove(token_file)

backup_file = os.path.join(home, TOKEN_FILE_BACKUP)
if os.path.isfile(backup_file):
  os.rename(backup_file, token_file)
        """ % (DEPOT_TOOLS_AUTH_TOKEN_FILE,
               DEPOT_TOOLS_AUTH_TOKEN_FILE_BACKUP),
    )


def RunSteps(api):
  # Check out Chrome.
  api.core.setup()

  src_dir = api.vars.checkout_root.join('src')
  out_dir = src_dir.join('out', 'Release')

  # Call GN.
  platform = 'linux64'  # This bot only runs on linux; don't bother checking.
  gn = src_dir.join('buildtools', platform, 'gn')
  api.step('GN',
           [gn, 'gen', out_dir],
           env={'CPPFLAGS': '-DSK_ALLOW_CROSSPROCESS_PICTUREIMAGEFILTERS=1',
                'GYP_GENERATORS': 'ninja'},
           cwd=src_dir)
  # Build Chrome.
  api.step('Build Chrome',
           ['ninja', '-C', out_dir, 'chrome'],
           cwd=src_dir)

  # Download boto file (needed by recreate_skps.py) to tmp dir.
  boto_file = api.path['slave_build'].join('tmp', '.boto')
  api.python.inline(
      'download boto file',
      """
import os
import urllib2

BOTO_URL = 'http://metadata/computeMetadata/v1/project/attributes/boto-file'

dest_path = '%s'
dest_dir = os.path.dirname(dest_path)
if not os.path.exists(dest_dir):
  os.makedirs(dest_dir)

req = urllib2.Request(BOTO_URL, headers={'Metadata-Flavor': 'Google'})
contents = urllib2.urlopen(req).read()

with open(dest_path, 'w') as f:
  f.write(contents)
        """ % boto_file)

  # Clean up the output dir.
  output_dir = api.path['slave_build'].join('skp_output')
  if api.path.exists(output_dir):
    api.file.rmtree('skp_output', output_dir)
  api.file.makedirs('skp_output', output_dir)

  # Capture the SKPs.
  path_var= api.path.pathsep.join([str(api.path['depot_tools']), '%(PATH)s'])
  env = {
      'CHROME_HEADLESS': '1',
      'PATH': path_var,
  }
  boto_env = {
      'AWS_CREDENTIAL_FILE': boto_file,
      'BOTO_CONFIG': boto_file,
  }
  recreate_skps_env = {}
  recreate_skps_env.update(env)
  recreate_skps_env.update(boto_env)
  asset_dir = api.vars.infrabots_dir.join('assets', 'skp')
  cmd = ['python', asset_dir.join('create.py'),
         '--chrome_src_path', src_dir,
         '--browser_executable', src_dir.join('out', 'Release', 'chrome'),
         '--target_dir', output_dir]
  if 'Canary' not in api.properties['buildername']:
    cmd.append('--upload_to_partner_bucket')
  api.step('Recreate SKPs',
           cmd=cmd,
           cwd=api.vars.skia_dir,
           env=recreate_skps_env)

  # Upload the SKPs.
  if 'Canary' not in api.properties['buildername']:
    cmd = ['python',
           api.vars.skia_dir.join('infra', 'bots', 'upload_skps.py'),
           '--target_dir', output_dir]
    with depot_tools_auth(api, UPDATE_SKPS_KEY):
      api.step('Upload SKPs',
               cmd=cmd,
               cwd=api.vars.skia_dir,
               env=env)


def GenTests(api):
  for mastername, slaves in TEST_BUILDERS.iteritems():
    for slavename, builders_by_slave in slaves.iteritems():
      for builder in builders_by_slave:
        test = (
            api.test(builder) +
            api.properties(buildername=builder,
                           mastername=mastername,
                           slavename=slavename,
                           revision='abc123',
                           buildnumber=2,
                           path_config='kitchen',
                           swarm_out_dir='[SWARM_OUT_DIR]') +
            api.path.exists(api.path['slave_build'].join('skp_output'))
        )
        yield test
