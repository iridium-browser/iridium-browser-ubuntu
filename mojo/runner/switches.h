// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_RUNNER_SWITCHES_H_
#define MOJO_RUNNER_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kApp[];
extern const char kChildProcess[];
extern const char kContentHandlers[];
extern const char kDeleteAfterLoad[];
extern const char kDisableCache[];
extern const char kEnableSandbox[];
extern const char kForceInProcess[];
extern const char kHelp[];
extern const char kMapOrigin[];
extern const char kURLMappings[];
extern const char kUseTemporaryUserDataDir[];
extern const char kUserDataDir[];

}  // namespace switches

#endif  // MOJO_RUNNER_SWITCHES_H_
