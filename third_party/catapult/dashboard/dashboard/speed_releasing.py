# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides the speed releasing table."""

import collections
import json
import urllib

from google.appengine.ext import ndb

from dashboard.common import request_handler
from dashboard.common import utils
from dashboard.models import graph_data
from dashboard.models import table_config

# These represent the revision ranges per milestone. For Clank, this is a
# point id, for Chromium this is a Chromium commit position.
CLANK_MILESTONES = {
    54: (1473196450, 1475824394),
    55: (1475841673, 1479536199),
    56: (1479546161, 1485025126),
    57: (1485025126, None),
}

CHROMIUM_MILESTONES = {
    54: (416640, 423768),
    55: (433391, 433400),
    56: (433400, 445288),
    57: (445288, None),
}

CURRENT_MILESTONE = max(CHROMIUM_MILESTONES.keys())

class SpeedReleasingHandler(request_handler.RequestHandler):
  """Request handler for requests for speed releasing page."""

  def get(self, *args):  # pylint: disable=unused-argument
    """Renders the UI for the speed releasing page."""
    self.RenderStaticHtml('speed_releasing.html')

  def post(self, *args):
    """Returns dynamic data for /speed_releasing.

    Outputs:
      JSON for the /speed_releasing page XHR request.
    """
    if args[0]:
      self._OutputTableJSON(args[0])
    else:
      self._OutputHomePageJSON()

  def _OutputTableJSON(self, table_name):
    """Obtains the JSON values that comprise the table.

    Args:
      table_name: The name of the requested report.
    """
    table_entity = ndb.Key('TableConfig', table_name).get()
    if not table_entity:
      self.response.out.write(json.dumps({'error': 'Invalid table name.'}))
      return

    rev_a = self.request.get('revA')
    rev_b = self.request.get('revB')
    milestone_param = self.request.get('m')

    master_bot_pairs = _GetMasterBotPairs(table_entity.bots)

    if milestone_param:
      milestone_param = int(milestone_param)
      if milestone_param not in CHROMIUM_MILESTONES:
        self.response.out.write(json.dumps({
            'error': 'No data for that milestone.'}))
        return
      milestone_dict = _GetUpdatedMilestoneDict(master_bot_pairs,
                                                table_entity.tests)
      rev_a, rev_b = milestone_dict[milestone_param]

    if not rev_a or not rev_b: # If no milestone param and <2 revs passed in.
      milestone_dict = _GetUpdatedMilestoneDict(master_bot_pairs,
                                                table_entity.tests)
      rev_a, rev_b = _GetEndRevOrCurrentMilestoneRevs(
          rev_a, rev_b, milestone_dict)

    rev_a, rev_b = _CheckRevisions(rev_a, rev_b)
    revisions = [rev_b, rev_a] # In reverse intentionally. This is to support
    # the format of the Chrome Health Dashboard which compares 'Current' to
    # 'Reference', in that order. The ordering here is for display only.
    display_a = _GetDisplayRev(master_bot_pairs, table_entity.tests, rev_a)
    display_b = _GetDisplayRev(master_bot_pairs, table_entity.tests, rev_b)

    values = {}
    self.GetDynamicVariables(values)
    self.response.out.write(json.dumps({
        'xsrf_token': values['xsrf_token'],
        'table_bots': master_bot_pairs,
        'table_tests': table_entity.tests,
        'table_layout': json.loads(table_entity.table_layout),
        'name': table_entity.key.string_id(),
        'values': _GetRowValues(revisions, master_bot_pairs,
                                table_entity.tests),
        'units': _GetTestToUnitsMap(master_bot_pairs, table_entity.tests),
        'revisions': revisions,
        'categories': _GetCategoryCounts(json.loads(table_entity.table_layout)),
        'urls': _GetDashboardURLMap(master_bot_pairs, table_entity.tests,
                                    rev_a, rev_b),
        'display_revisions': [display_b, display_a] # Similar to revisions.
    }))

  def _OutputHomePageJSON(self):
    """Returns a list of reports a user has permission to see."""
    all_entities = table_config.TableConfig.query().fetch()
    list_of_entities = []
    for entity in all_entities:
      list_of_entities.append(entity.key.string_id())
    self.response.out.write(json.dumps({
        'show_list': True,
        'list': list_of_entities
    }))

def _GetMasterBotPairs(bots):
  master_bot_pairs = []
  for bot in bots:
    master_bot_pairs.append(bot.parent().string_id() + '/' + bot.string_id())
  return master_bot_pairs

def _GetRowValues(revisions, bots, tests):
  """Builds a nested dict organizing values by rev/bot/test.

  Args:
    revisions: The revisions to get values for.
    bots: The Master/Bot pairs the tables cover.
    tests: The tests that go in each table.

  Returns:
    A dict with the following structure:
    revisionA: {
      bot1: {
        test1: value,
        test2: value,
        ...
      }
      ...
    }
    revisionB: {
      ...
    }
  """
  row_values = {}
  for rev in revisions:
    bot_values = {}
    for bot in bots:
      test_values = {}
      for test in tests:
        test_values[test] = _GetRow(bot, test, rev)
      bot_values[bot] = test_values
    row_values[rev] = bot_values
  return row_values

