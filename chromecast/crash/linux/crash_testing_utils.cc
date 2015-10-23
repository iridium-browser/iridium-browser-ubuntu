// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/crash_testing_utils.h"

#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

#define RCHECK(cond, retval, err) \
  do {                            \
    LOG(ERROR) << (err);          \
    if (!(cond)) {                \
      return (retval);            \
    }                             \
  } while (0)

namespace chromecast {
namespace {

const char kRatelimitKey[] = "ratelimit";
const char kRatelimitPeriodStartKey[] = "period_start";
const char kRatelimitPeriodDumpsKey[] = "period_dumps";

scoped_ptr<base::ListValue> ParseLockFile(const std::string& path) {
  std::string lockfile_string;
  RCHECK(base::ReadFileToString(base::FilePath(path), &lockfile_string),
         nullptr,
         "Failed to read file");

  std::vector<std::string> lines = base::SplitString(
      lockfile_string, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  scoped_ptr<base::ListValue> dumps = make_scoped_ptr(new base::ListValue());

  // Validate dumps
  for (const std::string& line : lines) {
    if (line.size() == 0)
      continue;
    scoped_ptr<base::Value> dump_info = DeserializeFromJson(line);
    DumpInfo info(dump_info.get());
    RCHECK(info.valid(), nullptr, "Invalid DumpInfo");
    dumps->Append(dump_info.Pass());
  }

  return dumps;
}

scoped_ptr<base::Value> ParseMetadataFile(const std::string& path) {
  return DeserializeJsonFromFile(base::FilePath(path));
}

int WriteLockFile(const std::string& path, base::ListValue* contents) {
  DCHECK(contents);
  std::string lockfile;

  for (const base::Value* elem : *contents) {
    scoped_ptr<std::string> dump_info = SerializeToJson(*elem);
    RCHECK(dump_info, -1, "Failed to serialize DumpInfo");
    lockfile += *dump_info;
    lockfile += "\n";  // Add line seperatators
  }

  return WriteFile(base::FilePath(path), lockfile.c_str(), lockfile.size()) >= 0
             ? 0
             : -1;
}

bool WriteMetadataFile(const std::string& path, const base::Value* metadata) {
  DCHECK(metadata);
  return SerializeJsonToFile(base::FilePath(path), *metadata);
}

}  // namespace

scoped_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string) {
  scoped_ptr<base::Value> value(DeserializeFromJson(json_string));
  return make_scoped_ptr(new DumpInfo(value.get()));
}

bool FetchDumps(const std::string& lockfile_path,
                ScopedVector<DumpInfo>* dumps) {
  DCHECK(dumps);
  scoped_ptr<base::ListValue> dump_list = ParseLockFile(lockfile_path);
  RCHECK(dump_list, false, "Failed to parse lockfile");

  dumps->clear();

  for (base::Value* elem : *dump_list) {
    scoped_ptr<DumpInfo> dump = make_scoped_ptr(new DumpInfo(elem));
    RCHECK(dump->valid(), false, "Invalid DumpInfo");
    dumps->push_back(dump.Pass());
  }

  return true;
}

bool ClearDumps(const std::string& lockfile_path) {
  scoped_ptr<base::ListValue> dump_list =
      make_scoped_ptr(new base::ListValue());
  return WriteLockFile(lockfile_path, dump_list.get()) == 0;
}

bool CreateFiles(const std::string& lockfile_path,
                 const std::string& metadata_path) {
  scoped_ptr<base::DictionaryValue> metadata =
      make_scoped_ptr(new base::DictionaryValue());

  base::DictionaryValue* ratelimit_fields = new base::DictionaryValue();
  metadata->Set(kRatelimitKey, make_scoped_ptr(ratelimit_fields));
  ratelimit_fields->SetString(kRatelimitPeriodStartKey, "0");
  ratelimit_fields->SetInteger(kRatelimitPeriodDumpsKey, 0);

  scoped_ptr<base::ListValue> dumps = make_scoped_ptr(new base::ListValue());

  return WriteLockFile(lockfile_path, dumps.get()) == 0 &&
         WriteMetadataFile(metadata_path, metadata.get());
}

bool AppendLockFile(const std::string& lockfile_path,
                    const std::string& metadata_path,
                    const DumpInfo& dump) {
  scoped_ptr<base::ListValue> contents = ParseLockFile(lockfile_path);
  if (!contents) {
    CreateFiles(lockfile_path, metadata_path);
    if (!(contents = ParseLockFile(lockfile_path))) {
      return false;
    }
  }

  contents->Append(dump.GetAsValue());

  return WriteLockFile(lockfile_path, contents.get()) == 0;
}

bool SetRatelimitPeriodStart(const std::string& metadata_path, time_t start) {
  scoped_ptr<base::Value> contents = ParseMetadataFile(metadata_path);

  base::DictionaryValue* dict;
  base::DictionaryValue* ratelimit_params;
  if (!contents || !contents->GetAsDictionary(&dict) ||
      !dict->GetDictionary(kRatelimitKey, &ratelimit_params)) {
    return false;
  }

  std::string period_start_str =
      base::StringPrintf("%lld", static_cast<long long>(start));
  ratelimit_params->SetString(kRatelimitPeriodStartKey, period_start_str);

  return WriteMetadataFile(metadata_path, contents.get()) == 0;
}

}  // namespace chromecast
