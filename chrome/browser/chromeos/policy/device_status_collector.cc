// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <memory>
#include <sstream>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::Time;
using base::TimeDelta;

namespace em = enterprise_management;

namespace {
// How many seconds of inactivity triggers the idle state.
const int kIdleStateThresholdSeconds = 300;

// How many days in the past to store active periods for.
const unsigned int kMaxStoredPastActivityDays = 30;

// How many days in the future to store active periods for.
const unsigned int kMaxStoredFutureActivityDays = 2;

// How often, in seconds, to update the device location.
const unsigned int kGeolocationPollIntervalSeconds = 30 * 60;

// How often, in seconds, to sample the hardware state.
const unsigned int kHardwareStatusSampleIntervalSeconds = 120;

// Keys for the geolocation status dictionary in local state.
const char kLatitude[] = "latitude";
const char kLongitude[] = "longitude";
const char kAltitude[] = "altitude";
const char kAccuracy[] = "accuracy";
const char kAltitudeAccuracy[] = "altitude_accuracy";
const char kHeading[] = "heading";
const char kSpeed[] = "speed";
const char kTimestamp[] = "timestamp";

// The location we read our CPU statistics from.
const char kProcStat[] = "/proc/stat";

// The location we read our CPU temperature and channel label from.
const char kHwmonDir[] = "/sys/class/hwmon/";
const char kDeviceDir[] = "device";
const char kHwmonDirectoryPattern[] = "hwmon*";
const char kCPUTempFilePattern[] = "temp*_input";

// Determine the day key (milliseconds since epoch for corresponding day in UTC)
// for a given |timestamp|.
int64_t TimestampToDayKey(Time timestamp) {
  Time::Exploded exploded;
  timestamp.LocalMidnight().LocalExplode(&exploded);
  Time out_time;
  bool conversion_success = Time::FromUTCExploded(exploded, &out_time);
  DCHECK(conversion_success);
  return (out_time - Time::UnixEpoch()).InMilliseconds();
}

// Helper function (invoked via blocking pool) to fetch information about
// mounted disks.
std::vector<em::VolumeInfo> GetVolumeInfo(
    const std::vector<std::string>& mount_points) {
  std::vector<em::VolumeInfo> result;
  for (const std::string& mount_point : mount_points) {
    base::FilePath mount_path(mount_point);
    int64_t free_size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
    int64_t total_size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
    if (free_size < 0 || total_size < 0) {
      LOG_IF(ERROR, !mount_point.empty()) << "Unable to get volume status for "
                                          << mount_point;
      continue;
    }
    em::VolumeInfo info;
    info.set_volume_id(mount_point);
    info.set_storage_total(total_size);
    info.set_storage_free(free_size);
    result.push_back(info);
  }
  return result;
}

// Reads the first CPU line from /proc/stat. Returns an empty string if
// the cpu data could not be read.
// The format of this line from /proc/stat is:
//
//   cpu  user_time nice_time system_time idle_time
//
// where user_time, nice_time, system_time, and idle_time are all integer
// values measured in jiffies from system startup.
std::string ReadCPUStatistics() {
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kProcStat), &contents)) {
    size_t eol = contents.find("\n");
    if (eol != std::string::npos) {
      std::string line = contents.substr(0, eol);
      if (line.compare(0, 4, "cpu ") == 0)
        return line;
    }
    // First line should always start with "cpu ".
    NOTREACHED() << "Could not parse /proc/stat contents: " << contents;
  }
  LOG(WARNING) << "Unable to read CPU statistics from " << kProcStat;
  return std::string();
}

// Reads the CPU temperature info from
// /sys/class/hwmon/hwmon*/device/temp*_input and
// /sys/class/hwmon/hwmon*/device/temp*_label files.
//
// temp*_input contains CPU temperature in millidegree Celsius
// temp*_label contains appropriate temperature channel label.
std::vector<em::CPUTempInfo> ReadCPUTempInfo() {
  std::vector<em::CPUTempInfo> contents;
  // Get directories /sys/class/hwmon/hwmon*
  base::FileEnumerator hwmon_enumerator(base::FilePath(kHwmonDir), false,
                                        base::FileEnumerator::DIRECTORIES,
                                        kHwmonDirectoryPattern);

  for (base::FilePath hwmon_path = hwmon_enumerator.Next(); !hwmon_path.empty();
       hwmon_path = hwmon_enumerator.Next()) {
    // Get files /sys/class/hwmon/hwmon*/device/temp*_input
    const base::FilePath hwmon_device_dir = hwmon_path.Append(kDeviceDir);
    base::FileEnumerator enumerator(hwmon_device_dir, false,
                                    base::FileEnumerator::FILES,
                                    kCPUTempFilePattern);
    for (base::FilePath temperature_path = enumerator.Next();
         !temperature_path.empty(); temperature_path = enumerator.Next()) {
      // Get appropriate temp*_label file.
      std::string label_path = temperature_path.MaybeAsASCII();
      if (label_path.empty()) {
        LOG(WARNING) << "Unable to parse a path to temp*_input file as ASCII";
        continue;
      }
      base::ReplaceSubstringsAfterOffset(&label_path, 0, "input", "label");

      // Read label.
      std::string label;
      if (!base::PathExists(base::FilePath(label_path)) ||
          !base::ReadFileToString(base::FilePath(label_path), &label)) {
        label = std::string();
      }

      // Read temperature in millidegree Celsius.
      std::string temperature_string;
      int32_t temperature = 0;
      if (base::ReadFileToString(temperature_path, &temperature_string) &&
          sscanf(temperature_string.c_str(), "%d", &temperature) == 1) {
        // CPU temp in millidegree Celsius to Celsius
        temperature /= 1000;

        em::CPUTempInfo info;
        info.set_cpu_label(label);
        info.set_cpu_temp(temperature);
        contents.push_back(info);
      } else {
        LOG(WARNING) << "Unable to read CPU temp from "
                     << temperature_path.MaybeAsASCII();
      }
    }
  }
  return contents;
}

