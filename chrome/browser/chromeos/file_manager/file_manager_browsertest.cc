// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Browser test for basic Chrome OS file manager functionality:
//  - The file list is updated when a file is added externally to the Downloads
//    folder.
//  - Selecting a file and copy-pasting it with the keyboard copies the file.
//  - Selecting a file and pressing delete deletes it.

#include <deque>
#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/drive_test_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/fileapi/external_mount_points.h"

// Slow tests are disabled on debug build. http://crbug.com/327719
// Disabled under MSAN, ASAN, and LSAN as well. http://crbug.com/468980.
#if !defined(NDEBUG) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define DISABLE_SLOW_FILESAPP_TESTS
#endif

using drive::DriveIntegrationServiceFactory;

namespace file_manager {
namespace {

enum EntryType {
  FILE,
  DIRECTORY,
};

enum TargetVolume { LOCAL_VOLUME, DRIVE_VOLUME, USB_VOLUME, };

enum SharedOption {
  NONE,
  SHARED,
};

enum GuestMode {
  NOT_IN_GUEST_MODE,
  IN_GUEST_MODE,
  IN_INCOGNITO
};

// This global operator is used from Google Test to format error messages.
std::ostream& operator<<(std::ostream& os, const GuestMode& guest_mode) {
  return os << (guest_mode == IN_GUEST_MODE ?
                "IN_GUEST_MODE" : "NOT_IN_GUEST_MODE");
}

// Maps the given string to EntryType. Returns true on success.
bool MapStringToEntryType(const base::StringPiece& value, EntryType* output) {
  if (value == "file")
    *output = FILE;
  else if (value == "directory")
    *output = DIRECTORY;
  else
    return false;
  return true;
}

// Maps the given string to SharedOption. Returns true on success.
bool MapStringToSharedOption(const base::StringPiece& value,
                             SharedOption* output) {
  if (value == "shared")
    *output = SHARED;
  else if (value == "none")
    *output = NONE;
  else
    return false;
  return true;
}

// Maps the given string to TargetVolume. Returns true on success.
bool MapStringToTargetVolume(const base::StringPiece& value,
                             TargetVolume* output) {
  if (value == "drive")
    *output = DRIVE_VOLUME;
  else if (value == "local")
    *output = LOCAL_VOLUME;
  else if (value == "usb")
    *output = USB_VOLUME;
  else
    return false;
  return true;
}

// Maps the given string to base::Time. Returns true on success.
bool MapStringToTime(const base::StringPiece& value, base::Time* time) {
  return base::Time::FromString(value.as_string().c_str(), time);
}

// Test data of file or directory.
struct TestEntryInfo {
  TestEntryInfo() : type(FILE), shared_option(NONE) {}

  TestEntryInfo(EntryType type,
                const std::string& source_file_name,
                const std::string& target_path,
                const std::string& mime_type,
                SharedOption shared_option,
                const base::Time& last_modified_time) :
      type(type),
      source_file_name(source_file_name),
      target_path(target_path),
      mime_type(mime_type),
      shared_option(shared_option),
      last_modified_time(last_modified_time) {
  }

  EntryType type;
  std::string source_file_name;  // Source file name to be used as a prototype.
  std::string target_path;  // Target file or directory path.
  std::string mime_type;
  SharedOption shared_option;
  base::Time last_modified_time;

  // Registers the member information to the given converter.
  static void RegisterJSONConverter(
      base::JSONValueConverter<TestEntryInfo>* converter);
};

// static
void TestEntryInfo::RegisterJSONConverter(
    base::JSONValueConverter<TestEntryInfo>* converter) {
  converter->RegisterCustomField("type",
                                 &TestEntryInfo::type,
                                 &MapStringToEntryType);
  converter->RegisterStringField("sourceFileName",
                                 &TestEntryInfo::source_file_name);
  converter->RegisterStringField("targetPath", &TestEntryInfo::target_path);
  converter->RegisterStringField("mimeType", &TestEntryInfo::mime_type);
  converter->RegisterCustomField("sharedOption",
                                 &TestEntryInfo::shared_option,
                                 &MapStringToSharedOption);
  converter->RegisterCustomField("lastModifiedTime",
                                 &TestEntryInfo::last_modified_time,
                                 &MapStringToTime);
}

// Message from JavaScript to add entries.
struct AddEntriesMessage {
  // Target volume to be added the |entries|.
  TargetVolume volume;

  // Entries to be added.
  ScopedVector<TestEntryInfo> entries;

  // Registers the member information to the given converter.
  static void RegisterJSONConverter(
      base::JSONValueConverter<AddEntriesMessage>* converter);
};

// static
void AddEntriesMessage::RegisterJSONConverter(
    base::JSONValueConverter<AddEntriesMessage>* converter) {
  converter->RegisterCustomField("volume",
                                 &AddEntriesMessage::volume,
                                 &MapStringToTargetVolume);
  converter->RegisterRepeatedMessage<TestEntryInfo>(
      "entries",
      &AddEntriesMessage::entries);
}

// Test volume.
class TestVolume {
 protected:
  explicit TestVolume(const std::string& name) : name_(name) {}
  virtual ~TestVolume() {}

