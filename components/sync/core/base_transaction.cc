// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/core/base_transaction.h"

#include "components/sync/base/cryptographer.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/nigori_handler.h"

namespace syncer {

//////////////////////////////////////////////////////////////////////////
// BaseTransaction member definitions
BaseTransaction::BaseTransaction(UserShare* share) : user_share_(share) {
  DCHECK(share && share->directory.get());
}

BaseTransaction::~BaseTransaction() {}

Cryptographer* BaseTransaction::GetCryptographer() const {
  return GetDirectory()->GetCryptographer(this->GetWrappedTrans());
}

ModelTypeSet BaseTransaction::GetEncryptedTypes() const {
  syncable::NigoriHandler* nigori_handler = GetDirectory()->GetNigoriHandler();
  return nigori_handler
             ? nigori_handler->GetEncryptedTypes(this->GetWrappedTrans())
             : ModelTypeSet();
}

}  // namespace syncer