// Returns the DeviceLocalAccount associated with the current kiosk session.
// Returns null if there is no active kiosk session, or if that kiosk
// session has been removed from policy since the session started, in which
// case we won't report its status).
std::unique_ptr<policy::DeviceLocalAccount> GetCurrentKioskDeviceLocalAccount(
    chromeos::CrosSettings* settings) {
  if (!user_manager::UserManager::Get()->IsLoggedInAsKioskApp())
    return std::unique_ptr<policy::DeviceLocalAccount>();
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const std::vector<policy::DeviceLocalAccount> accounts =
      policy::GetDeviceLocalAccounts(settings);

  for (const auto& device_local_account : accounts) {
    if (AccountId::FromUserEmail(device_local_account.user_id) ==
        user->GetAccountId()) {
      return base::MakeUnique<policy::DeviceLocalAccount>(device_local_account);
    }
  }
  LOG(WARNING) << "Kiosk app not found in list of device-local accounts";
  return std::unique_ptr<policy::DeviceLocalAccount>();
}

base::Version GetPlatformVersion() {
  int32_t major_version;
  int32_t minor_version;
  int32_t bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(&major_version, &minor_version,
                                               &bugfix_version);
  return base::Version(base::StringPrintf("%d.%d.%d", major_version,
                                          minor_version, bugfix_version));
}

// Helper routine to convert from Shill-provided signal strength (percent)
// to dBm units expected by server.
int ConvertWifiSignalStrength(int signal_strength) {
  // Shill attempts to convert WiFi signal strength from its internal dBm to a
  // percentage range (from 0-100) by adding 120 to the raw dBm value,
  // and then clamping the result to the range 0-100 (see
  // shill::WiFiService::SignalToStrength()).
  //
  // To convert back to dBm, we subtract 120 from the percentage value to yield
  // a clamped dBm value in the range of -119 to -20dBm.
  //
  // TODO(atwilson): Tunnel the raw dBm signal strength from Shill instead of
  // doing the conversion here so we can report non-clamped values
  // (crbug.com/463334).
  DCHECK_GT(signal_strength, 0);
  DCHECK_LE(signal_strength, 100);
  return signal_strength - 120;
}

}  // namespace

namespace policy {

DeviceStatusCollector::DeviceStatusCollector(
    PrefService* local_state,
    chromeos::system::StatisticsProvider* provider,
    const LocationUpdateRequester& location_update_requester,
    const VolumeInfoFetcher& volume_info_fetcher,
    const CPUStatisticsFetcher& cpu_statistics_fetcher,
    const CPUTempFetcher& cpu_temp_fetcher)
    : max_stored_past_activity_days_(kMaxStoredPastActivityDays),
      max_stored_future_activity_days_(kMaxStoredFutureActivityDays),
      local_state_(local_state),
      last_idle_check_(Time()),
      volume_info_fetcher_(volume_info_fetcher),
      cpu_statistics_fetcher_(cpu_statistics_fetcher),
      cpu_temp_fetcher_(cpu_temp_fetcher),
      statistics_provider_(provider),
      cros_settings_(chromeos::CrosSettings::Get()),
      location_update_requester_(location_update_requester),
      weak_factory_(this) {
  CHECK(content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));

  if (volume_info_fetcher_.is_null())
    volume_info_fetcher_ = base::Bind(&GetVolumeInfo);

  if (cpu_statistics_fetcher_.is_null())
    cpu_statistics_fetcher_ = base::Bind(&ReadCPUStatistics);

  if (cpu_temp_fetcher_.is_null())
    cpu_temp_fetcher_ = base::Bind(&ReadCPUTempInfo);

  idle_poll_timer_.Start(FROM_HERE,
                         TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                         this, &DeviceStatusCollector::CheckIdleState);
  hardware_status_sampling_timer_.Start(
      FROM_HERE,
      TimeDelta::FromSeconds(kHardwareStatusSampleIntervalSeconds),
      this, &DeviceStatusCollector::SampleHardwareStatus);