def _GetTestToUnitsMap(bots, tests):
  """Grabs the units on each test for only one bot."""
  units_map = {}
  if bots:
    bot = bots[0]
  for test in tests:
    test_path = bot + '/' + test
    test_entity = utils.TestMetadataKey(test_path).get()
    if test_entity:
      units_map[test] = test_entity.units
  return units_map

def _GetRow(bot, test, rev):
  test_path = bot + '/' + test
  test_key = utils.TestKey(test_path)
  row_key = utils.GetRowKey(test_key, rev)
  row = row_key.get()
  if row:
    return row.value
  return None

def _CheckRevisions(rev_a, rev_b):
  """Checks to ensure the revisions are valid."""
  rev_a = int(rev_a)
  rev_b = int(rev_b)
  if rev_b < rev_a:
    rev_a, rev_b = rev_b, rev_a
  return rev_a, rev_b

def _GetCategoryCounts(layout):
  categories = collections.defaultdict(lambda: 0)
  for test in layout:
    categories[layout[test][0]] += 1
  return categories

def _GetDashboardURLMap(bots, tests, rev_a, rev_b):
  """Get the /report links appropriate for the bot and test."""
  url_mappings = {}
  for bot in bots:
    for test in tests:
      test_parts = test.split('/')
      bot_parts = bot.split('/')

      # Checked should be the last part of the test path, if available.
      checked = 'all'
      if len(test_parts) > 1:
        checked = test_parts[len(test_parts) - 1]
      url_args = {
          'masters': bot_parts[0],
          'bots': bot_parts[1],
          'tests': test,
          'checked': checked,
          'start_rev': rev_a,
          'end_rev': rev_b,
      }
      url_mappings[bot + '/' + test] = '?%s' % urllib.urlencode(url_args)
  return url_mappings

def _GetDisplayRev(bots, tests, rev):
  """Creates a user friendly commit position to display.
  For V8 and ChromiumPerf masters, this will just be the passed in rev.
  """
  if bots and tests:
    test_path = bots[0] + '/' + tests[0]
    test_key = utils.TestKey(test_path)
    row_key = utils.GetRowKey(test_key, rev)
    row = row_key.get()
    if row and hasattr(row, 'r_commit_pos'): # Rule out masters like V8
      if rev != row.r_commit_pos: # Rule out ChromiumPerf
        if hasattr(row, 'a_default_rev') and hasattr(row, row.a_default_rev):
          return row.r_commit_pos + '-' + getattr(row, row.a_default_rev)[:3]
  return rev

def _UpdateNewestRevInMilestoneDict(bots, tests, milestone_dict):
  """Updates the most recent rev in the milestone dict.

  The global milestone dicts are declared with 'None' for the end of the
  current milestone range. If we might be using the last milestone, update
  the end of the current milestone range to be the most recent revision.
  """
  if bots and tests:
    test_path = bots[0] + '/' + tests[0]
    test_key = utils.TestKey(test_path)
    query = graph_data.Row.query()
    query = query.filter(
        graph_data.Row.parent_test == utils.OldStyleTestKey(test_key))
    query = query.order(-graph_data.Row.revision)
    row = query.get()
    if row:
      milestone_dict[CURRENT_MILESTONE] = (
          milestone_dict[CURRENT_MILESTONE][0], row.revision)
    else:
      milestone_dict[CURRENT_MILESTONE] = (
          milestone_dict[CURRENT_MILESTONE][0],
          milestone_dict[CURRENT_MILESTONE][0])

def _GetEndOfMilestone(rev, milestone_dict):
  """Finds the end of the milestone that 'rev' is in.

  Check that 'rev' is between [beginning, end) of the tuple. In case an end
  'rev' is passed in, return corresponding beginning rev. But since revs can
  double as end and beginning, favor returning corresponding end rev if 'rev'
  is a beginning rev.
  """
  beginning_rev = 0
  for _, value_tuple in milestone_dict.iteritems():
    if value_tuple[0] <= int(rev) < value_tuple[1]: # 'rev' is a beginning rev.
      return value_tuple[1] # Favor by returning here.
    if value_tuple[1] == int(rev): # 'rev' is an end rev.
      beginning_rev = value_tuple[0]
  if beginning_rev:
    return beginning_rev
  return milestone_dict[CURRENT_MILESTONE][1]

def _GetEndRevOrCurrentMilestoneRevs(rev_a, rev_b, milestone_dict):
  """If one/both of the revisions are None, change accordingly.

  If both are None, return most recent milestone, present.
  If one is None, return the other, end of that milestone.
  """
  if not rev_a and not rev_b:
    return  milestone_dict[CURRENT_MILESTONE]
  return (rev_a or rev_b), _GetEndOfMilestone((rev_a or rev_b), milestone_dict)

def _GetUpdatedMilestoneDict(master_bot_pairs, tests):
  """Gets the milestone_dict with the newest rev.

  Checks to see which milestone_dict to use (Clank/Chromium), and updates
  the 'None' to be the newest revision for one of the specified tests.
  """
  masters = set([m.split('/')[0] for m in master_bot_pairs])
  if 'ClankInternal' in masters:
    milestone_dict = CLANK_MILESTONES.copy()
  else:
    milestone_dict = CHROMIUM_MILESTONES.copy()
  # If we might access the end of the milestone_dict, update it to
  # be the newest revision instead of 'None'.
  _UpdateNewestRevInMilestoneDict(master_bot_pairs,
                                  tests, milestone_dict)
  return milestone_dict
