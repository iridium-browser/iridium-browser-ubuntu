# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the gslib module."""

from __future__ import print_function

import base64
import datetime
import errno
import mox
import os

from chromite.lib import cros_test_lib

from chromite.lib.paygen import gslib
from chromite.lib.paygen import utils


# Typical output for a GS failure that is not our fault, and we should retry.
GS_RETRY_FAILURE = ('GSResponseError: status=403, code=InvalidAccessKeyId,'
                    'reason="Forbidden", message="Blah Blah Blah"')
# Typical output for a failure that we should not retry.
GS_DONE_FAILURE = ('AccessDeniedException:')


class TestGsLib(cros_test_lib.MoxTestCase):
  """Test gslib module."""

  def setUp(self):
    self.bucket_name = 'somebucket'
    self.bucket_uri = 'gs://%s' % self.bucket_name

    # Because of autodetection, we no longer know which gsutil binary
    # will be used.
    self.gsutil = mox.IsA(str)

  def testRetryGSLib(self):
    """Test our retry decorator"""
    @gslib.RetryGSLib
    def Success():
      pass

    @gslib.RetryGSLib
    def SuccessArguments(arg1, arg2=False, arg3=False):
      self.assertEqual(arg1, 1)
      self.assertEqual(arg2, 2)
      self.assertEqual(arg3, 3)

    class RetryTestException(gslib.GSLibError):
      """Testing gslib.GSLibError exception for Retrying cases."""

      def __init__(self):
        super(RetryTestException, self).__init__(GS_RETRY_FAILURE)

    class DoneTestException(gslib.GSLibError):
      """Testing gslib.GSLibError exception for Done cases."""

      def __init__(self):
        super(DoneTestException, self).__init__(GS_DONE_FAILURE)

    @gslib.RetryGSLib
    def Fail():
      raise RetryTestException()

    @gslib.RetryGSLib
    def FailCount(counter, exception):
      """Pass in [count] times to fail before passing.

      Using [] means the same object is used each retry, but it's contents
      are mutable.
      """
      counter[0] -= 1
      if counter[0] >= 0:
        raise exception()

      if exception == RetryTestException:
        # Make sure retries ran down to -1.
        self.assertEquals(-1, counter[0])

    Success()
    SuccessArguments(1, 2, 3)
    SuccessArguments(1, arg3=3, arg2=2)

    FailCount([1], RetryTestException)
    FailCount([2], RetryTestException)

    self.assertRaises(RetryTestException, Fail)
    self.assertRaises(DoneTestException, FailCount, [1], DoneTestException)
    self.assertRaises(gslib.CopyFail, FailCount, [3], gslib.CopyFail)
    self.assertRaises(gslib.CopyFail, FailCount, [4], gslib.CopyFail)

  def testIsGsURI(self):
    self.assertTrue(gslib.IsGsURI('gs://bucket/foo/bar'))
    self.assertTrue(gslib.IsGsURI('gs://bucket'))
    self.assertTrue(gslib.IsGsURI('gs://'))

    self.assertFalse(gslib.IsGsURI('file://foo/bar'))
    self.assertFalse(gslib.IsGsURI('/foo/bar'))

  def testSplitGSUri(self):
    self.assertEqual(('foo', 'hi/there'),
                     gslib.SplitGSUri('gs://foo/hi/there'))
    self.assertEqual(('foo', 'hi/there/'),
                     gslib.SplitGSUri('gs://foo/hi/there/'))
    self.assertEqual(('foo', ''),
                     gslib.SplitGSUri('gs://foo'))
    self.assertEqual(('foo', ''),
                     gslib.SplitGSUri('gs://foo/'))
    self.assertRaises(gslib.URIError, gslib.SplitGSUri,
                      'file://foo/hi/there')
    self.assertRaises(gslib.URIError, gslib.SplitGSUri,
                      '/foo/hi/there')

  def testRunGsutilCommand(self):
    args = ['TheCommand', 'Arg1', 'Arg2']
    cmd = [self.gsutil] + args

    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    # Run 1.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(1)
    # Run 2.
    utils.RunCommand(cmd, redirect_stdout=False, redirect_stderr=True,
                     return_result=True).AndReturn(2)
    # Run 3.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True, error_ok=True).AndReturn(3)
    # Run 4.
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).AndRaise(utils.CommandFailedException())
    # Run 5.
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).AndRaise(OSError(errno.ENOENT, 'errmsg'))
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEqual(1, gslib.RunGsutilCommand(args))
    self.assertEqual(2, gslib.RunGsutilCommand(args, redirect_stdout=False))
    self.assertEqual(3, gslib.RunGsutilCommand(args, error_ok=True))
    self.assertRaises(gslib.GSLibError, gslib.RunGsutilCommand, args)
    self.assertRaises(gslib.GsutilMissingError, gslib.RunGsutilCommand, args)
    self.mox.VerifyAll()

  def testCopy(self):
    src_path = '/path/to/some/file'
    dest_path = 'gs://bucket/some/gs/path'

    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    # Run 1, success.
    cmd = [self.gsutil, 'cp', src_path, dest_path]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    # Run 2, failure.
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      cmd = [self.gsutil, 'cp', src_path, dest_path]
      utils.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True, return_result=True
      ).AndRaise(utils.CommandFailedException(GS_RETRY_FAILURE))
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.Copy(src_path, dest_path)
    self.assertRaises(gslib.CopyFail, gslib.Copy, src_path, dest_path)
    self.mox.VerifyAll()

  def testMove(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    src_path = 'gs://bucket/some/gs/path'
    dest_path = '/some/other/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'mv', src_path, dest_path]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.Move(src_path, dest_path)
    self.mox.VerifyAll()

  def testRemove(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path1 = 'gs://bucket/some/gs/path'
    path2 = 'gs://bucket/some/other/path'

    # Set up the test replay script.
    # Run 1, one path.
    utils.RunCommand([self.gsutil, 'rm', path1],
                     redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    # Run 2, two paths.
    utils.RunCommand([self.gsutil, 'rm', path1, path2],
                     redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    # Run 3, one path, recursive.
    utils.RunCommand([self.gsutil, 'rm', '-R', path1],
                     redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.Remove(path1)
    gslib.Remove(path1, path2)
    gslib.Remove(path1, recurse=True)
    self.mox.VerifyAll()

  def testRemoveNoMatch(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'rm', path]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.Remove(path, ignore_no_match=True)
    self.mox.VerifyAll()

  def testRemoveFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'rm', path]
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      utils.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True, return_result=True,
      ).AndRaise(utils.CommandFailedException(GS_RETRY_FAILURE))
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.RemoveFail,
                      gslib.Remove, path)
    self.mox.VerifyAll()

  def testCreateWithContents(self):
    gs_path = 'gs://chromeos-releases-test/create-with-contents-test'
    contents = 'Stuff with Rocks In'

    self.mox.StubOutWithMock(gslib, 'Copy')

    gslib.Copy(mox.IsA(str), gs_path)
    self.mox.ReplayAll()

    gslib.CreateWithContents(gs_path, contents)
    self.mox.VerifyAll()

  def testCat(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'cat', path]
    result = cros_test_lib.EasyAttr(error='', output='TheContent')
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(result)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.Cat(path)
    self.assertEquals('TheContent', result)
    self.mox.VerifyAll()

  def testCatFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'cat', path]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).AndRaise(utils.CommandFailedException())
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.CatFail, gslib.Cat, path)
    self.mox.VerifyAll()

  def testStat(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'stat', path]
    result = cros_test_lib.EasyAttr(error='', output='')
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(result)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertIs(gslib.Stat(path), None)
    self.mox.VerifyAll()

  def testStatFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    path = 'gs://bucket/some/gs/path'

    # Set up the test replay script.
    cmd = [self.gsutil, 'stat', path]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).AndRaise(utils.CommandFailedException())
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.StatFail, gslib.Stat, path)
    self.mox.VerifyAll()

  def testCreateBucket(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    cmd = [self.gsutil, 'mb', self.bucket_uri]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.CreateBucket(self.bucket_name)
    self.mox.VerifyAll()

  def testCreateBucketFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    cmd = [self.gsutil, 'mb', self.bucket_uri]
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      utils.RunCommand(
          cmd, redirect_stdout=True, redirect_stderr=True, return_result=True
      ).AndRaise(utils.CommandFailedException(GS_RETRY_FAILURE))
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.BucketOperationError,
                      gslib.CreateBucket, self.bucket_name)
    self.mox.VerifyAll()

  def testDeleteBucket(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    cmd = [self.gsutil, 'rm', '%s/*' % self.bucket_uri]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     error_ok=True, return_result=True)
    cmd = [self.gsutil, 'rb', self.bucket_uri]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.DeleteBucket(self.bucket_name)
    self.mox.VerifyAll()

  def testDeleteBucketFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    cmd = [self.gsutil, 'rm', '%s/*' % self.bucket_uri]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True, error_ok=True,
        return_result=True).AndRaise(
            utils.CommandFailedException(GS_DONE_FAILURE))
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.BucketOperationError,
                      gslib.DeleteBucket, self.bucket_name)
    self.mox.VerifyAll()

  def testFileSize(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = '%s/%s' % (self.bucket_uri, 'some/file/path')

    # Set up the test replay script.
    cmd = [self.gsutil, '-d', 'stat', gs_uri]
    size = 96
    output = '\n'.join(['header: x-goog-generation: 1386322968237000',
                        'header: x-goog-metageneration: 1',
                        'header: x-goog-stored-content-encoding: identity',
                        'header: x-goog-stored-content-length: %d' % size,
                        'header: Content-Type: application/octet-stream'])

    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).AndReturn(
            cros_test_lib.EasyAttr(output=output))
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.FileSize(gs_uri)
    self.assertEqual(size, result)
    self.mox.VerifyAll()

  def testFileTimestamp(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = '%s/%s' % (self.bucket_uri, 'some/file/path')

    # Set up the test replay script.
    cmd = [self.gsutil, 'ls', '-l', gs_uri]
    output = '\n'.join([
        '        96  2012-05-17T14:00:33  gs://bucket/chromeos.bin.md5',
        'TOTAL: 1 objects, 96 bytes (96.0 B)',
    ])
    cmd_result = cros_test_lib.EasyAttr(output=output)

    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.FileTimestamp(gs_uri)
    self.assertEqual(datetime.datetime(2012, 5, 17, 14, 0, 33),
                     result)
    self.mox.VerifyAll()

  def _TestCatWithHeaders(self, gs_uri, cmd_output, cmd_error):
    self.mox.StubOutWithMock(utils, 'RunCommand')

    # Set up the test replay script.
    # Run 1, versioning not enabled in bucket, one line of output.
    cmd = ['gsutil', '-d', 'cat', gs_uri]
    cmd_result = cros_test_lib.EasyAttr(output=cmd_output,
                                        error=cmd_error,
                                        cmdstr=' '.join(cmd))
    cmd[0] = mox.IsA(str)
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result)
    self.mox.ReplayAll()

  def testCatWithHeaders(self):
    gs_uri = '%s/%s' % (self.bucket_uri, 'some/file/path')
    generation = 123454321
    metageneration = 2
    error = '\n'.join([
        'header: x-goog-generation: %d' % generation,
        'header: x-goog-metageneration: %d' % metageneration,
    ])
    expected_output = 'foo'
    self._TestCatWithHeaders(gs_uri, expected_output, error)

    # Run the test verification.
    headers = {}
    result = gslib.Cat(gs_uri, headers=headers)
    self.assertEqual(generation, int(headers['generation']))
    self.assertEqual(metageneration, int(headers['metageneration']))
    self.assertEqual(result, expected_output)
    self.mox.VerifyAll()

  def testFileSizeNoSuchFile(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = '%s/%s' % (self.bucket_uri, 'some/file/path')

    # Set up the test replay script.
    cmd = [self.gsutil, '-d', 'stat', gs_uri]
    for _ in xrange(0, gslib.RETRY_ATTEMPTS + 1):
      utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                       return_result=True).AndRaise(
                           utils.CommandFailedException)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.URIError, gslib.FileSize, gs_uri)
    self.mox.VerifyAll()

  def testListFiles(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    files = [
        '%s/some/path' % self.bucket_uri,
        '%s/some/file/path' % self.bucket_uri,
    ]
    directories = [
        '%s/some/dir/' % self.bucket_uri,
        '%s/some/dir/path/' % self.bucket_uri,
    ]

    gs_uri = '%s/**' % self.bucket_uri
    cmd = [self.gsutil, 'ls', gs_uri]

    # Prepare cmd_result for a good run.
    # Fake a trailing empty line.
    output = '\n'.join(files + directories + [''])
    cmd_result_ok = cros_test_lib.EasyAttr(output=output, returncode=0)

    # Prepare exception for a run that finds nothing.
    stderr = 'CommandException: One or more URLs matched no objects.\n'
    empty_exception = utils.CommandFailedException(stderr)

    # Prepare exception for a run that triggers a GS failure.
    failure_exception = utils.CommandFailedException(GS_RETRY_FAILURE)

    # Set up the test replay script.
    # Run 1, runs ok.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result_ok)
    # Run 2, runs ok, sorts files.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result_ok)
    # Run 3, finds nothing.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndRaise(empty_exception)
    # Run 4, failure in GS.
    for _ix in xrange(gslib.RETRY_ATTEMPTS + 1):
      utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                       return_result=True).AndRaise(failure_exception)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.ListFiles(self.bucket_uri, recurse=True)
    self.assertEqual(files, result)
    result = gslib.ListFiles(self.bucket_uri, recurse=True, sort=True)
    self.assertEqual(sorted(files), result)
    result = gslib.ListFiles(self.bucket_uri, recurse=True)
    self.assertEqual([], result)
    self.assertRaises(gslib.GSLibError, gslib.ListFiles,
                      self.bucket_uri, recurse=True)
    self.mox.VerifyAll()

  def testListDirs(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    files = [
        '%s/some/path' % self.bucket_uri,
        '%s/some/file/path' % self.bucket_uri,
    ]
    directories = [
        '%s/some/dir/' % self.bucket_uri,
        '%s/some/dir/path/' % self.bucket_uri,
    ]

    gs_uri = '%s/**' % self.bucket_uri
    cmd = [self.gsutil, 'ls', gs_uri]

    # Prepare cmd_result for a good run.
    # Fake a trailing empty line.
    output = '\n'.join(files + directories + [''])
    cmd_result = cros_test_lib.EasyAttr(output=output, returncode=0)

    # Set up the test replay script.
    # Run 1.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result)
    # Run 2.
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True).AndReturn(cmd_result)
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.ListDirs(self.bucket_uri, recurse=True)
    self.assertEqual(directories, result)
    result = gslib.ListDirs(self.bucket_uri, recurse=True, sort=True)
    self.assertEqual(sorted(directories), result)
    self.mox.VerifyAll()

  def testCmp(self):
    uri1 = 'gs://some/gs/path'
    uri2 = 'gs://some/other/path'
    local_path = '/some/local/path'
    md5 = 'TheMD5Sum'

    self.mox.StubOutWithMock(gslib, 'MD5Sum')
    self.mox.StubOutWithMock(gslib.filelib, 'MD5Sum')

    # Set up the test replay script.
    # Run 1, same md5, both GS.
    gslib.MD5Sum(uri1).AndReturn(md5)
    gslib.MD5Sum(uri2).AndReturn(md5)
    # Run 2, different md5, both GS.
    gslib.MD5Sum(uri1).AndReturn(md5)
    gslib.MD5Sum(uri2).AndReturn('Other' + md5)
    # Run 3, same md5, one GS on local.
    gslib.MD5Sum(uri1).AndReturn(md5)
    gslib.filelib.MD5Sum(local_path).AndReturn(md5)
    # Run 4, different md5, one GS on local.
    gslib.MD5Sum(uri1).AndReturn(md5)
    gslib.filelib.MD5Sum(local_path).AndReturn('Other' + md5)
    # Run 5, missing file, both GS.
    gslib.MD5Sum(uri1).AndReturn(None)
    # Run 6, args are None.
    gslib.filelib.MD5Sum(None).AndReturn(None)
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertTrue(gslib.Cmp(uri1, uri2))
    self.assertFalse(gslib.Cmp(uri1, uri2))
    self.assertTrue(gslib.Cmp(uri1, local_path))
    self.assertFalse(gslib.Cmp(uri1, local_path))
    self.assertFalse(gslib.Cmp(uri1, uri2))
    self.assertFalse(gslib.Cmp(None, None))
    self.mox.VerifyAll()

  def testMD5SumAccessError(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = 'gs://bucket/foo/bar/somefile'
    crc32c = 'c96fd51e'
    crc32c_64 = base64.b64encode(base64.b16decode(crc32c, casefold=True))
    md5_sum = 'b026324c6904b2a9cb4b88d6d61c81d1'
    md5_sum_64 = base64.b64encode(base64.b16decode(md5_sum, casefold=True))
    output = '\n'.join([
        '%s:' % gs_uri,
        '        Creation time:          Tue, 04 Mar 2014 19:55:26 GMT',
        '        Content-Language:       en',
        '        Content-Length:         2',
        '        Content-Type:           application/octet-stream',
        '        Hash (crc32c):          %s' % crc32c_64,
        '        Hash (md5):             %s' % md5_sum_64,
        '        ETag:                   CMi938jU+bwCEAE=',
        '        Generation:             1393962926989000',
        '        Metageneration:         1',
        '        ACL:                    ACCESS DENIED. Note: you need OWNER '
        'permission',
        '                                on the object to read its ACL.',
    ])

    # Set up the test replay script.
    cmd = [self.gsutil, 'ls', '-L', gs_uri]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True, error_ok=True,
        return_result=True).AndReturn(
            cros_test_lib.EasyAttr(output=output))
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.MD5Sum(gs_uri)
    self.assertEqual(md5_sum, result)
    self.mox.VerifyAll()

  def testMD5SumAccessOK(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = 'gs://bucket/foo/bar/somefile'
    crc32c = 'c96fd51e'
    crc32c_64 = base64.b64encode(base64.b16decode(crc32c, casefold=True))
    md5_sum = 'b026324c6904b2a9cb4b88d6d61c81d1'
    md5_sum_64 = base64.b64encode(base64.b16decode(md5_sum, casefold=True))
    output = '\n'.join([
        '%s:' % gs_uri,
        '        Creation time:          Tue, 04 Mar 2014 19:55:26 GMT',
        '        Content-Language:       en',
        '        Content-Length:         2',
        '        Content-Type:           application/octet-stream',
        '        Hash (crc32c):          %s' % crc32c_64,
        '        Hash (md5):             %s' % md5_sum_64,
        '        ETag:                   CMi938jU+bwCEAE=',
        '        Generation:             1393962926989000',
        '        Metageneration:         1',
        '        ACL:            [',
        '  {',
        '    "entity": "project-owners-134157665460",',
        '    "projectTeam": {',
        '      "projectNumber": "134157665460",',
        '      "team": "owners"',
        '    },',
        '    "role": "OWNER"',
        '  }',
        ']',
    ])
    # Set up the test replay script.
    cmd = [self.gsutil, 'ls', '-L', gs_uri]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True, error_ok=True,
        return_result=True).AndReturn(
            cros_test_lib.EasyAttr(output=output))
    self.mox.ReplayAll()

    # Run the test verification.
    result = gslib.MD5Sum(gs_uri)
    self.assertEqual(md5_sum, result)
    self.mox.VerifyAll()

  def testSetACL(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = 'gs://bucket/foo/bar/somefile'
    acl_file = '/some/gs/acl/file'

    # Set up the test replay script.
    cmd = [self.gsutil, 'setacl', acl_file, gs_uri]
    utils.RunCommand(cmd, redirect_stdout=True, redirect_stderr=True,
                     return_result=True)
    self.mox.ReplayAll()

    # Run the test verification.
    gslib.SetACL(gs_uri, acl_file)
    self.mox.VerifyAll()

  def testSetACLFail(self):
    self.mox.StubOutWithMock(utils, 'RunCommand')
    gs_uri = 'gs://bucket/foo/bar/somefile'
    acl_file = '/some/gs/acl/file'

    # Set up the test replay script. (Multiple times because of retry logic)
    cmd = [self.gsutil, 'setacl', acl_file, gs_uri]
    utils.RunCommand(
        cmd, redirect_stdout=True, redirect_stderr=True,
        return_result=True).MultipleTimes().AndRaise(
            utils.CommandFailedException())
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertRaises(gslib.AclFail,
                      gslib.SetACL, gs_uri, acl_file)
    self.mox.VerifyAll()


class TestGsLibAccess(cros_test_lib.MoxTempDirTestCase):
  """Test access to gs lib functionality.

  The tests here require GS .boto access to the gs://chromeos-releases-public
  bucket, which is world-readable.  Any .boto setup should do, but without
  a .boto there will be failures.
  """
  small_gs_path = 'gs://chromeos-releases-public/small-test-file'

  @cros_test_lib.NetworkTest()
  def testCopyAndMD5Sum(self):
    """Higher-level functional test.  Test MD5Sum OK:

    1) List files on GS.
    2) Select a small one by asking for byte size of files on GS.
    3) Get MD5 sum of file on GS.
    4) Copy file down to local file.
    5) Recalculate MD5 sum for local file.
    6) Verify that MD5 values are the same.
    """
    gs_md5 = gslib.MD5Sum(self.small_gs_path)
    local_path = os.path.join(self.tempdir, 'md5-check-file')
    gslib.Copy(self.small_gs_path, local_path)
    local_md5 = gslib.filelib.MD5Sum(local_path)
    self.assertEqual(gs_md5, local_md5)

  @cros_test_lib.NetworkTest()
  def testExistsLazy(self):
    self.assertTrue(gslib.ExistsLazy(self.small_gs_path))

    bogus_gs_path = 'gs://chromeos-releases/wert/sdgi/sadg/sdgi'
    self.assertFalse(gslib.ExistsLazy(bogus_gs_path))

  @cros_test_lib.NetworkTest()
  def testExists(self):
    self.assertTrue(gslib.Exists(self.small_gs_path))

    bogus_gs_path = 'gs://chromeos-releases/wert/sdgi/sadg/sdgi'
    self.assertFalse(gslib.Exists(bogus_gs_path))

  @cros_test_lib.NetworkTest()
  def testExistsFalse(self):
    """Test Exists logic with non-standard output from gsutil."""
    expected_output = ('GSResponseError: status=404, code=NoSuchKey,'
                       ' reason="Not Found",'
                       ' message="The specified key does not exist."')
    err1 = gslib.StatFail(expected_output)
    err2 = gslib.StatFail('You are using a deprecated alias, "getacl",'
                          'for the "acl" command.\n' +
                          expected_output)

    uri = 'gs://any/fake/uri/will/do'
    cmd = ['stat', uri]

    self.mox.StubOutWithMock(gslib, 'RunGsutilCommand')

    # Set up the test replay script.
    # Run 1, normal.
    gslib.RunGsutilCommand(cmd, failed_exception=gslib.StatFail,
                           get_headers_from_stdout=True).AndRaise(err1)
    # Run 2, extra output.
    gslib.RunGsutilCommand(cmd, failed_exception=gslib.StatFail,
                           get_headers_from_stdout=True).AndRaise(err2)
    self.mox.ReplayAll()

    # Run the test verification
    self.assertFalse(gslib.Exists(uri))
    self.assertFalse(gslib.Exists(uri))
    self.mox.VerifyAll()

  @cros_test_lib.NetworkTest()
  def testMD5SumBadPath(self):
    """Higher-level functional test.  Test MD5Sum bad path:

    1) Make up random, non-existent gs path
    2) Ask for MD5Sum.  Make sure it fails, but with no exeption.
    """

    gs_path = 'gs://chromeos-releases/awsedrftgyhujikol'
    gs_md5 = gslib.MD5Sum(gs_path)
    self.assertTrue(gs_md5 is None)

  @cros_test_lib.NetworkTest()
  def testMD5SumBadBucket(self):
    """Higher-level functional test.  Test MD5Sum bad bucket:

    1) Make up random, non-existent gs bucket and path
    2) Ask for MD5Sum.  Make sure it fails, with exception
    """

    gs_path = 'gs://lokijuhygtfrdesxcv/awsedrftgyhujikol'
    gs_md5 = gslib.MD5Sum(gs_path)
    self.assertTrue(gs_md5 is None)