  // Watch for changes to the individual policies that control what the status
  // reports contain.
  base::Closure callback =
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVersionInfo, callback);
  activity_times_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceActivityTimes, callback);
  boot_mode_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBootMode, callback);
  location_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceLocation, callback);
  network_interfaces_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceNetworkInterfaces, callback);
  users_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceUsers, callback);
  hardware_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceHardwareStatus, callback);
  session_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceSessionStatus, callback);
  os_update_status_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportOsUpdateStatus, callback);
  running_kiosk_app_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportRunningKioskApp, callback);

  // The last known location is persisted in local state. This makes location
  // information available immediately upon startup and avoids the need to
  // reacquire the location on every user session change or browser crash.
  device::Geoposition position;
  std::string timestamp_str;
  int64_t timestamp;
  const base::DictionaryValue* location =
      local_state_->GetDictionary(prefs::kDeviceLocation);
  if (location->GetDouble(kLatitude, &position.latitude) &&
      location->GetDouble(kLongitude, &position.longitude) &&
      location->GetDouble(kAltitude, &position.altitude) &&
      location->GetDouble(kAccuracy, &position.accuracy) &&
      location->GetDouble(kAltitudeAccuracy, &position.altitude_accuracy) &&
      location->GetDouble(kHeading, &position.heading) &&
      location->GetDouble(kSpeed, &position.speed) &&
      location->GetString(kTimestamp, &timestamp_str) &&
      base::StringToInt64(timestamp_str, &timestamp)) {
    position.timestamp = Time::FromInternalValue(timestamp);
    position_ = position;
  }

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the the OS and firmware version info.
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&DeviceStatusCollector::OnOSVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&DeviceStatusCollector::OnOSFirmware,
                 weak_factory_.GetWeakPtr()));
}

DeviceStatusCollector::~DeviceStatusCollector() {
}

// static
void DeviceStatusCollector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kDeviceActivityTimes,
                                   new base::DictionaryValue);
  registry->RegisterDictionaryPref(prefs::kDeviceLocation,
                                   new base::DictionaryValue);
}

void DeviceStatusCollector::CheckIdleState() {
  CalculateIdleState(kIdleStateThresholdSeconds,
      base::Bind(&DeviceStatusCollector::IdleStateCallback,
                 base::Unretained(this)));
}

void DeviceStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
      base::Bind(&DeviceStatusCollector::UpdateReportingSettings,
                 weak_factory_.GetWeakPtr()))) {
    return;
  }

  // All reporting settings default to 'enabled'.
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceVersionInfo, &report_version_info_)) {
    report_version_info_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceActivityTimes, &report_activity_times_)) {
    report_activity_times_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceBootMode, &report_boot_mode_)) {
    report_boot_mode_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceNetworkInterfaces,
          &report_network_interfaces_)) {
    report_network_interfaces_ = true;
  }
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceUsers, &report_users_)) {
    report_users_ = true;
  }

  const bool already_reporting_hardware_status = report_hardware_status_;
  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceHardwareStatus, &report_hardware_status_)) {
    report_hardware_status_ = true;
  }

  if (!cros_settings_->GetBoolean(
          chromeos::kReportDeviceSessionStatus, &report_session_status_)) {
    report_session_status_ = true;
  }

  // Device location reporting is disabled by default because it is
  // not launched yet.
  if (!cros_settings_->GetBoolean(
      chromeos::kReportDeviceLocation, &report_location_)) {
    report_location_ = false;
  }

  if (report_location_) {
    ScheduleGeolocationUpdateRequest();
  } else {
    geolocation_update_timer_.Stop();
    position_ = device::Geoposition();
    local_state_->ClearPref(prefs::kDeviceLocation);
  }

  if (!report_hardware_status_) {
    ClearCachedHardwareStatus();
  } else if (!already_reporting_hardware_status) {
    // Turning on hardware status reporting - fetch an initial sample
    // immediately instead of waiting for the sampling timer to fire.
    SampleHardwareStatus();
  }

  // Os update status and running kiosk app reporting are disabled by default.
  if (!cros_settings_->GetBoolean(chromeos::kReportOsUpdateStatus,
                                  &report_os_update_status_)) {
    report_os_update_status_ = false;
  }
  if (!cros_settings_->GetBoolean(chromeos::kReportRunningKioskApp,
                                  &report_running_kiosk_app_)) {
    report_running_kiosk_app_ = false;
  }
}

Time DeviceStatusCollector::GetCurrentTime() {
  return Time::Now();
}

// Remove all out-of-range activity times from the local store.
void DeviceStatusCollector::PruneStoredActivityPeriods(Time base_time) {
  Time min_time =
      base_time - TimeDelta::FromDays(max_stored_past_activity_days_);
  Time max_time =
      base_time + TimeDelta::FromDays(max_stored_future_activity_days_);
  TrimStoredActivityPeriods(TimestampToDayKey(min_time), 0,
                            TimestampToDayKey(max_time));
}

