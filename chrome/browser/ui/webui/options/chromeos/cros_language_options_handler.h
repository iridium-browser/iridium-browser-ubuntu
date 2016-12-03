// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/options/language_options_handler.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace chromeos {
namespace options {

// Language options page UI handler for Chrome OS.  For non-Chrome OS,
// see LanguageOptionsHnadler.
class CrosLanguageOptionsHandler
    : public ::options::LanguageOptionsHandlerCommon {
 public:
  CrosLanguageOptionsHandler();
  ~CrosLanguageOptionsHandler() override;

  // OptionsPageUIHandler implementation.
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;

  // DOMMessageHandler implementation.
  void RegisterMessages() override;

  // The following static methods are public for ease of testing.

  // Gets the list of supported input methods.
  // The return value will look like:
  // [{'id': 'pinyin', 'displayName': 'Pinyin',
  //   'languageCodeSet': {'zh-CW': true}},  ...]
  //
  // Note that true in languageCodeSet does not mean anything. We just use
  // the dictionary as a set.
  static base::ListValue* GetInputMethodList();

  // Converts input method descriptors to the list of input methods.
  // The return value will look like:
  // [{'id': '_ext_ime_nejguenhnsnjnwychcnsdsdjketest',
  //   'displayName': 'Sample IME'},  ...]
  static base::ListValue* ConvertInputMethodDescriptorsToIMEList(
      const input_method::InputMethodDescriptors& descriptors);

 private:
  // LanguageOptionsHandlerCommon implementation.
  void SetApplicationLocale(const std::string& language_code) override;

  // Called when the sign-out button is clicked.
  void RestartCallback(const base::ListValue* args);

  // Called when the input method is disabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodDisableCallback(const base::ListValue* args);

  // Called when the input method is enabled.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodEnableCallback(const base::ListValue* args);

  // Called when the input method options page is opened.
  // |args| will contain the input method ID as string (ex. "mozc").
  void InputMethodOptionsOpenCallback(const base::ListValue* args);

  // Adds the name of the extension that provides the IME to each entry in the
  // |list| of extension IMEs.
  void AddImeProvider(base::ListValue* list);

  DISALLOW_COPY_AND_ASSIGN(CrosLanguageOptionsHandler);
};

}  // namespace options
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_CROS_LANGUAGE_OPTIONS_HANDLER_H_
