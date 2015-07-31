// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/abstract_profile_sync_service_test.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_profile_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autocomplete_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata_services/web_data_service_test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutocompleteSyncableService;
using autofill::AutofillChange;
using autofill::AutofillChangeList;
using autofill::AutofillEntry;
using autofill::ServerFieldType;
using autofill::AutofillKey;
using autofill::AutofillProfile;
using autofill::AutofillProfileChange;
using autofill::AutofillProfileSyncableService;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::PersonalDataManager;
using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using browser_sync::AutofillDataTypeController;
using browser_sync::AutofillProfileDataTypeController;
using content::BrowserThread;
using syncer::AUTOFILL;
using syncer::AUTOFILL_PROFILE;
using syncer::BaseNode;
using syncer::syncable::BASE_VERSION;
using syncer::syncable::CREATE;
using syncer::syncable::GET_TYPE_ROOT;
using syncer::syncable::MutableEntry;
using syncer::syncable::SERVER_SPECIFICS;
using syncer::syncable::SPECIFICS;
using syncer::syncable::UNITTEST;
using syncer::syncable::WriterTag;
using syncer::syncable::WriteTransaction;
using sync_driver::DataTypeController;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::Not;
using testing::SetArgumentPointee;
using testing::Return;

namespace syncable {
class Id;
}

namespace {

const char kTestProfileName[] = "test-profile";

void RunAndSignal(const base::Closure& cb, WaitableEvent* event) {
  cb.Run();
  event->Signal();
}

}  // namespace

class AutofillTableMock : public AutofillTable {
 public:
  AutofillTableMock() : AutofillTable("en-US") {}
  MOCK_METHOD2(RemoveFormElement,
               bool(const base::string16& name,
                    const base::string16& value));  // NOLINT
  MOCK_METHOD1(GetAllAutofillEntries,
               bool(std::vector<AutofillEntry>* entries));  // NOLINT
  MOCK_METHOD4(GetAutofillTimestamps,
               bool(const base::string16& name,  // NOLINT
                    const base::string16& value,
                    base::Time* date_created,
                    base::Time* date_last_used));
  MOCK_METHOD1(UpdateAutofillEntries,
               bool(const std::vector<AutofillEntry>&));  // NOLINT
  MOCK_METHOD1(GetAutofillProfiles,
               bool(std::vector<AutofillProfile*>*));  // NOLINT
  MOCK_METHOD1(UpdateAutofillProfile,
               bool(const AutofillProfile&));  // NOLINT
  MOCK_METHOD1(AddAutofillProfile,
               bool(const AutofillProfile&));  // NOLINT
  MOCK_METHOD1(RemoveAutofillProfile,
               bool(const std::string&));  // NOLINT
};

MATCHER_P(MatchProfiles, profile, "") {
  return (profile.Compare(arg) == 0);
}

class WebDatabaseFake : public WebDatabase {
 public:
  explicit WebDatabaseFake(AutofillTable* autofill_table) {
    AddTable(autofill_table);
  }
};

class MockAutofillBackend : public autofill::AutofillWebDataBackend {
 public:
  MockAutofillBackend(
      WebDatabase* web_database,
      const base::Closure& on_changed)
      : web_database_(web_database),
        on_changed_(on_changed) {
  }

  ~MockAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return web_database_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBThread* observer) override {}
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {
    DCHECK_CURRENTLY_ON(BrowserThread::DB);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, on_changed_);
  }

 private:
  WebDatabase* web_database_;
  base::Closure on_changed_;
};

class ProfileSyncServiceAutofillTest;

template<class AutofillProfile>
syncer::ModelType GetModelType() {
  return syncer::UNSPECIFIED;
}

template<>
syncer::ModelType GetModelType<AutofillEntry>() {
  return syncer::AUTOFILL;
}

template<>
syncer::ModelType GetModelType<AutofillProfile>() {
  return syncer::AUTOFILL_PROFILE;
}

class TokenWebDataServiceFake : public TokenWebData {
 public:
  TokenWebDataServiceFake()
      : TokenWebData(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)) {
  }

  bool IsDatabaseLoaded() override { return true; }

  AutofillWebDataService::Handle GetAllTokens(
      WebDataServiceConsumer* consumer) override {
    // TODO(tim): It would be nice if WebDataService was injected on
    // construction of ProfileOAuth2TokenService rather than fetched by
    // Initialize so that this isn't necessary (we could pass a NULL service).
    // We currently do return it via EXPECT_CALLs, but without depending on
    // order-of-initialization (which seems way more fragile) we can't tell
    // which component is asking at what time, and some components in these
    // Autofill tests require a WebDataService.
    return 0;
  }

 private:
  ~TokenWebDataServiceFake() override {}

  DISALLOW_COPY_AND_ASSIGN(TokenWebDataServiceFake);
};

