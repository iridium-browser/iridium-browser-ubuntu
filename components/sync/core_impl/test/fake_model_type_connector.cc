// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/test/fake_model_type_connector.h"

#include "components/sync/core/activation_context.h"

namespace syncer_v2 {

FakeModelTypeConnector::FakeModelTypeConnector() {}

FakeModelTypeConnector::~FakeModelTypeConnector() {}

void FakeModelTypeConnector::ConnectType(
    syncer::ModelType type,
    std::unique_ptr<ActivationContext> activation_context) {}

void FakeModelTypeConnector::DisconnectType(syncer::ModelType type) {}

}  // namespace syncer_v2
