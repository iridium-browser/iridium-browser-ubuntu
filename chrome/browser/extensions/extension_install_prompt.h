// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "extensions/common/permissions/coalesced_permission_message.h"
#include "extensions/common/url_pattern.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

class ExtensionInstallPromptShowParams;
class Profile;

namespace base {
class DictionaryValue;
class MessageLoop;
}  // namespace base

namespace content {
class WebContents;
}

namespace extensions {
class BundleInstaller;
class CrxInstallError;
class Extension;
class ExtensionInstallUI;
class ExtensionWebstorePrivateApiTest;
class MockGetAuthTokenFunction;
class PermissionSet;
}  // namespace extensions

namespace infobars {
class InfoBarDelegate;
}

// Displays all the UI around extension installation.
class ExtensionInstallPrompt
    : public base::SupportsWeakPtr<ExtensionInstallPrompt> {
 public:
  // This enum is associated with Extensions.InstallPrompt_Type UMA histogram.
  // Do not modify existing values and add new values only to the end.
  enum PromptType {
    UNSET_PROMPT_TYPE = -1,
    INSTALL_PROMPT = 0,
    INLINE_INSTALL_PROMPT,
    BUNDLE_INSTALL_PROMPT,
    RE_ENABLE_PROMPT,
    PERMISSIONS_PROMPT,
    EXTERNAL_INSTALL_PROMPT,
    POST_INSTALL_PERMISSIONS_PROMPT,
    LAUNCH_PROMPT,
    REMOTE_INSTALL_PROMPT,
    REPAIR_PROMPT,
    DELEGATED_PERMISSIONS_PROMPT,
    DELEGATED_BUNDLE_PERMISSIONS_PROMPT,
    NUM_PROMPT_TYPES
  };

  // The last prompt type to display; only used for testing.
  static PromptType g_last_prompt_type_for_tests;

  // Enumeration for permissions and retained files details.
  enum DetailsType {
    PERMISSIONS_DETAILS = 0,
    WITHHELD_PERMISSIONS_DETAILS,
    RETAINED_FILES_DETAILS,
    RETAINED_DEVICES_DETAILS,
  };

  // This enum is used to differentiate regular and withheld permissions for
  // segregation in the install prompt.
  enum PermissionsType {
    REGULAR_PERMISSIONS = 0,
    WITHHELD_PERMISSIONS,
    ALL_PERMISSIONS,
  };

  static std::string PromptTypeToString(PromptType type);

  // Extra information needed to display an installation or uninstallation
  // prompt. Gets populated with raw data and exposes getters for formatted
  // strings so that the GTK/views/Cocoa install dialogs don't have to repeat
  // that logic.
  // Ref-counted because we pass around the prompt independent of the full
  // ExtensionInstallPrompt.
  class Prompt : public base::RefCountedThreadSafe<Prompt> {
   public:
    explicit Prompt(PromptType type);

    void SetPermissions(
        const extensions::CoalescedPermissionMessages& permissions,
        PermissionsType permissions_type);
    void SetIsShowingDetails(DetailsType type,
                             size_t index,
                             bool is_showing_details);
    void SetWebstoreData(const std::string& localized_user_count,
                         bool show_user_count,
                         double average_rating,
                         int rating_count);

    PromptType type() const { return type_; }
    void set_type(PromptType type) { type_ = type; }

    // Getters for UI element labels.
    base::string16 GetDialogTitle() const;
    int GetDialogButtons() const;
    // Returns the empty string when there should be no "accept" button.
    base::string16 GetAcceptButtonLabel() const;
    base::string16 GetAbortButtonLabel() const;
    base::string16 GetPermissionsHeading(
        PermissionsType permissions_type) const;
    base::string16 GetRetainedFilesHeading() const;
    base::string16 GetRetainedDevicesHeading() const;

    bool ShouldShowPermissions() const;

    // Getters for webstore metadata. Only populated when the type is
    // INLINE_INSTALL_PROMPT, EXTERNAL_INSTALL_PROMPT, or REPAIR_PROMPT.

    // The star display logic replicates the one used by the webstore (from
    // components.ratingutils.setFractionalYellowStars). Callers pass in an
    // "appender", which will be repeatedly called back with the star images
    // that they append to the star display area.
    typedef void(*StarAppender)(const gfx::ImageSkia*, void*);
    void AppendRatingStars(StarAppender appender, void* data) const;
    base::string16 GetRatingCount() const;
    base::string16 GetUserCount() const;
    size_t GetPermissionCount(PermissionsType permissions_type) const;
    size_t GetPermissionsDetailsCount(PermissionsType permissions_type) const;
    base::string16 GetPermission(size_t index,
                                 PermissionsType permissions_type) const;
    base::string16 GetPermissionsDetails(
        size_t index,
        PermissionsType permissions_type) const;
    bool GetIsShowingDetails(DetailsType type, size_t index) const;
    size_t GetRetainedFileCount() const;
    base::string16 GetRetainedFile(size_t index) const;
    size_t GetRetainedDeviceCount() const;
    base::string16 GetRetainedDeviceMessageString(size_t index) const;

    // Populated for BUNDLE_INSTALL_PROMPT and
    // DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
    const extensions::BundleInstaller* bundle() const { return bundle_; }
    void set_bundle(const extensions::BundleInstaller* bundle) {
      bundle_ = bundle;
    }

    // Populated for all other types.
    const extensions::Extension* extension() const { return extension_; }
    void set_extension(const extensions::Extension* extension) {
      extension_ = extension;
    }

    // May be populated for POST_INSTALL_PERMISSIONS_PROMPT.
    void set_retained_files(const std::vector<base::FilePath>& retained_files) {
      retained_files_ = retained_files;
    }
    void set_retained_device_messages(
        const std::vector<base::string16>& retained_device_messages) {
      retained_device_messages_ = retained_device_messages;
    }

    const std::string& delegated_username() const {
      return delegated_username_;
    }
    void set_delegated_username(const std::string& delegated_username) {
      delegated_username_ = delegated_username;
    }

    const gfx::Image& icon() const { return icon_; }
    void set_icon(const gfx::Image& icon) { icon_ = icon; }

    bool has_webstore_data() const { return has_webstore_data_; }

   private:
    friend class base::RefCountedThreadSafe<Prompt>;

    struct InstallPromptPermissions {
      InstallPromptPermissions();
      ~InstallPromptPermissions();

      std::vector<base::string16> permissions;
      std::vector<base::string16> details;
      std::vector<bool> is_showing_details;
    };

    virtual ~Prompt();

    bool ShouldDisplayRevokeButton() const;

    // Returns the InstallPromptPermissions corresponding to
    // |permissions_type|.
    InstallPromptPermissions& GetPermissionsForType(
        PermissionsType permissions_type);
    const InstallPromptPermissions& GetPermissionsForType(
        PermissionsType permissions_type) const;

    bool ShouldDisplayRevokeFilesButton() const;

    PromptType type_;

    // Permissions that are being requested (may not be all of an extension's
    // permissions if only additional ones are being requested)
    InstallPromptPermissions prompt_permissions_;
    // Permissions that will be withheld upon install.
    InstallPromptPermissions withheld_prompt_permissions_;

    bool is_showing_details_for_retained_files_;
    bool is_showing_details_for_retained_devices_;

    // The extension or bundle being installed.
    const extensions::Extension* extension_;
    const extensions::BundleInstaller* bundle_;

    std::string delegated_username_;

    // The icon to be displayed.
    gfx::Image icon_;

    // These fields are populated only when the prompt type is
    // INLINE_INSTALL_PROMPT
    // Already formatted to be locale-specific.
    std::string localized_user_count_;
    // Range is kMinExtensionRating to kMaxExtensionRating
    double average_rating_;
    int rating_count_;

    // Whether we should display the user count (we anticipate this will be
    // false if localized_user_count_ represents the number zero).
    bool show_user_count_;

    // Whether or not this prompt has been populated with data from the
    // webstore.
    bool has_webstore_data_;

    std::vector<base::FilePath> retained_files_;
    std::vector<base::string16> retained_device_messages_;

    DISALLOW_COPY_AND_ASSIGN(Prompt);
  };

  static const int kMinExtensionRating = 0;
  static const int kMaxExtensionRating = 5;

  class Delegate {
   public:
    // We call this method to signal that the installation should continue.
    virtual void InstallUIProceed() = 0;

    // We call this method to signal that the installation should stop, with
    // |user_initiated| true if the installation was stopped by the user.
    virtual void InstallUIAbort(bool user_initiated) = 0;

   protected:
    virtual ~Delegate() {}
  };

  typedef base::Callback<void(ExtensionInstallPromptShowParams*,
                              ExtensionInstallPrompt::Delegate*,
                              scoped_refptr<ExtensionInstallPrompt::Prompt>)>
      ShowDialogCallback;

  // Callback to show the default extension install dialog.
  // The implementations of this function are platform-specific.
  static ShowDialogCallback GetDefaultShowDialogCallback();

  // Creates a dummy extension from the |manifest|, replacing the name and
  // description with the localizations if provided.
  static scoped_refptr<extensions::Extension> GetLocalizedExtensionForDisplay(
      const base::DictionaryValue* manifest,
      int flags,  // Extension::InitFromValueFlags
      const std::string& id,
      const std::string& localized_name,
      const std::string& localized_description,
      std::string* error);

  // Creates a prompt with a parent web content.
  explicit ExtensionInstallPrompt(content::WebContents* contents);

  // Creates a prompt with a profile and a native window. The most recently
  // active browser window (or a new browser window if there are no browser
  // windows) is used if a new tab needs to be opened.
  ExtensionInstallPrompt(Profile* profile, gfx::NativeWindow native_window);

  virtual ~ExtensionInstallPrompt();

  extensions::ExtensionInstallUI* install_ui() const {
    return install_ui_.get();
  }

  // This is called by the bundle installer to verify whether the bundle
  // should be installed.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |bundle|.
  virtual void ConfirmBundleInstall(
      extensions::BundleInstaller* bundle,
      const SkBitmap* icon,
      const extensions::PermissionSet* permissions);

  // This is called by the bundle installer to verify the permissions for a
  // delegated bundle install.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |bundle|.
  virtual void ConfirmPermissionsForDelegatedBundleInstall(
      extensions::BundleInstaller* bundle,
      const std::string& delegated_username,
      const SkBitmap* icon,
      const extensions::PermissionSet* permissions);

  // This is called by the standalone installer to verify whether the install
  // from the webstore should proceed.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmStandaloneInstall(Delegate* delegate,
                                        const extensions::Extension* extension,
                                        SkBitmap* icon,
                                        scoped_refptr<Prompt> prompt);

  // This is called by the installer to verify whether the installation from
  // the webstore should proceed. |show_dialog_callback| is optional and can be
  // NULL.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmWebstoreInstall(
      Delegate* delegate,
      const extensions::Extension* extension,
      const SkBitmap* icon,
      const ShowDialogCallback& show_dialog_callback);

  // This is called by the installer to verify whether the installation should
  // proceed. This is declared virtual for testing. |show_dialog_callback| is
  // optional and can be NULL.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmInstall(Delegate* delegate,
                              const extensions::Extension* extension,
                              const ShowDialogCallback& show_dialog_callback);

  // This is called by the webstore API to verify the permissions for a
  // delegated install.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmPermissionsForDelegatedInstall(
      Delegate* delegate,
      const extensions::Extension* extension,
      const std::string& delegated_username,
      const SkBitmap* icon);

  // This is called by the app handler launcher to verify whether the app
  // should be re-enabled. This is declared virtual for testing.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmReEnable(Delegate* delegate,
                               const extensions::Extension* extension);

  // This is called by the external install alert UI to verify whether the
  // extension should be enabled (external extensions are installed disabled).
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmExternalInstall(
      Delegate* delegate,
      const extensions::Extension* extension,
      const ShowDialogCallback& show_dialog_callback,
      scoped_refptr<Prompt> prompt);

  // This is called by the extension permissions API to verify whether an
  // extension may be granted additional permissions.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ConfirmPermissions(Delegate* delegate,
                                  const extensions::Extension* extension,
                                  const extensions::PermissionSet* permissions);

  // This is called by the app handler launcher to review what permissions the
  // extension or app currently has.
  //
  // We *MUST* eventually call either Proceed() or Abort() on |delegate|.
  virtual void ReviewPermissions(
      Delegate* delegate,
      const extensions::Extension* extension,
      const std::vector<base::FilePath>& retained_file_paths,
      const std::vector<base::string16>& retained_device_messages);

  // Installation was successful. This is declared virtual for testing.
  virtual void OnInstallSuccess(const extensions::Extension* extension,
                                SkBitmap* icon);

  // Installation failed. This is declared virtual for testing.
  virtual void OnInstallFailure(const extensions::CrxInstallError& error);

  void set_callback_for_test(const ShowDialogCallback& show_dialog_callback) {
    show_dialog_callback_ = show_dialog_callback;
  }

 protected:
  friend class extensions::ExtensionWebstorePrivateApiTest;
  friend class WebstoreStartupInstallUnpackFailureTest;

  // Whether or not we should record the oauth2 grant upon successful install.
  bool record_oauth2_grant_;

 private:
  friend class GalleryInstallApiTestObserver;

  // Sets the icon that will be used in any UI. If |icon| is NULL, or contains
  // an empty bitmap, then a default icon will be used instead.
  void SetIcon(const SkBitmap* icon);

  // ImageLoader callback.
  void OnImageLoaded(const gfx::Image& image);

  // Starts the process of showing a confirmation UI, which is split into two.
  // 1) Set off a 'load icon' task.
  // 2) Handle the load icon response and show the UI (OnImageLoaded).
  void LoadImageIfNeeded();

  // Shows the actual UI (the icon should already be loaded).
  void ShowConfirmation();

  Profile* profile_;

  base::MessageLoop* ui_loop_;

  // The extensions installation icon.
  SkBitmap icon_;

  // The extension we are showing the UI for, if type is not
  // BUNDLE_INSTALL_PROMPT or DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
  const extensions::Extension* extension_;

  // The bundle we are showing the UI for, if type BUNDLE_INSTALL_PROMPT or
  // DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
  const extensions::BundleInstaller* bundle_;

  // The name of the user we are asking about, if type
  // DELEGATED_PERMISSIONS_PROMPT or DELEGATED_BUNDLE_PERMISSIONS_PROMPT.
  std::string delegated_username_;

  // A custom set of permissions to show in the install prompt instead of the
  // extension's active permissions.
  scoped_refptr<const extensions::PermissionSet> custom_permissions_;

  // The object responsible for doing the UI specific actions.
  scoped_ptr<extensions::ExtensionInstallUI> install_ui_;

  // Parameters to show the confirmation UI.
  scoped_ptr<ExtensionInstallPromptShowParams> show_params_;

  // The delegate we will call Proceed/Abort on after confirmation UI.
  Delegate* delegate_;

  // A pre-filled prompt.
  scoped_refptr<Prompt> prompt_;

  // Used to show the confirm dialog.
  ShowDialogCallback show_dialog_callback_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INSTALL_PROMPT_H_