class WebDataServiceFake : public AutofillWebDataService {
 public:
  WebDataServiceFake()
      : AutofillWebDataService(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB)),
        web_database_(NULL),
        autocomplete_syncable_service_(NULL),
        autofill_profile_syncable_service_(NULL),
        syncable_service_created_or_destroyed_(false, false) {
  }

  void SetDatabase(WebDatabase* web_database) {
    web_database_ = web_database;
  }

  void StartSyncableService() {
    // The |autofill_profile_syncable_service_| must be constructed on the DB
    // thread.
    const base::Closure& on_changed_callback = base::Bind(
        &WebDataServiceFake::NotifyAutofillMultipleChangedOnUIThread,
        AsWeakPtr());

    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&WebDataServiceFake::CreateSyncableService,
                   base::Unretained(this),
                   on_changed_callback));
    syncable_service_created_or_destroyed_.Wait();
  }

  void ShutdownSyncableService() {
    // The |autofill_profile_syncable_service_| must be destructed on the DB
    // thread.
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&WebDataServiceFake::DestroySyncableService,
                   base::Unretained(this)));
    syncable_service_created_or_destroyed_.Wait();
  }

  bool IsDatabaseLoaded() override { return true; }

  WebDatabase* GetDatabase() override { return web_database_; }

  void OnAutofillEntriesChanged(const AutofillChangeList& changes) {
    WaitableEvent event(true, false);

    base::Closure notify_cb =
        base::Bind(&AutocompleteSyncableService::AutofillEntriesChanged,
                   base::Unretained(autocomplete_syncable_service_),
                   changes);
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&RunAndSignal, notify_cb, &event));
    event.Wait();
  }

  void OnAutofillProfileChanged(const AutofillProfileChange& changes) {
    WaitableEvent event(true, false);

    base::Closure notify_cb =
        base::Bind(&AutocompleteSyncableService::AutofillProfileChanged,
                   base::Unretained(autofill_profile_syncable_service_),
                   changes);
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&RunAndSignal, notify_cb, &event));
    event.Wait();
  }

 private:
  ~WebDataServiceFake() override {}

  void CreateSyncableService(const base::Closure& on_changed_callback) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    // These services are deleted in DestroySyncableService().
    backend_.reset(new MockAutofillBackend(
        GetDatabase(), on_changed_callback));
    AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
        this, backend_.get());
    AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
        this, backend_.get(), "en-US");

    autocomplete_syncable_service_ =
        AutocompleteSyncableService::FromWebDataService(this);
    autofill_profile_syncable_service_ =
        AutofillProfileSyncableService::FromWebDataService(this);

    syncable_service_created_or_destroyed_.Signal();
  }

  void DestroySyncableService() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    autocomplete_syncable_service_ = NULL;
    autofill_profile_syncable_service_ = NULL;
    backend_.reset();
    syncable_service_created_or_destroyed_.Signal();
  }

  WebDatabase* web_database_;
  AutocompleteSyncableService* autocomplete_syncable_service_;
  AutofillProfileSyncableService* autofill_profile_syncable_service_;
  scoped_ptr<autofill::AutofillWebDataBackend> backend_;

  WaitableEvent syncable_service_created_or_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WebDataServiceFake);
};

KeyedService* BuildMockWebDataServiceWrapper(content::BrowserContext* profile) {
  return new MockWebDataServiceWrapper(
      new WebDataServiceFake(),
      new TokenWebDataServiceFake());
}

ACTION_P(MakeAutocompleteSyncComponents, wds) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!BrowserThread::CurrentlyOn(BrowserThread::DB))
    return base::WeakPtr<syncer::SyncableService>();
  return AutocompleteSyncableService::FromWebDataService(wds)->AsWeakPtr();
}

ACTION_P(ReturnNewDataTypeManagerWithDebugListener, debug_listener) {
  return new sync_driver::DataTypeManagerImpl(
      base::Closure(),
      debug_listener,
      arg1,
      arg2,
      arg3,
      arg4);
}

ACTION_P(MakeAutofillProfileSyncComponents, wds) {
  EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (!BrowserThread::CurrentlyOn(BrowserThread::DB))
    return base::WeakPtr<syncer::SyncableService>();
  return AutofillProfileSyncableService::FromWebDataService(wds)->AsWeakPtr();
}

class AbstractAutofillFactory {
 public:
  virtual DataTypeController* CreateDataTypeController(
      ProfileSyncComponentsFactory* factory,
      TestingProfile* profile,
      ProfileSyncService* service) = 0;
  virtual void SetExpectation(ProfileSyncComponentsFactoryMock* factory,
                              ProfileSyncService* service,
                              AutofillWebDataService* wds,
                              DataTypeController* dtc) = 0;
  virtual ~AbstractAutofillFactory() {}
};

class AutofillEntryFactory : public AbstractAutofillFactory {
 public:
  DataTypeController* CreateDataTypeController(
      ProfileSyncComponentsFactory* factory,
      TestingProfile* profile,
      ProfileSyncService* service) override {
    return new AutofillDataTypeController(factory, profile);
  }

  void SetExpectation(ProfileSyncComponentsFactoryMock* factory,
                      ProfileSyncService* service,
                      AutofillWebDataService* wds,
                      DataTypeController* dtc) override {
    EXPECT_CALL(*factory, GetSyncableServiceForType(syncer::AUTOFILL)).
        WillOnce(MakeAutocompleteSyncComponents(wds));
  }
};

