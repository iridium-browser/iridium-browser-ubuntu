# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for remote_try.py."""

from __future__ import print_function

import json
import mock

from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import remote_try

# Tests need internal access.
# pylint: disable=protected-access

class RemoteTryHelperTestsBase(cros_test_lib.MockTestCase):
  """Tests for RemoteTryJob."""
  BRANCH = 'test-branch'
  PATCHES = ('5555', '6666')
  BUILD_CONFIGS = ('amd64-generic-paladin', 'arm-generic-paladin')
  UNKNOWN_CONFIGS = ('unknown-config')
  BUILD_GROUP = 'display'
  PASS_THROUGH_ARGS = ['funky', 'cold', 'medina']
  TEST_EMAIL = 'explicit_email'
  MASTER_BUILDBUCKET_ID = 'master_bb_id'

  def setUp(self):
    self.maxDiff = None
    self.PatchObject(git, 'GetProjectUserEmail', return_value='default_email')

  def _CreateJobMin(self):
    return remote_try.RemoteTryJob(
        self.BUILD_CONFIGS,
        self.BUILD_GROUP,
        'description')

  def _CreateJobMax(self):
    return remote_try.RemoteTryJob(
        self.BUILD_CONFIGS,
        self.BUILD_GROUP,
        'description',
        branch=self.BRANCH,
        pass_through_args=self.PASS_THROUGH_ARGS,
        local_patches=(),  # TODO: Populate/test these, somehow.
        committer_email=self.TEST_EMAIL,
        swarming=True,
        master_buildbucket_id=self.MASTER_BUILDBUCKET_ID)

  def _CreateJobUnknown(self):
    return remote_try.RemoteTryJob(
        self.UNKNOWN_CONFIGS,
        self.BUILD_GROUP,
        'description')


class RemoteTryHelperTestsMock(RemoteTryHelperTestsBase):
  """Perform real buildbucket requests against a fake instance."""

  def setUp(self):
    # This mocks out the class, then creates a return_value for a function on
    # instances of it. We do this instead of just mocking out the function to
    # ensure not real network requests are made in other parts of the class.
    client_mock = self.PatchObject(buildbucket_lib, 'BuildbucketClient')
    client_mock().PutBuildRequest.return_value = {
        'build': {'id': 'fake_buildbucket_id'}
    }

  def testDefaultDescription(self):
    """Verify job description formatting."""
    description = remote_try.DefaultDescription(self.BRANCH, self.PATCHES)
    self.assertEqual(description, '[test-branch] 5555,6666')

  def testDefaultEmail(self):
    """Verify a default discovered email address."""
    job = self._CreateJobMin()

    # Verify default email detection.
    self.assertEqual(job.user_email, 'default_email')

  def testExplicitEmail(self):
    """Verify an explicitly set email address."""
    job = self._CreateJobMax()

    # Verify explicit email detection.
    self.assertEqual(job.user_email, self.TEST_EMAIL)

  def testMinRequestBody(self):
    """Verify our request body with min options."""
    body = self._CreateJobMin()._GetRequestBody(self.BUILD_CONFIGS[0])

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'master.chromiumos.tryserver',
        'tags': [
            'cbb_branch:master',
            'cbb_config:amd64-generic-paladin',
            'cbb_display_label:display',
            'cbb_email:default_email',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        u'builder_name': u'Generic',
        u'properties': {
            u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
            u'cbb_branch': u'master',
            u'cbb_config': u'amd64-generic-paladin',
            u'cbb_display_label': u'display',
            u'cbb_email': u'default_email',
            u'cbb_extra_args': [],
            u'extra_args': [],
            u'name': u'description',
            u'owners': [u'default_email'],
            u'email': [u'default_email'],
            u'user': mock.ANY,
        }
    })

  def testMaxRequestBody(self):
    """Verify our request body with max options."""
    self.maxDiff = None
    body = self._CreateJobMax()._GetRequestBody(self.BUILD_CONFIGS[0])

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'luci.chromeos.general',
        'tags': [
            'cbb_branch:test-branch',
            'cbb_config:amd64-generic-paladin',
            'cbb_display_label:display',
            'cbb_email:explicit_email',
            'cbb_master_build_id:master_bb_id',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        u'builder_name': u'Generic',
        u'email_notify': [{u'email': u'explicit_email'}],
        u'properties': {
            u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
            u'cbb_branch': u'test-branch',
            u'cbb_config': u'amd64-generic-paladin',
            u'cbb_display_label': u'display',
            u'cbb_email': u'explicit_email',
            u'cbb_extra_args': [u'funky', u'cold', u'medina'],
            u'cbb_master_build_id': u'master_bb_id',
            u'email': [u'explicit_email'],
            u'extra_args': [u'funky', u'cold', u'medina'],
            u'name': u'description',
            u'owners': [u'explicit_email'],
            u'user': mock.ANY,
        }
    })

  def testUnknownRequestBody(self):
    """Verify our request body with max options."""
    self.maxDiff = None
    body = self._CreateJobUnknown()._GetRequestBody('unknown-config')

    self.assertEqual(body, {
        'parameters_json': mock.ANY,
        'bucket': 'master.chromiumos.tryserver',
        'tags': [
            'cbb_branch:master',
            'cbb_config:unknown-config',
            'cbb_display_label:display',
            'cbb_email:default_email',
        ]
    })

    parameters_parsed = json.loads(body['parameters_json'])

    self.assertEqual(parameters_parsed, {
        u'builder_name': u'Generic',
        u'properties': {
            u'bot': u'unknown-config',
            u'cbb_branch': u'master',
            u'cbb_config': u'unknown-config',
            u'cbb_display_label': u'display',
            u'cbb_email': u'default_email',
            u'cbb_extra_args': [],
            u'email': [u'default_email'],
            u'extra_args': [],
            u'name': u'description',
            u'owners': [u'default_email'],
            u'user': mock.ANY,
        }
    })

  def testMinDryRun(self):
    """Do a dryrun of posting the request, min options."""
    job = self._CreateJobMin()
    job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
    # TODO: Improve coverage to all of Submit. Verify behavior.

  def testMaxDryRun(self):
    """Do a dryrun of posting the request, max options."""
    job = self._CreateJobMax()
    job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
    # TODO: Improve coverage to all of Submit. Verify behavior.


