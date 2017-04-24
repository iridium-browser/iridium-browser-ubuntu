// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/common/switches.h"

namespace switches {

// Used internally by the main process to indicate that a new process should be
// a child process. Takes the absolute path to the service library to load as
// an argument. Not for user use.
const char kChildProcess[] = "child-process";

// Enables the sandbox on this process.
const char kEnableSandbox[] = "enable-sandbox";

// Provides a child process with a token string they can use to establish a
// primordial message pipe to the parent.
const char kPrimordialPipeToken[] = "primordial-pipe-token";

// The name of the service the process is starting for.
const char kProcessServiceName[] = "process-service-name";

// Specifies a JSON file from which to read a set of service metadata overrides.
// This can be used with the standalone mojo_runner to override executable and
// package resolution behavior.
const char kServiceOverrides[] = "service-overrides";

}  // namespace switches