void DeviceStatusCollector::TrimStoredActivityPeriods(int64_t min_day_key,
                                                      int min_day_trim_duration,
                                                      int64_t max_day_key) {
  const base::DictionaryValue* activity_times =
      local_state_->GetDictionary(prefs::kDeviceActivityTimes);

  std::unique_ptr<base::DictionaryValue> copy(activity_times->DeepCopy());
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t timestamp;
    if (base::StringToInt64(it.key(), &timestamp)) {
      // Remove data that is too old, or too far in the future.
      if (timestamp >= min_day_key && timestamp < max_day_key) {
        if (timestamp == min_day_key) {
          int new_activity_duration = 0;
          if (it.value().GetAsInteger(&new_activity_duration)) {
            new_activity_duration =
                std::max(new_activity_duration - min_day_trim_duration, 0);
          }
          copy->SetInteger(it.key(), new_activity_duration);
        }
        continue;
      }
    }
    // The entry is out of range or couldn't be parsed. Remove it.
    copy->Remove(it.key(), NULL);
  }
  local_state_->Set(prefs::kDeviceActivityTimes, *copy);
}

void DeviceStatusCollector::AddActivePeriod(Time start, Time end) {
  DCHECK(start < end);

  // Maintain the list of active periods in a local_state pref.
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  // Assign the period to day buckets in local time.
  Time midnight = start.LocalMidnight();
  while (midnight < end) {
    midnight += TimeDelta::FromDays(1);
    int64_t activity = (std::min(end, midnight) - start).InMilliseconds();
    std::string day_key = base::Int64ToString(TimestampToDayKey(start));
    int previous_activity = 0;
    activity_times->GetInteger(day_key, &previous_activity);
    activity_times->SetInteger(day_key, previous_activity + activity);
    start = midnight;
  }
}

void DeviceStatusCollector::ClearCachedHardwareStatus() {
  volume_info_.clear();
  resource_usage_.clear();
  last_cpu_active_ = 0;
  last_cpu_idle_ = 0;
}

void DeviceStatusCollector::IdleStateCallback(ui::IdleState state) {
  // Do nothing if device activity reporting is disabled.
  if (!report_activity_times_)
    return;

  Time now = GetCurrentTime();

  if (state == ui::IDLE_STATE_ACTIVE) {
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity.
    int active_seconds = (now - last_idle_check_).InSeconds();
    if (active_seconds < 0 ||
        active_seconds >= static_cast<int>((2 * kIdlePollIntervalSeconds))) {
      AddActivePeriod(now - TimeDelta::FromSeconds(kIdlePollIntervalSeconds),
                      now);
    } else {
      AddActivePeriod(last_idle_check_, now);
    }

    PruneStoredActivityPeriods(now);
  }
  last_idle_check_ = now;
}

std::unique_ptr<DeviceLocalAccount>
DeviceStatusCollector::GetAutoLaunchedKioskSessionInfo() {
  std::unique_ptr<DeviceLocalAccount> account =
      GetCurrentKioskDeviceLocalAccount(cros_settings_);
  if (account) {
    chromeos::KioskAppManager::App current_app;
    if (chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                                 &current_app) &&
        current_app.was_auto_launched_with_zero_delay) {
      return account;
    }
  }
  // No auto-launched kiosk session active.
  return std::unique_ptr<DeviceLocalAccount>();
}

void DeviceStatusCollector::SampleHardwareStatus() {
  // Results must be written in the creation thread since that's where they
  // are read from in the Get*StatusAsync methods.
  CHECK(content::BrowserThread::CurrentlyOn(creation_thread_));

  // If hardware reporting has been disabled, do nothing here.
  if (!report_hardware_status_)
    return;

  // Create list of mounted disk volumes to query status.
  std::vector<storage::MountPoints::MountPointInfo> external_mount_points;
  storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
      &external_mount_points);

  std::vector<std::string> mount_points;
  for (const auto& info : external_mount_points)
    mount_points.push_back(info.path.value());

  for (const auto& mount_info :
           chromeos::disks::DiskMountManager::GetInstance()->mount_points()) {
    // Extract a list of mount points to populate.
    mount_points.push_back(mount_info.first);
  }

  // Call out to the blocking pool to measure disk, CPU usage and CPU temp.
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(volume_info_fetcher_, mount_points),
      base::Bind(&DeviceStatusCollector::ReceiveVolumeInfo,
                 weak_factory_.GetWeakPtr()));

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE,
      cpu_statistics_fetcher_,
      base::Bind(&DeviceStatusCollector::ReceiveCPUStatistics,
                 weak_factory_.GetWeakPtr()));

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(), FROM_HERE, cpu_temp_fetcher_,
      base::Bind(&DeviceStatusCollector::StoreCPUTempInfo,
                 weak_factory_.GetWeakPtr()));
}