class AutofillProfileFactory : public AbstractAutofillFactory {
 public:
  DataTypeController* CreateDataTypeController(
      ProfileSyncComponentsFactory* factory,
      TestingProfile* profile,
      ProfileSyncService* service) override {
    return new AutofillProfileDataTypeController(factory, profile);
  }

  void SetExpectation(ProfileSyncComponentsFactoryMock* factory,
                      ProfileSyncService* service,
                      AutofillWebDataService* wds,
                      DataTypeController* dtc) override {
    EXPECT_CALL(*factory,
        GetSyncableServiceForType(syncer::AUTOFILL_PROFILE)).
        WillOnce(MakeAutofillProfileSyncComponents(wds));
  }
};

class MockPersonalDataManager : public PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
  MOCK_METHOD0(LoadProfiles, void());
  MOCK_METHOD0(LoadCreditCards, void());
  MOCK_METHOD0(Refresh, void());

  static KeyedService* Build(content::BrowserContext* profile) {
    return new MockPersonalDataManager();
  }
};

template <class T> class AddAutofillHelper;

class ProfileSyncServiceAutofillTest
   : public AbstractProfileSyncServiceTest,
     public syncer::DataTypeDebugInfoListener {
 public:
  // DataTypeDebugInfoListener implementation.
  void OnDataTypeConfigureComplete(const std::vector<
      syncer::DataTypeConfigurationStats>& configuration_stats) override {
    ASSERT_EQ(1u, configuration_stats.size());
    association_stats_ = configuration_stats[0].association_stats;
  }

 protected:
  ProfileSyncServiceAutofillTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        debug_ptr_factory_(this) {
  }
  ~ProfileSyncServiceAutofillTest() override {}

  AutofillProfileFactory profile_factory_;
  AutofillEntryFactory entry_factory_;

  AbstractAutofillFactory* GetFactory(syncer::ModelType type) {
    if (type == syncer::AUTOFILL) {
      return &entry_factory_;
    } else if (type == syncer::AUTOFILL_PROFILE) {
      return &profile_factory_;
    } else {
      NOTREACHED();
      return NULL;
    }
  }

  void SetUp() override {
    AbstractProfileSyncServiceTest::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories;
    testing_factories.push_back(std::make_pair(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        BuildAutoIssuingFakeProfileOAuth2TokenService));
    profile_ = profile_manager_.CreateTestingProfile(
        kTestProfileName,
        scoped_ptr<PrefServiceSyncable>(),
        base::UTF8ToUTF16(kTestProfileName),
        0,
        std::string(),
        testing_factories);
    web_database_.reset(new WebDatabaseFake(&autofill_table_));
    MockWebDataServiceWrapper* wrapper =
        static_cast<MockWebDataServiceWrapper*>(
            WebDataServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile_, BuildMockWebDataServiceWrapper));
    web_data_service_ =
        static_cast<WebDataServiceFake*>(wrapper->GetAutofillWebData().get());
    web_data_service_->SetDatabase(web_database_.get());

    personal_data_manager_ = static_cast<MockPersonalDataManager*>(
        autofill::PersonalDataManagerFactory::GetInstance()
            ->SetTestingFactoryAndUse(profile_,
                                      MockPersonalDataManager::Build));

    EXPECT_CALL(*personal_data_manager_, LoadProfiles()).Times(1);
    EXPECT_CALL(*personal_data_manager_, LoadCreditCards()).Times(1);

    personal_data_manager_->Init(
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS),
        profile_->GetPrefs(),
        AccountTrackerServiceFactory::GetForProfile(profile_),
        profile_->IsOffTheRecord());

    web_data_service_->StartSyncableService();

    // When UpdateAutofillEntries() is called with an empty list, the return
    // value should be |true|, rather than the default of |false|.
    std::vector<AutofillEntry> empty;
    EXPECT_CALL(autofill_table_, UpdateAutofillEntries(empty))
        .WillRepeatedly(Return(true));
  }

  void TearDown() override {
    // Note: The tear down order is important.
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(profile_, NULL);
    web_data_service_->ShutdownOnUIThread();
    web_data_service_->ShutdownSyncableService();
    web_data_service_ = NULL;
    // To prevent a leak, fully release TestURLRequestContext to ensure its
    // destruction on the IO message loop.
    profile_ = NULL;
    profile_manager_.DeleteTestingProfile(kTestProfileName);
    AbstractProfileSyncServiceTest::TearDown();
  }

  int GetSyncCount(syncer::ModelType type) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode node(&trans);
    if (node.InitTypeRoot(type) != syncer::BaseNode::INIT_OK)
      return 0;
    return node.GetTotalNodeCount() - 1;
  }

  void StartSyncService(const base::Closure& callback,
                        bool will_fail_association,
                        syncer::ModelType type) {
    AbstractAutofillFactory* factory = GetFactory(type);
    SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile_);
    signin->SetAuthenticatedAccountInfo("12345", "test_user@gmail.com");
    sync_service_ = TestProfileSyncService::BuildAutoStartAsyncInit(profile_,
                                                                    callback);

    ProfileSyncComponentsFactoryMock* components =
        sync_service_->components_factory_mock();
    DataTypeController* data_type_controller =
        factory->CreateDataTypeController(components, profile_, sync_service_);
    factory->SetExpectation(components,
                            sync_service_,
                            web_data_service_.get(),
                            data_type_controller);

    EXPECT_CALL(*components, CreateDataTypeManager(_, _, _, _, _)).
        WillOnce(ReturnNewDataTypeManagerWithDebugListener(
                     syncer::MakeWeakHandle(debug_ptr_factory_.GetWeakPtr())));

    EXPECT_CALL(*personal_data_manager_, IsDataLoaded()).
        WillRepeatedly(Return(true));

    // We need tokens to get the tests going
    ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
        ->UpdateCredentials(signin->GetAuthenticatedAccountId(),
                            "oauth2_login_token");

    sync_service_->RegisterDataTypeController(data_type_controller);
    sync_service_->Initialize();
    base::MessageLoop::current()->Run();

    // It's possible this test triggered an unrecoverable error, in which case
    // we can't get the sync count.
    if (sync_service_->SyncActive()) {
      EXPECT_EQ(GetSyncCount(type),
                association_stats_.num_sync_items_after_association);
    }
    EXPECT_EQ(association_stats_.num_sync_items_after_association,
              association_stats_.num_sync_items_before_association +
              association_stats_.num_sync_items_added -
              association_stats_.num_sync_items_deleted);
  }

  bool AddAutofillSyncNode(const AutofillEntry& entry) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(syncer::AUTOFILL) != BaseNode::INIT_OK) {
      return false;
    }

    syncer::WriteNode node(&trans);
    std::string tag = AutocompleteSyncableService::KeyToTag(
        base::UTF16ToUTF8(entry.key().name()),
        base::UTF16ToUTF8(entry.key().value()));
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(syncer::AUTOFILL, autofill_root, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS)
      return false;

    sync_pb::EntitySpecifics specifics;
    AutocompleteSyncableService::WriteAutofillEntry(entry, &specifics);
    sync_pb::AutofillSpecifics* autofill_specifics =
        specifics.mutable_autofill();
    node.SetAutofillSpecifics(*autofill_specifics);
    return true;
  }

  bool AddAutofillSyncNode(const AutofillProfile& profile) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(AUTOFILL_PROFILE) != BaseNode::INIT_OK) {
      return false;
    }
    syncer::WriteNode node(&trans);
    std::string tag = profile.guid();
    syncer::WriteNode::InitUniqueByCreationResult result =
        node.InitUniqueByCreation(syncer::AUTOFILL_PROFILE,
                                  autofill_root, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS)
      return false;

    sync_pb::EntitySpecifics specifics;
    AutofillProfileSyncableService::WriteAutofillProfile(profile, &specifics);
    sync_pb::AutofillProfileSpecifics* profile_specifics =
        specifics.mutable_autofill_profile();
    node.SetAutofillProfileSpecifics(*profile_specifics);
    return true;
  }

  bool GetAutofillEntriesFromSyncDB(std::vector<AutofillEntry>* entries,
                                    std::vector<AutofillProfile>* profiles) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(syncer::AUTOFILL) != BaseNode::INIT_OK) {
      return false;
    }

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != BaseNode::INIT_OK)
        return false;

      const sync_pb::AutofillSpecifics& autofill(
          child_node.GetAutofillSpecifics());
      if (autofill.has_value()) {
        AutofillKey key(base::UTF8ToUTF16(autofill.name()),
                        base::UTF8ToUTF16(autofill.value()));
        std::vector<base::Time> timestamps;
        int timestamps_count = autofill.usage_timestamp_size();
        for (int i = 0; i < timestamps_count; ++i) {
          timestamps.push_back(Time::FromInternalValue(
              autofill.usage_timestamp(i)));
        }
        entries->push_back(
            AutofillEntry(key, timestamps.front(), timestamps.back()));
      } else if (autofill.has_profile()) {
        AutofillProfile p;
        p.set_guid(autofill.profile().guid());
        AutofillProfileSyncableService::OverwriteProfileWithServerData(
            autofill.profile(), &p, "en-US");
        profiles->push_back(p);
      }
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  bool GetAutofillProfilesFromSyncDBUnderProfileNode(
      std::vector<AutofillProfile>* profiles) {
    syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode autofill_root(&trans);
    if (autofill_root.InitTypeRoot(AUTOFILL_PROFILE) != BaseNode::INIT_OK) {
      return false;
    }

    int64 child_id = autofill_root.GetFirstChildId();
    while (child_id != syncer::kInvalidId) {
      syncer::ReadNode child_node(&trans);
      if (child_node.InitByIdLookup(child_id) != BaseNode::INIT_OK)
        return false;

      const sync_pb::AutofillProfileSpecifics& autofill(
          child_node.GetAutofillProfileSpecifics());
        AutofillProfile p;
        p.set_guid(autofill.guid());
        AutofillProfileSyncableService::OverwriteProfileWithServerData(
            autofill, &p, "en-US");
        profiles->push_back(p);
      child_id = child_node.GetSuccessorId();
    }
    return true;
  }

  void SetIdleChangeProcessorExpectations() {
    EXPECT_CALL(autofill_table_, RemoveFormElement(_, _)).Times(0);
    EXPECT_CALL(autofill_table_, GetAutofillTimestamps(_, _, _, _)).Times(0);

    // Only permit UpdateAutofillEntries() to be called with an empty list.
    std::vector<AutofillEntry> empty;
    EXPECT_CALL(autofill_table_, UpdateAutofillEntries(Not(empty))).Times(0);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         int time_shift0,
                                         int time_shift1) {
    // Time deep in the past would cause Autocomplete sync to discard the
    // entries.
    static Time base_time = Time::Now().LocalMidnight();

    base::Time date_created = base_time + TimeDelta::FromSeconds(time_shift0);
    base::Time date_last_used = date_created;
    if (time_shift1 >= 0)
      date_last_used = base_time + TimeDelta::FromSeconds(time_shift1);
    return AutofillEntry(
        AutofillKey(base::ASCIIToUTF16(name), base::ASCIIToUTF16(value)),
        date_created, date_last_used);
  }

  static AutofillEntry MakeAutofillEntry(const char* name,
                                         const char* value,
                                         int time_shift) {
    return MakeAutofillEntry(name, value, time_shift, -1);
  }

  friend class AddAutofillHelper<AutofillEntry>;
  friend class AddAutofillHelper<AutofillProfile>;
  friend class FakeServerUpdater;

  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  AutofillTableMock autofill_table_;
  scoped_ptr<WebDatabaseFake> web_database_;
  scoped_refptr<WebDataServiceFake> web_data_service_;
  MockPersonalDataManager* personal_data_manager_;
  syncer::DataTypeAssociationStats association_stats_;
  base::WeakPtrFactory<DataTypeDebugInfoListener> debug_ptr_factory_;
};

