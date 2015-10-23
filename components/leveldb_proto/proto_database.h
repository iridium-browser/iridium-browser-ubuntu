// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}

namespace leveldb_proto {

// Interface for classes providing persistent storage of Protocol Buffer
// entries (T must be a Proto type extending MessageLite).
template <typename T>
class ProtoDatabase {
 public:
  using InitCallback = base::Callback<void(bool success)>;
  using UpdateCallback = base::Callback<void(bool success)>;
  using LoadCallback =
      base::Callback<void(bool success, scoped_ptr<std::vector<T>>)>;

  // A list of key-value (string, T) tuples.
  using KeyEntryVector = std::vector<std::pair<std::string, T>>;

  virtual ~ProtoDatabase() {}

  // Asynchronously initializes the object. |callback| will be invoked on the
  // calling thread when complete.
  virtual void Init(const base::FilePath& database_dir,
                    const InitCallback& callback) = 0;

  // Asynchronously saves |entries_to_save| and deletes entries from
  // |keys_to_remove| from the database. |callback| will be invoked on the
  // calling thread when complete.
  virtual void UpdateEntries(
      scoped_ptr<KeyEntryVector> entries_to_save,
      scoped_ptr<std::vector<std::string>> keys_to_remove,
      const UpdateCallback& callback) = 0;

  // Asynchronously loads all entries from the database and invokes |callback|
  // when complete.
  virtual void LoadEntries(const LoadCallback& callback) = 0;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PROTO_DATABASE_H_
