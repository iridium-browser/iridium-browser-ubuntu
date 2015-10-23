// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <algorithm>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/values.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace domain_reliability {

namespace {
typedef std::deque<DomainReliabilityBeacon> BeaconDeque;
typedef BeaconDeque::iterator BeaconIterator;
typedef BeaconDeque::const_iterator BeaconConstIterator;
}  // namespace

DomainReliabilityContext::Factory::~Factory() {
}

class DomainReliabilityContext::ResourceState {
 public:
  ResourceState(DomainReliabilityContext* context,
                const DomainReliabilityConfig::Resource* config)
      : context(context),
        config(config),
        successful_requests(0),
        failed_requests(0),
        uploading_successful_requests(0),
        uploading_failed_requests(0) {}
  ~ResourceState() {}

  // Serializes the resource state into a Value to be included in an upload.
  // If there is nothing to report (no beacons and all request counters are 0),
  // returns a scoped_ptr to NULL instead so the resource can be omitted.
  scoped_ptr<base::Value> ToValue(base::TimeTicks upload_time) const {
    if (successful_requests == 0 && failed_requests == 0)
      return scoped_ptr<base::Value>();

    DictionaryValue* resource_value = new DictionaryValue();
    resource_value->SetString("name", config->name);
    resource_value->SetInteger("successful_requests", successful_requests);
    resource_value->SetInteger("failed_requests", failed_requests);

    return scoped_ptr<Value>(resource_value);
  }

  // Remembers the current state of the resource data when an upload starts.
  void MarkUpload() {
    DCHECK_EQ(0u, uploading_successful_requests);
    DCHECK_EQ(0u, uploading_failed_requests);
    uploading_successful_requests = successful_requests;
    uploading_failed_requests = failed_requests;
  }

  // Uses the state remembered by |MarkUpload| to remove successfully uploaded
  // data but keep beacons and request counts added after the upload started.
  void CommitUpload() {
    successful_requests -= uploading_successful_requests;
    failed_requests -= uploading_failed_requests;
    uploading_successful_requests = 0;
    uploading_failed_requests = 0;
  }

  void RollbackUpload() {
    uploading_successful_requests = 0;
    uploading_failed_requests = 0;
  }

  DomainReliabilityContext* context;
  const DomainReliabilityConfig::Resource* config;

  uint32 successful_requests;
  uint32 failed_requests;

  // State saved during uploads; if an upload succeeds, these are used to
  // remove uploaded data from the beacon list and request counters.
  uint32 uploading_successful_requests;
  uint32 uploading_failed_requests;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceState);
};

// static
const size_t DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    const std::string& upload_reporter_string,
    const base::TimeTicks* last_network_change_time,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    scoped_ptr<const DomainReliabilityConfig> config)
    : config_(config.Pass()),
      time_(time),
      upload_reporter_string_(upload_reporter_string),
      scheduler_(time,
                 config_->collectors.size(),
                 scheduler_params,
                 base::Bind(&DomainReliabilityContext::ScheduleUpload,
                            base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      uploading_beacons_size_(0),
      last_network_change_time_(last_network_change_time),
      weak_factory_(this) {
  InitializeResourceStates();
}

DomainReliabilityContext::~DomainReliabilityContext() {}

void DomainReliabilityContext::OnBeacon(const GURL& url,
                                        const DomainReliabilityBeacon& beacon) {
  size_t index = config_->GetResourceIndexForUrl(url);
  if (index == DomainReliabilityConfig::kInvalidResourceIndex)
    return;
  DCHECK_GT(states_.size(), index);

  bool success = (beacon.status == "ok");

  ResourceState* state = states_[index];
  if (success)
    ++state->successful_requests;
  else
    ++state->failed_requests;

  bool reported = false;
  bool evicted = false;
  if (state->config->DecideIfShouldReportRequest(success)) {
    beacons_.push_back(beacon);
    beacons_.back().resource = state->config->name;
    if (beacons_.size() > kMaxQueuedBeacons) {
      RemoveOldestBeacon();
      evicted = true;
    }
    scheduler_.OnBeaconAdded();
    reported = true;
    UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.ReportedBeaconError",
                                -beacon.chrome_error);
    // TODO(ttuttle): Histogram HTTP response code?
  }

  UMA_HISTOGRAM_BOOLEAN("DomainReliability.BeaconReported", reported);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.OnBeaconDidEvict", evicted);
}

void DomainReliabilityContext::ClearBeacons() {
  for (auto& state : states_) {
    state->successful_requests = 0;
    state->failed_requests = 0;
    state->uploading_successful_requests = 0;
    state->uploading_failed_requests = 0;
  }
  beacons_.clear();
  uploading_beacons_size_ = 0;
}

scoped_ptr<base::Value> DomainReliabilityContext::GetWebUIData() const {
  base::DictionaryValue* context_value = new base::DictionaryValue();

  context_value->SetString("domain", config().domain);
  context_value->SetInteger("beacon_count", static_cast<int>(beacons_.size()));
  context_value->SetInteger("uploading_beacon_count",
      static_cast<int>(uploading_beacons_size_));
  context_value->Set("scheduler", scheduler_.GetWebUIData());

  return scoped_ptr<base::Value>(context_value);
}

void DomainReliabilityContext::GetQueuedBeaconsForTesting(
    std::vector<DomainReliabilityBeacon>* beacons_out) const {
  beacons_out->assign(beacons_.begin(), beacons_.end());
}

void DomainReliabilityContext::GetRequestCountsForTesting(
    size_t resource_index,
    uint32_t* successful_requests_out,
    uint32_t* failed_requests_out) const {
  DCHECK_NE(DomainReliabilityConfig::kInvalidResourceIndex, resource_index);
  DCHECK_GT(states_.size(), resource_index);

  const ResourceState& state = *states_[resource_index];
  *successful_requests_out = state.successful_requests;
  *failed_requests_out = state.failed_requests;
}

void DomainReliabilityContext::InitializeResourceStates() {
  for (auto& resource : config_->resources)
    states_.push_back(new ResourceState(this, resource));
}

void DomainReliabilityContext::ScheduleUpload(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  dispatcher_->ScheduleTask(
      base::Bind(
          &DomainReliabilityContext::StartUpload,
          weak_factory_.GetWeakPtr()),
      min_delay,
      max_delay);
}

void DomainReliabilityContext::StartUpload() {
  MarkUpload();

  DCHECK(upload_time_.is_null());
  upload_time_ = time_->NowTicks();
  std::string report_json;
  base::JSONWriter::Write(*CreateReport(upload_time_), &report_json);

  size_t collector_index = scheduler_.OnUploadStart();

  uploader_->UploadReport(
      report_json,
      config_->collectors[collector_index]->upload_url,
      base::Bind(
          &DomainReliabilityContext::OnUploadComplete,
          weak_factory_.GetWeakPtr()));

  UMA_HISTOGRAM_SPARSE_SLOWLY("DomainReliability.UploadCollectorIndex",
                              static_cast<int>(collector_index));
  if (!last_upload_time_.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadInterval",
                             upload_time_ - last_upload_time_);
  }
}

