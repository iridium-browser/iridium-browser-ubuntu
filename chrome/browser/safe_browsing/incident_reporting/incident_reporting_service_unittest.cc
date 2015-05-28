// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader.h"
#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"
#include "chrome/browser/safe_browsing/incident_reporting/tracked_preference_incident.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test fixture that sets up a test task runner and makes it the thread's
// runner. The fixture implements a fake envrionment data collector and a fake
// report uploader.
class IncidentReportingServiceTest : public testing::Test {
 protected:
  // An IRS class that allows a test harness to provide a fake environment
  // collector and report uploader via callbacks.
  class TestIncidentReportingService
      : public safe_browsing::IncidentReportingService {
   public:
    typedef base::Callback<void(Profile*)> PreProfileAddCallback;

    typedef base::Callback<
        void(safe_browsing::ClientIncidentReport_EnvironmentData*)>
        CollectEnvironmentCallback;

    typedef base::Callback<scoped_ptr<safe_browsing::LastDownloadFinder>(
        const safe_browsing::LastDownloadFinder::LastDownloadCallback&
            callback)> CreateDownloadFinderCallback;

    typedef base::Callback<scoped_ptr<safe_browsing::IncidentReportUploader>(
        const safe_browsing::IncidentReportUploader::OnResultCallback&,
        const safe_browsing::ClientIncidentReport& report)> StartUploadCallback;

    TestIncidentReportingService(
        const scoped_refptr<base::TaskRunner>& task_runner,
        const PreProfileAddCallback& pre_profile_add_callback,
        const CollectEnvironmentCallback& collect_environment_callback,
        const CreateDownloadFinderCallback& create_download_finder_callback,
        const StartUploadCallback& start_upload_callback)
        : IncidentReportingService(NULL,
                                   NULL,
                                   base::TimeDelta::FromMilliseconds(5),
                                   task_runner),
          pre_profile_add_callback_(pre_profile_add_callback),
          collect_environment_callback_(collect_environment_callback),
          create_download_finder_callback_(create_download_finder_callback),
          start_upload_callback_(start_upload_callback) {
      SetCollectEnvironmentHook(&CollectEnvironmentData, task_runner);
      test_instance_.Get().Set(this);
    }

    ~TestIncidentReportingService() override { test_instance_.Get().Set(NULL); }

    bool IsProcessingReport() const {
      return IncidentReportingService::IsProcessingReport();
    }

   protected:
    void OnProfileAdded(Profile* profile) override {
      pre_profile_add_callback_.Run(profile);
      safe_browsing::IncidentReportingService::OnProfileAdded(profile);
    }

    scoped_ptr<safe_browsing::LastDownloadFinder> CreateDownloadFinder(
        const safe_browsing::LastDownloadFinder::LastDownloadCallback& callback)
        override {
      return create_download_finder_callback_.Run(callback);
    }

    scoped_ptr<safe_browsing::IncidentReportUploader> StartReportUpload(
        const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
        const scoped_refptr<net::URLRequestContextGetter>&
            request_context_getter,
        const safe_browsing::ClientIncidentReport& report) override {
      return start_upload_callback_.Run(callback, report);
    }

   private:
    static TestIncidentReportingService& current() {
      return *test_instance_.Get().Get();
    }

    static void CollectEnvironmentData(
        safe_browsing::ClientIncidentReport_EnvironmentData* data) {
      current().collect_environment_callback_.Run(data);
    };

    static base::LazyInstance<base::ThreadLocalPointer<
        TestIncidentReportingService> >::Leaky test_instance_;

    PreProfileAddCallback pre_profile_add_callback_;
    CollectEnvironmentCallback collect_environment_callback_;
    CreateDownloadFinderCallback create_download_finder_callback_;
    StartUploadCallback start_upload_callback_;
  };

  // A type for specifying whether or not a profile created by CreateProfile
  // participates in safe browsing.
  enum SafeBrowsingDisposition {
    SAFE_BROWSING_OPT_OUT,
    SAFE_BROWSING_OPT_IN,
  };

