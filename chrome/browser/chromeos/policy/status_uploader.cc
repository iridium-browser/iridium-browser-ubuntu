// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/chromeos/logging.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace em = enterprise_management;

namespace {
// Minimum delay between two consecutive uploads
const int kMinUploadDelayMs = 60 * 1000;  // 60 seconds
// Minimum delay after scheduling an upload
const int kMinUploadScheduleDelayMs = 60 * 1000;  // 60 seconds
}  // namespace

namespace policy {

const int64_t StatusUploader::kDefaultUploadDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours

StatusUploader::StatusUploader(
    CloudPolicyClient* client,
    std::unique_ptr<DeviceStatusCollector> collector,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : client_(client),
      collector_(std::move(collector)),
      task_runner_(task_runner),
      upload_frequency_(
          base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs)),
      has_captured_media_(false),
      weak_factory_(this) {
  // StatusUploader is currently only created for registered clients, and
  // it is currently safe to assume that the client will not unregister while
  // StatusUploader is alive.
  //
  // If future changes result in StatusUploader's lifetime extending beyond
  // unregistration events, then this class should be updated
  // to skip status uploads for unregistered clients, and to observe the client
  // and kick off an upload when registration happens.
  DCHECK(client->is_registered());

  // Track whether any media capture devices are in use - this changes what
  // type of information we are allowed to upload.
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);

  // Listen for changes to the upload delay, and start sending updates to the
  // server.
  upload_frequency_observer_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kReportUploadFrequency,
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     base::Unretained(this)));

  // Update the upload frequency from settings.
  RefreshUploadFrequency();

  // Schedule our next status upload in a minute (last_upload_ is set to the
  // start of the epoch, so this will trigger an update in
  // kMinUploadScheduleDelayMs from now).
  ScheduleNextStatusUpload();
}

StatusUploader::~StatusUploader() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

void StatusUploader::ScheduleNextStatusUpload() {
  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of kMinUploadScheduleDelayMs).
  base::TimeDelta delay = std::max(
      (last_upload_ + upload_frequency_) - base::Time::NowFromSystemTime(),
      base::TimeDelta::FromMilliseconds(kMinUploadScheduleDelayMs));
  upload_callback_.Reset(base::Bind(&StatusUploader::UploadStatus,
                                    base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, upload_callback_.callback(), delay);
}