void DeviceStatusCollector::ReceiveCPUStatistics(const std::string& stats) {
  int cpu_usage_percent = 0;
  if (stats.empty()) {
    DLOG(WARNING) << "Unable to read CPU statistics";
  } else {
    // Parse the data from /proc/stat, whose format is defined at
    // https://www.kernel.org/doc/Documentation/filesystems/proc.txt.
    //
    // The CPU usage values in /proc/stat are measured in the imprecise unit
    // "jiffies", but we just care about the relative magnitude of "active" vs
    // "idle" so the exact value of a jiffy is irrelevant.
    //
    // An example value for this line:
    //
    // cpu 123 456 789 012 345 678
    //
    // We only care about the first four numbers: user_time, nice_time,
    // sys_time, and idle_time.
    uint64_t user = 0, nice = 0, system = 0, idle = 0;
    int vals = sscanf(stats.c_str(),
                      "cpu %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64, &user,
                      &nice, &system, &idle);
    DCHECK_EQ(4, vals);

    // The values returned from /proc/stat are cumulative totals, so calculate
    // the difference between the last sample and this one.
    uint64_t active = user + nice + system;
    uint64_t total = active + idle;
    uint64_t last_total = last_cpu_active_ + last_cpu_idle_;
    DCHECK_GE(active, last_cpu_active_);
    DCHECK_GE(idle, last_cpu_idle_);
    DCHECK_GE(total, last_total);

    if ((total - last_total) > 0) {
      cpu_usage_percent =
          (100 * (active - last_cpu_active_)) / (total - last_total);
    }
    last_cpu_active_ = active;
    last_cpu_idle_ = idle;
  }

  DCHECK_LE(cpu_usage_percent, 100);
  ResourceUsage usage = {cpu_usage_percent,
                         base::SysInfo::AmountOfAvailablePhysicalMemory()};

  resource_usage_.push_back(usage);

  // If our cache of samples is full, throw out old samples to make room for new
  // sample.
  if (resource_usage_.size() > kMaxResourceUsageSamples)
    resource_usage_.pop_front();
}

void DeviceStatusCollector::StoreCPUTempInfo(
    const std::vector<em::CPUTempInfo>& info) {
  if (info.empty()) {
    DLOG(WARNING) << "Unable to read CPU temp information.";
  }

  if (report_hardware_status_)
    cpu_temp_info_ = info;
}

