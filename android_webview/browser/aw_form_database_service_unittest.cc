// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "android_webview/browser/aw_form_database_service.h"
#include "base/android/jni_android.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util_android.h"

using autofill::AutofillWebDataService;
using autofill::FormFieldData;
using base::android::AttachCurrentThread;
using content::BrowserThread;
using testing::Test;

namespace android_webview {

class AwFormDatabaseServiceTest : public Test {
 public:
  AwFormDatabaseServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
    db_thread_.Start();
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    env_ = AttachCurrentThread();
    ASSERT_TRUE(env_ != NULL);
    ASSERT_TRUE(l10n_util::RegisterLocalizationUtil(env_));

    service_.reset(new AwFormDatabaseService(temp_dir_.path()));
  }

  void TearDown() override { service_->Shutdown(); }

  // The path to the temporary directory used for the test operations.
  base::ScopedTempDir temp_dir_;
  // A message loop for UI thread.
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  JNIEnv* env_;

  scoped_ptr<AwFormDatabaseService> service_;
};

// Disabling this test until we know why it crashes.
// TODO(sgurun): See http://crbug.com/287726 for details.
TEST_F(AwFormDatabaseServiceTest, DISABLED_HasAndClearFormData) {
  EXPECT_FALSE(service_->HasFormData());
  std::vector<FormFieldData> fields;
  FormFieldData field;
  field.name = base::ASCIIToUTF16("foo");
  field.value = base::ASCIIToUTF16("bar");
  fields.push_back(field);
  service_->get_autofill_webdata_service()->AddFormFields(fields);
  EXPECT_TRUE(service_->HasFormData());
  service_->ClearFormData();
  EXPECT_FALSE(service_->HasFormData());
}

}  // namespace android_webview