template <class T>
class AddAutofillHelper {
 public:
  AddAutofillHelper(ProfileSyncServiceAutofillTest* test,
                    const std::vector<T>& entries)
      : callback_(base::Bind(&AddAutofillHelper::AddAutofillCallback,
                             base::Unretained(this), test, entries)),
        success_(false) {
  }

  const base::Closure& callback() const { return callback_; }
  bool success() { return success_; }

 private:
  void AddAutofillCallback(ProfileSyncServiceAutofillTest* test,
                           const std::vector<T>& entries) {
    if (!test->CreateRoot(GetModelType<T>()))
      return;

    for (size_t i = 0; i < entries.size(); ++i) {
      if (!test->AddAutofillSyncNode(entries[i]))
        return;
    }
    success_ = true;
  }

  base::Closure callback_;
  bool success_;
};

// Overload write transaction to use custom NotifyTransactionComplete
class WriteTransactionTest: public WriteTransaction {
 public:
  WriteTransactionTest(const tracked_objects::Location& from_here,
                       WriterTag writer,
                       syncer::syncable::Directory* directory,
                       scoped_ptr<WaitableEvent>* wait_for_syncapi)
      : WriteTransaction(from_here, writer, directory),
        wait_for_syncapi_(wait_for_syncapi) { }

