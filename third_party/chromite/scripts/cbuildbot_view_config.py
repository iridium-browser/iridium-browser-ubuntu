# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for dumping and/or comparing build config contents."""

from __future__ import print_function

import json
import pprint

from chromite.cbuildbot import generate_chromeos_config
from chromite.lib import commandline
from chromite.lib import cros_build_lib


class _JSONEncoder(json.JSONEncoder):
  """Json Encoder that encodes objects as their dictionaries."""
  # pylint: disable=E0202
  def default(self, obj):
    return self.encode(obj.__dict__)


def _InjectDisplayPosition(config):
  """Add field to help buildbot masters order builders on the waterfall.

  Args:
    config: A dict of build config items.

  Returns:
    A similar config where each config item has a new 'display_position' value.
  """
  def _GetSortKey(items):
    my_config = items[1]
    # Allow configs to override the display_position.
    return (my_config.get('display_position', 1000000),
            generate_chromeos_config.GetDisplayPosition(my_config['name']),
            my_config['internal'], my_config['vm_tests'])

  source = sorted(config.iteritems(), key=_GetSortKey)
  return dict((name, dict(value.items() + [('display_position', idx)]))
              for idx, (name, value) in enumerate(source))


def _DumpConfigJson(cfg):
  """Dump |cfg| contents in JSON format.

  Args:
    cfg: A single build config.
  """
  print(json.dumps(cfg, cls=_JSONEncoder))


def _HideDefaults(cfg, default):
  """Hide the defaults from a given config entry.

  Args:
    cfg: A config entry.
    default: Default values to hide, if matched.

  Returns:
    The same config entry, but without any defaults.
  """
  d = {}
  for k, v in cfg.iteritems():
    if default.get(k) != v:
      if k == 'child_configs':
        d['child_configs'] = [_HideDefaults(child, default) for child in v]
      else:
        d[k] = v

  return d


def _DumpConfigPrettyJson(cfg):
  """Dump |cfg| contents in pretty JSON format.

  Args:
    cfg: A single build config.
  """
  print(json.dumps(cfg, cls=_JSONEncoder,
                   sort_keys=True, indent=4, separators=(',', ': ')))


def _DumpConfigPrettyPrint(cfg):
  """Dump |cfg| contents in pretty printer format.

  Args:
    cfg: A single build config.
  """
  pretty_printer = pprint.PrettyPrinter(indent=2)
  pretty_printer.pprint(cfg)


def _CompareConfig(old_cfg, new_cfg):
  """Compare two build configs targets, printing results.

  Args:
    old_cfg: The 'from' build config for comparison.
    new_cfg: The 'to' build config for comparison.
  """
  new_cfg = json.loads(json.dumps(new_cfg, cls=_JSONEncoder))
  for key in sorted(set(new_cfg.keys() + old_cfg.keys())):
    obj1, obj2 = old_cfg.get(key), new_cfg.get(key)
    if obj1 == obj2:
      continue
    elif obj1 is None:
      print('%s: added to config\n' % (key,))
      continue
    elif obj2 is None:
      print('%s: removed from config\n' % (key,))
      continue

    print('%s:' % (key,))

    for subkey in sorted(set(obj1.keys() + obj2.keys())):
      sobj1, sobj2 = obj1.get(subkey), obj2.get(subkey)
      if sobj1 != sobj2:
        print(' %s: %r, %r' % (subkey, sobj1, sobj2))

    print()


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  # Put options that control the mode of script into mutually exclusive group.
  mode = parser.add_mutually_exclusive_group(required=True)
  mode.add_argument('-c', '--compare', action='store',
                    type=commandline.argparse.FileType('rb'),
                    default=None, metavar='file_name',
                    help='Compare current config against a saved on disk '
                         'serialized (json) dump of a config.')
  mode.add_argument('-d', '--dump', action='store_true', default=False,
                    help='Dump the configs in JSON format.')

  parser.add_argument('--pretty', action='store_true', default=False,
                      help='If dumping, make json output human readable.')
  parser.add_argument('--for-buildbot', action='store_true', default=False,
                      help='Include the display position in data.')
  parser.add_argument('-s', '--separate-defaults', action='store_true',
                      default=False, help='Show the defaults separately.')
  parser.add_argument('config_targets', metavar='config_target', nargs='*',
                      help='Name of a cbuildbot config target.')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  if options.pretty and not options.dump:
    parser.error('The --pretty option does not make sense without --dump')

  # Possibly translate config contents first.
  convert = lambda x: x
  if options.for_buildbot:
    convert = _InjectDisplayPosition

  config = convert(generate_chromeos_config.GetConfig())

  # Separate the defaults and show them at the top. We prefix the name with
  # an underscore so that it sorts to the top.
  if options.separate_defaults:
    default = generate_chromeos_config.GetDefault()
    for k, v in config.iteritems():
      config[k] = _HideDefaults(v, default)
    config['_default'] = default

  # If config_targets specified, only dump/load those.
  if options.config_targets:
    temp_config = dict()
    for c in options.config_targets:
      try:
        temp_config[c] = config[c]
      except KeyError:
        cros_build_lib.Die('No such config id: %s', c)

    config = temp_config

  if config:
    if options.dump:
      if options.pretty:
        _DumpConfigPrettyJson(config)
      else:
        _DumpConfigJson(config)
    elif options.compare:
      # Load the previously saved build config for comparison.
      old_cfg = convert(json.load(options.compare))
      _CompareConfig(old_cfg, config)

  return 0
