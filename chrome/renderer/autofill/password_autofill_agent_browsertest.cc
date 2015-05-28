// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/test_password_autofill_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/renderer/render_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using blink::WebDocument;
using blink::WebElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebString;
using blink::WebView;

namespace {

const int kPasswordFillFormDataId = 1234;

// The name of the username/password element in the form.
const char kUsernameName[] = "username";
const char kPasswordName[] = "password";
const char kEmailName[] = "email";

const char kAliceUsername[] = "alice";
const char kAlicePassword[] = "password";
const char kBobUsername[] = "bob";
const char kBobPassword[] = "secret";
const char kCarolUsername[] = "Carol";
const char kCarolPassword[] = "test";
const char kCarolAlternateUsername[] = "RealCarolUsername";

const char kFormHTML[] =
    "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

const char kVisibleFormWithNoUsernameHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body>"
    "  <form name='LoginTestForm' action='http://www.bidule.com'>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kEmptyFormHTML[] =
    "<head> <style> form {display: inline;} </style> </head>"
    "<body> <form> </form> </body>";

const char kNonVisibleFormHTML[] =
    "<head> <style> form {display: none;} </style> </head>"
    "<body>"
    "  <form>"
    "    <div>"
    "      <input type='password' id='password'/>"
    "    </div>"
    "  </form>"
    "</body>";

const char kEmptyWebpage[] =
    "<html>"
    "   <head>"
    "   </head>"
    "   <body>"
    "   </body>"
    "</html>";

const char kRedirectionWebpage[] =
    "<html>"
    "   <head>"
    "       <meta http-equiv='Content-Type' content='text/html'>"
    "       <title>Redirection page</title>"
    "       <script></script>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "         function test(){}"
    "       </script>"
    "   </body>"
    "</html>";

const char kSimpleWebpage[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <form name='LoginTestForm'>"
    "           <input type='text' id='username'/>"
    "           <input type='password' id='password'/>"
    "           <input type='submit' value='Login'/>"
    "       </form>"
    "   </body>"
    "</html>";

const char kWebpageWithDynamicContent[] =
    "<html>"
    "   <head>"
    "       <meta charset='utf-8' />"
    "       <title>Title</title>"
    "   </head>"
    "   <body>"
    "       <script type='text/javascript'>"
    "           function addParagraph() {"
    "             var p = document.createElement('p');"
    "             document.body.appendChild(p);"
    "            }"
    "           window.onload = addParagraph;"
    "       </script>"
    "   </body>"
    "</html>";

const char kJavaScriptClick[] =
    "var event = new MouseEvent('click', {"
    "   'view': window,"
    "   'bubbles': true,"
    "   'cancelable': true"
    "});"
    "var form = document.getElementById('myform1');"
    "form.dispatchEvent(event);"
    "console.log('clicked!');";

const char kOnChangeDetectionScript[] =
    "<script>"
    "  usernameOnchangeCalled = false;"
    "  passwordOnchangeCalled = false;"
    "  document.getElementById('username').onchange = function() {"
    "    usernameOnchangeCalled = true;"
    "  };"
    "  document.getElementById('password').onchange = function() {"
    "    passwordOnchangeCalled = true;"
    "  };"
    "</script>";

const char kFormHTMLWithTwoTextFields[] =
    "<FORM name='LoginTestForm' id='LoginTestForm' "
    "action='http://www.bidule.com'>"
    "  <INPUT type='text' id='username'/>"
    "  <INPUT type='text' id='email'/>"
    "  <INPUT type='password' id='password'/>"
    "  <INPUT type='submit' value='Login'/>"
    "</FORM>";

// Sets the "readonly" attribute of |element| to the value given by |read_only|.
void SetElementReadOnly(WebInputElement& element, bool read_only) {
  element.setAttribute(WebString::fromUTF8("readonly"),
                       read_only ? WebString::fromUTF8("true") : WebString());
}

}  // namespace

namespace autofill {

class PasswordAutofillAgentTest : public ChromeRenderViewTest {
 public:
  PasswordAutofillAgentTest() {
  }

  // Simulates the fill password form message being sent to the renderer.
  // We use that so we don't have to make RenderView::OnFillPasswordForm()
  // protected.
  void SimulateOnFillPasswordForm(
      const PasswordFormFillData& fill_data) {
    AutofillMsg_FillPasswordForm msg(0, kPasswordFillFormDataId, fill_data);
    static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
        ->OnMessageReceived(msg);
  }

  // As above, but fills for an iframe.
  void SimulateOnFillPasswordFormForFrame(
      WebFrame* frame,
      const PasswordFormFillData& fill_data) {
    AutofillMsg_FillPasswordForm msg(0, kPasswordFillFormDataId, fill_data);
    content::RenderFrame::FromWebFrame(frame)->OnMessageReceived(msg);
  }

  void SendVisiblePasswordForms() {
    static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
        ->DidFinishLoad();
  }

  void SetUp() override {
    ChromeRenderViewTest::SetUp();

    // Add a preferred login and an additional login to the FillData.
    username1_ = ASCIIToUTF16(kAliceUsername);
    password1_ = ASCIIToUTF16(kAlicePassword);
    username2_ = ASCIIToUTF16(kBobUsername);
    password2_ = ASCIIToUTF16(kBobPassword);
    username3_ = ASCIIToUTF16(kCarolUsername);
    password3_ = ASCIIToUTF16(kCarolPassword);
    alternate_username3_ = ASCIIToUTF16(kCarolAlternateUsername);

    FormFieldData username_field;
    username_field.name = ASCIIToUTF16(kUsernameName);
    username_field.value = username1_;
    fill_data_.username_field = username_field;

    FormFieldData password_field;
    password_field.name = ASCIIToUTF16(kPasswordName);
    password_field.value = password1_;
    password_field.form_control_type = "password";
    fill_data_.password_field = password_field;

    PasswordAndRealm password2;
    password2.password = password2_;
    fill_data_.additional_logins[username2_] = password2;
    PasswordAndRealm password3;
    password3.password = password3_;
    fill_data_.additional_logins[username3_] = password3;

    UsernamesCollectionKey key;
    key.username = username3_;
    key.password = password3_;
    key.realm = "google.com";
    fill_data_.other_possible_usernames[key].push_back(alternate_username3_);

    // We need to set the origin so it matches the frame URL and the action so
    // it matches the form action, otherwise we won't autocomplete.
    UpdateOriginForHTML(kFormHTML);
    fill_data_.action = GURL("http://www.bidule.com");

    LoadHTML(kFormHTML);

    // Now retrieve the input elements so the test can access them.
    UpdateUsernameAndPasswordElements();
  }

  void TearDown() override {
    username_element_.reset();
    password_element_.reset();
    ChromeRenderViewTest::TearDown();
  }