  void NotifyTransactionComplete(syncer::ModelTypeSet types) override {
    // This is where we differ. Force a thread change here, giving another
    // thread a chance to create a WriteTransaction
    (*wait_for_syncapi_)->Wait();

    WriteTransaction::NotifyTransactionComplete(types);
  }

 private:
  scoped_ptr<WaitableEvent>* wait_for_syncapi_;
};

// Our fake server updater. Needs the RefCountedThreadSafe inheritance so we can
// post tasks with it.
class FakeServerUpdater : public base::RefCountedThreadSafe<FakeServerUpdater> {
 public:
  FakeServerUpdater(TestProfileSyncService* service,
                    scoped_ptr<WaitableEvent>* wait_for_start,
                    scoped_ptr<WaitableEvent>* wait_for_syncapi)
      : entry_(ProfileSyncServiceAutofillTest::MakeAutofillEntry("0", "0", 0)),
        service_(service),
        wait_for_start_(wait_for_start),
        wait_for_syncapi_(wait_for_syncapi),
        is_finished_(false, false) { }

  void Update() {
    // This gets called in a modelsafeworker thread.
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::DB));

    syncer::UserShare* user_share = service_->GetUserShare();
    syncer::syncable::Directory* directory = user_share->directory.get();

    // Create autofill protobuf.
    std::string tag = AutocompleteSyncableService::KeyToTag(
        base::UTF16ToUTF8(entry_.key().name()),
        base::UTF16ToUTF8(entry_.key().value()));
    sync_pb::AutofillSpecifics new_autofill;
    new_autofill.set_name(base::UTF16ToUTF8(entry_.key().name()));
    new_autofill.set_value(base::UTF16ToUTF8(entry_.key().value()));
    new_autofill.add_usage_timestamp(entry_.date_created().ToInternalValue());
    if (entry_.date_created() != entry_.date_last_used()) {
      new_autofill.add_usage_timestamp(
          entry_.date_last_used().ToInternalValue());
    }

    sync_pb::EntitySpecifics entity_specifics;
    entity_specifics.mutable_autofill()->CopyFrom(new_autofill);

    {
      // Tell main thread we've started
      (*wait_for_start_)->Signal();

      // Create write transaction.
      WriteTransactionTest trans(FROM_HERE, UNITTEST, directory,
                                 wait_for_syncapi_);

      // Create actual entry based on autofill protobuf information.
      // Simulates effects of UpdateLocalDataFromServerData
      MutableEntry parent(&trans, GET_TYPE_ROOT, syncer::AUTOFILL);
      MutableEntry item(&trans, CREATE, syncer::AUTOFILL, parent.GetId(), tag);
      ASSERT_TRUE(item.good());
      item.PutSpecifics(entity_specifics);
      item.PutServerSpecifics(entity_specifics);
      item.PutBaseVersion(1);
      syncer::syncable::Id server_item_id =
          service_->id_factory()->NewServerId();
      item.PutId(server_item_id);
      syncer::syncable::Id new_predecessor;
      ASSERT_TRUE(item.PutPredecessor(new_predecessor));
    }
    DVLOG(1) << "FakeServerUpdater finishing.";
    is_finished_.Signal();
  }

  void CreateNewEntry(const AutofillEntry& entry) {
    entry_ = entry;
    ASSERT_FALSE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    if (!BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&FakeServerUpdater::Update, this))) {
      NOTREACHED() << "Failed to post task to the db thread.";
      return;
    }
  }

  void CreateNewEntryAndWait(const AutofillEntry& entry) {
    entry_ = entry;
    ASSERT_FALSE(BrowserThread::CurrentlyOn(BrowserThread::DB));
    is_finished_.Reset();
    if (!BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
         base::Bind(&FakeServerUpdater::Update, this))) {
      NOTREACHED() << "Failed to post task to the db thread.";
      return;
    }
    is_finished_.Wait();
  }

 private:
  friend class base::RefCountedThreadSafe<FakeServerUpdater>;
  ~FakeServerUpdater() { }

  AutofillEntry entry_;
  TestProfileSyncService* service_;
  scoped_ptr<WaitableEvent>* wait_for_start_;
  scoped_ptr<WaitableEvent>* wait_for_syncapi_;
  WaitableEvent is_finished_;
  syncer::syncable::Id parent_id_;
};

