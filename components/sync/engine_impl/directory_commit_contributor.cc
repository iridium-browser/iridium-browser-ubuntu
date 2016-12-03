// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/directory_commit_contributor.h"

#include <stddef.h>

#include "components/sync/engine_impl/cycle/directory_type_debug_info_emitter.h"
#include "components/sync/engine_impl/directory_commit_contribution.h"

namespace syncer {

DirectoryCommitContributor::DirectoryCommitContributor(
    syncable::Directory* dir,
    ModelType type,
    DirectoryTypeDebugInfoEmitter* debug_info_emitter)
    : dir_(dir), type_(type), debug_info_emitter_(debug_info_emitter) {}

DirectoryCommitContributor::~DirectoryCommitContributor() {}

std::unique_ptr<CommitContribution> DirectoryCommitContributor::GetContribution(
    size_t max_entries) {
  return DirectoryCommitContribution::Build(dir_, type_, max_entries,
                                            debug_info_emitter_);
}

}  // namespace syncer