  bool CreateRootDirectory(const Profile* profile) {
    const base::FilePath path = profile->GetPath().Append(name_);
    return root_.path() == path || root_.Set(path);
  }

  const std::string& name() { return name_; }
  const base::FilePath root_path() { return root_.path(); }

 private:
  std::string name_;
  base::ScopedTempDir root_;
};

// The local volume class for test.
// This class provides the operations for a test volume that simulates local
// drive.
class LocalTestVolume : public TestVolume {
 public:
  explicit LocalTestVolume(const std::string& name) : TestVolume(name) {}
  ~LocalTestVolume() override {}

  // Adds this volume to the file system as a local volume. Returns true on
  // success.
  virtual bool Mount(Profile* profile) = 0;

  void CreateEntry(const TestEntryInfo& entry) {
    const base::FilePath target_path =
        root_path().AppendASCII(entry.target_path);

    entries_.insert(std::make_pair(target_path, entry));
    switch (entry.type) {
      case FILE: {
        const base::FilePath source_path =
            google_apis::test_util::GetTestFilePath("chromeos/file_manager").
            AppendASCII(entry.source_file_name);
        ASSERT_TRUE(base::CopyFile(source_path, target_path))
            << "Copy from " << source_path.value()
            << " to " << target_path.value() << " failed.";
        break;
      }
      case DIRECTORY:
        ASSERT_TRUE(base::CreateDirectory(target_path)) <<
            "Failed to create a directory: " << target_path.value();
        break;
    }
    ASSERT_TRUE(UpdateModifiedTime(entry));
  }

 private:
  // Updates ModifiedTime of the entry and its parents by referring
  // TestEntryInfo. Returns true on success.
  bool UpdateModifiedTime(const TestEntryInfo& entry) {
    const base::FilePath path = root_path().AppendASCII(entry.target_path);
    if (!base::TouchFile(path, entry.last_modified_time,
                         entry.last_modified_time))
      return false;

    // Update the modified time of parent directories because it may be also
    // affected by the update of child items.
    if (path.DirName() != root_path()) {
      const std::map<base::FilePath, const TestEntryInfo>::iterator it =
          entries_.find(path.DirName());
      if (it == entries_.end())
        return false;
      return UpdateModifiedTime(it->second);
    }
    return true;
  }

  std::map<base::FilePath, const TestEntryInfo> entries_;
};

class DownloadsTestVolume : public LocalTestVolume {
 public:
  DownloadsTestVolume() : LocalTestVolume("Downloads") {}
  ~DownloadsTestVolume() override {}

  bool Mount(Profile* profile) override {
    return CreateRootDirectory(profile) &&
           VolumeManager::Get(profile)
               ->RegisterDownloadsDirectoryForTesting(root_path());
  }
};

// Test volume for mimicing a specified type of volumes by a local folder.
class FakeTestVolume : public LocalTestVolume {
 public:
  FakeTestVolume(const std::string& name,
                 VolumeType volume_type,
                 chromeos::DeviceType device_type)
      : LocalTestVolume(name),
        volume_type_(volume_type),
        device_type_(device_type) {}
  ~FakeTestVolume() override {}

  // Simple test entries used for testing, e.g., read-only volumes.
  bool PrepareTestEntries(Profile* profile) {
    if (!CreateRootDirectory(profile))
      return false;
    // Must be in sync with BASIC_FAKE_ENTRY_SET in the JS test code.
    CreateEntry(
        TestEntryInfo(FILE, "text.txt", "hello.txt", "text/plain", NONE,
                      base::Time::Now()));
    CreateEntry(
        TestEntryInfo(DIRECTORY, std::string(), "A", std::string(), NONE,
                      base::Time::Now()));
    return true;
  }

  bool Mount(Profile* profile) override {
    if (!CreateRootDirectory(profile))
      return false;
    storage::ExternalMountPoints* const mount_points =
        storage::ExternalMountPoints::GetSystemInstance();

    // First revoke the existing mount point (if any).
    mount_points->RevokeFileSystem(name());
    const bool result =
        mount_points->RegisterFileSystem(name(),
                                         storage::kFileSystemTypeNativeLocal,
                                         storage::FileSystemMountOption(),
                                         root_path());
    if (!result)
      return false;

    VolumeManager::Get(profile)->AddVolumeForTesting(
        root_path(), volume_type_, device_type_, false /* read_only */);
    return true;
  }

