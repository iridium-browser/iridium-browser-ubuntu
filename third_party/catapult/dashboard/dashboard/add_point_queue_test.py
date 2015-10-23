# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for add_point module."""

import unittest

from dashboard import add_point_queue
from dashboard import testing_common
from dashboard import utils
from dashboard.models import graph_data
from dashboard.models import stoppage_alert


class GetOrCreateAncestorsTest(testing_common.TestCase):

  def setUp(self):
    super(GetOrCreateAncestorsTest, self).setUp()
    self.SetCurrentUser('foo@bar.com', is_admin=True)

  def testGetOrCreateAncestors_GetsExistingEntities(self):
    """Tests that _GetOrCreateAncestors doesn't add if entity already exists."""
    master_key = graph_data.Master(id='ChromiumPerf', parent=None).put()
    bot_key = graph_data.Bot(id='win7', parent=master_key).put()
    suite_key = graph_data.Test(id='dromaeo', parent=bot_key).put()
    subtest_key = graph_data.Test(id='dom', parent=suite_key).put()
    graph_data.Test(id='modify', parent=subtest_key).put()
    actual_parent = add_point_queue._GetOrCreateAncestors(
        'ChromiumPerf', 'win7', 'dromaeo/dom/modify')
    self.assertEqual('modify', actual_parent.key.id())
    # No extra Test or Bot objects should have been added to the database
    # beyond the four that were put in before the _GetOrCreateAncestors call.
    self.assertEqual(1, len(graph_data.Master.query().fetch()))
    self.assertEqual(1, len(graph_data.Bot.query().fetch()))
    self.assertEqual(3, len(graph_data.Test.query().fetch()))

  def testGetOrCreateAncestors_CreatesAllExpectedEntities(self):
    """Tests that _GetOrCreateAncestors adds if entity already exists."""
    parent = add_point_queue._GetOrCreateAncestors(
        'ChromiumPerf', 'win7', 'dromaeo/dom/modify')
    self.assertEqual('modify', parent.key.id())
    # Check that all the Bot and Test entities were correctly added.
    created_masters = graph_data.Master.query().fetch()
    created_bots = graph_data.Bot.query().fetch()
    created_tests = graph_data.Test.query().fetch()
    self.assertEqual(1, len(created_masters))
    self.assertEqual(1, len(created_bots))
    self.assertEqual(3, len(created_tests))
    self.assertEqual('ChromiumPerf', created_masters[0].key.id())
    self.assertIsNone(created_masters[0].key.parent())
    self.assertEqual('win7', created_bots[0].key.id())
    self.assertEqual('ChromiumPerf', created_bots[0].key.parent().id())
    self.assertEqual('dromaeo', created_tests[0].key.id())
    self.assertIsNone(created_tests[0].parent_test)
    self.assertEqual('win7', created_tests[0].bot.id())
    self.assertEqual('dom', created_tests[1].key.id())
    self.assertEqual('dromaeo', created_tests[1].parent_test.id())
    self.assertIsNone(created_tests[1].bot)
    self.assertEqual('modify', created_tests[2].key.id())
    self.assertEqual('dom', created_tests[2].parent_test.id())
    self.assertIsNone(created_tests[2].bot)

  def testGetOrCreateAncestors_UpdatesStoppageAlert(self):
    testing_common.AddTests(['M'], ['b'], {'suite': {'foo': {}}})
    row = testing_common.AddRows('M/b/suite/foo', {123})[0]
    test = utils.TestKey('M/b/suite/foo').get()
    alert_key = stoppage_alert.CreateStoppageAlert(test, row).put()
    test.stoppage_alert = alert_key
    test.put()
    add_point_queue._GetOrCreateAncestors('M', 'b', 'suite/foo')
    self.assertIsNone(test.key.get().stoppage_alert)
    self.assertTrue(alert_key.get().recovered)


if __name__ == '__main__':
  unittest.main()
