// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline/offline_internals_ui_message_handler.h"

#include <stdint.h>
#include <stdlib.h>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "content/public/browser/web_ui.h"
#include "net/base/network_change_notifier.h"

namespace offline_internals {

OfflineInternalsUIMessageHandler::OfflineInternalsUIMessageHandler()
    : offline_page_model_(nullptr),
      request_coordinator_(nullptr),
      weak_ptr_factory_(this) {}

OfflineInternalsUIMessageHandler::~OfflineInternalsUIMessageHandler() {}

std::string OfflineInternalsUIMessageHandler::GetStringFromDeletePageResult(
    offline_pages::DeletePageResult value) {
  switch (value) {
    case offline_pages::DeletePageResult::SUCCESS:
      return "Success";
    case offline_pages::DeletePageResult::CANCELLED:
      return "Cancelled";
    case offline_pages::DeletePageResult::STORE_FAILURE:
      return "Store failure";
    case offline_pages::DeletePageResult::DEVICE_FAILURE:
      return "Device failure";
    case offline_pages::DeletePageResult::NOT_FOUND:
      return "Not found";
    case offline_pages::DeletePageResult::RESULT_COUNT:
      break;
  }
  NOTREACHED();
  return "Unknown";
}

std::string OfflineInternalsUIMessageHandler::GetStringFromSavePageStatus() {
  return "Available";
}

void OfflineInternalsUIMessageHandler::HandleDeleteAllPages(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  // Pass back success because ClearAll doesn't return a status.
  offline_page_model_->ClearAll(
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id,
                 offline_pages::DeletePageResult::SUCCESS));
}

void OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages(
    const base::ListValue* args) {
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  std::vector<int64_t> offline_ids;
  const base::ListValue* offline_ids_from_arg;
  args->GetList(1, &offline_ids_from_arg);

  for (size_t i = 0; i < offline_ids_from_arg->GetSize(); i++) {
    std::string value;
    offline_ids_from_arg->GetString(i, &value);
    int64_t int_value;
    base::StringToInt64(value, &int_value);
    offline_ids.push_back(int_value);
  }

  offline_page_model_->DeletePagesByOfflineId(
      offline_ids,
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void OfflineInternalsUIMessageHandler::HandleDeletedPagesCallback(
    std::string callback_id,
    offline_pages::DeletePageResult result) {
  ResolveJavascriptCallback(
      base::StringValue(callback_id),
      base::StringValue(GetStringFromDeletePageResult(result)));
}

void OfflineInternalsUIMessageHandler::HandleStoredPagesCallback(
    std::string callback_id,
    const offline_pages::MultipleOfflinePageItemResult& pages) {
  base::ListValue results;

  for (const auto& page : pages) {
    base::DictionaryValue* offline_page = new base::DictionaryValue();
    results.Append(offline_page);
    offline_page->SetString("onlineUrl", page.url.spec());
    offline_page->SetString("namespace", page.client_id.name_space);
    offline_page->SetDouble("size", page.file_size);
    offline_page->SetString("id", std::to_string(page.offline_id));
    offline_page->SetString("filePath", page.GetOfflineURL().spec());
    offline_page->SetDouble("creationTime", page.creation_time.ToJsTime());
    offline_page->SetDouble("lastAccessTime", page.last_access_time.ToJsTime());
    offline_page->SetInteger("accessCount", page.access_count);
    offline_page->SetString("isExpired", page.IsExpired() ? "Yes" : "No");
  }
  ResolveJavascriptCallback(base::StringValue(callback_id), results);
}

void OfflineInternalsUIMessageHandler::HandleRequestQueueCallback(
    std::string callback_id,
    offline_pages::RequestQueue::GetRequestsResult result,
    const std::vector<offline_pages::SavePageRequest>& requests) {
  base::ListValue save_page_requests;
  if (result == offline_pages::RequestQueue::GetRequestsResult::SUCCESS) {
    for (const auto& request : requests) {
      base::DictionaryValue* save_page_request = new base::DictionaryValue();
      save_page_requests.Append(save_page_request);
      save_page_request->SetString("onlineUrl", request.url().spec());
      save_page_request->SetDouble("creationTime",
                                   request.creation_time().ToJsTime());
      save_page_request->SetString("status", GetStringFromSavePageStatus());
      save_page_request->SetString("namespace", request.client_id().name_space);
      save_page_request->SetDouble("lastAttempt",
                                   request.last_attempt_time().ToJsTime());
      save_page_request->SetString("id", std::to_string(request.request_id()));
    }
  }
  ResolveJavascriptCallback(base::StringValue(callback_id), save_page_requests);
}

void OfflineInternalsUIMessageHandler::HandleGetRequestQueue(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (request_coordinator_) {
    request_coordinator_->queue()->GetRequests(base::Bind(
        &OfflineInternalsUIMessageHandler::HandleRequestQueueCallback,
        weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::StringValue(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleGetStoredPages(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  if (offline_page_model_) {
    offline_page_model_->GetAllPagesWithExpired(
        base::Bind(&OfflineInternalsUIMessageHandler::HandleStoredPagesCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback_id));
  } else {
    base::ListValue results;
    ResolveJavascriptCallback(base::StringValue(callback_id), results);
  }
}

void OfflineInternalsUIMessageHandler::HandleSetRecordPageModel(
    const base::ListValue* args) {
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  offline_page_model_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetNetworkStatus(
    const base::ListValue* args) {
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  ResolveJavascriptCallback(
      *callback_id,
      base::StringValue(net::NetworkChangeNotifier::IsOffline() ? "Offline"
                                                                : "Online"));
}

void OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue(
    const base::ListValue* args) {
  bool should_record;
  CHECK(args->GetBoolean(0, &should_record));
  request_coordinator_->GetLogger()->SetIsLogging(should_record);
}

void OfflineInternalsUIMessageHandler::HandleGetLoggingState(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::DictionaryValue result;
  result.SetBoolean("modelIsLogging",
                    offline_page_model_->GetLogger()->GetIsLogging());
  result.SetBoolean("queueIsLogging",
                    request_coordinator_->GetLogger()->GetIsLogging());
  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleGetEventLogs(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::vector<std::string> logs;
  offline_page_model_->GetLogger()->GetLogs(&logs);
  request_coordinator_->GetLogger()->GetLogs(&logs);
  std::sort(logs.begin(), logs.end());

  base::ListValue result;
  result.AppendStrings(logs);

  ResolveJavascriptCallback(*callback_id, result);
}

void OfflineInternalsUIMessageHandler::HandleAddToRequestQueue(
    const base::ListValue* args) {
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::string url;
  CHECK(args->GetString(1, &url));

  // To be visible in Downloads UI, these items need a well-formed GUID
  // and AsyncNamespace in their ClientId.
  std::ostringstream id_stream;
  id_stream << base::GenerateGUID();

  ResolveJavascriptCallback(
      *callback_id,
      base::FundamentalValue(request_coordinator_->SavePageLater(
          GURL(url), offline_pages::ClientId(offline_pages::kAsyncNamespace,
                                             id_stream.str()),
          true)));
}

void OfflineInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "deleteAllPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteAllPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "deleteSelectedPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleDeleteSelectedPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetRequestQueue,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getStoredPages",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetStoredPages,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEventLogs",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetEventLogs,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setRecordRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordRequestQueue,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "setRecordPageModel",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleSetRecordPageModel,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getLoggingState",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetLoggingState,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "addToRequestQueue",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleAddToRequestQueue,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getNetworkStatus",
      base::Bind(&OfflineInternalsUIMessageHandler::HandleGetNetworkStatus,
                 weak_ptr_factory_.GetWeakPtr()));

  // Get the offline page model associated with this web ui.
  Profile* profile = Profile::FromWebUI(web_ui());
  offline_page_model_ =
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile);
  request_coordinator_ =
      offline_pages::RequestCoordinatorFactory::GetForBrowserContext(profile);
}

}  // namespace offline_internals
