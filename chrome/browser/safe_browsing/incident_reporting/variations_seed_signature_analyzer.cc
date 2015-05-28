// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_analyzer.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/variations_seed_signature_incident.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

void VerifyVariationsSeedSignatureOnUIThread(
    scoped_ptr<IncidentReceiver> incident_receiver) {
  chrome_variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (!variations_service)
    return;
  std::string invalid_signature =
      variations_service->GetInvalidVariationsSeedSignature();
  if (!invalid_signature.empty()) {
    scoped_ptr<
        ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident>
        variations_seed_signature(
            new ClientIncidentReport_IncidentData_VariationsSeedSignatureIncident());
    variations_seed_signature->set_variations_seed_signature(invalid_signature);
    incident_receiver->AddIncidentForProcess(make_scoped_ptr(
        new VariationsSeedSignatureIncident(variations_seed_signature.Pass())));
  }
}

}  // namespace

void RegisterVariationsSeedSignatureAnalysis() {
  scoped_refptr<SafeBrowsingService> safe_browsing_service(
      g_browser_process->safe_browsing_service());

  safe_browsing_service->RegisterDelayedAnalysisCallback(
      base::Bind(&VerifyVariationsSeedSignature));
}

void VerifyVariationsSeedSignature(
    scoped_ptr<IncidentReceiver> incident_receiver) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&VerifyVariationsSeedSignatureOnUIThread,
                 base::Passed(&incident_receiver)));
}

}  // namespace safe_browsing
