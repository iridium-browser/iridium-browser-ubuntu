// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "sync/api/string_ordinal.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace {

const char kUnpackedExtensionsBlacklistedError[] =
    "Loading of unpacked extensions is disabled by the administrator.";

const char kImportMinVersionNewer[] =
    "'import' version requested is newer than what is installed.";
const char kImportMissing[] = "'import' extension is not installed.";
const char kImportNotSharedModule[] = "'import' is not a shared module.";

// Manages an ExtensionInstallPrompt for a particular extension.
class SimpleExtensionLoadPrompt : public ExtensionInstallPrompt::Delegate {
 public:
  SimpleExtensionLoadPrompt(const Extension* extension,
                            Profile* profile,
                            const base::Closure& callback);
  ~SimpleExtensionLoadPrompt() override;

  void ShowPrompt();

  // ExtensionInstallUI::Delegate
  void InstallUIProceed() override;
  void InstallUIAbort(bool user_initiated) override;

 private:
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<const Extension> extension_;
  base::Closure callback_;
};

SimpleExtensionLoadPrompt::SimpleExtensionLoadPrompt(
    const Extension* extension,
    Profile* profile,
    const base::Closure& callback)
    : extension_(extension), callback_(callback) {
  scoped_ptr<extensions::ExtensionInstallUI> ui(
      extensions::CreateExtensionInstallUI(profile));
  install_ui_.reset(new ExtensionInstallPrompt(
      profile, ui->GetDefaultInstallDialogParent()));
}

SimpleExtensionLoadPrompt::~SimpleExtensionLoadPrompt() {
}

void SimpleExtensionLoadPrompt::ShowPrompt() {
  switch (ExtensionInstallPrompt::g_auto_confirm_for_tests) {
    case ExtensionInstallPrompt::NONE:
      install_ui_->ConfirmInstall(
          this,
          extension_.get(),
          ExtensionInstallPrompt::GetDefaultShowDialogCallback());
      break;
    case ExtensionInstallPrompt::ACCEPT:
      InstallUIProceed();
      break;
    case ExtensionInstallPrompt::CANCEL:
      InstallUIAbort(false);
  }
}

void SimpleExtensionLoadPrompt::InstallUIProceed() {
  callback_.Run();
  delete this;
}

void SimpleExtensionLoadPrompt::InstallUIAbort(bool user_initiated) {
  delete this;
}

}  // namespace

namespace extensions {

// static
scoped_refptr<UnpackedInstaller> UnpackedInstaller::Create(
    ExtensionService* extension_service) {
  DCHECK(extension_service);
  return scoped_refptr<UnpackedInstaller>(
      new UnpackedInstaller(extension_service));
}

UnpackedInstaller::UnpackedInstaller(ExtensionService* extension_service)
    : service_weak_(extension_service->AsWeakPtr()),
      prompt_for_plugins_(true),
      require_modern_manifest_version_(true),
      be_noisy_on_failure_(true),
      install_checker_(extension_service->profile()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

UnpackedInstaller::~UnpackedInstaller() {
}

void UnpackedInstaller::Load(const base::FilePath& path_in) {
  DCHECK(extension_path_.empty());
  extension_path_ = path_in;
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::GetAbsolutePath, this));
}

bool UnpackedInstaller::LoadFromCommandLine(const base::FilePath& path_in,
                                            std::string* extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension_path_.empty());

  if (!service_weak_.get())
    return false;
  // Load extensions from the command line synchronously to avoid a race
  // between extension loading and loading an URL from the command line.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  extension_path_ = base::MakeAbsoluteFilePath(path_in);

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return false;
  }

  std::string error;
  install_checker_.set_extension(
      file_util::LoadExtension(
          extension_path_, Manifest::COMMAND_LINE, GetFlags(), &error).get());

  if (!extension() ||
      !extension_l10n_util::ValidateExtensionLocales(
          extension_path_, extension()->manifest()->value(), &error)) {
    ReportExtensionLoadError(error);
    return false;
  }

  PermissionsUpdater(
      service_weak_->profile(), PermissionsUpdater::INIT_FLAG_TRANSIENT)
      .InitializePermissions(extension());
  ShowInstallPrompt();

  *extension_id = extension()->id();
  return true;
}

void UnpackedInstaller::ShowInstallPrompt() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_.get())
    return;

  const ExtensionSet& disabled_extensions =
      ExtensionRegistry::Get(service_weak_->profile())->disabled_extensions();
  if (service_weak_->show_extensions_prompts() && prompt_for_plugins_ &&
      PluginInfo::HasPlugins(extension()) &&
      !disabled_extensions.Contains(extension()->id())) {
    SimpleExtensionLoadPrompt* prompt = new SimpleExtensionLoadPrompt(
        extension(),
        install_checker_.profile(),
        base::Bind(&UnpackedInstaller::StartInstallChecks, this));
    prompt->ShowPrompt();
    return;
  }
  StartInstallChecks();
}

