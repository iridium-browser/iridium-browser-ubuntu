# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import traceback
import uuid

from google.appengine.api import datastore_errors
from google.appengine.api import taskqueue
from google.appengine.ext import ndb
from google.appengine.runtime import apiproxy_errors

from dashboard.common import utils
from dashboard.pinpoint.models import job_state
from dashboard.services import issue_tracker_service


# We want this to be fast to minimize overhead while waiting for tasks to
# finish, but don't want to consume too many resources.
_TASK_INTERVAL = 10


_CRYING_CAT_FACE = u'\U0001f63f'
_ROUND_PUSHPIN = u'\U0001f4cd'


OPTION_STATE = 'STATE'
OPTION_TAGS = 'TAGS'


def JobFromId(job_id):
  """Get a Job object from its ID. Its ID is just its key as a hex string.

  Users of Job should not have to import ndb. This function maintains an
  abstraction layer that separates users from the Datastore details.
  """
  job_key = ndb.Key('Job', int(job_id, 16))
  return job_key.get()


class Job(ndb.Model):
  """A Pinpoint job."""

  created = ndb.DateTimeProperty(required=True, auto_now_add=True)
  # Don't use `auto_now` for `updated`. When we do data migration, we need
  # to be able to modify the Job without changing the Job's completion time.
  updated = ndb.DateTimeProperty(required=True, auto_now_add=True)

  # The name of the Task Queue task this job is running on. If it's present, the
  # job is running. The task is also None for Task Queue retries.
  task = ndb.StringProperty()

  # The string contents of any Exception that was thrown to the top level.
  # If it's present, the job failed.
  exception = ndb.TextProperty()

  # Request parameters.
  arguments = ndb.JsonProperty(required=True)

  # If True, the service should pick additional Changes to run (bisect).
  # If False, only run the Changes explicitly added by the user.
  auto_explore = ndb.BooleanProperty(required=True)

  # TODO: The bug id is only used for posting bug comments when a job starts and
  # completes. This probably should not be the responsibility of Pinpoint.
  bug_id = ndb.IntegerProperty()

  state = ndb.PickleProperty(required=True, compressed=True)

  tags = ndb.JsonProperty()

  @classmethod
  def New(cls, arguments, quests, auto_explore, bug_id=None, tags=None):
    # Create job.
    return cls(
        arguments=arguments,
        auto_explore=auto_explore,
        bug_id=bug_id,
        tags=tags,
        state=job_state.JobState(quests))

  @property
  def job_id(self):
    return '%x' % self.key.id()

  @property
  def status(self):
    if self.task:
      return 'Running'

    if self.exception:
      return 'Failed'

    return 'Completed'

  @property
  def url(self):
    return 'https://%s/job/%s' % (os.environ['HTTP_HOST'], self.job_id)

  def AddChange(self, change):
    self.state.AddChange(change)

  def Start(self):
    self._Schedule()

    title = _ROUND_PUSHPIN + ' Pinpoint job started.'
    comment = '\n'.join((title, self.url))
    self._PostBugComment(comment, send_email=False)

  def _Complete(self):
    # Format bug comment.
    differences = tuple(self.state.Differences())

    if not differences:
      title = "<b>%s Couldn't reproduce a difference.</b>" % _ROUND_PUSHPIN
      self._PostBugComment('\n'.join((title, self.url)))
      return

    # Include list of Changes.
    owner = None
    cc_list = set()
    commit_details = []
    for _, change in differences:
      if change.patch:
        commit_info = change.patch.AsDict()
      else:
        commit_info = change.last_commit.AsDict()

      # TODO: Assign the largest difference, not the last one.
      owner = commit_info['author']
      cc_list.add(commit_info['author'])
      if 'reviewers' in commit_info:
        cc_list |= frozenset(commit_info['reviewers'])
      commit_details.append(_FormatCommitForBug(commit_info))

    # Header.
    if len(differences) == 1:
      status = 'Found a significant difference after 1 commit.'
    else:
      status = ('Found significant differences after each of %d commits.' %
                len(differences))

    title = '<b>%s %s</b>' % (_ROUND_PUSHPIN, status)
    header = '\n'.join((title, self.url))

    # Body.
    body = '\n\n'.join(commit_details)

    # Footer.
    footer = ('Understanding performance regressions:\n'
              '  http://g.co/ChromePerformanceRegressions')

    # Bring it all together.
    comment = '\n\n'.join((header, body, footer))
    self._PostBugComment(comment, status='Assigned',
                         cc_list=sorted(cc_list), owner=owner)

  def Fail(self):
    self.exception = traceback.format_exc()

    title = _CRYING_CAT_FACE + ' Pinpoint job stopped with an error.'
    comment = '\n'.join((title, self.url))
    self._PostBugComment(comment)

  def _Schedule(self):
    # Set a task name to deduplicate retries. This adds some latency, but we're
    # not latency-sensitive. If Job.Run() works asynchronously in the future,
    # we don't need to worry about duplicate tasks.
    # https://github.com/catapult-project/catapult/issues/3900
    task_name = str(uuid.uuid4())
    try:
      task = taskqueue.add(
          queue_name='job-queue', url='/api/run/' + self.job_id,
          name=task_name, countdown=_TASK_INTERVAL)
    except apiproxy_errors.DeadlineExceededError:
      task = taskqueue.add(
          queue_name='job-queue', url='/api/run/' + self.job_id,
          name=task_name, countdown=_TASK_INTERVAL)

    self.task = task.name

  def Run(self):
    self.exception = None  # In case the Job succeeds on retry.
    self.task = None  # In case an exception is thrown.

    try:
      self.state.Explore(self.auto_explore)
      work_left = self.state.ScheduleWork()

      # Schedule moar task.
      if work_left:
        self._Schedule()
      else:
        self._Complete()
    except BaseException:
      self.Fail()
      raise
    finally:
      # Don't use `auto_now` for `updated`. When we do data migration, we need
      # to be able to modify the Job without changing the Job's completion time.
      self.updated = datetime.datetime.now()
      try:
        self.put()
      except datastore_errors.BadRequestError:
        # The _JobState is too large to fit in an ndb property.
        # Load the Job from before we updated it, and fail it.
        job = self.key.get(use_cache=False)
        job.task = None
        job.Fail()
        job.updated = datetime.datetime.now()
        job.put()
        raise

  def AsDict(self, options=None):
    d = {
        'job_id': self.job_id,

        'arguments': self.arguments,
        'auto_explore': self.auto_explore,
        'bug_id': self.bug_id,

        'created': self.created.isoformat(),
        'updated': self.updated.isoformat(),
        'exception': self.exception,
        'status': self.status,
    }
    if not options:
      return d

    if OPTION_STATE in options:
      d.update(self.state.AsDict())
    if OPTION_TAGS in options:
      d['tags'] = {'tags': self.tags}
    return d

  def _PostBugComment(self, *args, **kwargs):
    if not self.bug_id:
      return

    issue_tracker = issue_tracker_service.IssueTrackerService(
        utils.ServiceAccountHttp())
    issue_tracker.AddBugComment(self.bug_id, *args, **kwargs)


def _FormatCommitForBug(commit_info):
  subject = '<b>%s</b> by %s' % (commit_info['subject'], commit_info['author'])
  return '\n'.join((subject, commit_info['url']))


# TODO: Remove after data migration.
_JobState = job_state.JobState