namespace {

// Checks if the field of type |field_type| in |profile1| includes all values
// of the field in |profile2|.
bool IncludesField(const AutofillProfile& profile1,
                   const AutofillProfile& profile2,
                   ServerFieldType field_type) {
  std::vector<base::string16> values1;
  profile1.GetRawMultiInfo(field_type, &values1);
  std::vector<base::string16> values2;
  profile2.GetRawMultiInfo(field_type, &values2);

  std::set<base::string16> values_set;
  for (size_t i = 0; i < values1.size(); ++i)
    values_set.insert(values1[i]);
  for (size_t i = 0; i < values2.size(); ++i)
    if (values_set.find(values2[i]) == values_set.end())
      return false;
  return true;
}

} // namespace

// TODO(skrul): Test abort startup.
// TODO(skrul): Test processing of cloud changes.
// TODO(tim): Add autofill data type controller test, and a case to cover
//            waiting for the PersonalDataManager.
TEST_F(ProfileSyncServiceAutofillTest, FailModelAssociation) {
  // Don't create the root autofill node so startup fails.
  StartSyncService(base::Closure(), true, syncer::AUTOFILL);
  EXPECT_TRUE(sync_service_->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceAutofillTest, EmptyNativeEmptySync) {
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  EXPECT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(0U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeEntriesEmptySync) {
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  ASSERT_EQ(1U, entries.size());
  EXPECT_TRUE(entries[0] == sync_entries[0]);
  EXPECT_EQ(0U, sync_profiles.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasProfileEmptySync) {
  std::vector<AutofillProfile*> profiles;
  std::vector<AutofillProfile> expected_profiles;
  // Owned by GetAutofillProfiles caller.
  AutofillProfile* profile0 = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(profile0,
      "54B3F9AA-335E-4F71-A27D-719C41564230", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  profiles.push_back(profile0);
  expected_profiles.push_back(*profile0);
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(profiles), Return(true)));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL_PROFILE);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(&sync_profiles));
  EXPECT_EQ(1U, sync_profiles.size());
  EXPECT_EQ(0, expected_profiles[0].Compare(sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeWithDuplicatesEmptySync) {
  // There is buggy autofill code that allows duplicate name/value
  // pairs to exist in the database with separate pair_ids.
  std::vector<AutofillEntry> entries;
  entries.push_back(MakeAutofillEntry("foo", "bar", 1));
  entries.push_back(MakeAutofillEntry("dup", "", 2));
  entries.push_back(MakeAutofillEntry("dup", "", 3));
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(entries), Return(true)));
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());
  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(2U, sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncNoMerge) {
  AutofillEntry native_entry(MakeAutofillEntry("native", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("sync", "entry", 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);

  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);

  AddAutofillHelper<AutofillEntry> add_autofill(this, sync_entries);

  EXPECT_CALL(autofill_table_, UpdateAutofillEntries(ElementsAre(sync_entry))).
      WillOnce(Return(true));

  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(add_autofill.success());

  std::set<AutofillEntry> expected_entries;
  expected_entries.insert(native_entry);
  expected_entries.insert(sync_entry);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  std::set<AutofillEntry> new_sync_entries_set(new_sync_entries.begin(),
                                               new_sync_entries.end());

  EXPECT_TRUE(expected_entries == new_sync_entries_set);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeEntry) {
  AutofillEntry native_entry(MakeAutofillEntry("merge", "entry", 1));
  AutofillEntry sync_entry(MakeAutofillEntry("merge", "entry", 2));
  AutofillEntry merged_entry(MakeAutofillEntry("merge", "entry", 1, 2));

  std::vector<AutofillEntry> native_entries;
  native_entries.push_back(native_entry);
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_entries), Return(true)));

  std::vector<AutofillEntry> sync_entries;
  sync_entries.push_back(sync_entry);
  AddAutofillHelper<AutofillEntry> add_autofill(this, sync_entries);

  EXPECT_CALL(autofill_table_,
      UpdateAutofillEntries(ElementsAre(merged_entry))).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(merged_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table_,
              UpdateAutofillProfile(MatchProfiles(sync_profile))).
      WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, HasNativeHasSyncMergeProfileCombine) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile* native_profile = new AutofillProfile;
  // Same address, but different names, phones and e-mails.
  autofill::test::SetProfileInfoWithGuid(native_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBF", "Billing", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "19482937549");

  AutofillProfile expected_profile(sync_profile);
  expected_profile.OverwriteWithOrAddTo(*native_profile, "en-US");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));
  EXPECT_CALL(autofill_table_,
              AddAutofillProfile(MatchProfiles(expected_profile))).
      WillOnce(Return(true));
  EXPECT_CALL(autofill_table_,
              RemoveAutofillProfile("23355099-1170-4B71-8ED4-144470CC9EBF")).
      WillOnce(Return(true));
  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  // Check that key fields are the same.
  EXPECT_TRUE(new_sync_profiles[0].IsSubsetOf(sync_profile, "en-US"));
  // Check that multivalued fields of the synced back data include original
  // data.
  EXPECT_TRUE(
      IncludesField(new_sync_profiles[0], sync_profile, autofill::NAME_FULL));
  EXPECT_TRUE(IncludesField(
      new_sync_profiles[0], sync_profile, autofill::EMAIL_ADDRESS));
  EXPECT_TRUE(IncludesField(
      new_sync_profiles[0], sync_profile, autofill::PHONE_HOME_WHOLE_NUMBER));
}

