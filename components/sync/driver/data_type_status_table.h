// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_STATUS_TABLE_H_
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_STATUS_TABLE_H_

#include <map>
#include <string>

#include "components/sync/driver/data_type_manager.h"

namespace sync_driver {

// Class to keep track of data types that have encountered an error during sync.
class DataTypeStatusTable {
 public:
  typedef std::map<syncer::ModelType, syncer::SyncError> TypeErrorMap;

  DataTypeStatusTable();
  DataTypeStatusTable(const DataTypeStatusTable& other);
  ~DataTypeStatusTable();

  // Copy and assign welcome.

  // Update the failed datatypes. Types will be added to their corresponding
  // error map based on their |error_type()|.
  void UpdateFailedDataTypes(const TypeErrorMap& errors);

  // Resets the current set of data type errors.
  void Reset();

  // Resets the set of types with cryptographer errors.
  void ResetCryptoErrors();

  // Resets those persistence errors that intersect with |purged_types|.
  void ResetPersistenceErrorsFrom(syncer::ModelTypeSet purged_types);

  // Removes |type| from the data_type_errors_ set. Returns true if the type
  // was removed from the error set, false if the type did not have a data type
  // error to begin with.
  bool ResetDataTypeErrorFor(syncer::ModelType type);

  // Removes |type| from the unread_errors_ set. Returns true if the type
  // was removed from the error set, false if the type did not have an unready
  // error to begin with.
  bool ResetUnreadyErrorFor(syncer::ModelType type);

  // Returns a list of all the errors this class has recorded.
  TypeErrorMap GetAllErrors() const;

  // Returns all types with failure errors. This includes, fatal, crypto, and
  // unready types.`
  syncer::ModelTypeSet GetFailedTypes() const;

  // Returns the types that are failing due to unrecoverable or datatype errors.
  syncer::ModelTypeSet GetFatalErrorTypes() const;

  // Returns the types that are failing due to cryptographer errors.
  syncer::ModelTypeSet GetCryptoErrorTypes() const;

  // Returns the types that are failing due to persistence errors.
  syncer::ModelTypeSet GetPersistenceErrorTypes() const;

  // Returns the types that cannot be configured due to not being ready.
  syncer::ModelTypeSet GetUnreadyErrorTypes() const;

  // Returns the types that triggered the unrecoverable error.
  syncer::ModelTypeSet GetUnrecoverableErrorTypes() const;

  // Returns the current unrecoverable error, if there is one.
  syncer::SyncError GetUnrecoverableError() const;

 private:
  // The current unrecoverable errors. Only one unrecoverable error can be
  // active at a time, but it may apply to more than one type.
  TypeErrorMap unrecoverable_errors_;

  // List of data types that failed due to runtime errors and should be
  // disabled. These are different from unrecoverable_errors_ in that
  // ResetDataTypeError can remove them from this list.
  TypeErrorMap data_type_errors_;

  // List of data types that failed due to the cryptographer not being ready.
  TypeErrorMap crypto_errors_;

  // List of data types that failed because sync did not persist the newest
  // version of their data.
  TypeErrorMap persistence_errors_;

  // List of data types that could not start due to not being ready. These can
  // be marked as ready by calling ResetUnreadyErrorFor(..).
  TypeErrorMap unready_errors_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_STATUS_TABLE_H_