  void UpdateOriginForHTML(const std::string& html) {
    std::string origin = "data:text/html;charset=utf-8," + html;
    fill_data_.origin = GURL(origin);
  }

  void UpdateUsernameAndPasswordElements() {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(kUsernameName));
    ASSERT_FALSE(element.isNull());
    username_element_ = element.to<blink::WebInputElement>();
    element = document.getElementById(WebString::fromUTF8(kPasswordName));
    ASSERT_FALSE(element.isNull());
    password_element_ = element.to<blink::WebInputElement>();
  }

  blink::WebInputElement GetInputElementByID(const std::string& id) {
    WebDocument document = GetMainFrame()->document();
    WebElement element =
        document.getElementById(WebString::fromUTF8(id.c_str()));
    return element.to<blink::WebInputElement>();
  }

  void ClearUsernameAndPasswordFields() {
    username_element_.setValue("");
    username_element_.setAutofilled(false);
    password_element_.setValue("");
    password_element_.setAutofilled(false);
  }

  void SimulateDidEndEditing(WebFrame* input_frame, WebInputElement& input) {
    static_cast<blink::WebAutofillClient*>(autofill_agent_)
        ->textFieldDidEndEditing(input);
  }

  void SimulateInputChangeForElement(const std::string& new_value,
                                     bool move_caret_to_end,
                                     WebFrame* input_frame,
                                     WebInputElement& input,
                                     bool is_user_input) {
    input.setValue(WebString::fromUTF8(new_value), is_user_input);
    // The field must have focus or AutofillAgent will think the
    // change should be ignored.
    while (!input.focused())
      input_frame->document().frame()->view()->advanceFocus(false);
    if (move_caret_to_end)
      input.setSelectionRange(new_value.length(), new_value.length());
    if (is_user_input) {
      AutofillMsg_FirstUserGestureObservedInTab msg(0);
      content::RenderFrame::FromWebFrame(input_frame)->OnMessageReceived(msg);

      // Also pass the message to the testing object.
      if (input_frame == GetMainFrame())
        password_autofill_agent_->FirstUserGestureObserved();
    }
    input_frame->toWebLocalFrame()->autofillClient()->textFieldDidChange(input);
    // Processing is delayed because of a Blink bug:
    // https://bugs.webkit.org/show_bug.cgi?id=16976
    // See PasswordAutofillAgent::TextDidChangeInTextField() for details.

    // Autocomplete will trigger a style recalculation when we put up the next
    // frame, but we don't want to wait that long. Instead, trigger a style
    // recalcuation manually after TextFieldDidChangeImpl runs.
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &PasswordAutofillAgentTest::LayoutMainFrame, base::Unretained(this)));

    base::MessageLoop::current()->RunUntilIdle();
  }

  void SimulateSuggestionChoice(WebInputElement& username_input) {
    base::string16 username(base::ASCIIToUTF16(kAliceUsername));
    base::string16 password(base::ASCIIToUTF16(kAlicePassword));
    SimulateSuggestionChoiceOfUsernameAndPassword(username_input, username,
                                                  password);
  }

  void SimulateSuggestionChoiceOfUsernameAndPassword(
      WebInputElement& input,
      const base::string16& username,
      const base::string16& password) {
    // This call is necessary to setup the autofill agent appropriate for the
    // user selection; simulates the menu actually popping up.
    render_thread_->sink().ClearMessages();
    static_cast<autofill::PageClickListener*>(autofill_agent_)
        ->FormControlElementClicked(input, false);

    AutofillMsg_FillPasswordSuggestion msg(0, username, password);
    static_cast<content::RenderFrameObserver*>(autofill_agent_)
        ->OnMessageReceived(msg);
  }

  void LayoutMainFrame() {
    GetMainFrame()->view()->layout();
  }

  void SimulateUsernameChange(const std::string& username,
                              bool move_caret_to_end,
                              bool is_user_input = false) {
    SimulateInputChangeForElement(username,
                                  move_caret_to_end,
                                  GetMainFrame(),
                                  username_element_,
                                  is_user_input);
  }

  void SimulateKeyDownEvent(const WebInputElement& element,
                            ui::KeyboardCode key_code) {
    blink::WebKeyboardEvent key_event;
    key_event.windowsKeyCode = key_code;
    static_cast<blink::WebAutofillClient*>(autofill_agent_)
        ->textFieldDidReceiveKeyDown(element, key_event);
  }

  void CheckTextFieldsStateForElements(const WebInputElement& username_element,
                                       const std::string& username,
                                       bool username_autofilled,
                                       const WebInputElement& password_element,
                                       const std::string& password,
                                       bool password_autofilled,
                                       bool checkSuggestedValue) {
    EXPECT_EQ(username,
              static_cast<std::string>(username_element.value().utf8()));
    EXPECT_EQ(username_autofilled, username_element.isAutofilled());
    EXPECT_EQ(password,
              static_cast<std::string>(
                  checkSuggestedValue ? password_element.suggestedValue().utf8()
                                      : password_element.value().utf8()))
        << "checkSuggestedValue == " << checkSuggestedValue;
    EXPECT_EQ(password_autofilled, password_element.isAutofilled());
  }

  // Checks the DOM-accessible value of the username element and the
  // *suggested* value of the password element.
  void CheckTextFieldsState(const std::string& username,
                            bool username_autofilled,
                            const std::string& password,
                            bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_,
                                    username,
                                    username_autofilled,
                                    password_element_,
                                    password,
                                    password_autofilled,
                                    true);
  }

  // Checks the DOM-accessible value of the username element and the
  // DOM-accessible value of the password element.
  void CheckTextFieldsDOMState(const std::string& username,
                               bool username_autofilled,
                               const std::string& password,
                               bool password_autofilled) {
    CheckTextFieldsStateForElements(username_element_,
                                    username,
                                    username_autofilled,
                                    password_element_,
                                    password,
                                    password_autofilled,
                                    false);
  }

  void CheckUsernameSelection(int start, int end) {
    EXPECT_EQ(start, username_element_.selectionStart());
    EXPECT_EQ(end, username_element_.selectionEnd());
  }

  // Checks the message sent to PasswordAutofillManager to build the suggestion
  // list. |username| is the expected username field value, and |show_all| is
  // the expected flag for the PasswordAutofillManager, whether to show all
  // suggestions, or only those starting with |username|.
  void CheckSuggestions(const std::string& username, bool show_all) {
    const IPC::Message* message =
        render_thread_->sink().GetFirstMessageMatching(
            AutofillHostMsg_ShowPasswordSuggestions::ID);
    EXPECT_TRUE(message);
    Tuple<int, base::i18n::TextDirection, base::string16, int, gfx::RectF>
        args;
    AutofillHostMsg_ShowPasswordSuggestions::Read(message, &args);
    EXPECT_EQ(kPasswordFillFormDataId, get<0>(args));
    EXPECT_EQ(ASCIIToUTF16(username), get<2>(args));
    EXPECT_EQ(show_all, static_cast<bool>(get<3>(args) & autofill::SHOW_ALL));

    render_thread_->sink().ClearMessages();
  }

  void ExpectFormSubmittedWithUsernameAndPasswords(
      const std::string& username_value,
      const std::string& password_value,
      const std::string& new_password_value) {
    const IPC::Message* message =
        render_thread_->sink().GetFirstMessageMatching(
            AutofillHostMsg_PasswordFormSubmitted::ID);
    ASSERT_TRUE(message);
    Tuple<autofill::PasswordForm> args;
    AutofillHostMsg_PasswordFormSubmitted::Read(message, &args);
    EXPECT_EQ(ASCIIToUTF16(username_value), get<0>(args).username_value);
    EXPECT_EQ(ASCIIToUTF16(password_value), get<0>(args).password_value);
    EXPECT_EQ(ASCIIToUTF16(new_password_value),
              get<0>(args).new_password_value);
  }

  base::string16 username1_;
  base::string16 username2_;
  base::string16 username3_;
  base::string16 password1_;
  base::string16 password2_;
  base::string16 password3_;
  base::string16 alternate_username3_;
  PasswordFormFillData fill_data_;

  WebInputElement username_element_;
  WebInputElement password_element_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordAutofillAgentTest);
};

