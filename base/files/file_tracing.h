// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_TRACING_H_
#define BASE_FILES_FILE_TRACING_H_

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/macros.h"

#define FILE_TRACING_PREFIX "File"

#define SCOPED_FILE_TRACE_WITH_SIZE(name, size) \
    FileTracing::ScopedTrace scoped_file_trace; \
    if (scoped_file_trace.ShouldInitialize()) \
      scoped_file_trace.Initialize(FILE_TRACING_PREFIX "::" name, this, size)

#define SCOPED_FILE_TRACE(name) SCOPED_FILE_TRACE_WITH_SIZE(name, 0)

namespace base {

class File;
class FilePath;

class BASE_EXPORT FileTracing {
 public:
  class Provider {
   public:
    // Whether the file tracing category is currently enabled.
    virtual bool FileTracingCategoryIsEnabled() const = 0;

    // Enables file tracing for |id|. Must be called before recording events.
    virtual void FileTracingEnable(void* id) = 0;

    // Disables file tracing for |id|.
    virtual void FileTracingDisable(void* id) = 0;

    // Begins an event for |id| with |name|. |path| tells where in the directory
    // structure the event is happening (and may be blank). |size| is reported
    // if not 0.
    virtual void FileTracingEventBegin(
        const char* name, void* id, const FilePath& path, int64 size) = 0;

    // Ends an event for |id| with |name|. |path| tells where in the directory
    // structure the event is happening (and may be blank). |size| is reported
    // if not 0.
    virtual void FileTracingEventEnd(
        const char* name, void* id, const FilePath& path, int64 size) = 0;
  };

  // Sets a global file tracing provider to query categories and record events.
  static void SetProvider(Provider* provider);

  // Enables file tracing while in scope.
  class ScopedEnabler {
   public:
    ScopedEnabler();
    ~ScopedEnabler();
  };

  class ScopedTrace {
   public:
    ScopedTrace();
    ~ScopedTrace();

    // Whether this trace should be initialized or not.
    bool ShouldInitialize() const;

    // Called only if the tracing category is enabled.
    void Initialize(const char* event, File* file, int64 size);

   private:
    // True if |Initialize()| has been called. Don't touch |path_|, |event_|,
    // or |bytes_| if |initialized_| is false.
    bool initialized_;

    // The event name to trace (e.g. "Read", "Write"). Prefixed with "File".
    const char* name_;

    // The file being traced. Must outlive this class.
    File* file_;

    // The size (in bytes) of this trace. Not reported if 0.
    int64 size_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTrace);
  };

  DISALLOW_COPY_AND_ASSIGN(FileTracing);
};

}  // namespace base

#endif  // BASE_FILES_FILE_TRACING_H_
