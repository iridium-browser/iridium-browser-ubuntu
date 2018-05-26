# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock

from dashboard.common import namespaced_stored_object
from dashboard.common import testing_common
from dashboard.pinpoint.models import change
from dashboard.pinpoint.models import job


_CHROMIUM_URL = 'https://chromium.googlesource.com/chromium/src'


_COMMENT_STARTED = (
    u"""\U0001f4cd Pinpoint job started.
https://testbed.example.com/job/1""")


_COMMENT_COMPLETED_NO_COMPARISON = (
    u"""<b>\U0001f4cd Job complete. See results below.</b>
https://testbed.example.com/job/1""")


_COMMENT_COMPLETED_NO_DIFFERENCES = (
    u"""<b>\U0001f4cd Couldn't reproduce a difference.</b>
https://testbed.example.com/job/1""")


_COMMENT_COMPLETED_WITH_COMMIT = (
    u"""<b>\U0001f4cd Found a significant difference after 1 commit.</b>
https://testbed.example.com/job/1

<b>Subject.</b> by author@chromium.org
https://example.com/repository/+/git_hash

Understanding performance regressions:
  http://g.co/ChromePerformanceRegressions""")


_COMMENT_COMPLETED_WITH_PATCH = (
    u"""<b>\U0001f4cd Found a significant difference after 1 commit.</b>
https://testbed.example.com/job/1

<b>Subject.</b> by author@chromium.org
https://codereview.com/c/672011/2f0d5c7

Understanding performance regressions:
  http://g.co/ChromePerformanceRegressions""")


_COMMENT_COMPLETED_TWO_DIFFERENCES = (
    u"""<b>\U0001f4cd Found significant differences after each of 2 commits.</b>
https://testbed.example.com/job/1

<b>Subject.</b> by author1@chromium.org
https://example.com/repository/+/git_hash_1

<b>Subject.</b> by author2@chromium.org
https://example.com/repository/+/git_hash_2

Understanding performance regressions:
  http://g.co/ChromePerformanceRegressions""")


_COMMENT_FAILED = (
    u"""\U0001f63f Pinpoint job stopped with an error.
https://testbed.example.com/job/1""")


@mock.patch('dashboard.common.utils.ServiceAccountHttp', mock.MagicMock())
class BugCommentTest(testing_common.TestCase):

  def setUp(self):
    super(BugCommentTest, self).setUp()

    self.add_bug_comment = mock.MagicMock()
    patcher = mock.patch('dashboard.services.issue_tracker_service.'
                         'IssueTrackerService')
    issue_tracker_service = patcher.start()
    issue_tracker_service.return_value = mock.MagicMock(
        AddBugComment=self.add_bug_comment)
    self.addCleanup(patcher.stop)

    namespaced_stored_object.Set('repositories', {
        'chromium': {'repository_url': _CHROMIUM_URL},
    })

  def tearDown(self):
    self.testbed.deactivate()

  def testNoBug(self):
    j = job.Job.New({}, [], False)
    j.put()
    j.Start()
    j.Run()

    self.assertFalse(self.add_bug_comment.called)

  def testStarted(self):
    j = job.Job.New({}, [], False, bug_id=123456)
    j.put()
    j.Start()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_STARTED, send_email=False)

  def testCompletedNoComparison(self):
    j = job.Job.New({}, [], False, bug_id=123456)
    j.put()
    j.Run()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_COMPLETED_NO_COMPARISON)

  def testCompletedNoDifference(self):
    j = job.Job.New({}, [], True, bug_id=123456)
    j.put()
    j.Run()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_COMPLETED_NO_DIFFERENCES)

  @mock.patch('dashboard.pinpoint.models.change.commit.Commit.AsDict')
  @mock.patch.object(job.job_state.JobState, 'Differences')
  def testCompletedWithCommit(self, differences, commit_as_dict):
    c = change.Change((change.Commit('chromium', 'git_hash'),))
    differences.return_value = [(1, c)]
    commit_as_dict.return_value = {
        'repository': 'chromium',
        'git_hash': 'git_hash',
        'author': 'author@chromium.org',
        'subject': 'Subject.',
        'reviewers': ['reviewer@chromium.org'],
        'url': 'https://example.com/repository/+/git_hash',
    }

    j = job.Job.New({}, [], True, bug_id=123456)
    j.put()
    j.Run()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_COMPLETED_WITH_COMMIT,
        status='Assigned', owner='author@chromium.org',
        cc_list=['author@chromium.org', 'reviewer@chromium.org'])

  @mock.patch('dashboard.pinpoint.models.change.patch.GerritPatch.AsDict')
  @mock.patch.object(job.job_state.JobState, 'Differences')
  def testCompletedWithPatch(self, differences, patch_as_dict):
    commits = (change.Commit('chromium', 'git_hash'),)
    patch = change.GerritPatch('https://codereview.com', 672011, '2f0d5c7')
    c = change.Change(commits, patch)
    differences.return_value = [(1, c)]
    patch_as_dict.return_value = {
        'author': 'author@chromium.org',
        'subject': 'Subject.',
        'url': 'https://codereview.com/c/672011/2f0d5c7',
    }

    j = job.Job.New({}, [], True, bug_id=123456)
    j.put()
    j.Run()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_COMPLETED_WITH_PATCH,
        status='Assigned', owner='author@chromium.org',
        cc_list=['author@chromium.org'])

  @mock.patch('dashboard.pinpoint.models.change.commit.Commit.AsDict')
  @mock.patch.object(job.job_state.JobState, 'Differences')
  def testCompletedMultipleDifferences(self, differences, commit_as_dict):
    c1 = change.Change((change.Commit('chromium', 'git_hash_1'),))
    c2 = change.Change((change.Commit('chromium', 'git_hash_2'),))
    differences.return_value = [(1, c1), (2, c2)]
    commit_as_dict.side_effect = (
        {
            'repository': 'chromium',
            'git_hash': 'git_hash_1',
            'author': 'author1@chromium.org',
            'subject': 'Subject.',
            'reviewers': ['reviewer1@chromium.org'],
            'url': 'https://example.com/repository/+/git_hash_1',
        },
        {
            'repository': 'chromium',
            'git_hash': 'git_hash_2',
            'author': 'author2@chromium.org',
            'subject': 'Subject.',
            'reviewers': ['reviewer1@chromium.org', 'reviewer2@chromium.org'],
            'url': 'https://example.com/repository/+/git_hash_2',
        },
    )

    j = job.Job.New({}, [], True, bug_id=123456)
    j.put()
    j.Run()

    self.add_bug_comment.assert_called_once_with(
        123456, _COMMENT_COMPLETED_TWO_DIFFERENCES,
        status='Assigned', owner='author2@chromium.org',
        cc_list=['author1@chromium.org', 'author2@chromium.org',
                 'reviewer1@chromium.org', 'reviewer2@chromium.org'])

  @mock.patch.object(job.job_state.JobState, 'ScheduleWork',
                     mock.MagicMock(side_effect=AssertionError))
  def testFailed(self):
    j = job.Job.New({}, [], False, bug_id=123456)
    j.put()
    with self.assertRaises(AssertionError):
      j.Run()

    self.add_bug_comment.assert_called_once_with(123456, _COMMENT_FAILED)