// Tests that the password login is autocompleted as expected when the browser
// sends back the password info.
TEST_F(PasswordAutofillAgentTest, InitialAutocomplete) {
  /*
   * Right now we are not sending the message to the browser because we are
   * loading a data URL and the security origin canAccessPasswordManager()
   * returns false.  May be we should mock URL loading to cirmcuvent this?
   TODO(jcivelli): find a way to make the security origin not deny access to the
                   password manager and then reenable this code.

  // The form has been loaded, we should have sent the browser a message about
  // the form.
  const IPC::Message* msg = render_thread_.sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsParsed::ID);
  ASSERT_TRUE(msg != NULL);

  Tuple1<std::vector<PasswordForm> > forms;
  AutofillHostMsg_PasswordFormsParsed::Read(msg, &forms);
  ASSERT_EQ(1U, forms.a.size());
  PasswordForm password_form = forms.a[0];
  EXPECT_EQ(PasswordForm::SCHEME_HTML, password_form.scheme);
  EXPECT_EQ(ASCIIToUTF16(kUsernameName), password_form.username_element);
  EXPECT_EQ(ASCIIToUTF16(kPasswordName), password_form.password_element);
  */

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that we correctly fill forms having an empty 'action' attribute.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForEmptyAction) {
  const char kEmptyActionFormHTML[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kEmptyActionFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin and action URLs.
  UpdateOriginForHTML(kEmptyActionFormHTML);
  fill_data_.action = fill_data_.origin;

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that if a password is marked as readonly, neither field is autofilled
// on page load.
TEST_F(PasswordAutofillAgentTest, NoInitialAutocompleteForReadOnlyPassword) {
  SetElementReadOnly(password_element_, true);

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Can still fill a password field if the username is set to a value that
// matches.
TEST_F(PasswordAutofillAgentTest,
       AutocompletePasswordForReadonlyUsernameMatched) {
  username_element_.setValue(username3_);
  SetElementReadOnly(username_element_, true);

  // Filled even though username is not the preferred match.
  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(UTF16ToUTF8(username3_), false,
                       UTF16ToUTF8(password3_), true);
}

// If a username field is empty and readonly, don't autofill.
TEST_F(PasswordAutofillAgentTest,
       NoAutocompletePasswordForReadonlyUsernameUnmatched) {
  username_element_.setValue(WebString::fromUTF8(""));
  SetElementReadOnly(username_element_, true);

  SimulateOnFillPasswordForm(fill_data_);
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a non-matching username precludes the autocomplete.
TEST_F(PasswordAutofillAgentTest, NoAutocompleteForFilledFieldUnmatched) {
  username_element_.setValue(WebString::fromUTF8("bogus"));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should be autocompleted.
  CheckTextFieldsState("bogus", false, std::string(), false);
}

// Don't try to complete a prefilled value even if it's a partial match
// to a username.
TEST_F(PasswordAutofillAgentTest, NoPartialMatchForPrefilledUsername) {
  username_element_.setValue(WebString::fromUTF8("ali"));

  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState("ali", false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, InputWithNoForms) {
  const char kNoFormInputs[] =
      "<input type='text' id='username'/>"
      "<input type='password' id='password'/>";
  LoadHTML(kNoFormInputs);

  SimulateOnFillPasswordForm(fill_data_);

  // Input elements that aren't in a <form> won't autofill.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForTextFieldPasswords) {
  const char kTextFieldPasswordFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='text' id='username'/>"
      "  <INPUT type='text' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kTextFieldPasswordFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kTextFieldPasswordFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, NoAutocompleteForPasswordFieldUsernames) {
  const char kPasswordFieldUsernameFormHTML[] =
      "<FORM name='LoginTestForm' action='http://www.bidule.com'>"
      "  <INPUT type='password' id='username'/>"
      "  <INPUT type='password' id='password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kPasswordFieldUsernameFormHTML);

  // Retrieve the input elements so the test can access them.
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kUsernameName));
  ASSERT_FALSE(element.isNull());
  username_element_ = element.to<blink::WebInputElement>();
  element = document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();

  // Set the expected form origin URL.
  UpdateOriginForHTML(kPasswordFieldUsernameFormHTML);

  SimulateOnFillPasswordForm(fill_data_);

  // Fields should still be empty.
  CheckTextFieldsState(std::string(), false, std::string(), false);
}

// Tests that having a matching username does not preclude the autocomplete.
TEST_F(PasswordAutofillAgentTest, InitialAutocompleteForMatchingFilledField) {
  username_element_.setValue(WebString::fromUTF8(kAliceUsername));

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that editing the password clears the autocompleted password field.
TEST_F(PasswordAutofillAgentTest, PasswordClearOnEdit) {
  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate the user changing the username to some unknown username.
  SimulateUsernameChange("alicia", true);

  // The password should have been cleared.
  CheckTextFieldsState("alicia", false, std::string(), false);
}

// Tests that we only autocomplete on focus lost and with a full username match
// when |wait_for_username| is true.
TEST_F(PasswordAutofillAgentTest, WaitUsername) {
  // Simulate the browser sending back the login info.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // No auto-fill should have taken place.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  // No autocomplete should happen when text is entered in the username.
  SimulateUsernameChange("a", true);
  CheckTextFieldsState("a", false, std::string(), false);
  SimulateUsernameChange("al", true);
  CheckTextFieldsState("al", false, std::string(), false);
  SimulateUsernameChange(kAliceUsername, true);
  CheckTextFieldsState(kAliceUsername, false, std::string(), false);

  // Autocomplete should happen only when the username textfield is blurred with
  // a full match.
  username_element_.setValue("a");
  static_cast<blink::WebAutofillClient*>(autofill_agent_)
      ->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("a", false, std::string(), false);
  username_element_.setValue("al");
  static_cast<blink::WebAutofillClient*>(autofill_agent_)
      ->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("al", false, std::string(), false);
  username_element_.setValue("alices");
  static_cast<blink::WebAutofillClient*>(autofill_agent_)
      ->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState("alices", false, std::string(), false);
  username_element_.setValue(ASCIIToUTF16(kAliceUsername));
  static_cast<blink::WebAutofillClient*>(autofill_agent_)
      ->textFieldDidEndEditing(username_element_);
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
}

// Tests that inline autocompletion works properly.
TEST_F(PasswordAutofillAgentTest, InlineAutocomplete) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  ClearUsernameAndPasswordFields();

  // Simulate the user typing in the first letter of 'alice', a stored
  // username.
  SimulateUsernameChange("a", true);
  // Both the username and password text fields should reflect selection of the
  // stored login.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // And the selection should have been set to 'lice', the last 4 letters.
  CheckUsernameSelection(1, 5);

  // Now the user types the next letter of the same username, 'l'.
  SimulateUsernameChange("al", true);
  // Now the fields should have the same value, but the selection should have a
  // different start value.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  CheckUsernameSelection(2, 5);

  // Test that deleting does not trigger autocomplete.
  SimulateKeyDownEvent(username_element_, ui::VKEY_BACK);
  SimulateUsernameChange("alic", true);
  CheckTextFieldsState("alic", false, std::string(), false);
  CheckUsernameSelection(4, 4);  // No selection.
  // Reset the last pressed key to something other than backspace.
  SimulateKeyDownEvent(username_element_, ui::VKEY_A);

  // Now lets say the user goes astray from the stored username and types the
  // letter 'f', spelling 'alf'.  We don't know alf (that's just sad), so in
  // practice the username should no longer be 'alice' and the selected range
  // should be empty.
  SimulateUsernameChange("alf", true);
  CheckTextFieldsState("alf", false, std::string(), false);
  CheckUsernameSelection(3, 3);  // No selection.

  // Ok, so now the user removes all the text and enters the letter 'b'.
  SimulateUsernameChange("b", true);
  // The username and password fields should match the 'bob' entry.
  CheckTextFieldsState(kBobUsername, true, kBobPassword, true);
  CheckUsernameSelection(1, 3);

  // Then, the user again removes all the text and types an uppercase 'C'.
  SimulateUsernameChange("C", true);
  // The username and password fields should match the 'Carol' entry.
  CheckTextFieldsState(kCarolUsername, true, kCarolPassword, true);
  CheckUsernameSelection(1, 5);
  // The user removes all the text and types a lowercase 'c'.  We only
  // want case-sensitive autocompletion, so the username and the selected range
  // should be empty.
  SimulateUsernameChange("c", true);
  CheckTextFieldsState("c", false, std::string(), false);
  CheckUsernameSelection(1, 1);

  // Check that we complete other_possible_usernames as well.
  SimulateUsernameChange("R", true);
  CheckTextFieldsState(kCarolAlternateUsername, true, kCarolPassword, true);
  CheckUsernameSelection(1, 17);
}

TEST_F(PasswordAutofillAgentTest, IsWebNodeVisibleTest) {
  blink::WebVector<blink::WebFormElement> forms1, forms2, forms3;
  blink::WebFrame* frame;

  LoadHTML(kVisibleFormWithNoUsernameHTML);
  frame = GetMainFrame();
  frame->document().forms(forms1);
  ASSERT_EQ(1u, forms1.size());
  EXPECT_TRUE(IsWebNodeVisible(forms1[0]));

  LoadHTML(kEmptyFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms2);
  ASSERT_EQ(1u, forms2.size());
  EXPECT_FALSE(IsWebNodeVisible(forms2[0]));

  LoadHTML(kNonVisibleFormHTML);
  frame = GetMainFrame();
  frame->document().forms(forms3);
  ASSERT_EQ(1u, forms3.size());
  EXPECT_FALSE(IsWebNodeVisible(forms3[0]));
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest) {
  render_thread_->sink().ClearMessages();
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  const IPC::Message* message = render_thread_->sink()
      .GetFirstMessageMatching(AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  Tuple<std::vector<autofill::PasswordForm>, bool> param;
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_TRUE(get<0>(param).size());

  render_thread_->sink().ClearMessages();
  LoadHTML(kEmptyFormHTML);
  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_FALSE(get<0>(param).size());

  render_thread_->sink().ClearMessages();
  LoadHTML(kNonVisibleFormHTML);
  message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID);
  EXPECT_TRUE(message);
  AutofillHostMsg_PasswordFormsRendered::Read(message, &param);
  EXPECT_FALSE(get<0>(param).size());
}

TEST_F(PasswordAutofillAgentTest, SendPasswordFormsTest_Redirection) {
  render_thread_->sink().ClearMessages();
  LoadHTML(kEmptyWebpage);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kRedirectionWebpage);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kSimpleWebpage);
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));

  render_thread_->sink().ClearMessages();
  LoadHTML(kWebpageWithDynamicContent);
  EXPECT_TRUE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordFormsRendered::ID));
}

