// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/mock_component_extension_ime_manager_delegate.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {

namespace input_method {
namespace {

const char kTestExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";
const char kTestExtensionId2[] = "dmpipdbjkoajgdeppkffbjhngfckdloi";
const char kTestImeComponentId[] = "test_engine_id";

enum CallsBitmap {
  NONE = 0U,
  ACTIVATE = 1U,
  DEACTIVATED = 2U,
  ONFOCUS = 4U,
  ONBLUR = 8U,
  ONCOMPOSITIONBOUNDSCHANGED = 16U
};

void InitInputMethod() {
  ComponentExtensionIMEManager* comp_ime_manager =
      new ComponentExtensionIMEManager;
  MockComponentExtIMEManagerDelegate* delegate =
      new MockComponentExtIMEManagerDelegate;

  ComponentExtensionIME ext1;
  ext1.id = kTestExtensionId;

  ComponentExtensionEngine ext1_engine1;
  ext1_engine1.engine_id = kTestImeComponentId;
  ext1_engine1.language_codes.push_back("en-US");
  ext1_engine1.layouts.push_back("us");
  ext1.engines.push_back(ext1_engine1);

  std::vector<ComponentExtensionIME> ime_list;
  ime_list.push_back(ext1);
  delegate->set_ime_list(ime_list);
  comp_ime_manager->Initialize(
      scoped_ptr<ComponentExtensionIMEManagerDelegate>(delegate).Pass());

  MockInputMethodManager* manager = new MockInputMethodManager;
  manager->SetComponentExtensionIMEManager(
      scoped_ptr<ComponentExtensionIMEManager>(comp_ime_manager).Pass());
  InitializeForTesting(manager);
}

class TestObserver : public InputMethodEngineInterface::Observer {
 public:
  TestObserver() : calls_bitmap_(NONE) {}
  ~TestObserver() override {}

  void OnActivate(const std::string& engine_id) override {
    calls_bitmap_ |= ACTIVATE;
  }
  void OnDeactivated(const std::string& engine_id) override {
    calls_bitmap_ |= DEACTIVATED;
  }
  void OnFocus(
      const InputMethodEngineInterface::InputContext& context) override {
    calls_bitmap_ |= ONFOCUS;
  }
  void OnBlur(int context_id) override { calls_bitmap_ |= ONBLUR; }
  bool IsInterestedInKeyEvent() const override { return true; }
  void OnKeyEvent(const std::string& engine_id,
                  const InputMethodEngineInterface::KeyboardEvent& event,
                  input_method::KeyEventHandle* key_data) override {}
  void OnInputContextUpdate(
      const InputMethodEngineInterface::InputContext& context) override {}
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineInterface::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::string& text,
                                int cursor_pos,
                                int anchor_pos) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    calls_bitmap_ |= ONCOMPOSITIONBOUNDSCHANGED;
  }
  void OnReset(const std::string& engine_id) override {}

  unsigned char GetCallsBitmapAndReset() {
    unsigned char ret = calls_bitmap_;
    calls_bitmap_ = NONE;
    return ret;
  }

 private:
  unsigned char calls_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class InputMethodEngineTest :  public testing::Test {
 public:
  InputMethodEngineTest() : observer_(NULL), input_view_("inputview.html") {
    languages_.push_back("en-US");
    layouts_.push_back("us");
    InitInputMethod();
    IMEBridge::Initialize();
    mock_ime_input_context_handler_.reset(new MockIMEInputContextHandler());
    IMEBridge::Get()->SetInputContextHandler(
        mock_ime_input_context_handler_.get());
  }
  ~InputMethodEngineTest() override {
    IMEBridge::Get()->SetInputContextHandler(NULL);
    engine_.reset();
    Shutdown();
  }

 protected:
  void CreateEngine(bool whitelisted) {
    engine_.reset(new InputMethodEngine());
    observer_ = new TestObserver();
    scoped_ptr<InputMethodEngineInterface::Observer> observer_ptr(observer_);
    engine_->Initialize(observer_ptr.Pass(),
                        whitelisted ? kTestExtensionId : kTestExtensionId2,
                        ProfileManager::GetActiveUserProfile());
  }

  void FocusIn(ui::TextInputType input_type) {
    IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT, ui::TEXT_INPUT_FLAG_NONE);
    engine_->FocusIn(input_context);
    IMEBridge::Get()->SetCurrentInputContext(input_context);
  }

  scoped_ptr<InputMethodEngine> engine_;

  TestObserver* observer_;
  std::vector<std::string> languages_;
  std::vector<std::string> layouts_;
  GURL options_page_;
  GURL input_view_;

  scoped_ptr<MockIMEInputContextHandler> mock_ime_input_context_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineTest);
};

}  // namespace

TEST_F(InputMethodEngineTest, TestSwitching) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Enable/disable without focus.
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  // Focus change when disabled.
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_3rd_Party) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_Whitelisted) {
  CreateEngine(true);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestHistograms) {
  CreateEngine(true);
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);
  std::vector<InputMethodEngineInterface::SegmentInfo> segments;
  engine_->SetComposition(
      engine_->GetCotextIdForTesting(), "test", 0, 0, 0, segments, NULL);
  std::string error;
  base::HistogramTester histograms;
  engine_->CommitText(1, "input", &error);
  engine_->CommitText(1,
                      "\xE5\x85\xA5\xE5\x8A\x9B",  // 2 UTF-8 characters
                      &error);
  engine_->CommitText(1, "input\xE5\x85\xA5\xE5\x8A\x9B", &error);
  histograms.ExpectTotalCount("InputMethod.CommitLength", 3);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 5, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 2, 1);
  histograms.ExpectBucketCount("InputMethod.CommitLength", 7, 1);
}

TEST_F(InputMethodEngineTest, TestCompositionBoundsChanged) {
  CreateEngine(true);
  // Enable/disable with focus.
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect());
  engine_->SetCompositionBounds(rects);
  EXPECT_EQ(ONCOMPOSITIONBOUNDSCHANGED,
            observer_->GetCallsBitmapAndReset());
}

}  // namespace input_method
}  // namespace chromeos
