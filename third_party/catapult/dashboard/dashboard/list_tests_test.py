# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for list_tests module."""

import json
import unittest

import webapp2
import webtest

from google.appengine.ext import ndb

from dashboard import datastore_hooks
from dashboard import layered_cache
from dashboard import list_tests
from dashboard import testing_common
from dashboard import utils
from dashboard.models import graph_data


class ListTestsTest(testing_common.TestCase):

  def setUp(self):
    super(ListTestsTest, self).setUp()
    app = webapp2.WSGIApplication(
        [('/list_tests', list_tests.ListTestsHandler)])
    self.testapp = webtest.TestApp(app)
    datastore_hooks.InstallHooks()
    self.UnsetCurrentUser()
    testing_common.SetInternalDomain('google.com')

  def _AddSampleData(self):
    testing_common.AddTests(
        ['Chromium'],
        ['win7', 'mac'],
        {
            'dromaeo': {
                'dom': {},
                'jslib': {},
            },
            'scrolling': {
                'commit_time': {
                    'www.yahoo.com': {},
                    'www.cnn.com': {},
                },
                'commit_time_ref': {},
            },
            'really': {
                'nested': {
                    'very': {
                        'deeply': {
                            'subtest': {}
                        }
                    },
                    'very_very': {}
                }
            },
        })

  def tearDown(self):
    self.testbed.deactivate()

  def testGetSubTests_FetchAndCacheBehavior(self):
    """Tests the behavior of GetSubTests and interaction with layered cache."""
    self._AddSampleData()

    # Set the has_rows flag to true on two of the Test entities.
    for test_path in [
        'Chromium/win7/really/nested/very/deeply/subtest',
        'Chromium/win7/really/nested/very_very']:
      test = utils.TestKey(test_path).get()
      test.has_rows = True
      test.put()

    # A tree-structured dict of dicts is constructed, and the 'has_rows'
    # flag is set to true for two of these tests. These two tests and
    # their parents are all included in the result.
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests',
        'suite': 'really',
        'bots': 'Chromium/win7,Chromium/mac'})
    self.assertEqual('*', response.headers.get('Access-Control-Allow-Origin'))
    expected = {
        'nested': {
            'has_rows': False,
            'sub_tests': {
                'very': {
                    'has_rows': False,
                    'sub_tests': {
                        'deeply': {
                            'has_rows': False,
                            'sub_tests': {
                                'subtest': {
                                    'has_rows': True,
                                    'sub_tests': {}
                                }
                            }
                        }
                    }
                },
                'very_very': {
                    'has_rows': True,
                    'sub_tests': {}
                }
            }
        }
    }
    # The response should be as expected.
    self.assertEqual(expected, json.loads(response.body))

    # The cache should be set for the win7 bot with the expected response.
    self.assertEqual(expected, layered_cache.Get(
        graph_data.LIST_TESTS_SUBTEST_CACHE_KEY % (
            'Chromium', 'win7', 'really')))

    # Change mac subtests in cache. Should be merged with win7.
    mac_subtests = {
        'mactest': {
            'has_rows': False,
            'sub_tests': {
                'graph': {
                    'has_rows': True,
                    'sub_tests': {}
                }
            }
        }
    }
    layered_cache.Set(
        graph_data.LIST_TESTS_SUBTEST_CACHE_KEY % ('Chromium', 'mac', 'really'),
        mac_subtests)
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests',
        'suite': 'really',
        'bots': 'Chromium/win7,Chromium/mac'})
    self.assertEqual('*', response.headers.get('Access-Control-Allow-Origin'))
    expected.update(mac_subtests)
    self.assertEqual(expected, json.loads(response.body))

  def testGetSubTests_ReturnsOnlyNonDeprecatedTests(self):
    """Checks that only data not marked as deprecated is returned.

    Sub-tests with the same name may be deprecated on one bot (indicating that
    that bot has not sent data recently with that name and not deprecated on
    another bot.
    """
    self._AddSampleData()

    # Set the deprecated flag to True for one test on one platform.
    test = utils.TestKey('Chromium/mac/dromaeo/jslib').get()
    test.deprecated = True
    test.put()

    # Set the has_rows flag to true for all of the test entities.
    for test_path in [
        'Chromium/win7/dromaeo/dom',
        'Chromium/win7/dromaeo/jslib',
        'Chromium/mac/dromaeo/dom',
        'Chromium/mac/dromaeo/jslib']:
      test = utils.TestKey(test_path).get()
      test.has_rows = True
      test.put()

    # When a request is made for subtests for the platform wherein that a
    # subtest is deprecated, that subtest will not be listed.
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests',
        'suite': 'dromaeo',
        'bots': 'Chromium/mac'})
    self.assertEqual('*', response.headers.get('Access-Control-Allow-Origin'))
    expected = {
        'dom': {
            'has_rows': True,
            'sub_tests': {}
        },
    }
    self.assertEqual(expected, json.loads(response.body))

    # When a request is made for subtests for multiple platforms, all subtests
    # that are not deprecated for at least one of the platforms will be listed.
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests',
        'suite': 'dromaeo',
        'bots': 'Chromium/mac,Chromium/win7'})
    self.assertEqual('*', response.headers.get('Access-Control-Allow-Origin'))
    expected = {
        'dom': {
            'has_rows': True,
            'sub_tests': {}
        },
        'jslib': {
            'has_rows': True,
            'sub_tests': {}
        }
    }
    self.assertEqual(expected, json.loads(response.body))

  def testGetSubTests_InternalData(self):
    """Checks that internal data is not returned for unauthorized users."""
    # When the user has a an internal account, internal-only data is given.
    self.SetCurrentUser('foo@google.com')
    self._AddSampleData()

    # Set internal_only on a bot and top-level test.
    bot = ndb.Key('Master', 'Chromium', 'Bot', 'win7').get()
    bot.internal_only = True
    bot.put()
    test = graph_data.Test.get_by_id('dromaeo', parent=bot.key)
    test.internal_only = True
    test.put()

    # Set internal_only and has_rows to true on two subtests.
    for name in ['dom', 'jslib']:
      subtest = graph_data.Test.get_by_id(name, parent=test.key)
      subtest.internal_only = True
      subtest.has_rows = True
      subtest.put()

    # All of the internal-only tests are returned.
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests', 'suite': 'dromaeo', 'bots': 'Chromium/win7'})
    expected = {
        'dom': {
            'has_rows': True,
            'sub_tests': {}
        },
        'jslib': {
            'has_rows': True,
            'sub_tests': {}
        }
    }
    self.assertEqual(expected, json.loads(response.body))

    # After setting the user to another domain, an empty dict is returned.
    self.SetCurrentUser('foo@yahoo.com')
    response = self.testapp.post('/list_tests', {
        'type': 'sub_tests', 'suite': 'dromaeo', 'bots': 'Chromium/win7'})
    self.assertEqual({}, json.loads(response.body))

  def test_MergeSubTestsDict(self):
    a = {
        'foo': {
            'has_rows': True,
            'sub_tests': {'a': {'has_rows': True, 'sub_tests': {}}},
        },
        'bar': {
            'has_rows': False,
            'sub_tests': {'b': {'has_rows': False, 'sub_tests': {}}},
        },
    }
    b = {
        'bar': {'has_rows': True, 'sub_tests': {}},
        'baz': {'has_rows': False, 'sub_tests': {}},
    }
    self.assertEqual(
        {
            'foo': {
                'has_rows': True,
                'sub_tests': {'a': {'has_rows': True, 'sub_tests': {}}},
            },
            'bar': {
                'has_rows': True,
                'sub_tests': {'b': {'has_rows': False, 'sub_tests': {}}}
            },
            'baz': {'has_rows': False, 'sub_tests': {}},
        },
        list_tests._MergeSubTestsDict(a, b))

  def testSubTestsDict(self):
    paths = [
        'a/b/c',
        'a/b/c',
        'a/b/d',
    ]
    expected = {
        'a': {
            'has_rows': False,
            'sub_tests': {
                'b': {
                    'has_rows': False,
                    'sub_tests': {
                        'c': {'has_rows': True, 'sub_tests': {}},
                        'd': {'has_rows': True, 'sub_tests': {}},
                    },
                },
            },
        },
    }
    self.assertEqual(
        expected, list_tests._SubTestsDict(paths))

  def test_GetTestsMatchingPattern(self):
    """Tests the basic functionality of the GetTestsMatchingPattern function."""
    self._AddSampleData()

    # A pattern can match tests with a particular bot and with a particular
    # number of levels of nesting.
    # The results are lexicographically ordered by test path.
    response = self.testapp.post('/list_tests', {
        'type': 'pattern',
        'p': 'Chromium/mac/*/*/www*'})
    expected = [
        'Chromium/mac/scrolling/commit_time/www.cnn.com',
        'Chromium/mac/scrolling/commit_time/www.yahoo.com',
    ]
    self.assertEqual(expected, json.loads(response.body))

    # The same thing is returned if has_rows is set to '0' or another string
    # that is not '1'.
    response = self.testapp.post('/list_tests', {
        'type': 'pattern',
        'has_rows': '0',
        'p': '*/mac/*/*/www*'})
    self.assertEqual(expected, json.loads(response.body))
    response = self.testapp.post('/list_tests', {
        'type': 'pattern',
        'has_rows': 'foo',
        'p': '*/mac/*/*/www*'})
    self.assertEqual(expected, json.loads(response.body))

  def test_GetTestsMatchingPattern_OnlyWithRows(self):
    """Tests GetTestsMatchingPattern with the parameter only_with_rows set."""
    self._AddSampleData()

    # When no Test entities have has_rows set, filtering with the parameter
    # 'has_rows' set to '1' results in no rows being returned.
    response = self.testapp.post('/list_tests', {
        'type': 'pattern',
        'has_rows': '1',
        'p': '*/mac/dromaeo/*'})
    self.assertEqual([], json.loads(response.body))

    # Set the has_rows flag on one of the tests.
    test = utils.TestKey('Chromium/mac/dromaeo/dom').get()
    test.has_rows = True
    test.put()

    # Even though multiple tests could match the pattern, only the test with
    # has_rows set is returned.
    response = self.testapp.post('/list_tests', {
        'type': 'pattern',
        'has_rows': '1',
        'p': '*/mac/dromaeo/*'})
    self.assertEqual(['Chromium/mac/dromaeo/dom'], json.loads(response.body))


if __name__ == '__main__':
  unittest.main()