// Tests that a password form in an iframe will not be filled in until a user
// interaction with the form.
TEST_F(PasswordAutofillAgentTest, IframeNoFillTest) {
  const char kIframeName[] = "iframe";
  const char kWebpageWithIframeStart[] =
      "<html>"
      "   <head>"
      "       <meta charset='utf-8' />"
      "       <title>Title</title>"
      "   </head>"
      "   <body>"
      "       <iframe name='iframe' src=\"";
  const char kWebpageWithIframeEnd[] =
      "\"></iframe>"
      "   </body>"
      "</html>";

  std::string origin("data:text/html;charset=utf-8,");
  origin += kSimpleWebpage;

  std::string page_html(kWebpageWithIframeStart);
  page_html += origin;
  page_html += kWebpageWithIframeEnd;

  LoadHTML(page_html.c_str());

  // Set the expected form origin and action URLs.
  fill_data_.origin = GURL(origin);
  fill_data_.action = GURL(origin);

  // Retrieve the input elements from the iframe since that is where we want to
  // test the autofill.
  WebFrame* iframe = GetMainFrame()->findChildByName(kIframeName);
  ASSERT_TRUE(iframe);

  SimulateOnFillPasswordFormForFrame(iframe, fill_data_);

  WebDocument document = iframe->document();

  WebElement username_element = document.getElementById(kUsernameName);
  WebElement password_element = document.getElementById(kPasswordName);
  ASSERT_FALSE(username_element.isNull());
  ASSERT_FALSE(password_element.isNull());

  WebInputElement username_input = username_element.to<WebInputElement>();
  WebInputElement password_input = password_element.to<WebInputElement>();
  ASSERT_FALSE(username_element.isNull());

  CheckTextFieldsStateForElements(
      username_input, "", false, password_input, "", false, false);

  // Simulate the user typing in the username in the iframe which should cause
  // an autofill.
  SimulateInputChangeForElement(
      kAliceUsername, true, iframe, username_input, true);

  CheckTextFieldsStateForElements(username_input,
                                  kAliceUsername,
                                  true,
                                  password_input,
                                  kAlicePassword,
                                  true,
                                  false);
}

