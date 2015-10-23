# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import sys

from hooks import install

from paste import fileapp
from paste import httpserver

import webapp2
from webapp2 import Route, RedirectHandler

from perf_insights_build import perf_insights_dev_server_config
from tracing_build import tracing_dev_server_config

_UNIT_TEST_HTML = """<html><body>
<h1>Run Unit Tests</h1>
<ul>
%s
</ul>
</body></html>
"""

_UNIT_TEST_LINK = '<li><a href="%s">%s</a></li>'


def _GetFilesIn(basedir):
  data_files = []
  for dirpath, dirnames, filenames in os.walk(basedir, followlinks=True):
    new_dirnames = [d for d in dirnames if not d.startswith('.')]
    del dirnames[:]
    dirnames += new_dirnames

    for f in filenames:
      if f.startswith('.'):
        continue
      if f == 'README.md':
        continue
      full_f = os.path.join(dirpath, f)
      rel_f = os.path.relpath(full_f, basedir)
      data_files.append(rel_f)

  data_files.sort()
  return data_files


class TestResultHandler(webapp2.RequestHandler):
  def post(self, *args, **kwargs):  # pylint: disable=unused-argument
    msg = self.request.body
    ostream = sys.stdout if 'PASSED' in msg else sys.stderr
    ostream.write(msg + '\n')
    return self.response.write('')


class TestsCompletedHandler(webapp2.RequestHandler):
  def post(self, *args, **kwargs):  # pylint: disable=unused-argument
    msg = self.request.body
    sys.stdout.write(msg + '\n')
    exit_code=(0 if 'ALL_PASSED' in msg else 1)
    if hasattr(self.app.server, 'please_exit'):
      self.app.server.please_exit(exit_code)
    return self.response.write('')


class DirectoryListingHandler(webapp2.RequestHandler):
  def get(self, *args, **kwargs):  # pylint: disable=unused-argument
    source_path = kwargs.pop('_source_path', None)
    mapped_path = kwargs.pop('_mapped_path', None)
    assert mapped_path.endswith('/')

    data_files_relative_to_top = _GetFilesIn(source_path)
    data_files = [mapped_path + x
                  for x in data_files_relative_to_top]

    files_as_json = json.dumps(data_files)
    self.response.content_type = 'application/json'
    return self.response.write(files_as_json)


class FileAppWithGZipHandling(fileapp.FileApp):
  def guess_type(self):
    content_type, content_encoding = \
        super(FileAppWithGZipHandling, self).guess_type()
    if not self.filename.endswith('.gz'):
      return content_type, content_encoding
    # By default, FileApp serves gzip files as their underlying type with
    # Content-Encoding of gzip. That causes them to show up on the client
    # decompressed. That ends up being surprising to our xhr.html system.
    return None, None

class SourcePathsHandler(webapp2.RequestHandler):
  def get(self, *args, **kwargs):  # pylint: disable=unused-argument
    source_paths = kwargs.pop('_source_paths', [])

    path = self.request.path
    # This is how we do it. Its... strange, but its what we've done since
    # the dawn of time. Aka 4 years ago, lol.
    for mapped_path in source_paths:
      rel = os.path.relpath(path, '/')
      candidate = os.path.join(mapped_path, rel)
      if os.path.exists(candidate):
        app = FileAppWithGZipHandling(candidate)
        app.cache_control(no_cache=True)
        return app
    self.abort(404)


class SimpleDirectoryHandler(webapp2.RequestHandler):
  def get(self, *args, **kwargs):  # pylint: disable=unused-argument
    top_path = os.path.abspath(kwargs.pop('_top_path', None))
    if not top_path.endswith(os.path.sep):
      top_path += os.path.sep

    joined_path = os.path.abspath(
        os.path.join(top_path, kwargs.pop('rest_of_path')))
    if not joined_path.startswith(top_path):
      self.response.set_status(403)
      return
    app = FileAppWithGZipHandling(joined_path)
    app.cache_control(no_cache=True)
    return app