bool DeviceStatusCollector::GetActivityTimes(
    em::DeviceStatusReportRequest* request) {
  DictionaryPrefUpdate update(local_state_, prefs::kDeviceActivityTimes);
  base::DictionaryValue* activity_times = update.Get();

  bool anything_reported = false;
  for (base::DictionaryValue::Iterator it(*activity_times); !it.IsAtEnd();
       it.Advance()) {
    int64_t start_timestamp;
    int activity_milliseconds;
    if (base::StringToInt64(it.key(), &start_timestamp) &&
        it.value().GetAsInteger(&activity_milliseconds)) {
      // This is correct even when there are leap seconds, because when a leap
      // second occurs, two consecutive seconds have the same timestamp.
      int64_t end_timestamp = start_timestamp + Time::kMillisecondsPerDay;

      em::ActiveTimePeriod* active_period = request->add_active_period();
      em::TimePeriod* period = active_period->mutable_time_period();
      period->set_start_timestamp(start_timestamp);
      period->set_end_timestamp(end_timestamp);
      active_period->set_active_duration(activity_milliseconds);
      if (start_timestamp >= last_reported_day_) {
        last_reported_day_ = start_timestamp;
        duration_for_last_reported_day_ = activity_milliseconds;
      }
      anything_reported = true;
    } else {
      NOTREACHED();
    }
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* request) {
  request->set_browser_version(version_info::GetVersionNumber());
  request->set_os_version(os_version_);
  request->set_firmware_version(firmware_version_);
  return true;
}

bool DeviceStatusCollector::GetBootMode(
    em::DeviceStatusReportRequest* request) {
  std::string dev_switch_mode;
  bool anything_reported = false;
  if (statistics_provider_->GetMachineStatistic(
          chromeos::system::kDevSwitchBootKey, &dev_switch_mode)) {
    if (dev_switch_mode == chromeos::system::kDevSwitchBootValueDev)
      request->set_boot_mode("Dev");
    else if (dev_switch_mode == chromeos::system::kDevSwitchBootValueVerified)
      request->set_boot_mode("Verified");
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetLocation(
    em::DeviceStatusReportRequest* request) {
  em::DeviceLocation* location = request->mutable_device_location();
  if (!position_.Validate()) {
    location->set_error_code(
        em::DeviceLocation::ERROR_CODE_POSITION_UNAVAILABLE);
    location->set_error_message(position_.error_message);
  } else {
    location->set_latitude(position_.latitude);
    location->set_longitude(position_.longitude);
    location->set_accuracy(position_.accuracy);
    location->set_timestamp(
        (position_.timestamp - Time::UnixEpoch()).InMilliseconds());
    // Lowest point on land is at approximately -400 meters.
    if (position_.altitude > -10000.)
      location->set_altitude(position_.altitude);
    if (position_.altitude_accuracy >= 0.)
      location->set_altitude_accuracy(position_.altitude_accuracy);
    if (position_.heading >= 0. && position_.heading <= 360)
      location->set_heading(position_.heading);
    if (position_.speed >= 0.)
      location->set_speed(position_.speed);
    location->set_error_code(em::DeviceLocation::ERROR_CODE_NONE);
  }
  return true;
}

bool DeviceStatusCollector::GetNetworkInterfaces(
    em::DeviceStatusReportRequest* request) {
  // Maps shill device type strings to proto enum constants.
  static const struct {
    const char* type_string;
    em::NetworkInterface::NetworkDeviceType type_constant;
  } kDeviceTypeMap[] = {
    { shill::kTypeEthernet,  em::NetworkInterface::TYPE_ETHERNET,  },
    { shill::kTypeWifi,      em::NetworkInterface::TYPE_WIFI,      },
    { shill::kTypeWimax,     em::NetworkInterface::TYPE_WIMAX,     },
    { shill::kTypeBluetooth, em::NetworkInterface::TYPE_BLUETOOTH, },
    { shill::kTypeCellular,  em::NetworkInterface::TYPE_CELLULAR,  },
  };

  // Maps shill device connection status to proto enum constants.
  static const struct {
    const char* state_string;
    em::NetworkState::ConnectionState state_constant;
  } kConnectionStateMap[] = {
    { shill::kStateIdle,              em::NetworkState::IDLE },
    { shill::kStateCarrier,           em::NetworkState::CARRIER },
    { shill::kStateAssociation,       em::NetworkState::ASSOCIATION },
    { shill::kStateConfiguration,     em::NetworkState::CONFIGURATION },
    { shill::kStateReady,             em::NetworkState::READY },
    { shill::kStatePortal,            em::NetworkState::PORTAL },
    { shill::kStateOffline,           em::NetworkState::OFFLINE },
    { shill::kStateOnline,            em::NetworkState::ONLINE },
    { shill::kStateDisconnect,        em::NetworkState::DISCONNECT },
    { shill::kStateFailure,           em::NetworkState::FAILURE },
    { shill::kStateActivationFailure,
        em::NetworkState::ACTIVATION_FAILURE },
  };

  chromeos::NetworkStateHandler::DeviceStateList device_list;
  chromeos::NetworkStateHandler* network_state_handler =
      chromeos::NetworkHandler::Get()->network_state_handler();
  network_state_handler->GetDeviceList(&device_list);

  bool anything_reported = false;
  chromeos::NetworkStateHandler::DeviceStateList::const_iterator device;
  for (device = device_list.begin(); device != device_list.end(); ++device) {
    // Determine the type enum constant for |device|.
    size_t type_idx = 0;
    for (; type_idx < arraysize(kDeviceTypeMap); ++type_idx) {
      if ((*device)->type() == kDeviceTypeMap[type_idx].type_string)
        break;
    }

    // If the type isn't in |kDeviceTypeMap|, the interface is not relevant for
    // reporting. This filters out VPN devices.
    if (type_idx >= arraysize(kDeviceTypeMap))
      continue;

    em::NetworkInterface* interface = request->add_network_interface();
    interface->set_type(kDeviceTypeMap[type_idx].type_constant);
    if (!(*device)->mac_address().empty())
      interface->set_mac_address((*device)->mac_address());
    if (!(*device)->meid().empty())
      interface->set_meid((*device)->meid());
    if (!(*device)->imei().empty())
      interface->set_imei((*device)->imei());
    if (!(*device)->path().empty())
      interface->set_device_path((*device)->path());
    anything_reported = true;
  }

  // Don't write any network state if we aren't in a kiosk or public session.
  if (!GetAutoLaunchedKioskSessionInfo() &&
      !user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    return anything_reported;

  // Walk the various networks and store their state in the status report.
  chromeos::NetworkStateHandler::NetworkStateList state_list;
  network_state_handler->GetNetworkListByType(
      chromeos::NetworkTypePattern::Default(),
      true,   // configured_only
      false,  // visible_only
      0,      // no limit to number of results
      &state_list);

  for (const chromeos::NetworkState* state : state_list) {
    // Determine the connection state and signal strength for |state|.
    em::NetworkState::ConnectionState connection_state_enum =
        em::NetworkState::UNKNOWN;
    const std::string connection_state_string(state->connection_state());
    for (size_t i = 0; i < arraysize(kConnectionStateMap); ++i) {
      if (connection_state_string == kConnectionStateMap[i].state_string) {
        connection_state_enum = kConnectionStateMap[i].state_constant;
        break;
      }
    }

    // Copy fields from NetworkState into the status report.
    em::NetworkState* proto_state = request->add_network_state();
    proto_state->set_connection_state(connection_state_enum);
    anything_reported = true;

    // Report signal strength for wifi connections.
    if (state->type() == shill::kTypeWifi) {
      // If shill has provided a signal strength, convert it to dBm and store it
      // in the status report. A signal_strength() of 0 connotes "no signal"
      // rather than "really weak signal", so we only report signal strength if
      // it is non-zero.
      if (state->signal_strength()) {
        proto_state->set_signal_strength(
            ConvertWifiSignalStrength(state->signal_strength()));
      }
    }

    if (!state->device_path().empty())
      proto_state->set_device_path(state->device_path());

    if (!state->ip_address().empty())
      proto_state->set_ip_address(state->ip_address());

    if (!state->gateway().empty())
      proto_state->set_gateway(state->gateway());
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetUsers(em::DeviceStatusReportRequest* request) {
  const user_manager::UserList& users =
      chromeos::ChromeUserManager::Get()->GetUsers();

  bool anything_reported = false;
  for (auto* user : users) {
    // Only users with gaia accounts (regular) are reported.
    if (!user->HasGaiaAccount())
      continue;

    em::DeviceUser* device_user = request->add_user();
    if (chromeos::ChromeUserManager::Get()->ShouldReportUser(user->email())) {
      device_user->set_type(em::DeviceUser::USER_TYPE_MANAGED);
      device_user->set_email(user->email());
    } else {
      device_user->set_type(em::DeviceUser::USER_TYPE_UNMANAGED);
      // Do not report the email address of unmanaged users.
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool DeviceStatusCollector::GetHardwareStatus(
    em::DeviceStatusReportRequest* status) {
  // Add volume info.
  status->clear_volume_info();
  for (const em::VolumeInfo& info : volume_info_) {
    *status->add_volume_info() = info;
  }

  status->set_system_ram_total(base::SysInfo::AmountOfPhysicalMemory());
  status->clear_system_ram_free();
  status->clear_cpu_utilization_pct();
  for (const ResourceUsage& usage : resource_usage_) {
    status->add_cpu_utilization_pct(usage.cpu_usage_percent);
    status->add_system_ram_free(usage.bytes_of_ram_free);
  }

  // Add CPU temp info.
  status->clear_cpu_temp_info();
  for (const em::CPUTempInfo& info : cpu_temp_info_) {
    *status->add_cpu_temp_info() = info;
  }
  return true;
}

bool DeviceStatusCollector::GetOsUpdateStatus(
    em::DeviceStatusReportRequest* status) {
  const base::Version platform_version(GetPlatformVersion());
  if (!platform_version.IsValid())
    return false;

  const std::string required_platform_version_string =
      chromeos::KioskAppManager::Get()
          ->GetAutoLaunchAppRequiredPlatformVersion();
  if (required_platform_version_string.empty())
    return false;

  const base::Version required_platfrom_version(
      required_platform_version_string);

  em::OsUpdateStatus* os_update_status = status->mutable_os_update_status();
  os_update_status->set_new_required_platform_version(
      required_platfrom_version.GetString());

  if (platform_version == required_platfrom_version) {
    os_update_status->set_update_status(em::OsUpdateStatus::OS_UP_TO_DATE);
    return true;
  }

  const chromeos::UpdateEngineClient::Status update_engine_status =
      chromeos::DBusThreadManager::Get()
          ->GetUpdateEngineClient()
          ->GetLastStatus();
  if (update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING ||
      update_engine_status.status ==
          chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_IN_PROGRESS);
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else if (update_engine_status.status ==
             chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_UPDATE_NEED_REBOOT);
    // Note the new_version could be a dummy "0.0.0.0" for some edge cases,
    // e.g. update engine is somehow restarted without a reboot.
    os_update_status->set_new_platform_version(
        update_engine_status.new_version);
  } else {
    os_update_status->set_update_status(
        em::OsUpdateStatus::OS_IMAGE_DOWNLOAD_NOT_STARTED);
  }

  return true;
}

bool DeviceStatusCollector::GetRunningKioskApp(
    em::DeviceStatusReportRequest* status) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  CHECK(content::BrowserThread::CurrentlyOn(creation_thread_));

  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  // Only generate running kiosk app reports if we are in an auto-launched kiosk
  // session.
  if (!account)
    return false;

  em::AppStatus* running_kiosk_app = status->mutable_running_kiosk_app();
  running_kiosk_app->set_app_id(account->kiosk_app_id);

  const std::string app_version = GetAppVersion(account->kiosk_app_id);
  if (app_version.empty()) {
    DLOG(ERROR) << "Unable to get version for extension: "
                << account->kiosk_app_id;
  } else {
    running_kiosk_app->set_extension_version(app_version);
  }

  chromeos::KioskAppManager::App app_info;
  if (chromeos::KioskAppManager::Get()->GetApp(account->kiosk_app_id,
                                               &app_info)) {
    running_kiosk_app->set_required_platform_version(
        app_info.required_platform_version);
  }
  return true;
}

void DeviceStatusCollector::GetDeviceStatusAsync(
    const DeviceStatusCallback& response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  CHECK(content::BrowserThread::CurrentlyOn(creation_thread_));

  std::unique_ptr<em::DeviceStatusReportRequest> status =
      base::MakeUnique<em::DeviceStatusReportRequest>();
  bool got_status = false;

  if (report_activity_times_)
    got_status |= GetActivityTimes(status.get());

  if (report_version_info_)
    got_status |= GetVersionInfo(status.get());

  if (report_boot_mode_)
    got_status |= GetBootMode(status.get());

  if (report_location_)
    got_status |= GetLocation(status.get());

  if (report_network_interfaces_)
    got_status |= GetNetworkInterfaces(status.get());

  if (report_users_)
    got_status |= GetUsers(status.get());

  if (report_hardware_status_)
    got_status |= GetHardwareStatus(status.get());

  if (report_os_update_status_)
    got_status |= GetOsUpdateStatus(status.get());

  if (report_running_kiosk_app_)
    got_status |= GetRunningKioskApp(status.get());

  // Wipe pointer if we didn't actually add any data.
  if (!got_status)
    status.reset();

  content::BrowserThread::PostTask(creation_thread_, FROM_HERE,
                                   base::Bind(response, base::Passed(&status)));
}

void DeviceStatusCollector::GetDeviceSessionStatusAsync(
    const DeviceSessionStatusCallback& response) {
  // Only generate session status reports if session status reporting is
  // enabled.
  if (!report_session_status_) {
    content::BrowserThread::PostTask(creation_thread_, FROM_HERE,
                                     base::Bind(response, nullptr));
    return;
  }

  std::unique_ptr<const DeviceLocalAccount> account =
      GetAutoLaunchedKioskSessionInfo();
  // Only generate session status reports if we are in an auto-launched kiosk
  // session.
  if (!account) {
    content::BrowserThread::PostTask(creation_thread_, FROM_HERE,
                                     base::Bind(response, nullptr));
    return;
  }

  std::unique_ptr<em::SessionStatusReportRequest> status =
      base::MakeUnique<em::SessionStatusReportRequest>();

  // Get the account ID associated with this user.
  status->set_device_local_account_id(account->account_id);
  em::AppStatus* app_status = status->add_installed_apps();
  app_status->set_app_id(account->kiosk_app_id);

  // Look up the app and get the version.
  const std::string app_version = GetAppVersion(account->kiosk_app_id);
  if (app_version.empty()) {
    DLOG(ERROR) << "Unable to get version for extension: "
                << account->kiosk_app_id;
  } else {
    app_status->set_extension_version(app_version);
  }

  content::BrowserThread::PostTask(creation_thread_, FROM_HERE,
                                   base::Bind(response, base::Passed(&status)));
}

std::string DeviceStatusCollector::GetAppVersion(
    const std::string& kiosk_app_id) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(
          user_manager::UserManager::Get()->GetActiveUser());
  const extensions::ExtensionRegistry* const registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::Extension* const extension = registry->GetExtensionById(
      kiosk_app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension)
    return std::string();
  return extension->VersionString();
}

void DeviceStatusCollector::OnSubmittedSuccessfully() {
  TrimStoredActivityPeriods(last_reported_day_, duration_for_last_reported_day_,
                            std::numeric_limits<int64_t>::max());
}

void DeviceStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

void DeviceStatusCollector::OnOSFirmware(const std::string& version) {
  firmware_version_ = version;
}

void DeviceStatusCollector::ScheduleGeolocationUpdateRequest() {
  if (geolocation_update_timer_.IsRunning() || geolocation_update_in_progress_)
    return;

  if (position_.Validate()) {
    TimeDelta elapsed = GetCurrentTime() - position_.timestamp;
    TimeDelta interval =
        TimeDelta::FromSeconds(kGeolocationPollIntervalSeconds);
    if (elapsed <= interval) {
      geolocation_update_timer_.Start(
          FROM_HERE,
          interval - elapsed,
          this,
          &DeviceStatusCollector::ScheduleGeolocationUpdateRequest);
      return;
    }
  }

  geolocation_update_in_progress_ = true;
  if (location_update_requester_.is_null()) {
    geolocation_subscription_ =
        device::GeolocationProvider::GetInstance()->AddLocationUpdateCallback(
            base::Bind(&DeviceStatusCollector::ReceiveGeolocationUpdate,
                       weak_factory_.GetWeakPtr()),
            true);
  } else {
    location_update_requester_.Run(base::Bind(
        &DeviceStatusCollector::ReceiveGeolocationUpdate,
        weak_factory_.GetWeakPtr()));
  }
}

void DeviceStatusCollector::ReceiveGeolocationUpdate(
    const device::Geoposition& position) {
  geolocation_update_in_progress_ = false;

  // Ignore update if device location reporting has since been disabled.
  if (!report_location_)
    return;

  if (position.Validate()) {
    position_ = position;
    base::DictionaryValue location;
    location.SetDouble(kLatitude, position.latitude);
    location.SetDouble(kLongitude, position.longitude);
    location.SetDouble(kAltitude, position.altitude);
    location.SetDouble(kAccuracy, position.accuracy);
    location.SetDouble(kAltitudeAccuracy, position.altitude_accuracy);
    location.SetDouble(kHeading, position.heading);
    location.SetDouble(kSpeed, position.speed);
    location.SetString(kTimestamp,
        base::Int64ToString(position.timestamp.ToInternalValue()));
    local_state_->Set(prefs::kDeviceLocation, location);
  }

  ScheduleGeolocationUpdateRequest();
}

void DeviceStatusCollector::ReceiveVolumeInfo(
    const std::vector<em::VolumeInfo>& info) {
  if (report_hardware_status_)
    volume_info_ = info;
}

}  // namespace policy