// Tests that a password will only be filled as a suggested and will not be
// accessible by the DOM until a user gesture has occurred.
TEST_F(PasswordAutofillAgentTest, GestureRequiredTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);

  // However, it should only have completed with the suggested value, as tested
  // above, and it should not have completed into the DOM accessible value for
  // the password field.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Verfies that a DOM-activated UI event will not cause an autofill.
TEST_F(PasswordAutofillAgentTest, NoDOMActivationTest) {
  // Trigger the initial autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  ExecuteJavaScript(kJavaScriptClick);
  CheckTextFieldsDOMState(kAliceUsername, true, "", true);
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled on page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsOnLoad) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should have been autocompleted...
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // ... but since there hasn't been a user gesture yet, the autocompleted
  // password should only be visible to the user.
  CheckTextFieldsDOMState(kAliceUsername, true, std::string(), true);

  // A JavaScript onChange event should have been triggered for the username,
  // but not yet for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  // TODO(isherman): Re-enable this check once http://crbug.com/333144 is fixed.
  // EXPECT_EQ(0, password_onchange_called);

  // Simulate a user click so that the password field's real value is filled.
  SimulateElementClick(kUsernameName);
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // Now, a JavaScript onChange event should have been triggered for the
  // password as well.
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

// Verifies that password autofill triggers onChange events in JavaScript for
// forms that are filled after page load.
TEST_F(PasswordAutofillAgentTest,
       PasswordAutofillTriggersOnChangeEventsWaitForUsername) {
  std::string html = std::string(kFormHTML) + kOnChangeDetectionScript;
  LoadHTML(html.c_str());
  UpdateOriginForHTML(html);
  UpdateUsernameAndPasswordElements();

  // Simulate the browser sending back the login info, it triggers the
  // autocomplete.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // The username and password should not yet have been autocompleted.
  CheckTextFieldsState(std::string(), false, std::string(), false);

  // Simulate a click just to force a user gesture, since the username value is
  // set directly.
  SimulateElementClick(kUsernameName);

  // Simulate the user entering her username and selecting the matching autofill
  // from the dropdown.
  SimulateUsernameChange(kAliceUsername, true, true);
  SimulateSuggestionChoice(username_element_);

  // The username and password should now have been autocompleted.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);

  // JavaScript onChange events should have been triggered both for the username
  // and for the password.
  int username_onchange_called = -1;
  int password_onchange_called = -1;
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("usernameOnchangeCalled ? 1 : 0"),
          &username_onchange_called));
  EXPECT_EQ(1, username_onchange_called);
  ASSERT_TRUE(
      ExecuteJavaScriptAndReturnIntValue(
          ASCIIToUTF16("passwordOnchangeCalled ? 1 : 0"),
          &password_onchange_called));
  EXPECT_EQ(1, password_onchange_called);
}

// Tests that |FillSuggestion| properly fills the username and password.
TEST_F(PasswordAutofillAgentTest, FillSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the password field is not autocompletable, it should not be affected.
  SetElementReadOnly(password_element_, true);
  EXPECT_FALSE(password_autofill_agent_->FillSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);
  SetElementReadOnly(password_element_, false);

  // After filling with the suggestion, both fields should be autocompleted.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(username_length, username_length);

  // Try Filling with a suggestion with password different from the one that was
  // initially sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->FillSuggestion(
      username_element_, kBobUsername, kCarolPassword));
  CheckTextFieldsDOMState(kBobUsername, true, kCarolPassword, true);
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(username_length, username_length);
}

// Tests that |PreviewSuggestion| properly previews the username and password.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestion) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  // Neither field should have been autocompleted.
  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  // If the password field is not autocompletable, it should not be affected.
  SetElementReadOnly(password_element_, true);
  EXPECT_FALSE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(std::string(), username_element_.suggestedValue().utf8());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_EQ(std::string(), password_element_.suggestedValue().utf8());
  EXPECT_FALSE(password_element_.isAutofilled());
  SetElementReadOnly(password_element_, false);

  // After selecting the suggestion, both fields should be previewed
  // with suggested values.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(
      kAliceUsername,
      static_cast<std::string>(username_element_.suggestedValue().utf8()));
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(
      kAlicePassword,
      static_cast<std::string>(password_element_.suggestedValue().utf8()));
  EXPECT_TRUE(password_element_.isAutofilled());
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(0, username_length);

  // Try previewing with a password different from the one that was initially
  // sent to the renderer.
  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kBobUsername, kCarolPassword));
  EXPECT_EQ(
      kBobUsername,
      static_cast<std::string>(username_element_.suggestedValue().utf8()));
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(
      kCarolPassword,
      static_cast<std::string>(password_element_.suggestedValue().utf8()));
  EXPECT_TRUE(password_element_.isAutofilled());
  username_length = strlen(kBobUsername);
  CheckUsernameSelection(0, username_length);
}

// Tests that |PreviewSuggestion| properly sets the username selection range.
TEST_F(PasswordAutofillAgentTest, PreviewSuggestionSelectionRange) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));
  EXPECT_EQ(
      kAliceUsername,
      static_cast<std::string>(username_element_.suggestedValue().utf8()));
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(
      kAlicePassword,
      static_cast<std::string>(password_element_.suggestedValue().utf8()));
  EXPECT_TRUE(password_element_.isAutofilled());
  int username_length = strlen(kAliceUsername);
  CheckUsernameSelection(3, username_length);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with password being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithPasswordAutofilled) {
  password_element_.setValue(WebString::fromUTF8("sec"));
  password_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, "sec", true);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_TRUE(username_element_.value().isEmpty());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("sec"), password_element_.value());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(0, 0);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username being previously autofilled.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithUsernameAutofilled) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_EQ(ASCIIToUTF16("ali"), username_element_.value());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_TRUE(password_element_.value().isEmpty());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(password_element_.isAutofilled());
  CheckUsernameSelection(3, 3);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with username and password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithAutofilledUsernameAndPassword) {
  username_element_.setValue(WebString::fromUTF8("ali"));
  username_element_.setSelectionRange(3, 3);
  username_element_.setAutofilled(true);
  password_element_.setValue(WebString::fromUTF8("sec"));
  password_element_.setAutofilled(true);

  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState("ali", true, "sec", true);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_EQ(ASCIIToUTF16("ali"), username_element_.value());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(ASCIIToUTF16("sec"), password_element_.value());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(3, 3);
}