void StatusUploader::RefreshUploadFrequency() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED != settings->PrepareTrustedValues(
          base::Bind(&StatusUploader::RefreshUploadFrequency,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - update our cached upload_frequency (we cache the
  // value because CrosSettings can become untrusted at arbitrary times and we
  // want to use the last trusted value).
  int frequency;
  if (settings->GetInteger(chromeos::kReportUploadFrequency, &frequency)) {
    CHROMEOS_SYSLOG(WARNING) << "Changing status upload frequency from "
                             << upload_frequency_ << " to "
                             << base::TimeDelta::FromMilliseconds(frequency);
    upload_frequency_ = base::TimeDelta::FromMilliseconds(
        std::max(kMinUploadDelayMs, frequency));
  }

  // Schedule a new upload with the new frequency - only do this if we've
  // already performed the initial upload, because we want the initial upload
  // to happen in a minute after startup and not get cancelled by settings
  // changes.
  if (!last_upload_.is_null())
    ScheduleNextStatusUpload();
}

bool StatusUploader::IsSessionDataUploadAllowed() {
  // Check if we're in an auto-launched kiosk session.
  std::unique_ptr<DeviceLocalAccount> account =
      collector_->GetAutoLaunchedKioskSessionInfo();
  if (!account) {
    CHROMEOS_SYSLOG(WARNING)
        << "Not a kiosk session, data upload is not allowed.";
    return false;
  }

  // Check if there has been any user input.
  base::TimeTicks last_activity_time =
      ui::UserActivityDetector::Get()->last_activity_time();
  std::string last_activity_name =
      ui::UserActivityDetector::Get()->last_activity_name();
  if (!last_activity_time.is_null()) {
    CHROMEOS_SYSLOG(WARNING)
        << "User input " << last_activity_name << " detected "
        << (base::TimeTicks::Now() - last_activity_time) << " ago ("
        << (base::SysInfo::Uptime() -
            (base::TimeTicks::Now() - last_activity_time))
        << " after last boot), data upload is not allowed.";
    return false;
  }

  // Screenshot is allowed as long as we have not captured media.
  if (has_captured_media_) {
    CHROMEOS_SYSLOG(WARNING)
        << "Media has been captured, data upload is not allowed.";
    return false;
  } else {
    return true;
  }
}

void StatusUploader::OnRequestUpdate(int render_process_id,
                                     int render_frame_id,
                                     content::MediaStreamType stream_type,
                                     const content::MediaRequestState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If a video or audio capture stream is opened, set a flag so we disallow
  // upload of potentially sensitive data.
  if (state == content::MEDIA_REQUEST_STATE_OPENING &&
      (stream_type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
       stream_type == content::MEDIA_DEVICE_VIDEO_CAPTURE)) {
    has_captured_media_ = true;
  }
}

void StatusUploader::UploadStatus() {
  // Submit the responses of the asynchronous calls to collector_
  // in a small ref-counted state tracker class, so that we can
  //   a) track that both responses fired and
  //   b) quick subsequent calls to UploadStatus() won't mess up state.
  scoped_refptr<StatusGetter> getter(
      new StatusGetter(weak_factory_.GetWeakPtr()));

  // Note that the two base::Binds keep references to getter,
  // so getter stays alive until both callbacks are called.
  collector_->GetDeviceStatusAsync(
      base::Bind(&StatusGetter::OnDeviceStatusReceived, getter));

  collector_->GetDeviceSessionStatusAsync(
      base::Bind(&StatusGetter::OnSessionStatusReceived, getter));
}

StatusUploader::StatusGetter::StatusGetter(
    const base::WeakPtr<StatusUploader>& uploader)
    : uploader_(uploader),
      device_status_response_received_(false),
      session_status_response_received_(false) {}

StatusUploader::StatusGetter::~StatusGetter() {}

void StatusUploader::StatusGetter::OnDeviceStatusReceived(
    std::unique_ptr<em::DeviceStatusReportRequest> device_status) {
  DCHECK(!device_status_response_received_);
  device_status_ = std::move(device_status);
  device_status_response_received_ = true;
  CheckDone();
}

void StatusUploader::StatusGetter::OnSessionStatusReceived(
    std::unique_ptr<em::SessionStatusReportRequest> session_status) {
  DCHECK(!session_status_response_received_);
  session_status_ = std::move(session_status);
  session_status_response_received_ = true;
  CheckDone();
}

void StatusUploader::StatusGetter::CheckDone() {
  // Did we receive BOTH responses?
  if (device_status_response_received_ && session_status_response_received_) {
    // Notify the uploader if it's still alive
    StatusUploader* uploader = uploader_.get();
    if (uploader) {
      uploader->OnStatusReceived(std::move(device_status_),
                                 std::move(session_status_));
      // Reset just to make sure this doesn't get called multiple times
      uploader_.reset();
    }
  }
}

void StatusUploader::OnStatusReceived(
    std::unique_ptr<em::DeviceStatusReportRequest> device_status,
    std::unique_ptr<em::SessionStatusReportRequest> session_status) {
  bool have_device_status = device_status != nullptr;
  bool have_session_status = session_status != nullptr;
  if (!have_device_status && !have_session_status) {
    CHROMEOS_SYSLOG(WARNING)
        << "Skipping status upload because no data to upload";
    // Don't have any status to upload - just set our timer for next time.
    last_upload_ = base::Time::NowFromSystemTime();
    ScheduleNextStatusUpload();
    return;
  }

  CHROMEOS_SYSLOG(WARNING) << "Starting status upload: have_device_status = "
                           << have_device_status;
  client_->UploadDeviceStatus(device_status.get(), session_status.get(),
                              base::Bind(&StatusUploader::OnUploadCompleted,
                                         weak_factory_.GetWeakPtr()));
}

void StatusUploader::OnUploadCompleted(bool success) {
  // Set the last upload time, regardless of whether the upload was successful
  // or not (we don't change the time of the next upload based on whether this
  // upload succeeded or not - if a status upload fails, we just skip it and
  // wait until it's time to try again.
  CHROMEOS_SYSLOG_IF(ERROR, !success) << "Error uploading status: "
                                      << client_->status();
  CHROMEOS_SYSLOG_IF(WARNING, success) << "Status upload successful";
  last_upload_ = base::Time::NowFromSystemTime();

  // If the upload was successful, tell the collector so it can clear its cache
  // of pending items.
  if (success)
    collector_->OnSubmittedSuccessfully();

  ScheduleNextStatusUpload();
}

}  // namespace policy
