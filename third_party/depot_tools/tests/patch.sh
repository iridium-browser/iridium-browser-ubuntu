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
  git checkout -q -b work
  echo "some work done on a branch" >> test
  git add test; git commit -q -m "branch work"

  git config rietveld.server localhost:10000

  # Prevent the editor from coming up when you upload.
  export GIT_EDITOR=$(which true)

  test_expect_success "upload succeeds (needs a server running on localhost)" \
    "$GIT_CL upload --no-oauth2 -m test master | grep -q 'Issue created'"

  test_expect_success "git-cl status now knows the issue" \
    "$GIT_CL_STATUS | grep -q 'Issue number'"

  ISSUE=$($GIT_CL_STATUS | awk '$0 ~ "Issue number:" { print $3 }')

  git checkout -q -b test2 master

  test_expect_success "$GIT_CL patch $ISSUE --no-oauth2"
)
SUCCESS=$?

cleanup

if [ $SUCCESS == 0 ]; then
  echo PASS
fi
