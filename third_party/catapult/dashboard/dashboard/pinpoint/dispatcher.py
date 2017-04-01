# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dispatches requests to request handler classes."""

import webapp2

from dashboard.pinpoint.handlers import isolated
from dashboard.pinpoint.handlers import job
from dashboard.pinpoint.handlers import new
from dashboard.pinpoint.handlers import run


_URL_MAPPING = [
    webapp2.Route(r'/isolated', isolated.IsolatedHandler),
    webapp2.Route(r'/isolated/<builder_name>/<git_hash>/<target>',
                  isolated.IsolatedHandler),
    webapp2.Route(r'/job/<job_id>', job.JobHandler),
    webapp2.Route(r'/new', new.NewHandler),
    webapp2.Route(r'/run/<job_id>', run.RunHandler),
]

APP = webapp2.WSGIApplication(_URL_MAPPING, debug=False)
