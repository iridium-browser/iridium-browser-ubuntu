// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_
#define COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace app_modal {

class JavaScriptDialogExtensionsClient;
class JavaScriptNativeDialogFactory;

class JavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  static JavaScriptDialogManager* GetInstance();

  JavaScriptNativeDialogFactory* native_dialog_factory() {
    return native_dialog_factory_.get();
  }

  // Sets the JavaScriptNativeDialogFactory used to create platform specific
  // dialog window instances.
  void SetNativeDialogFactory(
      scoped_ptr<JavaScriptNativeDialogFactory> factory);

  // JavaScript dialogs may be opened by an extensions/app, thus they need
  // access to extensions functionality. This sets a client interface to
  // access //extensions.
  void SetExtensionsClient(
      scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client);

 private:
  friend struct DefaultSingletonTraits<JavaScriptDialogManager>;

  JavaScriptDialogManager();
  ~JavaScriptDialogManager() override;

  // JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           const GURL& origin_url,
                           const std::string& accept_lang,
                           content::JavaScriptMessageType message_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           const DialogClosedCallback& callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             const base::string16& message_text,
                             bool is_reload,
                             const DialogClosedCallback& callback) override;
  bool HandleJavaScriptDialog(content::WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;
  void CancelActiveAndPendingDialogs(
      content::WebContents* web_contents) override;
  void ResetDialogState(content::WebContents* web_contents) override;

  base::string16 GetTitle(content::WebContents* web_contents,
                          const GURL& origin_url,
                          const std::string& accept_lang,
                          bool is_alert);

  // Wrapper around a DialogClosedCallback so that we can intercept it before
  // passing it onto the original callback.
  void OnDialogClosed(content::WebContents* web_contents,
                      DialogClosedCallback callback,
                      bool success,
                      const base::string16& user_input);

  // Mapping between the WebContents and their extra data. The key
  // is a void* because the pointer is just a cookie and is never dereferenced.
  JavaScriptAppModalDialog::ExtraDataMap javascript_dialog_extra_data_;

  scoped_ptr<JavaScriptNativeDialogFactory> native_dialog_factory_;
  scoped_ptr<JavaScriptDialogExtensionsClient> extensions_client_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogManager);
};

}  // namespace app_modal

#endif  // COMPONENTS_APP_MODAL_JAVASCRIPT_DIALOG_MANAGER_H_
