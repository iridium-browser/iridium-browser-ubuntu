// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_ANALYZER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

namespace safe_browsing {

class IncidentReceiver;

// Registers a process-wide analysis with the incident reporting service that
// will examine how effective the blacklist was.
void RegisterBlacklistLoadAnalysis();

// Retrieves the set of blacklisted modules that are loaded in the process.
// Returns true if successful, false otherwise.
bool GetLoadedBlacklistedModules(std::vector<base::string16>* module_names);

// Callback to pass to the incident reporting service. The incident reporting
// service will decide when to start the analysis.
void VerifyBlacklistLoadState(scoped_ptr<IncidentReceiver> incident_receiver);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BLACKLIST_LOAD_ANALYZER_H_