 private:
  const VolumeType volume_type_;
  const chromeos::DeviceType device_type_;
};

// The drive volume class for test.
// This class provides the operations for a test volume that simulates Google
// drive.
class DriveTestVolume : public TestVolume {
 public:
  DriveTestVolume() : TestVolume("drive"), integration_service_(NULL) {}
  ~DriveTestVolume() override {}

  void CreateEntry(const TestEntryInfo& entry) {
    const base::FilePath path =
        base::FilePath::FromUTF8Unsafe(entry.target_path);
    const std::string target_name = path.BaseName().AsUTF8Unsafe();

    // Obtain the parent entry.
    drive::FileError error = drive::FILE_ERROR_OK;
    scoped_ptr<drive::ResourceEntry> parent_entry(new drive::ResourceEntry);
    integration_service_->file_system()->GetResourceEntry(
        drive::util::GetDriveMyDriveRootPath().Append(path).DirName(),
        google_apis::test_util::CreateCopyResultCallback(
            &error, &parent_entry));
    content::RunAllBlockingPoolTasksUntilIdle();
    ASSERT_EQ(drive::FILE_ERROR_OK, error);
    ASSERT_TRUE(parent_entry);

    switch (entry.type) {
      case FILE:
        CreateFile(entry.source_file_name,
                   parent_entry->resource_id(),
                   target_name,
                   entry.mime_type,
                   entry.shared_option == SHARED,
                   entry.last_modified_time);
        break;
      case DIRECTORY:
        CreateDirectory(
            parent_entry->resource_id(), target_name, entry.last_modified_time);
        break;
    }
  }

  // Creates an empty directory with the given |name| and |modification_time|.
  void CreateDirectory(const std::string& parent_id,
                       const std::string& target_name,
                       const base::Time& modification_time) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    scoped_ptr<google_apis::FileResource> entry;
    fake_drive_service_->AddNewDirectory(
        parent_id, target_name, drive::AddNewDirectoryOptions(),
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    fake_drive_service_->SetLastModifiedTime(
        entry->file_id(),
        modification_time,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_TRUE(error == google_apis::HTTP_SUCCESS);
    ASSERT_TRUE(entry);
    CheckForUpdates();
  }

  // Creates a test file with the given spec.
  // Serves |test_file_name| file. Pass an empty string for an empty file.
  void CreateFile(const std::string& source_file_name,
                  const std::string& parent_id,
                  const std::string& target_name,
                  const std::string& mime_type,
                  bool shared_with_me,
                  const base::Time& modification_time) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;

    std::string content_data;
    if (!source_file_name.empty()) {
      base::FilePath source_file_path =
          google_apis::test_util::GetTestFilePath("chromeos/file_manager").
              AppendASCII(source_file_name);
      ASSERT_TRUE(base::ReadFileToString(source_file_path, &content_data));
    }

    scoped_ptr<google_apis::FileResource> entry;
    fake_drive_service_->AddNewFile(
        mime_type,
        content_data,
        parent_id,
        target_name,
        shared_with_me,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_CREATED, error);
    ASSERT_TRUE(entry);

    fake_drive_service_->SetLastModifiedTime(
        entry->file_id(),
        modification_time,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::MessageLoop::current()->RunUntilIdle();
    ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
    ASSERT_TRUE(entry);

    CheckForUpdates();
  }

  // Notifies FileSystem that the contents in FakeDriveService are
  // changed, hence the new contents should be fetched.
  void CheckForUpdates() {
    if (integration_service_ && integration_service_->file_system()) {
      integration_service_->file_system()->CheckForUpdates();
    }
  }

  // Sets the url base for the test server to be used to generate share urls
  // on the files and directories.
  void ConfigureShareUrlBase(const GURL& share_url_base) {
    fake_drive_service_->set_share_url_base(share_url_base);
  }

  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    profile_ = profile;
    fake_drive_service_ = new drive::FakeDriveService;
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    if (!CreateRootDirectory(profile))
      return NULL;
    integration_service_ = new drive::DriveIntegrationService(
        profile, NULL, fake_drive_service_, std::string(), root_path(), NULL);
    return integration_service_;
  }

 private:
  Profile* profile_;
  drive::FakeDriveService* fake_drive_service_;
  drive::DriveIntegrationService* integration_service_;
};

// Listener to obtain the test relative messages synchronously.
class FileManagerTestListener : public content::NotificationObserver {
 public:
  struct Message {
    int type;
    std::string message;
    scoped_refptr<extensions::TestSendMessageFunction> function;
  };

  FileManagerTestListener() {
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
                   content::NotificationService::AllSources());
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE,
                   content::NotificationService::AllSources());
  }