void DomainReliabilityContext::OnUploadComplete(
    const DomainReliabilityUploader::UploadResult& result) {
  if (result.is_success())
    CommitUpload();
  else
    RollbackUpload();
  base::TimeTicks first_beacon_time = scheduler_.first_beacon_time();
  scheduler_.OnUploadComplete(result);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.UploadSuccess",
      result.is_success());
  base::TimeTicks now = time_->NowTicks();
  UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadLatency",
                           now - first_beacon_time);
  DCHECK(!upload_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES("DomainReliability.UploadDuration",
                             now - upload_time_);
  UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadCollectorRetryDelay",
                           scheduler_.last_collector_retry_delay());
  last_upload_time_ = upload_time_;
  upload_time_ = base::TimeTicks();
}

scoped_ptr<const Value> DomainReliabilityContext::CreateReport(
    base::TimeTicks upload_time) const {
  scoped_ptr<ListValue> beacons_value(new ListValue());
  for (const auto& beacon : beacons_) {
    beacons_value->Append(
        beacon.ToValue(upload_time, *last_network_change_time_));
  }

  scoped_ptr<ListValue> resources_value(new ListValue());
  for (const auto& state : states_) {
    scoped_ptr<Value> resource_report = state->ToValue(upload_time);
    if (resource_report)
      resources_value->Append(resource_report.release());
  }

  scoped_ptr<DictionaryValue> report_value(new DictionaryValue());
  if (!config().version.empty())
    report_value->SetString("config_version", config().version);
  report_value->SetString("reporter", upload_reporter_string_);
  report_value->Set("entries", beacons_value.release());
  if (!resources_value->empty())
    report_value->Set("resources", resources_value.release());

  return report_value.Pass();
}

void DomainReliabilityContext::MarkUpload() {
  for (auto& state : states_)
    state->MarkUpload();
  DCHECK_EQ(0u, uploading_beacons_size_);
  uploading_beacons_size_ = beacons_.size();
  DCHECK_NE(0u, uploading_beacons_size_);
}

void DomainReliabilityContext::CommitUpload() {
  for (auto& state : states_)
    state->CommitUpload();
  BeaconIterator begin = beacons_.begin();
  BeaconIterator end = begin + uploading_beacons_size_;
  beacons_.erase(begin, end);
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RollbackUpload() {
  for (auto& state : states_)
    state->RollbackUpload();
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RemoveOldestBeacon() {
  DCHECK(!beacons_.empty());

  VLOG(1) << "Beacon queue for " << config().domain << " full; "
          << "removing oldest beacon";

  beacons_.pop_front();

  // If that just removed a beacon counted in uploading_beacons_size_, decrement
  // that.
  if (uploading_beacons_size_ > 0)
    --uploading_beacons_size_;
}

}  // namespace domain_reliability