TEST_F(ProfileSyncServiceAutofillTest, MergeProfileWithDifferentGuid) {
  AutofillProfile sync_profile;

  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "23355099-1170-4B71-8ED4-144470CC9EBE", "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  std::string native_guid = "EDC609ED-7EEE-4F27-B00C-423242A9C44B";
  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      native_guid.c_str(), "Billing",
      "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);

  EXPECT_CALL(autofill_table_, AddAutofillProfile(_)).
      WillOnce(Return(true));
  EXPECT_CALL(autofill_table_, RemoveAutofillProfile(native_guid)).
      WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, sync_profile.Compare(new_sync_profiles[0]));
  EXPECT_EQ(sync_profile.guid(), new_sync_profiles[0].guid());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddEntry) {
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillEntry added_entry(MakeAutofillEntry("added", "entry", 1));

  EXPECT_CALL(autofill_table_, GetAutofillTimestamps(_, _, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(added_entry.date_created()),
                     SetArgumentPointee<3>(added_entry.date_last_used()),
                     Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::ADD, added_entry.key()));

  web_data_service_->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(added_entry == new_sync_entries[0]);
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeAddProfile) {
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).WillOnce(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  SetIdleChangeProcessorExpectations();
  CreateRootHelper create_root(this, syncer::AUTOFILL_PROFILE);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(create_root.success());

  AutofillProfile added_profile;
  autofill::test::SetProfileInfoWithGuid(&added_profile,
      "D6ADA912-D374-4C0A-917D-F5C8EBE43011", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  AutofillProfileChange change(
      AutofillProfileChange::ADD, added_profile.guid(), &added_profile);
  web_data_service_->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_profiles.size());
  EXPECT_EQ(0, added_profile.Compare(new_sync_profiles[0]));
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeUpdateEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillEntry updated_entry(MakeAutofillEntry("my", "entry", 1, 2));

  EXPECT_CALL(autofill_table_, GetAutofillTimestamps(_, _, _, _)).
      WillOnce(DoAll(SetArgumentPointee<2>(updated_entry.date_created()),
                     SetArgumentPointee<3>(updated_entry.date_last_used()),
                     Return(true)));

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::UPDATE,
                                   updated_entry.key()));
  web_data_service_->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(1U, new_sync_entries.size());
  EXPECT_TRUE(updated_entry == new_sync_entries[0]);
}


TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveEntry) {
  AutofillEntry original_entry(MakeAutofillEntry("my", "entry", 1));
  std::vector<AutofillEntry> original_entries;
  original_entries.push_back(original_entry);

  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(original_entries), Return(true)));
  EXPECT_CALL(*personal_data_manager_, Refresh());
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());

  AutofillChangeList changes;
  changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                   original_entry.key()));
  web_data_service_->OnAutofillEntriesChanged(changes);

  std::vector<AutofillEntry> new_sync_entries;
  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&new_sync_entries,
                                           &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_entries.size());
}

TEST_F(ProfileSyncServiceAutofillTest, ProcessUserChangeRemoveProfile) {
  AutofillProfile sync_profile;
  autofill::test::SetProfileInfoWithGuid(&sync_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");
  AutofillProfile* native_profile = new AutofillProfile;
  autofill::test::SetProfileInfoWithGuid(native_profile,
      "3BA5FA1B-1EC4-4BB3-9B57-EC92BE3C1A09", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  std::vector<AutofillProfile*> native_profiles;
  native_profiles.push_back(native_profile);
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(native_profiles), Return(true)));

  std::vector<AutofillProfile> sync_profiles;
  sync_profiles.push_back(sync_profile);
  AddAutofillHelper<AutofillProfile> add_autofill(this, sync_profiles);
  EXPECT_CALL(*personal_data_manager_, Refresh());
  StartSyncService(add_autofill.callback(), false, syncer::AUTOFILL_PROFILE);
  ASSERT_TRUE(add_autofill.success());

  AutofillProfileChange change(
      AutofillProfileChange::REMOVE, sync_profile.guid(), NULL);
  web_data_service_->OnAutofillProfileChanged(change);

  std::vector<AutofillProfile> new_sync_profiles;
  ASSERT_TRUE(GetAutofillProfilesFromSyncDBUnderProfileNode(
      &new_sync_profiles));
  ASSERT_EQ(0U, new_sync_profiles.size());
}

// http://crbug.com/57884
TEST_F(ProfileSyncServiceAutofillTest, DISABLED_ServerChangeRace) {
  // Once for MergeDataAndStartSyncing() and twice for ProcessSyncChanges(), via
  // LoadAutofillData().
  EXPECT_CALL(autofill_table_, GetAllAutofillEntries(_)).
      Times(3).WillRepeatedly(Return(true));
  // On the other hand Autofill and Autocomplete are separated now, so
  // GetAutofillProfiles() should not be called.
  EXPECT_CALL(autofill_table_, GetAutofillProfiles(_)).Times(0);
  EXPECT_CALL(autofill_table_, UpdateAutofillEntries(_)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*personal_data_manager_, Refresh()).Times(3);
  CreateRootHelper create_root(this, syncer::AUTOFILL);
  StartSyncService(create_root.callback(), false, syncer::AUTOFILL);
  ASSERT_TRUE(create_root.success());

  // (true, false) means we have to reset after |Signal|, init to unsignaled.
  scoped_ptr<WaitableEvent> wait_for_start(new WaitableEvent(true, false));
  scoped_ptr<WaitableEvent> wait_for_syncapi(new WaitableEvent(true, false));
  scoped_refptr<FakeServerUpdater> updater(new FakeServerUpdater(
      sync_service_, &wait_for_start, &wait_for_syncapi));

  // This server side update will stall waiting for CommitWaiter.
  updater->CreateNewEntry(MakeAutofillEntry("server", "entry", 1));
  wait_for_start->Wait();

  AutofillEntry syncapi_entry(MakeAutofillEntry("syncapi", "entry", 2));
  ASSERT_TRUE(AddAutofillSyncNode(syncapi_entry));
  DVLOG(1) << "Syncapi update finished.";

  // If we reach here, it means syncapi succeeded and we didn't deadlock. Yay!
  // Signal FakeServerUpdater that it can complete.
  wait_for_syncapi->Signal();

  // Make another entry to ensure nothing broke afterwards and wait for finish
  // to clean up.
  updater->CreateNewEntryAndWait(MakeAutofillEntry("server2", "entry2", 3));

  std::vector<AutofillEntry> sync_entries;
  std::vector<AutofillProfile> sync_profiles;
  ASSERT_TRUE(GetAutofillEntriesFromSyncDB(&sync_entries, &sync_profiles));
  EXPECT_EQ(3U, sync_entries.size());
  EXPECT_EQ(0U, sync_profiles.size());
  for (size_t i = 0; i < sync_entries.size(); i++) {
    DVLOG(1) << "Entry " << i << ": " << sync_entries[i].key().name()
             << ", " << sync_entries[i].key().value();
  }
}