  Message GetNextMessage() {
    if (messages_.empty())
      content::RunMessageLoop();
    const Message entry = messages_.front();
    messages_.pop_front();
    return entry;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    Message entry;
    entry.type = type;
    entry.message = type != extensions::NOTIFICATION_EXTENSION_TEST_PASSED
                        ? *content::Details<std::string>(details).ptr()
                        : std::string();
    entry.function =
        type == extensions::NOTIFICATION_EXTENSION_TEST_MESSAGE
            ? content::Source<extensions::TestSendMessageFunction>(source).ptr()
            : NULL;
    messages_.push_back(entry);
    base::MessageLoopForUI::current()->Quit();
  }

 private:
  std::deque<Message> messages_;
  content::NotificationRegistrar registrar_;
};

// The base test class.
class FileManagerBrowserTestBase : public ExtensionApiTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override;

  void SetUpOnMainThread() override;

  // Adds an incognito and guest-mode flags for tests in the guest mode.
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Loads our testing extension and sends it a string identifying the current
  // test.
  virtual void StartTest();
  void RunTestMessageLoop();

  // Overriding point for test configurations.
  virtual const char* GetTestManifestName() const {
    return "file_manager_test_manifest.json";
  }
  virtual GuestMode GetGuestModeParam() const = 0;
  virtual const char* GetTestCaseNameParam() const = 0;
  virtual void OnMessage(const std::string& name,
                         const base::DictionaryValue& value,
                         std::string* output);

  scoped_ptr<LocalTestVolume> local_volume_;
  linked_ptr<DriveTestVolume> drive_volume_;
  std::map<Profile*, linked_ptr<DriveTestVolume> > drive_volumes_;
  scoped_ptr<FakeTestVolume> usb_volume_;
  scoped_ptr<FakeTestVolume> mtp_volume_;

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile);
  DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  scoped_ptr<DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
};

void FileManagerBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();

  local_volume_.reset(new DownloadsTestVolume);
  if (GetGuestModeParam() != IN_GUEST_MODE) {
    create_drive_integration_service_ =
        base::Bind(&FileManagerBrowserTestBase::CreateDriveIntegrationService,
                   base::Unretained(this));
    service_factory_for_test_.reset(
        new DriveIntegrationServiceFactory::ScopedFactoryForTest(
            &create_drive_integration_service_));
  }
}

void FileManagerBrowserTestBase::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
  ASSERT_TRUE(local_volume_->Mount(profile()));

  if (GetGuestModeParam() != IN_GUEST_MODE) {
    // Install the web server to serve the mocked share dialog.
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    const GURL share_url_base(embedded_test_server()->GetURL(
        "/chromeos/file_manager/share_dialog_mock/index.html"));
    drive_volume_ = drive_volumes_[profile()->GetOriginalProfile()];
    drive_volume_->ConfigureShareUrlBase(share_url_base);
    test_util::WaitUntilDriveMountPointIsAdded(profile());
  }

  net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
}

void FileManagerBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (GetGuestModeParam() == IN_GUEST_MODE) {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitchNative(chromeos::switches::kLoginUser, "");
    command_line->AppendSwitch(switches::kIncognito);
  }
  if (GetGuestModeParam() == IN_INCOGNITO) {
    command_line->AppendSwitch(switches::kIncognito);
  }
  ExtensionApiTest::SetUpCommandLine(command_line);
}

void FileManagerBrowserTestBase::StartTest() {
  base::FilePath root_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

  // Launch the extension.
  const base::FilePath path =
      root_path.Append(FILE_PATH_LITERAL("ui/file_manager/integration_tests"));
  const extensions::Extension* const extension =
      LoadExtensionAsComponentWithManifest(path, GetTestManifestName());
  ASSERT_TRUE(extension);

  RunTestMessageLoop();
}

void FileManagerBrowserTestBase::RunTestMessageLoop() {
  // Handle the messages from JavaScript.
  // The while loop is break when the test is passed or failed.
  FileManagerTestListener listener;
  while (true) {
    FileManagerTestListener::Message entry = listener.GetNextMessage();
    if (entry.type == extensions::NOTIFICATION_EXTENSION_TEST_PASSED) {
      // Test succeed.
      break;
    } else if (entry.type == extensions::NOTIFICATION_EXTENSION_TEST_FAILED) {
      // Test failed.
      ADD_FAILURE() << entry.message;
      break;
    }

    // Parse the message value as JSON.
    const scoped_ptr<const base::Value> value(
        base::JSONReader::Read(entry.message));

    // If the message is not the expected format, just ignore it.
    const base::DictionaryValue* message_dictionary = NULL;
    std::string name;
    if (!value || !value->GetAsDictionary(&message_dictionary) ||
        !message_dictionary->GetString("name", &name))
      continue;

    std::string output;
    OnMessage(name, *message_dictionary, &output);
    if (HasFatalFailure())
      break;

    entry.function->Reply(output);
  }
}

