// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_install_helper.h"

#include <string>

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/utility_process_host.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::UtilityProcessHost;

namespace {

const char kImageDecodeError[] = "Image decode failed";

}  // namespace

namespace extensions {

WebstoreInstallHelper::WebstoreInstallHelper(
    Delegate* delegate,
    const std::string& id,
    const std::string& manifest,
    const GURL& icon_url,
    net::URLRequestContextGetter* context_getter)
    : delegate_(delegate),
      id_(id),
      manifest_(manifest),
      icon_url_(icon_url),
      context_getter_(context_getter),
      icon_decode_complete_(false),
      manifest_parse_complete_(false),
      parse_error_(Delegate::UNKNOWN_ERROR) {
}

WebstoreInstallHelper::~WebstoreInstallHelper() {}

void WebstoreInstallHelper::Start() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (icon_url_.is_empty()) {
    icon_decode_complete_ = true;
  } else {
    // No existing |icon_fetcher_| to avoid unbalanced AddRef().
    CHECK(!icon_fetcher_.get());
    AddRef();  // Balanced in OnFetchComplete().
    icon_fetcher_.reset(new chrome::BitmapFetcher(icon_url_, this));
    icon_fetcher_->Start(
        context_getter_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebstoreInstallHelper::StartWorkOnIOThread, this));
}

void WebstoreInstallHelper::StartWorkOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  utility_host_ = UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get().get())->AsWeakPtr();
  utility_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_JSON_PARSER_NAME));
  utility_host_->StartBatchMode();

  utility_host_->Send(new ChromeUtilityMsg_ParseJSON(manifest_));
}

bool WebstoreInstallHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebstoreInstallHelper, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                        OnJSONParseSucceeded)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_ParseJSON_Failed,
                        OnJSONParseFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebstoreInstallHelper::OnFetchComplete(const GURL& url,
                                            const SkBitmap* image) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // OnFetchComplete should only be called as icon_fetcher_ delegate to avoid
  // unbalanced Release().
  CHECK(icon_fetcher_.get());

  if (image)
    icon_ = *image;
  icon_decode_complete_ = true;
  if (icon_.empty()) {
    error_ = kImageDecodeError;
    parse_error_ = Delegate::ICON_ERROR;
  }
  icon_fetcher_.reset();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebstoreInstallHelper::ReportResultsIfComplete, this));
  Release();  // Balanced in Start().
}

void WebstoreInstallHelper::OnJSONParseSucceeded(
    const base::ListValue& wrapper) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  manifest_parse_complete_ = true;
  const base::Value* value = NULL;
  CHECK(wrapper.Get(0, &value));
  if (value->IsType(base::Value::TYPE_DICTIONARY)) {
    parsed_manifest_.reset(
        static_cast<const base::DictionaryValue*>(value)->DeepCopy());
  } else {
    parse_error_ = Delegate::MANIFEST_ERROR;
  }
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::OnJSONParseFailed(
    const std::string& error_message) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  manifest_parse_complete_ = true;
  error_ = error_message;
  parse_error_ = Delegate::MANIFEST_ERROR;
  ReportResultsIfComplete();
}

void WebstoreInstallHelper::ReportResultsIfComplete() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!icon_decode_complete_ || !manifest_parse_complete_)
    return;

  // The utility_host_ will take care of deleting itself after this call.
  if (utility_host_.get()) {
    utility_host_->EndBatchMode();
    utility_host_.reset();
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebstoreInstallHelper::ReportResultFromUIThread, this));
}

void WebstoreInstallHelper::ReportResultFromUIThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (error_.empty() && parsed_manifest_)
    delegate_->OnWebstoreParseSuccess(id_, icon_, parsed_manifest_.release());
  else
    delegate_->OnWebstoreParseFailure(id_, parse_error_, error_);
}

}  // namespace extensions
