// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/ping_manager.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/request_sender.h"
#include "components/update_client/utils.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

namespace update_client {

namespace {

// Returns a string literal corresponding to the value of the downloader |d|.
const char* DownloaderToString(CrxDownloader::DownloadMetrics::Downloader d) {
  switch (d) {
    case CrxDownloader::DownloadMetrics::kUrlFetcher:
      return "direct";
    case CrxDownloader::DownloadMetrics::kBits:
      return "bits";
    default:
      return "unknown";
  }
}

// Returns a string representing a sequence of download complete events
// corresponding to each download metrics in |item|.
std::string BuildDownloadCompleteEventElements(const CrxUpdateItem* item) {
  using base::StringAppendF;
  std::string download_events;
  for (size_t i = 0; i != item->download_metrics.size(); ++i) {
    const CrxDownloader::DownloadMetrics& metrics = item->download_metrics[i];
    std::string event("<event eventtype=\"14\"");
    StringAppendF(&event, " eventresult=\"%d\"", metrics.error == 0);
    StringAppendF(&event, " downloader=\"%s\"",
                  DownloaderToString(metrics.downloader));
    if (metrics.error) {
      StringAppendF(&event, " errorcode=\"%d\"", metrics.error);
    }
    StringAppendF(&event, " url=\"%s\"", metrics.url.spec().c_str());

    // -1 means that the  byte counts are not known.
    if (metrics.downloaded_bytes != -1) {
      StringAppendF(&event, " downloaded=\"%s\"",
                    base::Int64ToString(metrics.downloaded_bytes).c_str());
    }
    if (metrics.total_bytes != -1) {
      StringAppendF(&event, " total=\"%s\"",
                    base::Int64ToString(metrics.total_bytes).c_str());
    }

    if (metrics.download_time_ms) {
      StringAppendF(&event, " download_time_ms=\"%s\"",
                    base::Uint64ToString(metrics.download_time_ms).c_str());
    }
    StringAppendF(&event, "/>");

    download_events += event;
  }
  return download_events;
}

// Returns a string representing one ping event xml element for an update item.
std::string BuildUpdateCompleteEventElement(const CrxUpdateItem* item) {
  DCHECK(item->state == CrxUpdateItem::State::kNoUpdate ||
         item->state == CrxUpdateItem::State::kUpdated);

  using base::StringAppendF;

  std::string ping_event("<event eventtype=\"3\"");
  const int event_result = item->state == CrxUpdateItem::State::kUpdated;
  StringAppendF(&ping_event, " eventresult=\"%d\"", event_result);
  if (item->error_category)
    StringAppendF(&ping_event, " errorcat=\"%d\"", item->error_category);
  if (item->error_code)
    StringAppendF(&ping_event, " errorcode=\"%d\"", item->error_code);
  if (item->extra_code1)
    StringAppendF(&ping_event, " extracode1=\"%d\"", item->extra_code1);
  if (HasDiffUpdate(item))
    StringAppendF(&ping_event, " diffresult=\"%d\"", !item->diff_update_failed);
  if (item->diff_error_category) {
    StringAppendF(&ping_event, " differrorcat=\"%d\"",
                  item->diff_error_category);
  }
  if (item->diff_error_code)
    StringAppendF(&ping_event, " differrorcode=\"%d\"", item->diff_error_code);
  if (item->diff_extra_code1) {
    StringAppendF(&ping_event, " diffextracode1=\"%d\"",
                  item->diff_extra_code1);
  }
  if (!item->previous_fp.empty())
    StringAppendF(&ping_event, " previousfp=\"%s\"", item->previous_fp.c_str());
  if (!item->next_fp.empty())
    StringAppendF(&ping_event, " nextfp=\"%s\"", item->next_fp.c_str());
  StringAppendF(&ping_event, "/>");
  return ping_event;
}

// Builds a ping message for the specified update item.
std::string BuildPing(const Configurator& config, const CrxUpdateItem* item) {
  const char app_element_format[] =
      "<app appid=\"%s\" version=\"%s\" nextversion=\"%s\">"
      "%s"
      "%s"
      "</app>";
  const std::string app_element(base::StringPrintf(
      app_element_format,
      item->id.c_str(),                                    // "appid"
      item->previous_version.GetString().c_str(),          // "version"
      item->next_version.GetString().c_str(),              // "nextversion"
      BuildUpdateCompleteEventElement(item).c_str(),       // update event
      BuildDownloadCompleteEventElements(item).c_str()));  // download events

  return BuildProtocolRequest(config.GetBrowserVersion().GetString(),
                              config.GetChannel(), config.GetLang(),
                              config.GetOSLongName(), app_element, "");
}

// Sends a fire and forget ping. The instances of this class have no
// ownership and they self-delete upon completion. One instance of this class
// can send only one ping.
class PingSender {
 public:
  explicit PingSender(const Configurator& config);
  ~PingSender();

  bool SendPing(const CrxUpdateItem* item);

 private:
  void OnRequestSenderComplete(const net::URLFetcher* source);

  const Configurator& config_;
  scoped_ptr<RequestSender> request_sender_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PingSender);
};

PingSender::PingSender(const Configurator& config) : config_(config) {
}

PingSender::~PingSender() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PingSender::OnRequestSenderComplete(const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delete this;
}

bool PingSender::SendPing(const CrxUpdateItem* item) {
  DCHECK(item);
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<GURL> urls(config_.PingUrl());

  if (urls.empty())
    return false;

  request_sender_.reset(new RequestSender(config_));
  request_sender_->Send(
      BuildPing(config_, item), urls,
      base::Bind(&PingSender::OnRequestSenderComplete, base::Unretained(this)));
  return true;
}

}  // namespace

PingManager::PingManager(const Configurator& config) : config_(config) {
}

PingManager::~PingManager() {
}

// Sends a fire and forget ping when the updates are complete. The ping
// sender object self-deletes after sending the ping has completed asynchrously.
void PingManager::OnUpdateComplete(const CrxUpdateItem* item) {
  PingSender* ping_sender(new PingSender(config_));
  if (!ping_sender->SendPing(item))
    delete ping_sender;
}

}  // namespace update_client