class TestOverviewHandler(webapp2.RequestHandler):
  def get(self, *args, **kwargs):  # pylint: disable=unused-argument
    links = []
    for name, path in kwargs.pop('pds').iteritems():
      links.append(_UNIT_TEST_LINK % (path, name))
    self.response.out.write(_UNIT_TEST_HTML % '\n'.join(links))


def CreateApp(pds, args):
  default_tests = dict((pd.GetName(), pd.GetRunUnitTestsUrl()) for pd in pds)
  routes = [
    Route('/tests.html', TestOverviewHandler, defaults={'pds': default_tests}),
    Route('', RedirectHandler, defaults={'_uri': '/tests.html'}),
    Route('/', RedirectHandler, defaults={'_uri': '/tests.html'}),
  ]
  for pd in pds:
    routes += pd.GetRoutes(args)
    routes += [
      Route('/%s/notify_test_result' % pd.GetName(), TestResultHandler),
      Route('/%s/notify_tests_completed' % pd.GetName(), TestsCompletedHandler)
    ]

  for pd in pds:
    # Test data system.
    for mapped_path, source_path in pd.GetTestDataPaths(args):
      routes.append(Route('%s__file_list__' % mapped_path,
                          DirectoryListingHandler,
                          defaults={
                              '_source_path': source_path,
                              '_mapped_path': mapped_path
                          }))
      routes.append(Route('%s<rest_of_path:.+>' % mapped_path,
                          SimpleDirectoryHandler,
                          defaults={'_top_path': source_path}))

  # This must go last, because its catch-all.
  #
  # Its funky that we have to add in the root path. The long term fix is to
  # stop with the crazy multi-source-pathing thing.
  all_paths = []
  for pd in pds:
    all_paths += pd.GetSourcePaths(args)
  routes.append(
    Route('/<:.+>', SourcePathsHandler,
          defaults={'_source_paths': all_paths}))


  app = webapp2.WSGIApplication(routes=routes, debug=True)
  return app

def _AddPleaseExitMixinToServer(server):
  # Shutting down httpserver gracefully and yielding a return code requires
  # a bit of mixin code.

  exitCodeAttempt = []
  def please_exit(exitCode):
    if len(exitCodeAttempt) > 0:
      return
    exitCodeAttempt.append(exitCode)
    server.running = False

  real_serve_forever = server.serve_forever

  def serve_forever():
    try:
      real_serve_forever()
    except KeyboardInterrupt:
        # allow CTRL+C to shutdown
        return 255

    if len(exitCodeAttempt) == 1:
      return exitCodeAttempt[0]
    # The serve_forever returned for some reason separate from
    # exit_please.
    return 0

  server.please_exit = please_exit
  server.serve_forever = serve_forever


def _AddCommandLineArguments(pds, argv):
  parser = argparse.ArgumentParser(description='Run development server')
  parser.add_argument(
    '--no-install-hooks', dest='install_hooks', action='store_false')
  parser.add_argument('-p', '--port', default=8003, type=int)
  for pd in pds:
    g = parser.add_argument_group(pd.GetName())
    pd.AddOptionstToArgParseGroup(g)
  args = parser.parse_args(args=argv[1:])
  return args


def Main(argv):
  pds = [
      perf_insights_dev_server_config.PerfInsightsDevServerConfig(),
      tracing_dev_server_config.TracingDevServerConfig(),
  ]

  args = _AddCommandLineArguments(pds, argv)

  if args.install_hooks:
    install.InstallHooks()

  app = CreateApp(pds, args)

  server = httpserver.serve(app, host='127.0.0.1', port=args.port,
                            start_loop=False)
  _AddPleaseExitMixinToServer(server)
  app.server = server

  sys.stderr.write('Now running on http://127.0.0.1:%i\n' % server.server_port)

  return server.serve_forever()