void FileManagerBrowserTestBase::OnMessage(const std::string& name,
                                           const base::DictionaryValue& value,
                                           std::string* output) {
  if (name == "getTestName") {
    // Pass the test case name.
    *output = GetTestCaseNameParam();
    return;
  }

  if (name == "getRootPaths") {
    // Pass the root paths.
    const scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
    res->SetString("downloads",
        "/" + util::GetDownloadsMountPointName(profile()));
    res->SetString("drive",
        "/" + drive::util::GetDriveMountPointPath(profile()
            ).BaseName().AsUTF8Unsafe() + "/root");
    base::JSONWriter::Write(res.get(), output);
    return;
  }

  if (name == "isInGuestMode") {
    // Obtain whether the test is in guest mode or not.
    *output = GetGuestModeParam() != NOT_IN_GUEST_MODE ? "true" : "false";
    return;
  }

  if (name == "getCwsWidgetContainerMockUrl") {
    // Obtain whether the test is in guest mode or not.
    const GURL url = embedded_test_server()->GetURL(
          "/chromeos/file_manager/cws_container_mock/index.html");
    std::string origin = url.GetOrigin().spec();

    // Removes trailing a slash.
    if (*origin.rbegin() == '/')
      origin.resize(origin.length() - 1);

    const scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
    res->SetString("url", url.spec());
    res->SetString("origin", origin);
    base::JSONWriter::Write(res.get(), output);
    return;
  }

  if (name == "addEntries") {
    // Add entries to the specified volume.
    base::JSONValueConverter<AddEntriesMessage> add_entries_message_converter;
    AddEntriesMessage message;
    ASSERT_TRUE(add_entries_message_converter.Convert(value, &message));

    for (size_t i = 0; i < message.entries.size(); ++i) {
      switch (message.volume) {
        case LOCAL_VOLUME:
          local_volume_->CreateEntry(*message.entries[i]);
          break;
        case DRIVE_VOLUME:
          if (drive_volume_.get())
            drive_volume_->CreateEntry(*message.entries[i]);
          break;
        case USB_VOLUME:
          if (usb_volume_)
            usb_volume_->CreateEntry(*message.entries[i]);
          break;
        default:
          NOTREACHED();
          break;
      }
    }

    return;
  }

  if (name == "mountFakeUsb") {
    usb_volume_.reset(new FakeTestVolume("fake-usb",
                                         VOLUME_TYPE_REMOVABLE_DISK_PARTITION,
                                         chromeos::DEVICE_TYPE_USB));
    usb_volume_->Mount(profile());
    return;
  }

  if (name == "mountFakeMtp") {
    mtp_volume_.reset(new FakeTestVolume("fake-mtp",
                                         VOLUME_TYPE_MTP,
                                         chromeos::DEVICE_TYPE_UNKNOWN));
    ASSERT_TRUE(mtp_volume_->PrepareTestEntries(profile()));

    mtp_volume_->Mount(profile());
    return;
  }

  if (name == "useCellularNetwork") {
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_3G);
    return;
  }

  if (name == "clickNotificationButton") {
    std::string extension_id;
    std::string notification_id;
    int index;
    ASSERT_TRUE(value.GetString("extensionId", &extension_id));
    ASSERT_TRUE(value.GetString("notificationId", &notification_id));
    ASSERT_TRUE(value.GetInteger("index", &index));

    const std::string delegate_id = extension_id + "-" + notification_id;
    const Notification* notification = g_browser_process->
        notification_ui_manager()->FindById(delegate_id, profile());
    ASSERT_TRUE(notification);

    notification->delegate()->ButtonClick(index);
    return;
  }

  FAIL() << "Unknown test message: " << name;
}

drive::DriveIntegrationService*
FileManagerBrowserTestBase::CreateDriveIntegrationService(Profile* profile) {
  drive_volumes_[profile->GetOriginalProfile()].reset(new DriveTestVolume());
  return drive_volumes_[profile->GetOriginalProfile()]->
      CreateDriveIntegrationService(profile);
}

// Parameter of FileManagerBrowserTest.
// The second value is the case name of JavaScript.
typedef std::tr1::tuple<GuestMode, const char*> TestParameter;

