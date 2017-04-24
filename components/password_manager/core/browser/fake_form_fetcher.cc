// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/fake_form_fetcher.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/statistics_table.h"

using autofill::PasswordForm;

namespace password_manager {

FakeFormFetcher::FakeFormFetcher() = default;

FakeFormFetcher::~FakeFormFetcher() = default;

void FakeFormFetcher::AddConsumer(Consumer* consumer) {
  consumers_.insert(consumer);
}

FormFetcher::State FakeFormFetcher::GetState() const {
  return state_;
}

const std::vector<InteractionsStats>& FakeFormFetcher::GetInteractionsStats()
    const {
  return stats_;
}

const std::vector<const autofill::PasswordForm*>&
FakeFormFetcher::GetFederatedMatches() const {
  return federated_;
}

void FakeFormFetcher::SetNonFederated(
    const std::vector<const autofill::PasswordForm*>& non_federated,
    size_t filtered_count) {
  state_ = State::NOT_WAITING;
  for (Consumer* consumer : consumers_) {
    consumer->ProcessMatches(non_federated, filtered_count);
  }
}

void FakeFormFetcher::Fetch() {
  state_ = State::WAITING;
}

}  // namespace password_manager
