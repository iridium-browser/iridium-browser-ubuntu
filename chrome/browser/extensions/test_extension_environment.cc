// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_environment.h"

#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

using content::BrowserThread;

namespace {

std::unique_ptr<base::DictionaryValue> MakeExtensionManifest(
    const base::Value& manifest_extra) {
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  const base::DictionaryValue* manifest_extra_dict;
  if (manifest_extra.GetAsDictionary(&manifest_extra_dict)) {
    manifest->MergeDictionary(manifest_extra_dict);
  } else {
    std::string manifest_json;
    base::JSONWriter::Write(manifest_extra, &manifest_json);
    ADD_FAILURE() << "Expected dictionary; got \"" << manifest_json << "\"";
  }
  return manifest;
}

std::unique_ptr<base::DictionaryValue> MakePackagedAppManifest() {
  return extensions::DictionaryBuilder()
      .Set("name", "Test App Name")
      .Set("version", "2.0")
      .Set("manifest_version", 2)
      .Set("app", extensions::DictionaryBuilder()
                      .Set("background",
                           extensions::DictionaryBuilder()
                               .Set("scripts", extensions::ListBuilder()
                                                   .Append("background.js")
                                                   .Build())
                               .Build())
                      .Build())
      .Build();
}

}  // namespace

// Extra environment state required for ChromeOS.
class TestExtensionEnvironment::ChromeOSEnv {
 public:
  ChromeOSEnv() {}

 private:
#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeOSEnv);
};

// static
ExtensionService* TestExtensionEnvironment::CreateExtensionServiceForProfile(
    TestingProfile* profile) {
  TestExtensionSystem* extension_system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile));
  return extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
}

TestExtensionEnvironment::TestExtensionEnvironment()
    : thread_bundle_(new content::TestBrowserThreadBundle),
      extension_service_(nullptr) {
  Init();
}

TestExtensionEnvironment::TestExtensionEnvironment(
    base::MessageLoopForUI* message_loop)
    : extension_service_(nullptr) {
  Init();
}

void TestExtensionEnvironment::Init() {
  profile_.reset(new TestingProfile);
#if defined(OS_CHROMEOS)
  if (!chromeos::DeviceSettingsService::IsInitialized())
    chromeos_env_.reset(new ChromeOSEnv);
#endif
}

TestExtensionEnvironment::~TestExtensionEnvironment() {
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_.get();
}

TestExtensionSystem* TestExtensionEnvironment::GetExtensionSystem() {
  return static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (!extension_service_)
    extension_service_ = CreateExtensionServiceForProfile(profile());
  return extension_service_;
}

ExtensionPrefs* TestExtensionEnvironment::GetExtensionPrefs() {
  return ExtensionPrefs::Get(profile_.get());
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value& manifest_extra) {
  std::unique_ptr<base::DictionaryValue> manifest =
      MakeExtensionManifest(manifest_extra);
  scoped_refptr<Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::Value& manifest_extra,
    const std::string& id) {
  std::unique_ptr<base::DictionaryValue> manifest =
      MakeExtensionManifest(manifest_extra);
  scoped_refptr<Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).SetID(id).Build();
  GetExtensionService()->AddExtension(result.get());
  return result.get();
}

scoped_refptr<Extension> TestExtensionEnvironment::MakePackagedApp(
    const std::string& id,
    bool install) {
  scoped_refptr<Extension> result = ExtensionBuilder()
                                        .SetManifest(MakePackagedAppManifest())
                                        .AddFlags(Extension::FROM_WEBSTORE)
                                        .SetID(id)
                                        .Build();
  if (install)
    GetExtensionService()->AddExtension(result.get());
  return result;
}

std::unique_ptr<content::WebContents> TestExtensionEnvironment::MakeTab()
    const {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), NULL));
  // Create a tab id.
  SessionTabHelper::CreateForWebContents(contents.get());
  return contents;
}

void TestExtensionEnvironment::DeleteProfile() {
  profile_.reset();
  extension_service_ = nullptr;
}

}  // namespace extensions