  // A type for specifying the action to be taken by the test fixture during
  // profile initialization (before NOTIFICATION_PROFILE_ADDED is sent).
  enum OnProfileAdditionAction {
    ON_PROFILE_ADDITION_NO_ACTION,
    ON_PROFILE_ADDITION_ADD_INCIDENT,  // Add an incident to the service.
    ON_PROFILE_ADDITION_ADD_TWO_INCIDENTS,  // Add two incidents to the service.
  };

  // A type for specifying the action to be taken by the test fixture when the
  // service creates a LastDownloadFinder.
  enum OnCreateDownloadFinderAction {
    // Post a task that reports a download.
    ON_CREATE_DOWNLOAD_FINDER_DOWNLOAD_FOUND,
    // Post a task that reports no downloads found.
    ON_CREATE_DOWNLOAD_FINDER_NO_DOWNLOADS,
    // Immediately return due to a lack of eligible profiles.
    ON_CREATE_DOWNLOAD_FINDER_NO_PROFILES,
  };

  // A type for specifying the action to be taken by the test fixture when its
  // delayed analysis callback is run.
  enum OnDelayedAnalysisAction {
    ON_DELAYED_ANALYSIS_NO_ACTION,
    ON_DELAYED_ANALYSIS_ADD_INCIDENT,  // Add an incident to the service.
  };

  static const char kFakeOsName[];
  static const char kFakeDownloadToken[];
  static const char kTestTrackedPrefPath[];

  IncidentReportingServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        thread_task_runner_handle_(task_runner_),
        profile_manager_(TestingBrowserProcess::GetGlobal()),
        instance_(new TestIncidentReportingService(
            task_runner_,
            base::Bind(&IncidentReportingServiceTest::PreProfileAdd,
                       base::Unretained(this)),
            base::Bind(&IncidentReportingServiceTest::CollectEnvironmentData,
                       base::Unretained(this)),
            base::Bind(&IncidentReportingServiceTest::CreateDownloadFinder,
                       base::Unretained(this)),
            base::Bind(&IncidentReportingServiceTest::StartUpload,
                       base::Unretained(this)))),
        on_create_download_finder_action_(
            ON_CREATE_DOWNLOAD_FINDER_DOWNLOAD_FOUND),
        on_delayed_analysis_action_(ON_DELAYED_ANALYSIS_NO_ACTION),
        upload_result_(safe_browsing::IncidentReportUploader::UPLOAD_SUCCESS),
        environment_collected_(),
        download_finder_created_(),
        download_finder_destroyed_(),
        uploader_destroyed_(),
        delayed_analysis_ran_() {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  // Sets the action to be taken by the test fixture when the service creates a
  // LastDownloadFinder.
  void SetCreateDownloadFinderAction(OnCreateDownloadFinderAction action) {
    on_create_download_finder_action_ = action;
  }

  // Creates and returns a profile (owned by the profile manager) with or
  // without safe browsing enabled. An incident will be created within
  // PreProfileAdd if requested. |incidents_sent|, if provided, will be set
  // in the profile's preference.
  TestingProfile* CreateProfile(const std::string& profile_name,
                                SafeBrowsingDisposition safe_browsing_opt_in,
                                OnProfileAdditionAction on_addition_action,
                                scoped_ptr<base::Value> incidents_sent) {
    // Create prefs for the profile with safe browsing enabled or not.
    scoped_ptr<TestingPrefServiceSyncable> prefs(
        new TestingPrefServiceSyncable);
    chrome::RegisterUserProfilePrefs(prefs->registry());
    prefs->SetBoolean(prefs::kSafeBrowsingEnabled,
                      safe_browsing_opt_in == SAFE_BROWSING_OPT_IN);
    if (incidents_sent)
      prefs->Set(prefs::kSafeBrowsingIncidentsSent, *incidents_sent);

    // Remember whether or not to create an incident.
    profile_properties_[profile_name].on_addition_action = on_addition_action;

    // Boom (or fizzle).
    return profile_manager_.CreateTestingProfile(
        profile_name,
        prefs.Pass(),
        base::ASCIIToUTF16(profile_name),
        0,              // avatar_id (unused)
        std::string(),  // supervised_user_id (unused)
        TestingProfile::TestingFactories());
  }

  // Configures a callback to run when the next upload is started that will post
  // a task to delete the profile. This task will run before the upload
  // finishes.
  void DeleteProfileOnUpload(Profile* profile) {
    ASSERT_TRUE(on_start_upload_callback_.is_null());
    on_start_upload_callback_ =
        base::Bind(&IncidentReportingServiceTest::DelayedDeleteProfile,
                   base::Unretained(this),
                   profile);
  }

  // Returns an incident suitable for testing.
  scoped_ptr<safe_browsing::Incident> MakeTestIncident(const char* value) {
    scoped_ptr<safe_browsing::
                   ClientIncidentReport_IncidentData_TrackedPreferenceIncident>
        incident(
            new safe_browsing::
                ClientIncidentReport_IncidentData_TrackedPreferenceIncident());
    incident->set_path(kTestTrackedPrefPath);
    if (value)
      incident->set_atomic_value(value);
    return make_scoped_ptr(
        new safe_browsing::TrackedPreferenceIncident(incident.Pass(),
                                                     false /* is_personal */));
  }

  // Adds a test incident to the service.
  void AddTestIncident(Profile* profile) {
    scoped_ptr<safe_browsing::IncidentReceiver> receiver(
        instance_->GetIncidentReceiver());
    if (profile)
      receiver->AddIncidentForProfile(profile, MakeTestIncident(nullptr));
    else
      receiver->AddIncidentForProcess(MakeTestIncident(nullptr));
  }

  // Registers the callback to be run for delayed analysis.
  void RegisterAnalysis(OnDelayedAnalysisAction on_delayed_analysis_action) {
    on_delayed_analysis_action_ = on_delayed_analysis_action;
    instance_->RegisterDelayedAnalysisCallback(
        base::Bind(&IncidentReportingServiceTest::OnDelayedAnalysis,
                   base::Unretained(this)));
  }

  // Confirms that the test incident(s) was/were uploaded by the service, then
  // clears the instance for subsequent incidents.
  void ExpectTestIncidentUploaded(int incident_count) {
    ASSERT_TRUE(uploaded_report_);
    ASSERT_EQ(incident_count, uploaded_report_->incident_size());
    for (int i = 0; i < incident_count; ++i) {
      ASSERT_TRUE(uploaded_report_->incident(i).has_incident_time_msec());
      ASSERT_NE(0LL, uploaded_report_->incident(i).incident_time_msec());
      ASSERT_TRUE(uploaded_report_->incident(i).has_tracked_preference());
      ASSERT_TRUE(
          uploaded_report_->incident(i).tracked_preference().has_path());
      ASSERT_EQ(std::string(kTestTrackedPrefPath),
                uploaded_report_->incident(i).tracked_preference().path());
    }
    ASSERT_TRUE(uploaded_report_->has_environment());
    ASSERT_TRUE(uploaded_report_->environment().has_os());
    ASSERT_TRUE(uploaded_report_->environment().os().has_os_name());
    ASSERT_EQ(std::string(kFakeOsName),
              uploaded_report_->environment().os().os_name());
    ASSERT_EQ(std::string(kFakeDownloadToken),
              uploaded_report_->download().token());

    uploaded_report_.reset();
  }

  void AssertNoUpload() { ASSERT_FALSE(uploaded_report_); }

  bool HasCollectedEnvironmentData() const { return environment_collected_; }
  bool HasCreatedDownloadFinder() const { return download_finder_created_; }
  bool DownloadFinderDestroyed() const { return download_finder_destroyed_; }
  bool UploaderDestroyed() const { return uploader_destroyed_; }
  bool DelayedAnalysisRan() const { return delayed_analysis_ran_; }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;
  TestingProfileManager profile_manager_;
  scoped_ptr<TestIncidentReportingService> instance_;
  base::Closure on_start_upload_callback_;
  OnCreateDownloadFinderAction on_create_download_finder_action_;
  OnDelayedAnalysisAction on_delayed_analysis_action_;
  safe_browsing::IncidentReportUploader::Result upload_result_;
  bool environment_collected_;
  bool download_finder_created_;
  scoped_ptr<safe_browsing::ClientIncidentReport> uploaded_report_;
  bool download_finder_destroyed_;
  bool uploader_destroyed_;
  bool delayed_analysis_ran_;

 private:
  // A fake IncidentReportUploader that posts a task to provide a given response
  // back to the incident reporting service. It also reports back to the test
  // harness via a closure when it is deleted by the incident reporting service.
  class FakeUploader : public safe_browsing::IncidentReportUploader {
   public:
    FakeUploader(
        const base::Closure& on_deleted,
        const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
        safe_browsing::IncidentReportUploader::Result result)
        : safe_browsing::IncidentReportUploader(callback),
          on_deleted_(on_deleted),
          result_(result) {
      // Post a task that will provide the response.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&FakeUploader::FinishUpload, base::Unretained(this)));
    }
    ~FakeUploader() override { on_deleted_.Run(); }

   private:
    void FinishUpload() {
      // Callbacks have a tendency to delete the uploader, so no touching
      // anything after this.
      callback_.Run(result_,
                    scoped_ptr<safe_browsing::ClientIncidentResponse>());
    }

    base::Closure on_deleted_;
    safe_browsing::IncidentReportUploader::Result result_;

    DISALLOW_COPY_AND_ASSIGN(FakeUploader);
  };

  class FakeDownloadFinder : public safe_browsing::LastDownloadFinder {
   public:
    static scoped_ptr<safe_browsing::LastDownloadFinder> Create(
        const base::Closure& on_deleted,
        scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails>
            download,
        const safe_browsing::LastDownloadFinder::LastDownloadCallback&
            callback) {
      // Post a task to run the callback.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(callback, base::Passed(&download)));
      return scoped_ptr<safe_browsing::LastDownloadFinder>(
          new FakeDownloadFinder(on_deleted));
    }

    ~FakeDownloadFinder() override { on_deleted_.Run(); }

   private:
    explicit FakeDownloadFinder(const base::Closure& on_deleted)
        : on_deleted_(on_deleted) {}

    base::Closure on_deleted_;

    DISALLOW_COPY_AND_ASSIGN(FakeDownloadFinder);
  };

  // Properties for a profile that impact the behavior of the test.
  struct ProfileProperties {
    ProfileProperties() : on_addition_action(ON_PROFILE_ADDITION_NO_ACTION) {}

    // The action taken by the test fixture during profile initialization
    // (before NOTIFICATION_PROFILE_ADDED is sent).
    OnProfileAdditionAction on_addition_action;
  };

  // Posts a task to delete the profile.
  void DelayedDeleteProfile(Profile* profile) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfileManager::DeleteTestingProfile,
                   base::Unretained(&profile_manager_),
                   profile->GetProfileUserName()));
  }

  // A callback run by the test fixture when a profile is added. An incident
  // is added.
  void PreProfileAdd(Profile* profile) {
    // The instance must have already been created.
    ASSERT_TRUE(instance_);
    // Add a test incident to the service if requested.
    OnProfileAdditionAction action =
        profile_properties_[profile->GetProfileUserName()].on_addition_action;
    switch (action) {
      case ON_PROFILE_ADDITION_ADD_INCIDENT:
        AddTestIncident(profile);
        break;
      case ON_PROFILE_ADDITION_ADD_TWO_INCIDENTS:
        AddTestIncident(profile);
        AddTestIncident(profile);
        break;
      default:
        ASSERT_EQ(ON_PROFILE_ADDITION_NO_ACTION, action);
        break;
    }
  }

  // A fake CollectEnvironmentData implementation invoked by the service during
  // operation.
  void CollectEnvironmentData(
      safe_browsing::ClientIncidentReport_EnvironmentData* data) {
    ASSERT_NE(
        static_cast<safe_browsing::ClientIncidentReport_EnvironmentData*>(NULL),
        data);
    data->mutable_os()->set_os_name(kFakeOsName);
    environment_collected_ = true;
  }

  // A fake CreateDownloadFinder implementation invoked by the service during
  // operation.
  scoped_ptr<safe_browsing::LastDownloadFinder> CreateDownloadFinder(
      const safe_browsing::LastDownloadFinder::LastDownloadCallback& callback) {
    download_finder_created_ = true;
    scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails> download;
    if (on_create_download_finder_action_ ==
        ON_CREATE_DOWNLOAD_FINDER_NO_PROFILES) {
      return scoped_ptr<safe_browsing::LastDownloadFinder>();
    }
    if (on_create_download_finder_action_ ==
        ON_CREATE_DOWNLOAD_FINDER_DOWNLOAD_FOUND) {
      download.reset(new safe_browsing::ClientIncidentReport_DownloadDetails);
      download->set_token(kFakeDownloadToken);
    }
    return scoped_ptr<safe_browsing::LastDownloadFinder>(
        FakeDownloadFinder::Create(
            base::Bind(&IncidentReportingServiceTest::OnDownloadFinderDestroyed,
                       base::Unretained(this)),
            download.Pass(),
            callback));
  }

  // A fake StartUpload implementation invoked by the service during operation.
  scoped_ptr<safe_browsing::IncidentReportUploader> StartUpload(
      const safe_browsing::IncidentReportUploader::OnResultCallback& callback,
      const safe_browsing::ClientIncidentReport& report) {
    // Remember the report that is being uploaded.
    uploaded_report_.reset(new safe_browsing::ClientIncidentReport(report));
    // Run and clear the OnStartUpload callback, if provided.
    if (!on_start_upload_callback_.is_null()) {
      on_start_upload_callback_.Run();
      on_start_upload_callback_ = base::Closure();
    }
    return make_scoped_ptr(new FakeUploader(
        base::Bind(&IncidentReportingServiceTest::OnUploaderDestroyed,
                   base::Unretained(this)),
        callback, upload_result_));
  }

  void OnDownloadFinderDestroyed() { download_finder_destroyed_ = true; }
  void OnUploaderDestroyed() { uploader_destroyed_ = true; }

  void OnDelayedAnalysis(scoped_ptr<safe_browsing::IncidentReceiver> receiver) {
    delayed_analysis_ran_ = true;
    if (on_delayed_analysis_action_ == ON_DELAYED_ANALYSIS_ADD_INCIDENT)
      receiver->AddIncidentForProcess(MakeTestIncident(nullptr));
  }

  // A mapping of profile name to its corresponding properties.
  std::map<std::string, ProfileProperties> profile_properties_;
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    IncidentReportingServiceTest::TestIncidentReportingService> >::Leaky
    IncidentReportingServiceTest::TestIncidentReportingService::test_instance_ =
        LAZY_INSTANCE_INITIALIZER;