// Test fixture class for normal (not multi-profile related) tests.
class FileManagerBrowserTest :
      public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<TestParameter> {
  GuestMode GetGuestModeParam() const override {
    return std::tr1::get<0>(GetParam());
  }
  const char* GetTestCaseNameParam() const override {
    return std::tr1::get<1>(GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTest, Test) {
  StartTest();
}

// Unlike TEST/TEST_F, which are macros that expand to further macros,
// INSTANTIATE_TEST_CASE_P is a macro that expands directly to code that
// stringizes the arguments. As a result, macros passed as parameters (such as
// prefix or test_case_name) will not be expanded by the preprocessor. To work
// around this, indirect the macro for INSTANTIATE_TEST_CASE_P, so that the
// pre-processor will expand macros such as MAYBE_test_name before
// instantiating the test.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator) \
  INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator)

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_FileDisplay DISABLED_FileDisplay
#else
#define MAYBE_FileDisplay FileDisplay
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_FileDisplay,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDrive"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayMtp"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchNormal"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchCaseInsensitive"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchNotFound")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenVideoFiles DISABLED_OpenVideoFiles
#else
#define MAYBE_OpenVideoFiles OpenVideoFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenVideoFiles,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenAudioFiles DISABLED_OpenAudioFiles
#else
#define MAYBE_OpenAudioFiles OpenAudioFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenAudioFiles,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(IN_GUEST_MODE, "audioOpenDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "audioOpenDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "audioOpenDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioAutoAdvanceDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioRepeatSingleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioNoRepeatSingleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioRepeatMultipleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioNoRepeatMultipleFileDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenImageFiles DISABLED_OpenImageFiles
#else
#define MAYBE_OpenImageFiles OpenImageFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenImageFiles,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "imageOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "imageOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "imageOpenDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_CreateNewFolder DISABLED_CreateNewFolder
#else
#define MAYBE_CreateNewFolder CreateNewFolder
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_CreateNewFolder,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderAfterSelectFile"),
                      TestParameter(IN_GUEST_MODE,
                                    "createNewFolderDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_KeyboardOperations DISABLED_KeyboardOperations
#else
#define MAYBE_KeyboardOperations KeyboardOperations
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_KeyboardOperations,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "keyboardDeleteDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "keyboardDeleteDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardDeleteDrive"),
                      TestParameter(IN_GUEST_MODE, "keyboardCopyDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDrive"),
                      TestParameter(IN_GUEST_MODE, "renameFileDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "renameFileDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "renameFileDrive"),
                      TestParameter(IN_GUEST_MODE,
                                    "renameNewDirectoryDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "renameNewDirectoryDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "renameNewDirectoryDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_DriveSpecific DISABLED_DriveSpecific
#else
#define MAYBE_DriveSpecific DriveSpecific
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_DriveSpecific,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarRecent"),
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarOffline"),
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarSharedWithMe"),
        TestParameter(NOT_IN_GUEST_MODE, "autocomplete"),
        TestParameter(NOT_IN_GUEST_MODE, "pinFileOnMobileNetwork"),
        TestParameter(NOT_IN_GUEST_MODE, "clickFirstSearchResult"),
        TestParameter(NOT_IN_GUEST_MODE, "pressEnterToSearch")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_Transfer DISABLED_Transfer
#else
#define MAYBE_Transfer Transfer
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Transfer,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDriveToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDownloadsToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromRecentToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromRecentToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_RestorePrefs DISABLED_RestorePrefs
#else
#define MAYBE_RestorePrefs RestorePrefs
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_RestorePrefs,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(IN_GUEST_MODE, "restoreCurrentView"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreCurrentView")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ShareDialog DISABLED_ShareDialog
#else
#define MAYBE_ShareDialog ShareDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ShareDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "shareFile"),
                      TestParameter(NOT_IN_GUEST_MODE, "shareDirectory")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_RestoreGeometry DISABLED_RestoreGeometry
#else
#define MAYBE_RestoreGeometry RestoreGeometry
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_RestoreGeometry,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "restoreGeometry"),
                      TestParameter(IN_GUEST_MODE, "restoreGeometry")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_Traverse DISABLED_Traverse
#else
#define MAYBE_Traverse Traverse
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Traverse,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_SuggestAppDialog DISABLED_SuggestAppDialog
#else
#define MAYBE_SuggestAppDialog SuggestAppDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_SuggestAppDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "suggestAppDialog")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExecuteDefaultTaskOnDownloads \
  DISABLED_ExecuteDefaultTaskOnDownloads
#else
#define MAYBE_ExecuteDefaultTaskOnDownloads ExecuteDefaultTaskOnDownloads
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ExecuteDefaultTaskOnDownloads,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "executeDefaultTaskOnDownloads"),
        TestParameter(IN_GUEST_MODE, "executeDefaultTaskOnDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExecuteDefaultTaskOnDrive DISABLED_ExecuteDefaultTaskOnDrive
#else
#define MAYBE_ExecuteDefaultTaskOnDrive ExecuteDefaultTaskOnDrive
#endif
INSTANTIATE_TEST_CASE_P(
    MAYBE_ExecuteDefaultTaskOnDrive,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "executeDefaultTaskOnDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DefaultActionDialog DISABLED_DefaultActionDialog
#else
#define MAYBE_DefaultActionDialog DefaultActionDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_DefaultActionDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "defaultActionDialogOnDownloads"),
        TestParameter(IN_GUEST_MODE, "defaultActionDialogOnDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "defaultActionDialogOnDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_GenericTask DISABLED_GenericTask
#else
#define MAYBE_GenericTask GenericTask
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_GenericTask,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "genericTaskIsNotExecuted"),
        TestParameter(NOT_IN_GUEST_MODE, "genericAndNonGenericTasksAreMixed")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_FolderShortcuts DISABLED_FolderShortcuts
#else
#define MAYBE_FolderShortcuts FolderShortcuts
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_FolderShortcuts,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "traverseFolderShortcuts"),
        TestParameter(NOT_IN_GUEST_MODE, "addRemoveFolderShortcuts")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_SortColumns DISABLED_SortColumns
#else
#define MAYBE_SortColumns SortColumns
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_SortColumns,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "sortColumns"),
                      TestParameter(IN_GUEST_MODE, "sortColumns")));

