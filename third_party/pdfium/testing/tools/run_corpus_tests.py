#!/usr/bin/env python
# Copyright 2015 The PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import subprocess
import shutil
import sys

# Nomenclature:
#   x_root - "x"
#   x_filename - "x.ext"
#   x_path - "path/to/a/b/c/x.ext"
#   c_dir - "path/to/a/b/c"

def extract_suppressions(filename):
  with open(filename) as f:
    suppressions = [y for y in [
      x.split('#')[0].strip() for x in f.readlines()] if y]
  return suppressions

def test_one_file(input_filename, source_dir, working_dir,
                  pdfium_test_path, pdfium_diff_path):
  input_root, _ = os.path.splitext(input_filename)
  input_path = os.path.join(source_dir, input_filename)
  pdf_path = os.path.join(working_dir, input_filename)
  actual_path_template = os.path.join(working_dir, input_root + '.pdf.%d.png')
  expected_path_template = os.path.join(source_dir,
                                        input_root + '_expected.pdf.%d.png')
  try:
    shutil.copyfile(input_path, pdf_path)
    sys.stdout.flush()
    subprocess.check_call([pdfium_test_path, '--png', pdf_path])
    i = 0;
    while True:
      expected_path = expected_path_template % i;
      actual_path = actual_path_template % i;
      if not os.path.exists(expected_path):
        if i == 0:
          print "WARNING: no expected results files found for " + input_filename
        break
      print "Checking " + actual_path
      sys.stdout.flush()
      subprocess.check_call([pdfium_diff_path, expected_path, actual_path])
      i += 1
  except subprocess.CalledProcessError as e:
    print "FAILURE: " + input_filename + "; " + str(e)
    return False
  return True

def main():
  if sys.platform.startswith('linux'):
    os_name = 'linux'
  elif sys.platform.startswith('win'):
    os_name = 'win'
  elif sys.platform.startswith('darwin'):
    os_name = 'mac'
  else:
    print 'Confused, can not determine OS, aborting.'
    return 1

  parser = optparse.OptionParser()
  parser.add_option('--build-dir', default=os.path.join('out', 'Debug'),
                    help='relative path from the base source directory')
  options, args = parser.parse_args()

  # Expect |my_dir| to be .../pdfium/testing/tools.
  my_dir = os.path.dirname(os.path.realpath(__file__))
  testing_dir = os.path.dirname(my_dir)
  pdfium_dir = os.path.dirname(testing_dir)
  if (os.path.basename(my_dir) != 'tools' or
      os.path.basename(testing_dir) != 'testing'):
    print 'Confused, can not find pdfium root directory, aborting.'
    return 1

  # Find path to build directory.  This depends on whether this is a
  # standalone build vs. a build as part of a chromium checkout. For
  # standalone, we expect a path like .../pdfium/out/Debug, but for
  # chromium, we expect a path like .../src/out/Debug two levels
  # higher (to skip over the third_party/pdfium path component under
  # which chromium sticks pdfium).
  base_dir = pdfium_dir
  one_up_dir = os.path.dirname(base_dir)
  two_up_dir = os.path.dirname(one_up_dir)
  if (os.path.basename(two_up_dir) == 'src' and
      os.path.basename(one_up_dir) == 'third_party'):
    base_dir = two_up_dir
  build_dir = os.path.join(base_dir, options.build_dir)

  # Compiled binaries are found under the build path.
  pdfium_test_path = os.path.join(build_dir, 'pdfium_test')
  pdfium_diff_path = os.path.join(build_dir, 'pdfium_diff')
  if os_name == 'win':
    pdfium_test_path = pdfium_test_path + '.exe'
    pdfium_diff_path = pdfium_diff_path + '.exe'
  # TODO(tsepez): Mac may require special handling here.

  # Place generated files under the build directory, not source directory.
  working_dir = os.path.join(build_dir, 'gen', 'pdfium', 'testing', 'corpus')
  if not os.path.exists(working_dir):
    os.makedirs(working_dir)

  suppression_list = extract_suppressions(
    os.path.join(testing_dir, 'SUPPRESSIONS'))

  platform_suppression_filename = 'SUPPRESSIONS_%s' % os_name
  platform_suppression_list = extract_suppressions(
    os.path.join(testing_dir, platform_suppression_filename))

  # test files are under .../pdfium/testing/corpus.
  failures = []
  walk_from_dir = os.path.join(testing_dir, 'corpus');
  input_file_re = re.compile('^[a-zA-Z0-9_.]+[.]pdf$')
  for source_dir, _, filename_list in os.walk(walk_from_dir):
    for input_filename in filename_list:
      if input_file_re.match(input_filename):
         input_path = os.path.join(source_dir, input_filename)
         if os.path.isfile(input_path):
           if input_filename in suppression_list:
             print "Not running %s, found in SUPPRESSIONS file" % input_filename
             continue
           if input_filename in platform_suppression_list:
             print ("Not running %s, found in %s file" %
                    (input_filename, platform_suppression_filename))
             continue

         if not test_one_file(input_filename, source_dir, working_dir,
                                pdfium_test_path, pdfium_diff_path):
             failures.append(input_path)

  if failures:
    print '\n\nSummary of Failures:'
    for failure in failures:
      print failure
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(main())