// Tests that |ClearPreview| properly clears previewed username and password
// with neither username nor password being previously autofilled.
TEST_F(PasswordAutofillAgentTest,
       ClearPreviewWithNotAutofilledUsernameAndPassword) {
  // Simulate the browser sending the login info, but set |wait_for_username|
  // to prevent the form from being immediately filled.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsDOMState(std::string(), false, std::string(), false);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, kAliceUsername, kAlicePassword));

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_TRUE(username_element_.value().isEmpty());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(username_element_.isAutofilled());
  EXPECT_TRUE(password_element_.value().isEmpty());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_FALSE(password_element_.isAutofilled());
  CheckUsernameSelection(0, 0);
}

// Tests that |ClearPreview| properly restores the original selection range of
// username field that has initially been filled by inline autocomplete.
TEST_F(PasswordAutofillAgentTest, ClearPreviewWithInlineAutocompletedUsername) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Simulate the user typing in the first letter of 'alice', a stored username.
  SimulateUsernameChange("a", true);
  // Both the username and password text fields should reflect selection of the
  // stored login.
  CheckTextFieldsState(kAliceUsername, true, kAlicePassword, true);
  // The selection should have been set to 'lice', the last 4 letters.
  CheckUsernameSelection(1, 5);

  EXPECT_TRUE(password_autofill_agent_->PreviewSuggestion(
      username_element_, "alicia", "secret"));
  EXPECT_EQ(
      "alicia",
      static_cast<std::string>(username_element_.suggestedValue().utf8()));
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_EQ(
      "secret",
      static_cast<std::string>(password_element_.suggestedValue().utf8()));
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(1, 6);

  EXPECT_TRUE(
      password_autofill_agent_->DidClearAutofillSelection(username_element_));

  EXPECT_EQ(kAliceUsername, username_element_.value().utf8());
  EXPECT_TRUE(username_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(username_element_.isAutofilled());
  EXPECT_TRUE(password_element_.value().isEmpty());
  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
  CheckUsernameSelection(1, 5);
}

// Tests that logging is off by default.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_NoMessage) {
  render_thread_->sink().ClearMessages();
  SendVisiblePasswordForms();
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_RecordSavePasswordProgress::ID);
  EXPECT_FALSE(message);
}

// Test that logging can be turned on by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Activated) {
  // Turn the logging on.
  AutofillMsg_SetLoggingState msg_activate(0, true);
  // Up-cast to access OnMessageReceived, which is private in the agent.
  EXPECT_TRUE(static_cast<IPC::Listener*>(password_autofill_agent_)
                  ->OnMessageReceived(msg_activate));

  render_thread_->sink().ClearMessages();
  SendVisiblePasswordForms();
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_RecordSavePasswordProgress::ID);
  EXPECT_TRUE(message);
}

// Test that logging can be turned off by a message.
TEST_F(PasswordAutofillAgentTest, OnChangeLoggingState_Deactivated) {
  // Turn the logging on and then off.
  AutofillMsg_SetLoggingState msg_activate(0, /*active=*/true);
  // Up-cast to access OnMessageReceived, which is private in the agent.
  EXPECT_TRUE(static_cast<IPC::Listener*>(password_autofill_agent_)
                  ->OnMessageReceived(msg_activate));
  AutofillMsg_SetLoggingState msg_deactivate(0, /*active=*/false);
  EXPECT_TRUE(static_cast<IPC::Listener*>(password_autofill_agent_)
                  ->OnMessageReceived(msg_deactivate));

  render_thread_->sink().ClearMessages();
  SendVisiblePasswordForms();
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_RecordSavePasswordProgress::ID);
  EXPECT_FALSE(message);
}

// Test that the agent sends an IPC call to get the current activity state of
// password saving logging soon after construction.
TEST_F(PasswordAutofillAgentTest, SendsLoggingStateUpdatePingOnConstruction) {
  const IPC::Message* message = render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_PasswordAutofillAgentConstructed::ID);
  EXPECT_TRUE(message);
}

// Tests that one user click on a username field is sufficient to bring up a
// credential suggestion popup, and the user can autocomplete the password by
// selecting the credential from the popup.
TEST_F(PasswordAutofillAgentTest, ClickAndSelect) {
  // SimulateElementClick() is called so that a user gesture is actually made
  // and the password can be filled. However, SimulateElementClick() does not
  // actually lead to the AutofillAgent's InputElementClicked() method being
  // called, so SimulateSuggestionChoice has to manually call
  // InputElementClicked().
  ClearUsernameAndPasswordFields();
  SimulateOnFillPasswordForm(fill_data_);
  SimulateElementClick(kUsernameName);
  SimulateSuggestionChoice(username_element_);
  CheckSuggestions(kAliceUsername, true);

  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// Tests the autosuggestions that are given when the element is clicked.
// Specifically, tests when the user clicks on the username element after page
// load and the element is autofilled, when the user clicks on an element that
// has a non-matching username, and when the user clicks on an element that's
// already been autofilled and they've already modified.
TEST_F(PasswordAutofillAgentTest, CredentialsOnClick) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the username element. This should produce a
  // message with all the usernames.
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, false);
  CheckSuggestions(std::string(), false);

  // Now simulate a user typing in an unrecognized username and then
  // clicking on the username element. This should also produce a message with
  // all the usernames.
  SimulateUsernameChange("baz", true);
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, true);
  CheckSuggestions("baz", true);

  // Now simulate a user typing in the first letter of the username and then
  // clicking on the username element. While the typing of the first letter will
  // inline autocomplete, clicking on the element should still produce a full
  // suggestion list.
  SimulateUsernameChange("a", true);
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(username_element_, true);
  CheckSuggestions(kAliceUsername, true);
}

// Tests that there are no autosuggestions from the password manager when the
// user clicks on the password field and the username field is editable when
// FillOnAccountSelect is enabled.
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyNoCredentialsOnPasswordClick) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce no
  // message.
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordSuggestions::ID));
}

