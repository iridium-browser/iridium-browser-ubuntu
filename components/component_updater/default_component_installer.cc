// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
// TODO(ddorwin): Find a better place for ReadManifest.
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/utils.h"

using update_client::CrxComponent;

namespace component_updater {

namespace {

// Version "0" corresponds to no installed version. By the server's conventions,
// we represent it as a dotted quad.
const char kNullVersion[] = "0.0.0.0";

}  // namespace

ComponentInstallerTraits::~ComponentInstallerTraits() {
}

DefaultComponentInstaller::DefaultComponentInstaller(
    scoped_ptr<ComponentInstallerTraits> installer_traits)
    : current_version_(kNullVersion),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  installer_traits_ = installer_traits.Pass();
}

DefaultComponentInstaller::~DefaultComponentInstaller() {
}

void DefaultComponentInstaller::Register(
    ComponentUpdateService* cus,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_ = cus->GetSequencedTaskRunner();

  if (!installer_traits_) {
    NOTREACHED() << "A DefaultComponentInstaller has been created but "
                 << "has no installer traits.";
    return;
  }
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::StartRegistration,
                 this, cus),
      base::Bind(&DefaultComponentInstaller::FinishRegistration,
                 this, cus, callback));
}

void DefaultComponentInstaller::OnUpdateError(int error) {
  NOTREACHED() << "Component update error: " << error;
}

bool DefaultComponentInstaller::InstallHelper(
    const base::DictionaryValue& manifest,
    const base::FilePath& unpack_path,
    const base::FilePath& install_path) {
  if (!base::Move(unpack_path, install_path))
    return false;
  if (!installer_traits_->OnCustomInstall(manifest, install_path))
    return false;
  if (!installer_traits_->VerifyInstallation(manifest, install_path))
    return false;
  return true;
}

bool DefaultComponentInstaller::Install(const base::DictionaryValue& manifest,
                                        const base::FilePath& unpack_path) {
  std::string manifest_version;
  manifest.GetStringASCII("version", &manifest_version);
  base::Version version(manifest_version);
  if (!version.IsValid())
    return false;
  if (current_version_.CompareTo(version) > 0)
    return false;
  base::FilePath install_path =
      installer_traits_->GetBaseDirectory().AppendASCII(version.GetString());
  if (base::PathExists(install_path)) {
    if (!base::DeleteFile(install_path, true))
      return false;
  }
  if (!InstallHelper(manifest, unpack_path, install_path)) {
    base::DeleteFile(install_path, true);
    return false;
  }
  current_version_ = version;
  // TODO(ddorwin): Change the parameter to scoped_ptr<base::DictionaryValue>
  // so we can avoid this DeepCopy.
  current_manifest_.reset(manifest.DeepCopy());
  scoped_ptr<base::DictionaryValue> manifest_copy(
      current_manifest_->DeepCopy());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::ComponentReady,
                 this, base::Passed(&manifest_copy)));
  return true;
}

bool DefaultComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  if (current_version_.Equals(base::Version(kNullVersion)))
    return false;  // No component has been installed yet.

  *installed_file = installer_traits_->GetBaseDirectory()
                        .AppendASCII(current_version_.GetString())
                        .AppendASCII(file);
  return true;
}

bool DefaultComponentInstaller::Uninstall() {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::UninstallOnTaskRunner, this));
  return true;
}

void DefaultComponentInstaller::StartRegistration(ComponentUpdateService* cus) {
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath base_dir = installer_traits_->GetBaseDirectory();
  if (!base::PathExists(base_dir) && !base::CreateDirectory(base_dir)) {
    NOTREACHED() << "Could not create the base directory for "
                 << installer_traits_->GetName() << " ("
                 << base_dir.MaybeAsASCII() << ").";
    return;
  }

  base::FilePath latest_path;
  base::Version latest_version(kNullVersion);
  scoped_ptr<base::DictionaryValue> latest_manifest;

  std::vector<base::FilePath> older_paths;
  base::FileEnumerator file_enumerator(
      base_dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next();
       !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by component installer so do not try to remove them.
    if (!version.IsValid())
      continue;

    // |version| not newer than the latest found version (kNullVersion if no
    // version has been found yet) is marked for removal.
    if (version.CompareTo(latest_version) <= 0) {
      older_paths.push_back(path);
      continue;
    }

    scoped_ptr<base::DictionaryValue> manifest =
        update_client::ReadManifest(path);
    if (!manifest || !installer_traits_->VerifyInstallation(*manifest, path)) {
      DLOG(ERROR) << "Failed to read manifest or verify installation for "
                  << installer_traits_->GetName() << " ("
                  << path.MaybeAsASCII() << ").";
      older_paths.push_back(path);
      continue;
    }

    // New valid |version| folder found!

    if (latest_manifest) {
      DCHECK(!latest_path.empty());
      older_paths.push_back(latest_path);
    }

    latest_path = path;
    latest_version = version;
    latest_manifest = manifest.Pass();
  }

  if (latest_manifest) {
    current_version_ = latest_version;
    current_manifest_ = latest_manifest.Pass();
    // TODO(ddorwin): Remove these members and pass them directly to
    // FinishRegistration().
    base::ReadFileToString(latest_path.AppendASCII("manifest.fingerprint"),
                           &current_fingerprint_);
  }

  // Remove older versions of the component. None should be in use during
  // browser startup.
  for (const auto& older_path : older_paths)
    base::DeleteFile(older_path, true);
}

void DefaultComponentInstaller::UninstallOnTaskRunner() {
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  const base::FilePath base_dir = installer_traits_->GetBaseDirectory();

  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by the component installer, so do not try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeleteFile(path, true))
      DLOG(ERROR) << "Couldn't delete " << path.value();
  }

  // Delete the base directory if it's empty now.
  if (base::IsDirectoryEmpty(base_dir)) {
    if (base::DeleteFile(base_dir, false))
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
  }
}

base::FilePath DefaultComponentInstaller::GetInstallDirectory() {
  return installer_traits_->GetBaseDirectory()
      .AppendASCII(current_version_.GetString());
}

void DefaultComponentInstaller::FinishRegistration(
    ComponentUpdateService* cus,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (installer_traits_->CanAutoUpdate()) {
    CrxComponent crx;
    crx.name = installer_traits_->GetName();
    crx.installer = this;
    crx.version = current_version_;
    crx.fingerprint = current_fingerprint_;
    installer_traits_->GetHash(&crx.pk_hash);
    if (!cus->RegisterComponent(crx)) {
      NOTREACHED() << "Component registration failed for "
                   << installer_traits_->GetName();
      return;
    }

    if (!callback.is_null())
      callback.Run();
  }

  if (!current_manifest_)
    return;

  scoped_ptr<base::DictionaryValue> manifest_copy(
      current_manifest_->DeepCopy());
  ComponentReady(manifest_copy.Pass());
}

void DefaultComponentInstaller::ComponentReady(
    scoped_ptr<base::DictionaryValue> manifest) {
  installer_traits_->ComponentReady(
      current_version_, GetInstallDirectory(), manifest.Pass());
}

}  // namespace component_updater
