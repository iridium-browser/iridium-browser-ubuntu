// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_LEVELDB_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_LEVELDB_DATABASE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_collision_warner.h"

namespace leveldb {
class DB;
class Env;
struct Options;
}  // namespace leveldb

namespace leveldb_proto {

// Interacts with the LevelDB third party module.
// Once constructed, function calls and destruction should all occur on the
// same thread (not necessarily the same as the constructor).
class LevelDB {
 public:
  LevelDB();
  virtual ~LevelDB();

  virtual bool InitWithOptions(const base::FilePath& database_dir,
                               const leveldb::Options& options);
  virtual bool Init(const base::FilePath& database_dir);
  virtual bool Save(const base::StringPairs& pairs_to_save,
                    const std::vector<std::string>& keys_to_remove);
  virtual bool Load(std::vector<std::string>* entries);

 private:
  DFAKE_MUTEX(thread_checker_);

  // The declaration order of these members matters: |db_| depends on |env_| and
  // therefore has to be destructed first.
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_LEVELDB_DATABASE_H_