INSTANTIATE_TEST_CASE_P(
    TabIndex,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "searchBoxFocus")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabindexFocus DISABLED_TabindexFocus
#else
#define MAYBE_TabindexFocus TabindexFocus
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexFocus,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "tabindexFocus")));

INSTANTIATE_TEST_CASE_P(
    TabindexFocusDownloads,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDownloads"),
                      TestParameter(IN_GUEST_MODE, "tabindexFocusDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabindexFocusDirectorySelected \
  DISABLED_TabindexFocusDirectorySelected
#else
#define MAYBE_TabindexFocusDirectorySelected TabindexFocusDirectorySelected
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexFocusDirectorySelected,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDirectorySelected")));

INSTANTIATE_TEST_CASE_P(
    TabindexOpenDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexOpenDialogDownloads")));

// Fails on official build. http://crbug.com/482121.
#if defined(OFFICIAL_BUILD)
#define MAYBE_TabindexSaveFileDialog DISABLED_TabindexSaveFileDialog
#else
#define MAYBE_TabindexSaveFileDialog TabindexSaveFileDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexSaveFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexSaveFileDialogDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenFileDialog DISABLED_OpenFileDialog
#else
#define MAYBE_OpenFileDialog OpenFileDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "openFileDialogOnDownloads"),
                      TestParameter(IN_GUEST_MODE,
                                    "openFileDialogOnDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "openFileDialogOnDrive"),
                      TestParameter(IN_INCOGNITO,
                                    "openFileDialogOnDownloads"),
                      TestParameter(IN_INCOGNITO,
                                    "openFileDialogOnDrive"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "unloadFileDialog")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CopyBetweenWindows DISABLED_CopyBetweenWindows
#else
#define MAYBE_CopyBetweenWindows CopyBetweenWindows
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_CopyBetweenWindows,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsLocalToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsLocalToUsb"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsUsbToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsDriveToLocal"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsDriveToUsb"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsUsbToLocal")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ShowGridView DISABLED_ShowGridView
#else
#define MAYBE_ShowGridView ShowGridView
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ShowGridView,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "showGridViewDrive")));

// Structure to describe an account info.
struct TestAccountInfo {
  const char* const gaia_id;
  const char* const email;
  const char* const hash;
  const char* const display_name;
};

enum {
  DUMMY_ACCOUNT_INDEX = 0,
  PRIMARY_ACCOUNT_INDEX = 1,
  SECONDARY_ACCOUNT_INDEX_START = 2,
};

static const TestAccountInfo kTestAccounts[] = {
    {"gaia-id-d", "__dummy__@invalid.domain", "hashdummy", "Dummy Account"},
    {"gaia-id-a", "alice@invalid.domain", "hashalice", "Alice"},
    {"gaia-id-b", "bob@invalid.domain", "hashbob", "Bob"},
    {"gaia-id-c", "charlie@invalid.domain", "hashcharlie", "Charlie"},
};

