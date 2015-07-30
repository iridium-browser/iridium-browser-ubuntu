// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/launcher_search_provider.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"
#include "chrome/browser/chromeos/launcher_search_provider/service.h"
#include "chrome/common/extensions/api/launcher_search_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"

namespace extensions {

LauncherSearchProviderSetSearchResultsFunction::
    ~LauncherSearchProviderSetSearchResultsFunction() {
}

bool LauncherSearchProviderSetSearchResultsFunction::RunSync() {
  using chromeos::launcher_search_provider::ErrorReporter;
  using chromeos::launcher_search_provider::Service;
  using extensions::api::launcher_search_provider::SetSearchResults::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Either render_view_host or render_frame_host will be set. See
  // crbug.com/304341.
  IPC::Sender* sender = nullptr;
  int routing_id = 0;
  if (render_view_host()) {
    sender = render_view_host();
    routing_id = render_view_host()->GetRoutingID();
  } else {
    sender = render_frame_host();
    routing_id = render_frame_host()->GetRoutingID();
  }
  DCHECK(sender);

  scoped_ptr<ErrorReporter> error_reporter(
      new ErrorReporter(sender, routing_id));
  Service* const service = Service::Get(GetProfile());
  service->SetSearchResults(extension(), error_reporter.Pass(),
                            params->query_id, params->results);

  return true;
}

}  // namespace extensions