class RemoteTryHelperTestsNetork(RemoteTryHelperTestsBase):
  """Perform real buildbucket requests against a test instance."""

  def verifyBuildbucketRequest(self,
                               buildbucket_id,
                               expected_bucket,
                               expected_tags,
                               expected_parameters):
    """Verify the contents of a push to the TEST buildbucket instance.

    Args:
      buildbucket_id: Id to verify.
      expected_bucket: Bucket the push was supposed to go to as a string.
      expected_tags: List of buildbucket tags as strings.
      expected_parameters: Python dict equivalent to json string in
                           parameters_json.
    """
    buildbucket_client = buildbucket_lib.BuildbucketClient(
        auth.GetAccessToken, buildbucket_lib.BUILDBUCKET_TEST_HOST,
        service_account_json=buildbucket_lib.GetServiceAccount(
            constants.CHROMEOS_SERVICE_ACCOUNT))

    request = buildbucket_client.GetBuildRequest(buildbucket_id, False)

    self.assertEqual(request['build']['id'], buildbucket_id)
    self.assertEqual(request['build']['bucket'], expected_bucket)
    self.assertItemsEqual(request['build']['tags'], expected_tags)

    request_parameters = json.loads(request['build']['parameters_json'])
    self.assertEqual(request_parameters, expected_parameters)

  @cros_test_lib.NetworkTest()
  def testMinTestBucket(self):
    """Talk to a test buildbucket instance with min job settings."""
    # Submit jobs
    job = self._CreateJobMin()
    results = job.Submit(testjob=True)
    buildbucket_ids = [r.buildbucket_id for r in results]

    self.verifyBuildbucketRequest(
        buildbucket_ids[0],
        'master.chromiumos.tryserver',
        [
            'builder:Generic',
            'cbb_branch:master',
            'cbb_config:amd64-generic-paladin',
            'cbb_display_label:display',
            'cbb_email:default_email',
        ],
        {
            u'builder_name': u'Generic',
            u'properties': {
                u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
                u'cbb_branch': u'master',
                u'cbb_config': u'amd64-generic-paladin',
                u'cbb_display_label': u'display',
                u'cbb_email': u'default_email',
                u'cbb_extra_args': [],
                u'email': [u'default_email'],
                u'extra_args': [],
                u'name': u'description',
                u'owners': [u'default_email'],
                u'user': mock.ANY,
            },
        })

    self.verifyBuildbucketRequest(
        buildbucket_ids[1],
        'master.chromiumos.tryserver',
        [
            'builder:Generic',
            'cbb_branch:master',
            'cbb_config:arm-generic-paladin',
            'cbb_display_label:display',
            'cbb_email:default_email',
        ],
        {
            u'builder_name': u'Generic',
            u'properties': {
                u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
                u'cbb_branch': u'master',
                u'cbb_config': u'arm-generic-paladin',
                u'cbb_display_label': u'display',
                u'cbb_email': u'default_email',
                u'cbb_extra_args': [],
                u'email': [u'default_email'],
                u'extra_args': [],
                u'name': u'description',
                u'owners': [u'default_email'],
                u'user': mock.ANY,
            },
        })

    self.assertEqual(results, [
        remote_try.ScheduledBuild(
            buildbucket_id=buildbucket_ids[0],
            build_config='amd64-generic-paladin',
            url=(u'http://cros-goldeneye/chromeos/healthmonitoring/'
                 u'buildDetails?buildbucketId=%s' % buildbucket_ids[0])),
        remote_try.ScheduledBuild(
            buildbucket_id=buildbucket_ids[1],
            build_config='arm-generic-paladin',
            url=(u'http://cros-goldeneye/chromeos/healthmonitoring/'
                 u'buildDetails?buildbucketId=%s' % buildbucket_ids[1])),
    ])



  @cros_test_lib.NetworkTest()
  def testMaxTestBucket(self):
    """Talk to a test buildbucket instance with max job settings."""
    # Submit jobs
    job = self._CreateJobMax()
    results = job.Submit(testjob=True)
    buildbucket_ids = [r.buildbucket_id for r in results]

    # Verify buildbucket contents.
    self.verifyBuildbucketRequest(
        buildbucket_ids[0],
        'luci.chromeos.general',
        [
            'builder:Generic',
            'cbb_branch:test-branch',
            'cbb_display_label:display',
            'cbb_config:amd64-generic-paladin',
            'cbb_email:explicit_email',
            'cbb_master_build_id:master_bb_id',
        ],
        {
            u'builder_name': u'Generic',
            u'email_notify': [{u'email': u'explicit_email'}],
            u'properties': {
                u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
                u'cbb_branch': u'test-branch',
                u'cbb_config': u'amd64-generic-paladin',
                u'cbb_display_label': u'display',
                u'cbb_email': u'explicit_email',
                u'cbb_extra_args': [u'funky', u'cold', u'medina'],
                u'cbb_master_build_id': u'master_bb_id',
                u'email': [u'explicit_email'],
                u'extra_args': [u'funky', u'cold', u'medina'],
                u'name': u'description',
                u'owners': [u'explicit_email'],
                u'user': mock.ANY,
            },
        })

    self.verifyBuildbucketRequest(
        buildbucket_ids[1],
        'luci.chromeos.general',
        [
            'builder:Generic',
            'cbb_branch:test-branch',
            'cbb_display_label:display',
            'cbb_config:arm-generic-paladin',
            'cbb_email:explicit_email',
            'cbb_master_build_id:master_bb_id',
        ],
        {
            u'builder_name': u'Generic',
            u'email_notify': [{u'email': u'explicit_email'}],
            u'properties': {
                u'bot': [u'amd64-generic-paladin', u'arm-generic-paladin'],
                u'cbb_branch': u'test-branch',
                u'cbb_config': u'arm-generic-paladin',
                u'cbb_display_label': u'display',
                u'cbb_email': u'explicit_email',
                u'cbb_extra_args': [u'funky', u'cold', u'medina'],
                u'cbb_master_build_id': u'master_bb_id',
                u'email': [u'explicit_email'],
                u'extra_args': [u'funky', u'cold', u'medina'],
                u'name': u'description',
                u'owners': [u'explicit_email'],
                u'user': mock.ANY,
            },
        })

    self.assertEqual(results, [
        remote_try.ScheduledBuild(
            buildbucket_id=buildbucket_ids[0],
            build_config='amd64-generic-paladin',
            url=(u'http://cros-goldeneye/chromeos/healthmonitoring/'
                 u'buildDetails?buildbucketId=%s' % buildbucket_ids[0])),
        remote_try.ScheduledBuild(
            buildbucket_id=buildbucket_ids[1],
            build_config='arm-generic-paladin',
            url=(u'http://cros-goldeneye/chromeos/healthmonitoring/'
                 u'buildDetails?buildbucketId=%s' % buildbucket_ids[1])),
    ])

  # pylint: disable=protected-access
  def testPostConfigsToBuildBucket(self):
    """Check syntax for PostConfigsToBuildBucket."""
    self.PatchObject(auth, 'Login')
    self.PatchObject(auth, 'Token')
    self.PatchObject(remote_try.RemoteTryJob, '_PutConfigToBuildBucket')
    remote_try_job = remote_try.RemoteTryJob(
        ['build_config'],
        'display_label',
        remote_description='description')
    remote_try_job._PostConfigsToBuildBucket(testjob=True, dryrun=True)