// Tests the autosuggestions that are given when a password element is clicked,
// the username element is not editable, and FillOnAccountSelect is enabled.
// Specifically, tests when the user clicks on the password element after page
// load, and the corresponding username element is readonly (and thus
// uneditable), that the credentials for the already-filled username are
// suggested.
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyCredentialsOnPasswordClick) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Simulate the page loading with a prefilled username element that is
  // uneditable.
  username_element_.setValue("alicia");
  SetElementReadOnly(username_element_, true);

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce a
  // message with "alicia" suggested as the credential.
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  CheckSuggestions("alicia", false);
}

// Tests that there are no autosuggestions from the password manager when the
// user clicks on the password field (not the username field).
TEST_F(PasswordAutofillAgentTest, NoCredentialsOnPasswordClick) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  // Clear the text fields to start fresh.
  ClearUsernameAndPasswordFields();

  // Call SimulateElementClick() to produce a user gesture on the page so
  // autofill will actually fill.
  SimulateElementClick(kUsernameName);

  // Simulate a user clicking on the password element. This should produce no
  // message.
  render_thread_->sink().ClearMessages();
  static_cast<PageClickListener*>(autofill_agent_)
      ->FormControlElementClicked(password_element_, false);
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      AutofillHostMsg_ShowPasswordSuggestions::ID));
}

// The user types in a username and a password, but then just before sending
// the form off, a script clears them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_ScriptCleared) {
  SimulateInputChangeForElement(
      "temp", true, GetMainFrame(), username_element_, true);
  SimulateInputChangeForElement(
      "random", true, GetMainFrame(), password_element_, true);

  // Simulate that the username and the password value was cleared by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString());
  password_element_.setValue(WebString());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but this time
// it's the user who clears the username and the password. This test checks
// that in that case, the last non-empty username and password are not
// remembered.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_UserCleared) {
  SimulateInputChangeForElement(
      "temp", true, GetMainFrame(), username_element_, true);
  SimulateInputChangeForElement(
      "random", true, GetMainFrame(), password_element_, true);

  // Simulate that the user actually cleared the username and password again.
  SimulateInputChangeForElement("", true, GetMainFrame(), username_element_,
                                true);
  SimulateInputChangeForElement(
      "", true, GetMainFrame(), password_element_, true);
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent respects the user having cleared the
  // password.
  ExpectFormSubmittedWithUsernameAndPasswords("", "", "");
}

// Similar to RememberLastNonEmptyPasswordOnSubmit_ScriptCleared, but uses the
// new password instead of the current password.
TEST_F(PasswordAutofillAgentTest,
       RememberLastNonEmptyUsernameAndPasswordOnSubmit_New) {
  const char kNewPasswordFormHTML[] =
      "<FORM name='LoginTestForm'>"
      "  <INPUT type='text' id='username' autocomplete='username'/>"
      "  <INPUT type='password' id='password' autocomplete='new-password'/>"
      "  <INPUT type='submit' value='Login'/>"
      "</FORM>";
  LoadHTML(kNewPasswordFormHTML);
  UpdateUsernameAndPasswordElements();

  SimulateInputChangeForElement(
      "temp", true, GetMainFrame(), username_element_, true);
  SimulateInputChangeForElement(
      "random", true, GetMainFrame(), password_element_, true);

  // Simulate that the username and the password value was cleared by
  // the site's JavaScript before submit.
  username_element_.setValue(WebString());
  password_element_.setValue(WebString());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last non-empty
  // password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "", "random");
}

// The user first accepts a suggestion, but then overwrites the password. This
// test checks that the overwritten password is not reverted back if the user
// triggers autofill through focusing (but not changing) the username again.
TEST_F(PasswordAutofillAgentTest,
       NoopEditingDoesNotOverwriteManuallyEditedPassword) {
  // Simulate having credentials which needed to wait until the user starts
  // typing the username to be filled (e.g., PSL-matched credentials). Those are
  // the ones which can be filled as a result of TextFieldDidEndEditing.
  fill_data_.wait_for_username = true;
  SimulateOnFillPasswordForm(fill_data_);
  // Simulate that the user typed her name to make the autofill work.
  SimulateInputChangeForElement(kAliceUsername,
                                /*move_caret_to_end=*/true,
                                GetMainFrame(),
                                username_element_,
                                /*is_user_input=*/true);
  SimulateDidEndEditing(GetMainFrame(), username_element_);
  const std::string old_username(username_element_.value().utf8());
  const std::string old_password(password_element_.value().utf8());
  const std::string new_password(old_password + "modify");

  // The user changes the password.
  SimulateInputChangeForElement(new_password,
                                /*move_caret_to_end=*/true,
                                GetMainFrame(),
                                password_element_,
                                /*is_user_input=*/true);

  // The user switches back into the username field, but leaves that without
  // changes.
  SimulateDidEndEditing(GetMainFrame(), username_element_);

  // The password should have stayed as the user changed it.
  CheckTextFieldsDOMState(old_username, true, new_password, false);
  // The password should not have a suggested value.
  CheckTextFieldsState(old_username, true, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest,
       InlineAutocompleteOverwritesManuallyEditedPassword) {
  // Simulate the browser sending back the login info.
  SimulateOnFillPasswordForm(fill_data_);

  ClearUsernameAndPasswordFields();

  // The user enters a password
  SimulateInputChangeForElement("someOtherPassword",
                                /*move_caret_to_end=*/true,
                                GetMainFrame(),
                                password_element_,
                                /*is_user_input=*/true);

  // Simulate the user typing a stored username.
  SimulateUsernameChange(kAliceUsername, true);
  // The autofileld password should replace the typed one.
  CheckTextFieldsDOMState(kAliceUsername, true, kAlicePassword, true);
}

// The user types in a username and a password, but then just before sending
// the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still remember the username and the password
// typed by the user.
TEST_F(PasswordAutofillAgentTest,
       RememberLastTypedUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateInputChangeForElement("temp", true, GetMainFrame(), username_element_,
                                true);
  SimulateInputChangeForElement("random", true, GetMainFrame(),
                                password_element_, true);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// The username/password is autofilled by password manager then just before
// sending the form off, a script changes them. This test checks that
// PasswordAutofillAgent can still get the username and the password autofilled.
TEST_F(PasswordAutofillAgentTest,
       RememberLastAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the autofilled
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords(kAliceUsername, kAlicePassword,
                                              "");
}

// The username/password is autofilled by password manager then user types in a
// username and a password. Then just before sending the form off, a script
// changes them. This test checks that PasswordAutofillAgent can still remember
// the username and the password typed by the user.
TEST_F(
    PasswordAutofillAgentTest,
    RememberLastTypedAfterAutofilledUsernameAndPasswordOnSubmit_ScriptChanged) {
  SimulateOnFillPasswordForm(fill_data_);

  SimulateInputChangeForElement("temp", true, GetMainFrame(), username_element_,
                                true);
  SimulateInputChangeForElement("random", true, GetMainFrame(),
                                password_element_, true);

  // Simulate that the username and the password value was changed by the
  // site's JavaScript before submit.
  username_element_.setValue(WebString("new username"));
  password_element_.setValue(WebString("new password"));
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

// The user starts typing username then it is autofilled.
// PasswordAutofillAgent should remember the username that was autofilled,
// not last typed.
TEST_F(PasswordAutofillAgentTest, RememberAutofilledUsername) {
  SimulateInputChangeForElement("Te", true, GetMainFrame(), username_element_,
                                true);
  // Simulate that the username was changed by autofilling.
  username_element_.setValue(WebString("temp"));
  SimulateInputChangeForElement("random", true, GetMainFrame(),
                                password_element_, true);

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent still remembered the last typed
  // username and password and sent that to the browser.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

TEST_F(PasswordAutofillAgentTest, FormFillDataMustHaveUsername) {
  ClearUsernameAndPasswordFields();

  PasswordFormFillData no_username_fill_data = fill_data_;
  no_username_fill_data.username_field.name = base::string16();
  SimulateOnFillPasswordForm(no_username_fill_data);

  // The username and password should not have been autocompleted.
  CheckTextFieldsState("", false, "", false);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnly) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  ClearUsernameAndPasswordFields();

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string(), true, std::string(), false);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnlyReadonlyUsername) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  ClearUsernameAndPasswordFields();

  username_element_.setValue("alice");
  SetElementReadOnly(username_element_, true);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string("alice"), false, std::string(), true);
}

TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyNotPreferredUsername) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  ClearUsernameAndPasswordFields();

  username_element_.setValue("Carol");
  SetElementReadOnly(username_element_, true);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  CheckTextFieldsState(std::string("Carol"), false, std::string(), true);
}

TEST_F(PasswordAutofillAgentTest, FillOnAccountSelectOnlyNoUsername) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  EXPECT_TRUE(password_element_.suggestedValue().isEmpty());
  EXPECT_TRUE(password_element_.isAutofilled());
}

TEST_F(PasswordAutofillAgentTest, ShowPopupNoUsername) {
  // Load a form with no username and update test data.
  LoadHTML(kVisibleFormWithNoUsernameHTML);
  username_element_.reset();
  WebDocument document = GetMainFrame()->document();
  WebElement element =
      document.getElementById(WebString::fromUTF8(kPasswordName));
  ASSERT_FALSE(element.isNull());
  password_element_ = element.to<blink::WebInputElement>();
  fill_data_.username_field = FormFieldData();
  UpdateOriginForHTML(kVisibleFormWithNoUsernameHTML);
  fill_data_.additional_logins.clear();
  fill_data_.other_possible_usernames.clear();

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  // Simulate the browser sending back the login info for an initial page load.
  SimulateOnFillPasswordForm(fill_data_);

  password_element_.setValue("");
  password_element_.setAutofilled(false);

  SimulateSuggestionChoiceOfUsernameAndPassword(
      password_element_, base::string16(), ASCIIToUTF16(kAlicePassword));
  CheckSuggestions(std::string(), false);
  EXPECT_EQ(ASCIIToUTF16(kAlicePassword), password_element_.value());
  EXPECT_TRUE(password_element_.isAutofilled());
}

// Tests with fill-on-account-select enabled that if the username element is
// read-only and filled with an unknown username, then the password field is not
// highlighted as autofillable (regression test for https://crbug.com/442564).
TEST_F(PasswordAutofillAgentTest,
       FillOnAccountSelectOnlyReadonlyUnknownUsername) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableFillOnAccountSelect);

  ClearUsernameAndPasswordFields();

  username_element_.setValue("foobar");
  SetElementReadOnly(username_element_, true);

  CheckTextFieldsState(std::string("foobar"), false, std::string(), false);
}

// Test that the last plain text field before a password field is chosen as a
// username, in a form with 2 plain text fields without username predictions.
TEST_F(PasswordAutofillAgentTest, FindingUsernameWithoutAutofillPredictions) {
  LoadHTML(kFormHTMLWithTwoTextFields);
  UpdateUsernameAndPasswordElements();
  blink::WebInputElement email_element = GetInputElementByID(kEmailName);
  SimulateInputChangeForElement("temp", true, GetMainFrame(), username_element_,
                                true);
  SimulateInputChangeForElement("temp@google.com", true, GetMainFrame(),
                                email_element, true);
  SimulateInputChangeForElement("random", true, GetMainFrame(),
                                password_element_, true);
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent identifies the second field (e-mail)
  // as username.
  ExpectFormSubmittedWithUsernameAndPasswords("temp@google.com", "random", "");
}

// Tests that username predictions are followed when identifying the username
// in a password form with two plain text fields.
TEST_F(PasswordAutofillAgentTest, FindingUsernameWithAutofillPredictions) {
  LoadHTML(kFormHTMLWithTwoTextFields);
  UpdateUsernameAndPasswordElements();
  blink::WebInputElement email_element = GetInputElementByID(kEmailName);
  SimulateInputChangeForElement("temp", true, GetMainFrame(), username_element_,
                                true);
  SimulateInputChangeForElement("temp@google.com", true, GetMainFrame(),
                                email_element, true);
  SimulateInputChangeForElement("random", true, GetMainFrame(),
                                password_element_, true);

  // Find FormData for visible password form.
  blink::WebFormElement form_element = username_element_.form();
  FormData form_data;
  ASSERT_TRUE(WebFormElementToFormData(
      form_element, blink::WebFormControlElement(), REQUIRE_NONE, EXTRACT_NONE,
      &form_data, nullptr));
  // Simulate Autofill predictions: the first field is username.
  std::map<autofill::FormData, autofill::FormFieldData> predictions;
  predictions[form_data] = form_data.fields[0];
  AutofillMsg_AutofillUsernameDataReceived msg(0, predictions);
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->OnMessageReceived(msg);

  // The prediction should still match even if the form changes, as long
  // as the particular element doesn't change.
  std::string add_field_to_form =
      "var form = document.getElementById('LoginTestForm');"
      "var new_input = document.createElement('input');"
      "new_input.setAttribute('type', 'text');"
      "new_input.setAttribute('id', 'other_field');"
      "form.appendChild(new_input);";
  ExecuteJavaScript(add_field_to_form.c_str());

  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSendSubmitEvent(username_element_.form());
  static_cast<content::RenderFrameObserver*>(password_autofill_agent_)
      ->WillSubmitForm(username_element_.form());

  // Observe that the PasswordAutofillAgent identifies the first field as
  // username.
  ExpectFormSubmittedWithUsernameAndPasswords("temp", "random", "");
}

}  // namespace autofill
