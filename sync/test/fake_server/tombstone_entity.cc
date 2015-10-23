// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/fake_server/tombstone_entity.h"

#include <string>

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/sync.pb.h"
#include "sync/test/fake_server/fake_server_entity.h"

using std::string;

using syncer::ModelType;

namespace fake_server {

TombstoneEntity::~TombstoneEntity() { }

// static
scoped_ptr<FakeServerEntity> TombstoneEntity::Create(const string& id) {
  const ModelType model_type = GetModelTypeFromId(id);
  CHECK_NE(model_type, syncer::UNSPECIFIED) << "Invalid ID was given: " << id;
  return scoped_ptr<FakeServerEntity>(new TombstoneEntity(id, model_type));
}

TombstoneEntity::TombstoneEntity(const string& id,
                                 const ModelType& model_type)
    : FakeServerEntity(id, model_type, 0, string()) {
  sync_pb::EntitySpecifics specifics;
  AddDefaultFieldValue(model_type, &specifics);
  SetSpecifics(specifics);
}

string TombstoneEntity::GetParentId() const {
  return string();
}

void TombstoneEntity::SerializeAsProto(sync_pb::SyncEntity* proto) const {
  FakeServerEntity::SerializeBaseProtoFields(proto);
}

bool TombstoneEntity::IsDeleted() const {
  return true;
}

}  // namespace fake_server