const char IncidentReportingServiceTest::kFakeOsName[] = "fakedows";
const char IncidentReportingServiceTest::kFakeDownloadToken[] = "fakedlt";
const char IncidentReportingServiceTest::kTestTrackedPrefPath[] = "some_pref";

// Tests that an incident added during profile initialization when safe browsing
// is on is uploaded.
TEST_F(IncidentReportingServiceTest, AddIncident) {
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that environment collection took place.
  EXPECT_TRUE(HasCollectedEnvironmentData());

  // Verify that the most recent download was looked for.
  EXPECT_TRUE(HasCreatedDownloadFinder());

  // Verify that report upload took place and contained the incident,
  // environment data, and download details.
  ExpectTestIncidentUploaded(1);

  // Verify that the download finder and the uploader were destroyed.
  ASSERT_TRUE(DownloadFinderDestroyed());
  ASSERT_TRUE(UploaderDestroyed());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that multiple incidents are coalesced into the same report.
TEST_F(IncidentReportingServiceTest, CoalesceIncidents) {
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_TWO_INCIDENTS, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that environment collection took place.
  EXPECT_TRUE(HasCollectedEnvironmentData());

  // Verify that the most recent download was looked for.
  EXPECT_TRUE(HasCreatedDownloadFinder());

  // Verify that report upload took place and contained the incident,
  // environment data, and download details.
  ExpectTestIncidentUploaded(2);

  // Verify that the download finder and the uploader were destroyed.
  ASSERT_TRUE(DownloadFinderDestroyed());
  ASSERT_TRUE(UploaderDestroyed());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that an incident added during profile initialization when safe browsing
// is off is not uploaded.
TEST_F(IncidentReportingServiceTest, NoSafeBrowsing) {
  // Create the profile, thereby causing the test to begin.
  CreateProfile("profile1", SAFE_BROWSING_OPT_OUT,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that no report upload took place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that no incident report is uploaded if there is no recent download.
TEST_F(IncidentReportingServiceTest, NoDownloadNoUpload) {
  // Tell the fixture to return no downloads found.
  SetCreateDownloadFinderAction(ON_CREATE_DOWNLOAD_FINDER_NO_DOWNLOADS);

  // Create the profile, thereby causing the test to begin.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that the download finder was run but that no report upload took
  // place.
  EXPECT_TRUE(HasCreatedDownloadFinder());
  AssertNoUpload();
  EXPECT_TRUE(DownloadFinderDestroyed());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that no incident report is uploaded if there is no recent download.
TEST_F(IncidentReportingServiceTest, NoProfilesNoUpload) {
  // Tell the fixture to pretend there are no profiles eligible for finding
  // downloads.
  SetCreateDownloadFinderAction(ON_CREATE_DOWNLOAD_FINDER_NO_PROFILES);

  // Create the profile, thereby causing the test to begin.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that the download finder was run but that no report upload took
  // place.
  EXPECT_TRUE(HasCreatedDownloadFinder());
  AssertNoUpload();
  // Although CreateDownloadFinder was called, no instance was returned so there
  // is nothing to have been destroyed.
  EXPECT_FALSE(DownloadFinderDestroyed());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that an identical incident added after upload is not uploaded again.
TEST_F(IncidentReportingServiceTest, OneIncidentOneUpload) {
  // Create the profile, thereby causing the test to begin.
  Profile* profile = CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                                   ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that report upload took place and contained the incident and
  // environment data.
  ExpectTestIncidentUploaded(1);

  // Add the incident to the service again.
  AddTestIncident(profile);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that no additional report upload took place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that two incidents of the same type with different payloads lead to two
// uploads.
TEST_F(IncidentReportingServiceTest, TwoIncidentsTwoUploads) {
  // Create the profile, thereby causing the test to begin.
  Profile* profile = CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                                   ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that report upload took place and contained the incident and
  // environment data.
  ExpectTestIncidentUploaded(1);

  // Add a variation on the incident to the service.
  instance_->GetIncidentReceiver()->AddIncidentForProfile(
      profile, MakeTestIncident("leeches"));

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that an additional report upload took place.
  ExpectTestIncidentUploaded(1);

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that the same incident added for two different profiles in sequence
// results in two uploads.
TEST_F(IncidentReportingServiceTest, TwoProfilesTwoUploads) {
  // Create the profile, thereby causing the test to begin.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that report upload took place and contained the incident and
  // environment data.
  ExpectTestIncidentUploaded(1);

  // Create a second profile with its own incident on addition.
  CreateProfile("profile2", SAFE_BROWSING_OPT_IN,
                ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that a second report upload took place.
  ExpectTestIncidentUploaded(1);

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that an upload succeeds if the profile is destroyed while it is
// pending.
TEST_F(IncidentReportingServiceTest, ProfileDestroyedDuringUpload) {
  // Create a profile for which an incident will be added.
  Profile* profile = CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                                   ON_PROFILE_ADDITION_ADD_INCIDENT, nullptr);

  // Hook up a callback to run when the upload is started that will post a task
  // to delete the profile. This task will run before the upload finishes.
  DeleteProfileOnUpload(profile);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that report upload took place and contained the incident and
  // environment data.
  ExpectTestIncidentUploaded(1);

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());

  // The lack of a crash indicates that the deleted profile was not accessed by
  // the service while handling the upload response.
}

// Tests that no upload results from adding an incident that is not affiliated
// with a profile.
TEST_F(IncidentReportingServiceTest, ProcessWideNoProfileNoUpload) {
  // Add the test incident.
  AddTestIncident(NULL);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // No upload should have taken place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that there is an upload when a profile is present for a proc-wide
// incident and that pruning works.
TEST_F(IncidentReportingServiceTest, ProcessWideOneUpload) {
  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Add the test incident.
  AddTestIncident(NULL);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // An upload should have taken place.
  ExpectTestIncidentUploaded(1);

  // Add the incident to the service again.
  AddTestIncident(NULL);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that no additional report upload took place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that two process-wide incidents of the same type with different
// payloads added via the same callback lead to two uploads.
TEST_F(IncidentReportingServiceTest, ProcessWideTwoUploads) {
  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Add the test incident.
  scoped_ptr<safe_browsing::IncidentReceiver> receiver(
      instance_->GetIncidentReceiver());
  receiver->AddIncidentForProcess(MakeTestIncident(nullptr));

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // An upload should have taken place.
  ExpectTestIncidentUploaded(1);

  // Add a variation on the incident to the service.
  receiver->AddIncidentForProcess(MakeTestIncident("leeches"));

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that an additional report upload took place.
  ExpectTestIncidentUploaded(1);

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that there is an upload when a profile appears after a proc-wide
// incident.
TEST_F(IncidentReportingServiceTest, ProcessWideOneUploadAfterProfile) {
  // Add the test incident.
  AddTestIncident(NULL);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that no report upload took place.
  AssertNoUpload();

  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // An upload should have taken place.
  ExpectTestIncidentUploaded(1);

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

TEST_F(IncidentReportingServiceTest, NoCollectionWithoutIncident) {
  // Register a callback.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_NO_ACTION);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Confirm that the callback was not run.
  ASSERT_FALSE(DelayedAnalysisRan());

  // No collection should have taken place.
  ASSERT_FALSE(HasCollectedEnvironmentData());

  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Confirm that the callback was run.
  ASSERT_TRUE(DelayedAnalysisRan());

  // Still no collection should have taken place.
  ASSERT_FALSE(HasCollectedEnvironmentData());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that delayed analysis callbacks are called following the addition of a
// profile that participates in safe browsing.
TEST_F(IncidentReportingServiceTest, AnalysisAfterProfile) {
  // Register a callback.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_NO_ACTION);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Not run yet.
  ASSERT_FALSE(DelayedAnalysisRan());

  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // And now they have.
  ASSERT_TRUE(DelayedAnalysisRan());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that delayed analysis callbacks are called following their registration
// when a profile that participates in safe browsing is already present.
TEST_F(IncidentReportingServiceTest, AnalysisWhenRegisteredWithProfile) {
  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Register a callback.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_NO_ACTION);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Confirm that the callbacks were run.
  ASSERT_TRUE(DelayedAnalysisRan());

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that no upload results from a delayed analysis incident when no
// safe browsing profile is present.
TEST_F(IncidentReportingServiceTest, DelayedAnalysisNoProfileNoUpload) {
  // Register a callback that will add an incident.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_ADD_INCIDENT);

  // Add a profile that does not participate in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_OUT,
                ON_PROFILE_ADDITION_NO_ACTION, nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // The callback should not have been run.
  ASSERT_FALSE(DelayedAnalysisRan());

  // No upload should have taken place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that there is an upload when a profile is present for a delayed
// analysis incident and that pruning works.
TEST_F(IncidentReportingServiceTest, DelayedAnalysisOneUpload) {
  // Register a callback that will add an incident.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_ADD_INCIDENT);

  // Add a profile that participates in safe browsing.
  CreateProfile("profile1", SAFE_BROWSING_OPT_IN, ON_PROFILE_ADDITION_NO_ACTION,
                nullptr);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // The callback should have been run.
  ASSERT_TRUE(DelayedAnalysisRan());

  // An upload should have taken place.
  ExpectTestIncidentUploaded(1);

  // Add the incident to the service again.
  AddTestIncident(NULL);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that no additional report upload took place.
  AssertNoUpload();

  // Ensure that no report processing remains.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Tests that the service stops processing when no download is found.
TEST_F(IncidentReportingServiceTest, NoDownloadNoWaiting) {
  // Tell the fixture to return no downloads found.
  SetCreateDownloadFinderAction(ON_CREATE_DOWNLOAD_FINDER_NO_DOWNLOADS);

  // Register a callback.
  RegisterAnalysis(ON_DELAYED_ANALYSIS_NO_ACTION);

  // Add a profile that participates in safe browsing.
  Profile* profile = CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                                   ON_PROFILE_ADDITION_NO_ACTION, nullptr);

  // Add an incident.
  AddTestIncident(profile);

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  // Verify that the download finder was run but that no report upload took
  // place.
  EXPECT_TRUE(HasCreatedDownloadFinder());
  AssertNoUpload();
  EXPECT_TRUE(DownloadFinderDestroyed());

  // Ensure that the report is dropped.
  ASSERT_FALSE(instance_->IsProcessingReport());
}

// Test that a profile's prune state is properly cleaned upon load.
TEST_F(IncidentReportingServiceTest, CleanLegacyPruneState) {
  const std::string omnibox_type(base::IntToString(
      static_cast<int32_t>(safe_browsing::IncidentType::OMNIBOX_INTERACTION)));
  const std::string preference_type(base::IntToString(
      static_cast<int32_t>(safe_browsing::IncidentType::TRACKED_PREFERENCE)));

  // Set up a prune state dict with data to be cleared (and not).
  scoped_ptr<base::DictionaryValue> incidents_sent(new base::DictionaryValue());
  base::DictionaryValue* type_dict = new base::DictionaryValue();
  type_dict->SetStringWithoutPathExpansion("foo", "47");
  incidents_sent->SetWithoutPathExpansion(omnibox_type, type_dict);
  type_dict = new base::DictionaryValue();
  type_dict->SetStringWithoutPathExpansion("bar", "43");
  incidents_sent->SetWithoutPathExpansion(preference_type, type_dict);

  // Add a profile.
  Profile* profile =
      CreateProfile("profile1", SAFE_BROWSING_OPT_IN,
                    ON_PROFILE_ADDITION_NO_ACTION, incidents_sent.Pass());

  // Let all tasks run.
  task_runner_->RunUntilIdle();

  const base::DictionaryValue* new_state =
      profile->GetPrefs()->GetDictionary(prefs::kSafeBrowsingIncidentsSent);
  // The legacy value must be gone.
  ASSERT_FALSE(new_state->HasKey(omnibox_type));
  // But other data must be untouched.
  ASSERT_TRUE(new_state->HasKey(preference_type));
}

// Parallel uploads
// Shutdown during processing
// environment colection taking longer than incident delay timer
// environment colection taking longer than incident delay timer, and then
// another incident arriving
