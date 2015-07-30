// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/common/process_type.h"
#include "url/gurl.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using tracked_objects::BirthOnThreadSnapshot;
using tracked_objects::DeathDataSnapshot;
using tracked_objects::LocationSnapshot;
using tracked_objects::TaskSnapshot;
using tracked_objects::ProcessDataPhaseSnapshot;

namespace {

// Re-serializes the |location| into |dictionary|.
void LocationSnapshotToValue(const LocationSnapshot& location,
                             base::DictionaryValue* dictionary) {
  dictionary->SetString("file_name", location.file_name);
  // Note: This function name is not escaped, and templates have less-than
  // characters, which means this is not suitable for display as HTML unless
  // properly escaped.
  dictionary->SetString("function_name", location.function_name);
  dictionary->SetInteger("line_number", location.line_number);
}

// Re-serializes the |birth| into |dictionary|.  Prepends the |prefix| to the
// "thread" and "location" key names in the dictionary.
void BirthOnThreadSnapshotToValue(const BirthOnThreadSnapshot& birth,
                                  const std::string& prefix,
                                  base::DictionaryValue* dictionary) {
  DCHECK(!prefix.empty());

  scoped_ptr<base::DictionaryValue> location_value(new base::DictionaryValue);
  LocationSnapshotToValue(birth.location, location_value.get());
  dictionary->Set(prefix + "_location", location_value.release());

  dictionary->Set(prefix + "_thread", new base::StringValue(birth.thread_name));
}

// Re-serializes the |death_data| into |dictionary|.
void DeathDataSnapshotToValue(const DeathDataSnapshot& death_data,
                              base::DictionaryValue* dictionary) {
  dictionary->SetInteger("count", death_data.count);
  dictionary->SetInteger("run_ms", death_data.run_duration_sum);
  dictionary->SetInteger("run_ms_max", death_data.run_duration_max);
  dictionary->SetInteger("run_ms_sample", death_data.run_duration_sample);
  dictionary->SetInteger("queue_ms", death_data.queue_duration_sum);
  dictionary->SetInteger("queue_ms_max", death_data.queue_duration_max);
  dictionary->SetInteger("queue_ms_sample", death_data.queue_duration_sample);
}

// Re-serializes the |snapshot| into |dictionary|.
void TaskSnapshotToValue(const TaskSnapshot& snapshot,
                         base::DictionaryValue* dictionary) {
  BirthOnThreadSnapshotToValue(snapshot.birth, "birth", dictionary);

  scoped_ptr<base::DictionaryValue> death_data(new base::DictionaryValue);
  DeathDataSnapshotToValue(snapshot.death_data, death_data.get());
  dictionary->Set("death_data", death_data.release());

  dictionary->SetString("death_thread", snapshot.death_thread_name);
}

}  // anonymous namespace

namespace task_profiler {

// static
void TaskProfilerDataSerializer::ToValue(
    const ProcessDataPhaseSnapshot& process_data_phase,
    base::ProcessId process_id,
    int process_type,
    base::DictionaryValue* dictionary) {
  scoped_ptr<base::ListValue> tasks_list(new base::ListValue);
  for (const auto& task : process_data_phase.tasks) {
    scoped_ptr<base::DictionaryValue> snapshot(new base::DictionaryValue);
    TaskSnapshotToValue(task, snapshot.get());
    tasks_list->Append(snapshot.release());
  }
  dictionary->Set("list", tasks_list.release());

  dictionary->SetInteger("process_id", process_id);
  dictionary->SetString("process_type",
                        content::GetProcessTypeNameInEnglish(process_type));
}

}  // namespace task_profiler