// Test fixture class for testing multi-profile features.
class MultiProfileFileManagerBrowserTest : public FileManagerBrowserTestBase {
 protected:
  // Enables multi-profiles.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Logs in to a dummy profile (For making MultiProfileWindowManager happy;
    // browser test creates a default window and the manager tries to assign a
    // user for it, and we need a profile connected to a user.)
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].email);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].hash);
  }

  // Logs in to the primary profile of this test.
  void SetUpOnMainThread() override {
    const TestAccountInfo& info = kTestAccounts[PRIMARY_ACCOUNT_INDEX];

    AddUser(info, true);
    FileManagerBrowserTestBase::SetUpOnMainThread();
  }

  // Loads all users to the current session and sets up necessary fields.
  // This is used for preparing all accounts in PRE_ test setup, and for testing
  // actual login behavior.
  void AddAllUsers() {
    for (size_t i = 0; i < arraysize(kTestAccounts); ++i)
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
  }

  // Returns primary profile (if it is already created.)
  Profile* profile() override {
    Profile* const profile = chromeos::ProfileHelper::GetProfileByUserIdHash(
        kTestAccounts[PRIMARY_ACCOUNT_INDEX].hash);
    return profile ? profile : FileManagerBrowserTestBase::profile();
  }

  // Sets the test case name (used as a function name in test_cases.js to call.)
  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    if (log_in)
      user_manager->UserLoggedIn(info.email, info.hash, false);
    user_manager->SaveUserDisplayName(info.email,
                                      base::UTF8ToUTF16(info.display_name));
    SigninManagerFactory::GetForProfile(
        chromeos::ProfileHelper::GetProfileByUserIdHash(info.hash))
        ->SetAuthenticatedAccountInfo(info.gaia_id, info.email);
  }

 private:
  GuestMode GetGuestModeParam() const override { return NOT_IN_GUEST_MODE; }

  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

  std::string test_case_name_;
};

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_PRE_BasicDownloads DISABLED_PRE_BasicDownloads
#define MAYBE_BasicDownloads DISABLED_BasicDownloads
#else
#define MAYBE_PRE_BasicDownloads PRE_BasicDownloads
#define MAYBE_BasicDownloads BasicDownloads
#endif
IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_PRE_BasicDownloads) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_BasicDownloads) {
  AddAllUsers();

  // Sanity check that normal operations work in multi-profile setting as well.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_PRE_BasicDrive DISABLED_PRE_BasicDrive
#define MAYBE_BasicDrive DISABLED_BasicDrive
#else
#define MAYBE_PRE_BasicDrive PRE_BasicDrive
#define MAYBE_BasicDrive BasicDrive
#endif
IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_PRE_BasicDrive) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, MAYBE_BasicDrive) {
  AddAllUsers();

  // Sanity check that normal operations work in multi-profile setting as well.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

template<GuestMode M>
class GalleryBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return M; }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

 protected:
  const char* GetTestManifestName() const override {
    return "gallery_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) {
    test_case_name_ = name;
  }

 private:
  base::ListValue scripts_;
  std::string test_case_name_;
};

typedef GalleryBrowserTestBase<NOT_IN_GUEST_MODE> GalleryBrowserTest;
typedef GalleryBrowserTestBase<IN_GUEST_MODE> GalleryBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenSingleImageOnDownloads) {
  set_test_case_name("openSingleImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_OpenSingleImageOnDrive DISABLED_OpenSingleImageOnDrive
#else
#define MAYBE_OpenSingleImageOnDrive OpenSingleImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_OpenSingleImageOnDrive) {
  set_test_case_name("openSingleImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       OpenMultipleImagesOnDownloads) {
  set_test_case_name("openMultipleImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, OpenMultipleImagesOnDrive) {
  set_test_case_name("openMultipleImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       TraverseSlideImagesOnDownloads) {
  set_test_case_name("traverseSlideImagesOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, TraverseSlideImagesOnDrive) {
  set_test_case_name("traverseSlideImagesOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       RenameImageOnDownloads) {
  set_test_case_name("renameImageOnDownloads");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_RenameImageOnDrive DISABLED_RenameImageOnDrive
#else
#define MAYBE_RenameImageOnDrive RenameImageOnDrive
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_RenameImageOnDrive) {
  set_test_case_name("renameImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       DeleteImageOnDownloads) {
  set_test_case_name("deleteImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, DeleteImageOnDrive) {
  set_test_case_name("deleteImageOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       RotateImageOnDownloads) {
  set_test_case_name("rotateImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, RotateImageOnDrive) {
  set_test_case_name("rotateImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CropImageOnDownloads DISABLED_CropImageOnDownloads
#else
#define MAYBE_CropImageOnDownloads CropImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       CropImageOnDownloads) {
  set_test_case_name("cropImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, CropImageOnDrive) {
  set_test_case_name("cropImageOnDrive");
  StartTest();
}

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExposureImageOnDownloads DISABLED_ExposureImageOnDownloads
#else
#define MAYBE_ExposureImageOnDownloads ExposureImageOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, MAYBE_ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTestInGuestMode,
                       ExposureImageOnDownloads) {
  set_test_case_name("exposureImageOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(GalleryBrowserTest, ExposureImageOnDrive) {
  set_test_case_name("exposureImageOnDrive");
  StartTest();
}

template<GuestMode M>
class VideoPlayerBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return M; }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        chromeos::switches::kEnableVideoPlayerChromecastSupport);
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
  }

  const char* GetTestManifestName() const override {
    return "video_player_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) {
    test_case_name_ = name;
  }

 private:
  std::string test_case_name_;
};

typedef VideoPlayerBrowserTestBase<NOT_IN_GUEST_MODE> VideoPlayerBrowserTest;
typedef VideoPlayerBrowserTestBase<IN_GUEST_MODE>
    VideoPlayerBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenSingleVideoOnDownloads) {
  set_test_case_name("openSingleVideoOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenSingleVideoOnDrive) {
  set_test_case_name("openSingleVideoOnDrive");
  StartTest();
}

}  // namespace
}  // namespace file_manager