void UnpackedInstaller::StartInstallChecks() {
  // TODO(crbug.com/421128): Enable these checks all the time.  The reason
  // they are disabled for extensions loaded from the command-line is that
  // installing unpacked extensions is asynchronous, but there can be
  // dependencies between the extensions loaded by the command line.
  if (extension()->manifest()->location() != Manifest::COMMAND_LINE) {
    ExtensionService* service = service_weak_.get();
    if (!service || service->browser_terminating())
      return;

    // TODO(crbug.com/420147): Move this code to a utility class to avoid
    // duplication of SharedModuleService::CheckImports code.
    if (SharedModuleInfo::ImportsModules(extension())) {
      const std::vector<SharedModuleInfo::ImportInfo>& imports =
          SharedModuleInfo::GetImports(extension());
      std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
      for (i = imports.begin(); i != imports.end(); ++i) {
        Version version_required(i->minimum_version);
        const Extension* imported_module =
            service->GetExtensionById(i->extension_id, true);
        if (!imported_module) {
          ReportExtensionLoadError(kImportMissing);
          return;
        } else if (imported_module &&
                   !SharedModuleInfo::IsSharedModule(imported_module)) {
          ReportExtensionLoadError(kImportNotSharedModule);
          return;
        } else if (imported_module && (version_required.IsValid() &&
                                       imported_module->version()->CompareTo(
                                           version_required) < 0)) {
          ReportExtensionLoadError(kImportMinVersionNewer);
          return;
        }
      }
    }
  }

  install_checker_.Start(
      ExtensionInstallChecker::CHECK_REQUIREMENTS |
          ExtensionInstallChecker::CHECK_MANAGEMENT_POLICY,
      true /* fail fast */,
      base::Bind(&UnpackedInstaller::OnInstallChecksComplete, this));
}

void UnpackedInstaller::OnInstallChecksComplete(int failed_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!install_checker_.policy_error().empty()) {
    ReportExtensionLoadError(install_checker_.policy_error());
    return;
  }

  if (!install_checker_.requirement_errors().empty()) {
    ReportExtensionLoadError(
        JoinString(install_checker_.requirement_errors(), ' '));
    return;
  }

  InstallExtension();
}

int UnpackedInstaller::GetFlags() {
  std::string id = crx_file::id_util::GenerateIdForPath(extension_path_);
  bool allow_file_access =
      Manifest::ShouldAlwaysAllowFileAccess(Manifest::UNPACKED);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(service_weak_->profile());
  if (prefs->HasAllowFileAccessSetting(id))
    allow_file_access = prefs->AllowFileAccess(id);

  int result = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  if (allow_file_access)
    result |= Extension::ALLOW_FILE_ACCESS;
  if (require_modern_manifest_version_)
    result |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;

  return result;
}

bool UnpackedInstaller::IsLoadingUnpackedAllowed() const {
  if (!service_weak_.get())
    return true;
  // If there is a "*" in the extension blacklist, then no extensions should be
  // allowed at all (except explicitly whitelisted extensions).
  return !ExtensionManagementFactory::GetForBrowserContext(
              service_weak_->profile())->BlacklistedByDefault();
}

void UnpackedInstaller::GetAbsolutePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  extension_path_ = base::MakeAbsoluteFilePath(extension_path_);

  std::string error;
  if (!file_util::CheckForIllegalFilenames(extension_path_, &error)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UnpackedInstaller::ReportExtensionLoadError, this, error));
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&UnpackedInstaller::CheckExtensionFileAccess, this));
}

void UnpackedInstaller::CheckExtensionFileAccess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_.get())
    return;

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::LoadWithFileAccess, this, GetFlags()));
}

void UnpackedInstaller::LoadWithFileAccess(int flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::string error;
  install_checker_.set_extension(
      file_util::LoadExtension(
          extension_path_, Manifest::UNPACKED, flags, &error).get());

  if (!extension() ||
      !extension_l10n_util::ValidateExtensionLocales(
          extension_path_, extension()->manifest()->value(), &error)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UnpackedInstaller::ReportExtensionLoadError, this, error));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::ShowInstallPrompt, this));
}

void UnpackedInstaller::ReportExtensionLoadError(const std::string &error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_weak_.get()) {
    ExtensionErrorReporter::GetInstance()->ReportLoadError(
        extension_path_,
        error,
        service_weak_->profile(),
        be_noisy_on_failure_);
  }

  if (!callback_.is_null()) {
    callback_.Run(nullptr, extension_path_, error);
    callback_.Reset();
  }
}

void UnpackedInstaller::InstallExtension() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionsUpdater perms_updater(service_weak_->profile());
  perms_updater.InitializePermissions(extension());
  perms_updater.GrantActivePermissions(extension());

  service_weak_->OnExtensionInstalled(
      extension(), syncer::StringOrdinal(), kInstallFlagInstallImmediately);

  if (!callback_.is_null()) {
    callback_.Run(extension(), extension_path_, std::string());
    callback_.Reset();
  }
}

}  // namespace extensions
