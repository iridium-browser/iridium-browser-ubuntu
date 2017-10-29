#!/usr/bin/env bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

. ./test-lib.sh

setup_git_remote
setup_git_checkout

(
  set -e
  cd git_checkout
  git checkout -q -b work HEAD^
  git checkout -q -t -b work2 work
  echo "some work done on a branch that tracks a local branch" >> test
  git add test; git commit -q -m "local tracking branch work"

  git config rietveld.server localhost:10000

  # Prevent the editor from coming up when you upload.
  export GIT_EDITOR=$(which true)
  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload --no-oauth2 -m test | grep -q 'Issue created'"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
