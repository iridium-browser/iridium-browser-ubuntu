// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/enumerate_modules_model.h"

#include <Tlhelp32.h>
#include <stddef.h>
#include <stdint.h>
#include <wintrust.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/service_providers_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"
#include "crypto/sha2.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// The period of time (in milliseconds) to wait until checking to see if any
// incompatible modules exist.
static const int kModuleCheckDelayMs = 45 * 1000;

// The path to the Shell Extension key in the Windows registry.
static const wchar_t kRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

// Short-hand for things on the blacklist you should simply get rid of.
static const ModuleEnumerator::RecommendedAction kUninstallLink =
    static_cast<ModuleEnumerator::RecommendedAction>(
        ModuleEnumerator::UNINSTALL | ModuleEnumerator::SEE_LINK);

// Short-hand for things on the blacklist we are investigating and have info.
static const ModuleEnumerator::RecommendedAction kInvestigatingLink =
    static_cast<ModuleEnumerator::RecommendedAction>(
        ModuleEnumerator::INVESTIGATING | ModuleEnumerator::SEE_LINK);

// A sort method that sorts by bad modules first, then by full name (including
// path).
static bool ModuleSort(const ModuleEnumerator::Module& a,
                       const ModuleEnumerator::Module& b) {
  if (a.status != b.status)
    return a.status > b.status;

  if (a.location == b.location)
    return a.name < b.name;

  return a.location < b.location;
}

namespace {

// Used to protect the LoadedModuleVector which is accessed
// from both the UI thread and the FILE thread.
base::Lock* lock = NULL;

// A struct to help de-duping modules before adding them to the enumerated
// modules vector.
struct FindModule {
 public:
  explicit FindModule(const ModuleEnumerator::Module& x)
    : module(x) {}
  bool operator()(const ModuleEnumerator::Module& module_in) const {
    return (module.location == module_in.location) &&
           (module.name == module_in.name);
  }

  const ModuleEnumerator::Module& module;
};

// Returns the long path name given a short path name. A short path name is a
// path that follows the 8.3 convention and has ~x in it. If the path is already
// a long path name, the function returns the current path without modification.
bool ConvertToLongPath(const base::string16& short_path,
                       base::string16* long_path) {
  wchar_t long_path_buf[MAX_PATH];
  DWORD return_value = GetLongPathName(short_path.c_str(), long_path_buf,
                                       MAX_PATH);
  if (return_value != 0 && return_value < MAX_PATH) {
    *long_path = long_path_buf;
    return true;
  }

  return false;
}

}  // namespace

ModuleEnumerator::Module::Module() {
}

ModuleEnumerator::Module::Module(const Module& rhs) = default;

ModuleEnumerator::Module::Module(ModuleType type,
                                 ModuleStatus status,
                                 const base::string16& location,
                                 const base::string16& name,
                                 const base::string16& product_name,
                                 const base::string16& description,
                                 const base::string16& version,
                                 const base::string16& digital_signer,
                                 RecommendedAction recommended_action)
    : type(type),
      status(status),
      location(location),
      name(name),
      product_name(product_name),
      description(description),
      version(version),
      digital_signer(digital_signer),
      recommended_action(recommended_action),
      duplicate_count(0),
      normalized(false) {
}

ModuleEnumerator::Module::~Module() {
}

// The browser process module blacklist. This lists modules that are known
// to cause compatibility issues within the browser process. When adding to this
// list, make sure that all paths are lower-case, in long pathname form, end
// with a slash and use environments variables (or just look at one of the
// comments below and keep it consistent with that). When adding an entry with
// an environment variable not currently used in the list below, make sure to
// update the list in PreparePathMappings. Filename, Description/Signer, and
// Location must be entered as hashes (see GenerateHash). Filename is mandatory.
// Entries without any Description, Signer info, or Location will never be
// marked as confirmed bad (only as suspicious).
const ModuleEnumerator::BlacklistEntry ModuleEnumerator::kModuleBlacklist[] = {
  // NOTE: Please keep this list sorted by dll name, then location.

  // Version 3.2.1.6 seems to be implicated in most cases (and 3.2.2.2 in some).
  // There is a more recent version available for download.
  // accelerator.dll, "%programfiles%\\speedbit video accelerator\\".
  { "7ba9402f", "c9132d48", "", "", "", ALL, kInvestigatingLink },

  // apiqq0.dll, "%temp%\\".
  { "26134911", "59145acf", "", "", "", ALL, kUninstallLink },

  // arking0.dll, "%systemroot%\\system32\\".
  { "f5d8f549", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // arking1.dll, "%systemroot%\\system32\\".
  { "c60ca062", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // aswjsflt.dll, "%ProgramFiles%\\avast software\\avast\\", "AVAST Software".
  // NOTE: The digital signature of the DLL is double null terminated.
  // Avast Antivirus prior to version 8.0 would kill the Chrome child process
  // when blocked from running.
  { "2ea5422a", "6b3a1b00", "a7db0e0c", "", "8.0", XP,
    static_cast<RecommendedAction>(UPDATE | SEE_LINK | NOTIFY_USER) },

  // aswjsflt.dll, "%ProgramFiles%\\alwil software\\avast5\\", "AVAST Software".
  // NOTE: The digital signature of the DLL is double null terminated.
  // Avast Antivirus prior to version 8.0 would kill the Chrome child process
  // when blocked from running.
  { "2ea5422a", "d8686924", "a7db0e0c", "", "8.0", XP,
    static_cast<RecommendedAction>(UPDATE | SEE_LINK | NOTIFY_USER) },

  // Said to belong to Killer NIC from BigFoot Networks (not verified). Versions
  // 6.0.0.7 and 6.0.0.10 implicated.
  // bfllr.dll, "%systemroot%\\system32\\".
  { "6bb57633", "23d01d5b", "", "", "", ALL, kInvestigatingLink },

  // clickpotatolitesahook.dll, "". Different version each report.
  { "0396e037.dll", "", "", "", "", ALL, kUninstallLink },

  // cvasds0.dll, "%temp%\\".
  { "5ce0037c", "59145acf", "", "", "", ALL, kUninstallLink },

  // cwalsp.dll, "%systemroot%\\system32\\".
  { "e579a039", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // datamngr.dll (1), "%programfiles%\\searchqu toolbar\\datamngr\\".
  { "7add320b", "470a3da3", "", "", "", ALL, kUninstallLink },

  // datamngr.dll (2), "%programfiles%\\windows searchqu toolbar\\".
  { "7add320b", "7a3c8be3", "", "", "", ALL, kUninstallLink },

  // dsoqq0.dll, "%temp%\\".
  { "1c4df325", "59145acf", "", "", "", ALL, kUninstallLink },

  // flt.dll, "%programfiles%\\tueagles\\".
  { "6d01f4a1", "7935e9c2", "", "", "", ALL, kUninstallLink },

  // This looks like a malware edition of a Brazilian Bank plugin, sometimes
  // referred to as Malware.Banc.A.
  // gbieh.dll, "%programfiles%\\gbplugin\\".
  { "4cb4f2e3", "88e4a3b1", "", "", "", ALL, kUninstallLink },

  // hblitesahook.dll. Each report has different version number in location.
  { "5d10b363", "", "", "", "", ALL, kUninstallLink },

  // icf.dll, "%systemroot%\\system32\\".
  { "303825ed", "23d01d5b", "", "", "", ALL, INVESTIGATING },

  // idmmbc.dll (IDM), "%systemroot%\\system32\\". See: http://crbug.com/26892/.
  { "b8dce5c3", "23d01d5b", "", "", "6.03", ALL,
      static_cast<RecommendedAction>(UPDATE | DISABLE) },

  // imon.dll (NOD32), "%systemroot%\\system32\\". See: http://crbug.com/21715.
  { "8f42f22e", "23d01d5b", "", "", "4.0", ALL,
      static_cast<RecommendedAction>(UPDATE | DISABLE) },

  // is3lsp.dll, "%commonprogramfiles%\\is3\\anti-spyware\\".
  { "7ffbdce9", "bc5673f2", "", "", "", ALL,
      static_cast<RecommendedAction>(UPDATE | DISABLE | SEE_LINK) },

  // jsi.dll, "%programfiles%\\profilecraze\\".
  { "f9555eea", "e3548061", "", "", "", ALL, kUninstallLink },

  // kernel.dll, "%programfiles%\\contentwatch\\internet protection\\modules\\".
  { "ead2768e", "4e61ce60", "", "", "", ALL, INVESTIGATING },

  // mgking0.dll, "%systemroot%\\system32\\".
  { "d0893e38", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // mgking0.dll, "%temp%\\".
  { "d0893e38", "59145acf", "", "", "", ALL, kUninstallLink },

  // mgking1.dll, "%systemroot%\\system32\\".
  { "3e837222", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // mgking1.dll, "%temp%\\".
  { "3e837222", "59145acf", "", "", "", ALL, kUninstallLink },

  // mstcipha.ime, "%systemroot%\\system32\\".
  { "5523579e", "23d01d5b", "", "", "", ALL, INVESTIGATING },

  // mwtsp.dll, "%systemroot%\\system32\\".
  { "9830bff6", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // nodqq0.dll, "%temp%\\".
  { "b86ce04d", "59145acf", "", "", "", ALL, kUninstallLink },

  // nProtect GameGuard Anti-cheat system. Every report has a different
  // location, since it is installed into and run from a game folder. Various
  // versions implicated.
  // npggnt.des, no fixed location.
  { "f2c8790d", "", "", "", "", ALL, kInvestigatingLink },

  // nvlsp.dll,
  // "%programfiles%\\nvidia corporation\\networkaccessmanager\\bin32\\".
  { "37f907e2", "3ad0ff23", "", "", "", ALL, INVESTIGATING },

  // post0.dll, "%systemroot%\\system32\\".
  { "7405c0c8", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // questbrwsearch.dll, "%programfiles%\\questbrwsearch\\".
  { "0953ed09", "f0d5eeda", "", "", "", ALL, kUninstallLink },

  // questscan.dll, "%programfiles%\\questscan\\".
  { "f4f3391e", "119d20f7", "", "", "", ALL, kUninstallLink },

  // radhslib.dll (Naomi web filter), "%programfiles%\\rnamfler\\".
  // See http://crbug.com/12517.
  { "7edcd250", "0733dc3e", "", "", "", ALL, INVESTIGATING },

  // rlls.dll, "%programfiles%\\relevantknowledge\\".
  { "a1ed94a7", "ea9d6b36", "", "", "", ALL, kUninstallLink },

  // rooksdol.dll, "%programfiles%\\trusteer\\rapport\\bin\\".
  { "802aefef", "06120e13", "", "", "3.5.1008.40", ALL, UPDATE },

  // scanquery.dll, "%programfiles%\\scanquery\\".
  { "0b52d2ae", "a4cc88b1", "", "", "", ALL, kUninstallLink },

  // sdata.dll, "%programdata%\\srtserv\\".
  { "1936d5cc", "223c44be", "", "", "", ALL, kUninstallLink },

  // searchtree.dll,
  // "%programfiles%\\contentwatch\\internet protection\\modules\\".
  { "f6915a31", "4e61ce60", "", "", "", ALL, INVESTIGATING },

  // sgprxy.dll, "%commonprogramfiles%\\is3\\anti-spyware\\".
  { "005965ea", "bc5673f2", "", "", "", ALL, INVESTIGATING },

  // snxhk.dll, "%ProgramFiles%\\avast software\\avast\\", "AVAST Software".
  // NOTE: The digital signature of the DLL is double null terminated.
  // Avast Antivirus prior to version 8.0 would kill the Chrome child process
  // when blocked from running.
  { "46c16aa8", "6b3a1b00", "a7db0e0c", "", "8.0", XP,
    static_cast<RecommendedAction>(UPDATE | SEE_LINK | NOTIFY_USER) },

  // snxhk.dll, "%ProgramFiles%\\alwil software\\avast5\\", "AVAST Software".
  // NOTE: The digital signature of the DLL is double null terminated.
  // Avast Antivirus prior to version 8.0 would kill the Chrome child process
  // when blocked from running.
  { "46c16aa8", "d8686924", "a7db0e0c", "", "8.0", XP,
    static_cast<RecommendedAction>(UPDATE | SEE_LINK | NOTIFY_USER) },

  // sprotector.dll, "". Different location each report.
  { "24555d74", "", "", "", "", ALL, kUninstallLink },

  // swi_filter_0001.dll (Sophos Web Intelligence),
  // "%programfiles%\\sophos\\sophos anti-virus\\web intelligence\\".
  // A small random sample all showed version 1.0.5.0.
  { "61112d7b", "25fb120f", "", "", "", ALL, kInvestigatingLink },

  // twking0.dll, "%systemroot%\\system32\\".
  { "0355549b", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // twking1.dll, "%systemroot%\\system32\\".
  { "02e44508", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // vksaver.dll, "%systemroot%\\system32\\".
  { "c4a784d5", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // vlsp.dll (Venturi Firewall?), "%systemroot%\\system32\\".
  { "2e4eb93d", "23d01d5b", "", "", "", ALL, INVESTIGATING },

  // vmn3_1dn.dll, "%appdata%\\roaming\\vmndtxtb\\".
  { "bba2037d", "9ab68585", "", "", "", ALL, kUninstallLink },

  // webanalyzer.dll,
  // "%programfiles%\\contentwatch\\internet protection\\modules\\".
  { "c70b697d", "4e61ce60", "", "", "", ALL, INVESTIGATING },

  // wowst0.dll, "%systemroot%\\system32\\".
  { "38ad9963", "23d01d5b", "", "", "", ALL, kUninstallLink },

  // wxbase28u_vc_cw.dll, "%systemroot%\\system32\\".
  { "e967210d", "23d01d5b", "", "", "", ALL, kUninstallLink },
};

// Generates an 8 digit hash from the input given.
static void GenerateHash(const std::string& input, std::string* output) {
  if (input.empty()) {
    *output = "";
    return;
  }

  uint8_t hash[4];
  crypto::SHA256HashString(input, hash, sizeof(hash));
  *output = base::ToLowerASCII(base::HexEncode(hash, sizeof(hash)));
}

// -----------------------------------------------------------------------------

// static
void ModuleEnumerator::NormalizeModule(Module* module) {
  base::string16 path = module->location;
  if (!ConvertToLongPath(path, &module->location))
    module->location = path;

  module->location = base::i18n::ToLower(module->location);

  // Location contains the filename, so the last slash is where the path
  // ends.
  size_t last_slash = module->location.find_last_of(L"\\");
  if (last_slash != base::string16::npos) {
    module->name = module->location.substr(last_slash + 1);
    module->location = module->location.substr(0, last_slash + 1);
  } else {
    module->name = module->location;
    module->location.clear();
  }

  // Some version strings have things like (win7_rtm.090713-1255) appended
  // to them. Remove that.
  size_t first_space = module->version.find_first_of(L" ");
  if (first_space != base::string16::npos)
    module->version = module->version.substr(0, first_space);

  module->normalized = true;
}

// static
ModuleEnumerator::ModuleStatus ModuleEnumerator::Match(
    const ModuleEnumerator::Module& module,
    const ModuleEnumerator::BlacklistEntry& blacklisted) {
  // All modules must be normalized before matching against blacklist.
  DCHECK(module.normalized);
  // Filename is mandatory and version should not contain spaces.
  DCHECK(strlen(blacklisted.filename) > 0);
  DCHECK(!strstr(blacklisted.version_from, " "));
  DCHECK(!strstr(blacklisted.version_to, " "));

  base::win::Version version = base::win::GetVersion();
  switch (version) {
     case base::win::VERSION_XP:
      if (!(blacklisted.os & XP)) return NOT_MATCHED;
      break;
    default:
      break;
  }

  std::string filename_hash, location_hash;
  GenerateHash(base::WideToUTF8(module.name), &filename_hash);
  GenerateHash(base::WideToUTF8(module.location), &location_hash);

  // Filenames are mandatory. Location is mandatory if given.
  if (filename_hash == blacklisted.filename &&
          (std::string(blacklisted.location).empty() ||
          location_hash == blacklisted.location)) {
    // We have a name match against the blacklist (and possibly location match
    // also), so check version.
    Version module_version(base::UTF16ToASCII(module.version));
    Version version_min(blacklisted.version_from);
    Version version_max(blacklisted.version_to);
    bool version_ok = !version_min.IsValid() && !version_max.IsValid();
    if (!version_ok) {
      bool too_low = version_min.IsValid() &&
          (!module_version.IsValid() ||
          module_version.CompareTo(version_min) < 0);
      bool too_high = version_max.IsValid() &&
          (!module_version.IsValid() ||
          module_version.CompareTo(version_max) >= 0);
      version_ok = !too_low && !too_high;
    }

    if (version_ok) {
      // At this point, the names match and there is no version specified
      // or the versions also match.

      std::string desc_or_signer(blacklisted.desc_or_signer);
      std::string signer_hash, description_hash;
      GenerateHash(base::WideToUTF8(module.digital_signer), &signer_hash);
      GenerateHash(base::WideToUTF8(module.description), &description_hash);

      // If signatures match (or both are empty), then we have a winner.
      if (signer_hash == desc_or_signer)
        return CONFIRMED_BAD;

      // If descriptions match (or both are empty) and the locations match, then
      // we also have a confirmed match.
      if (description_hash == desc_or_signer &&
          !location_hash.empty() && location_hash == blacklisted.location)
        return CONFIRMED_BAD;

      // We are not sure, but it is likely bad.
      return SUSPECTED_BAD;
    }
  }

  return NOT_MATCHED;
}

ModuleEnumerator::ModuleEnumerator(EnumerateModulesModel* observer)
    : enumerated_modules_(NULL),
      observer_(observer),
      limited_mode_(false),
      callback_thread_id_(BrowserThread::ID_COUNT) {
}

void ModuleEnumerator::ScanNow(ModulesVector* list, bool limited_mode) {
  enumerated_modules_ = list;

  limited_mode_ = limited_mode;

  if (!limited_mode_) {
    CHECK(BrowserThread::GetCurrentThreadIdentifier(&callback_thread_id_));
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&ModuleEnumerator::ScanImpl, this));
  } else {
    // Run it synchronously.
    ScanImpl();
  }
}

ModuleEnumerator::~ModuleEnumerator() {
}

void ModuleEnumerator::ScanImpl() {
  base::TimeTicks start_time = base::TimeTicks::Now();

  enumerated_modules_->clear();

  // Make sure the path mapping vector is setup so we can collapse paths.
  PreparePathMappings();

  // Enumerating loaded modules must happen first since the other types of
  // modules check for duplication against the loaded modules.
  base::TimeTicks checkpoint = base::TimeTicks::Now();
  EnumerateLoadedModules();
  base::TimeTicks checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateLoadedModules",
                      checkpoint2 - checkpoint);

  checkpoint = checkpoint2;
  EnumerateShellExtensions();
  checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateShellExtensions",
                      checkpoint2 - checkpoint);

  checkpoint = checkpoint2;
  EnumerateWinsockModules();
  checkpoint2 = base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Conflicts.EnumerateWinsockModules",
                      checkpoint2 - checkpoint);

  MatchAgainstBlacklist();

  std::sort(enumerated_modules_->begin(),
            enumerated_modules_->end(), ModuleSort);

  if (!limited_mode_) {
    // Send a reply back on the UI thread.
    BrowserThread::PostTask(callback_thread_id_, FROM_HERE,
                            base::Bind(&ModuleEnumerator::ReportBack, this));
  } else {
    // We are on the main thread already.
    ReportBack();
  }

  UMA_HISTOGRAM_TIMES("Conflicts.EnumerationTotalTime",
                      base::TimeTicks::Now() - start_time);
}

void ModuleEnumerator::EnumerateLoadedModules() {
  // Get all modules in the current process.
  base::win::ScopedHandle snap(::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,
                               ::GetCurrentProcessId()));
  if (!snap.Get())
    return;

  // Walk the module list.
  MODULEENTRY32 module = { sizeof(module) };
  if (!::Module32First(snap.Get(), &module))
    return;

  do {
    // It would be weird to present chrome.exe as a loaded module.
    if (_wcsicmp(chrome::kBrowserProcessExecutableName, module.szModule) == 0)
      continue;

    Module entry;
    entry.type = LOADED_MODULE;
    entry.location = module.szExePath;
    PopulateModuleInformation(&entry);

    NormalizeModule(&entry);
    CollapsePath(&entry);
    enumerated_modules_->push_back(entry);
  } while (::Module32Next(snap.Get(), &module));
}

void ModuleEnumerator::EnumerateShellExtensions() {
  ReadShellExtensions(HKEY_LOCAL_MACHINE);
  ReadShellExtensions(HKEY_CURRENT_USER);
}

void ModuleEnumerator::ReadShellExtensions(HKEY parent) {
  base::win::RegistryValueIterator registration(parent, kRegPath);
  while (registration.Valid()) {
    std::wstring key(std::wstring(L"CLSID\\") + registration.Name() +
        L"\\InProcServer32");
    base::win::RegKey clsid;
    if (clsid.Open(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ) != ERROR_SUCCESS) {
      ++registration;
      continue;
    }
    base::string16 dll;
    if (clsid.ReadValue(L"", &dll) != ERROR_SUCCESS) {
      ++registration;
      continue;
    }
    clsid.Close();

    Module entry;
    entry.type = SHELL_EXTENSION;
    entry.location = dll;
    PopulateModuleInformation(&entry);

    NormalizeModule(&entry);
    CollapsePath(&entry);
    AddToListWithoutDuplicating(entry);

    ++registration;
  }
}

void ModuleEnumerator::EnumerateWinsockModules() {
  // Add to this list the Winsock LSP DLLs.
  WinsockLayeredServiceProviderList layered_providers;
  GetWinsockLayeredServiceProviders(&layered_providers);
  for (size_t i = 0; i < layered_providers.size(); ++i) {
    Module entry;
    entry.type = WINSOCK_MODULE_REGISTRATION;
    entry.status = NOT_MATCHED;
    entry.normalized = false;
    entry.location = layered_providers[i].path;
    entry.description = layered_providers[i].name;
    entry.recommended_action = NONE;
    entry.duplicate_count = 0;

    wchar_t expanded[MAX_PATH];
    DWORD size = ExpandEnvironmentStrings(
        entry.location.c_str(), expanded, MAX_PATH);
    if (size != 0 && size <= MAX_PATH) {
      entry.digital_signer =
          GetSubjectNameFromDigitalSignature(base::FilePath(expanded));
    }
    entry.version = base::IntToString16(layered_providers[i].version);

    // Paths have already been collapsed.
    NormalizeModule(&entry);
    AddToListWithoutDuplicating(entry);
  }
}

void ModuleEnumerator::PopulateModuleInformation(Module* module) {
  module->status = NOT_MATCHED;
  module->duplicate_count = 0;
  module->normalized = false;
  module->digital_signer =
      GetSubjectNameFromDigitalSignature(base::FilePath(module->location));
  module->recommended_action = NONE;
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(module->location)));
  if (version_info) {
    module->description = version_info->file_description();
    module->version = version_info->file_version();
    module->product_name = version_info->product_name();
  }
}

void ModuleEnumerator::AddToListWithoutDuplicating(const Module& module) {
  DCHECK(module.normalized);
  // These are registered modules, not loaded modules so the same module
  // can be registered multiple times, often dozens of times. There is no need
  // to list each registration, so we just increment the count for each module
  // that is counted multiple times.
  ModulesVector::iterator iter;
  iter = std::find_if(enumerated_modules_->begin(),
                      enumerated_modules_->end(),
                      FindModule(module));
  if (iter != enumerated_modules_->end()) {
    iter->duplicate_count++;
    iter->type = static_cast<ModuleType>(iter->type | module.type);
  } else {
    enumerated_modules_->push_back(module);
  }
}

void ModuleEnumerator::PreparePathMappings() {
  path_mapping_.clear();

  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  std::vector<base::string16> env_vars;
  env_vars.push_back(L"LOCALAPPDATA");
  env_vars.push_back(L"ProgramFiles");
  env_vars.push_back(L"ProgramData");
  env_vars.push_back(L"USERPROFILE");
  env_vars.push_back(L"SystemRoot");
  env_vars.push_back(L"TEMP");
  env_vars.push_back(L"TMP");
  env_vars.push_back(L"CommonProgramFiles");
  for (std::vector<base::string16>::const_iterator variable = env_vars.begin();
       variable != env_vars.end(); ++variable) {
    std::string path;
    if (environment->GetVar(base::UTF16ToASCII(*variable).c_str(), &path)) {
      path_mapping_.push_back(
          std::make_pair(base::i18n::ToLower(base::UTF8ToUTF16(path)) + L"\\",
                         L"%" + base::i18n::ToLower(*variable) + L"%"));
    }
  }
}

void ModuleEnumerator::CollapsePath(Module* entry) {
  // Take the path and see if we can use any of the substitution values
  // from the vector constructed above to replace c:\windows with, for
  // example, %systemroot%. The most collapsed path (the one with the
  // minimum length) wins.
  size_t min_length = MAXINT;
  base::string16 location = entry->location;
  for (PathMapping::const_iterator mapping = path_mapping_.begin();
       mapping != path_mapping_.end(); ++mapping) {
    const base::string16& prefix = mapping->first;
    if (base::StartsWith(base::i18n::ToLower(location),
                         base::i18n::ToLower(prefix),
                         base::CompareCase::SENSITIVE)) {
      base::string16 new_location = mapping->second +
                              location.substr(prefix.length() - 1);
      size_t length = new_location.length() - mapping->second.length();
      if (length < min_length) {
        entry->location = new_location;
        min_length = length;
      }
    }
  }
}

void ModuleEnumerator::MatchAgainstBlacklist() {
  for (size_t m = 0; m < enumerated_modules_->size(); ++m) {
    // Match this module against the blacklist.
    Module* module = &(*enumerated_modules_)[m];
    module->status = GOOD;  // We change this below potentially.
    for (size_t i = 0; i < arraysize(kModuleBlacklist); ++i) {
      #if !defined(NDEBUG)
        // This saves time when constructing the blacklist.
        std::string hashes(kModuleBlacklist[i].filename);
        std::string hash1, hash2, hash3;
        GenerateHash(kModuleBlacklist[i].filename, &hash1);
        hashes += " - " + hash1;
        GenerateHash(kModuleBlacklist[i].location, &hash2);
        hashes += " - " + hash2;
        GenerateHash(kModuleBlacklist[i].desc_or_signer, &hash3);
        hashes += " - " + hash3;
      #endif

      ModuleStatus status = Match(*module, kModuleBlacklist[i]);
      if (status != NOT_MATCHED) {
        // We have a match against the blacklist. Mark it as such.
        module->status = status;
        module->recommended_action = kModuleBlacklist[i].help_tip;
        break;
      }
    }

    // Modules loaded from these locations are frequently malicious
    // and notorious for changing frequently so they are not good candidates
    // for blacklisting individually. Mark them as suspicious if we haven't
    // classified them as bad yet.
    if (module->status == NOT_MATCHED || module->status == GOOD) {
      if (base::StartsWith(module->location, L"%temp%",
                           base::CompareCase::INSENSITIVE_ASCII) ||
          base::StartsWith(module->location, L"%tmp%",
                           base::CompareCase::INSENSITIVE_ASCII)) {
        module->status = SUSPECTED_BAD;
      }
    }
  }
}

void ModuleEnumerator::ReportBack() {
  if (!limited_mode_)
    DCHECK_CURRENTLY_ON(callback_thread_id_);
  observer_->DoneScanning();
}

base::string16 ModuleEnumerator::GetSubjectNameFromDigitalSignature(
    const base::FilePath& filename) {
  HCERTSTORE store = NULL;
  HCRYPTMSG message = NULL;

  // Find the crypto message for this filename.
  bool result = !!CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                   filename.value().c_str(),
                                   CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                   CERT_QUERY_FORMAT_FLAG_BINARY,
                                   0,
                                   NULL,
                                   NULL,
                                   NULL,
                                   &store,
                                   &message,
                                   NULL);
  if (!result)
    return base::string16();

  // Determine the size of the signer info data.
  DWORD signer_info_size = 0;
  result = !!CryptMsgGetParam(message,
                              CMSG_SIGNER_INFO_PARAM,
                              0,
                              NULL,
                              &signer_info_size);
  if (!result)
    return base::string16();

  // Allocate enough space to hold the signer info.
  std::unique_ptr<BYTE[]> signer_info_buffer(new BYTE[signer_info_size]);
  CMSG_SIGNER_INFO* signer_info =
      reinterpret_cast<CMSG_SIGNER_INFO*>(signer_info_buffer.get());

  // Obtain the signer info.
  result = !!CryptMsgGetParam(message,
                              CMSG_SIGNER_INFO_PARAM,
                              0,
                              signer_info,
                              &signer_info_size);
  if (!result)
    return base::string16();

  // Search for the signer certificate.
  CERT_INFO CertInfo = {0};
  PCCERT_CONTEXT cert_context = NULL;
  CertInfo.Issuer = signer_info->Issuer;
  CertInfo.SerialNumber = signer_info->SerialNumber;

  cert_context = CertFindCertificateInStore(
      store,
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      0,
      CERT_FIND_SUBJECT_CERT,
      &CertInfo,
      NULL);
  if (!cert_context)
    return base::string16();

  // Determine the size of the Subject name.
  DWORD subject_name_size = CertGetNameString(
      cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);
  if (!subject_name_size)
    return base::string16();

  base::string16 subject_name;
  subject_name.resize(subject_name_size);

  // Get subject name.
  if (!(CertGetNameString(cert_context,
                          CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          0,
                          NULL,
                          const_cast<LPWSTR>(subject_name.c_str()),
                          subject_name_size))) {
    return base::string16();
  }

  return subject_name;
}

//  ----------------------------------------------------------------------------

// static
EnumerateModulesModel* EnumerateModulesModel::GetInstance() {
  return base::Singleton<EnumerateModulesModel>::get();
}

bool EnumerateModulesModel::ShouldShowConflictWarning() const {
  // If the user has acknowledged the conflict notification, then we don't need
  // to show it again (because the scanning only happens once per the lifetime
  // of the process). If we were to run the scanning more than once, then we'd
  // need to clear the flag somewhere when we are ready to show it again.
  if (conflict_notification_acknowledged_)
    return false;

  return confirmed_bad_modules_detected_ > 0;
}

void EnumerateModulesModel::AcknowledgeConflictNotification() {
  if (!conflict_notification_acknowledged_) {
    conflict_notification_acknowledged_ = true;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_MODULE_INCOMPATIBILITY_ICON_CHANGE,
        content::Source<EnumerateModulesModel>(this),
        content::NotificationService::NoDetails());
  }
}

void EnumerateModulesModel::ScanNow() {
  if (scanning_)
    return;  // A scan is already in progress.

  lock->Acquire();  // Balanced in DoneScanning();

  scanning_ = true;

  // Instruct the ModuleEnumerator class to load this on the File thread.
  // ScanNow does not block.
  if (!module_enumerator_.get())
    module_enumerator_ = new ModuleEnumerator(this);
  module_enumerator_->ScanNow(&enumerated_modules_, limited_mode_);
}

base::ListValue* EnumerateModulesModel::GetModuleList() const {
  if (scanning_)
    return NULL;

  lock->Acquire();

  if (enumerated_modules_.empty()) {
    lock->Release();
    return NULL;
  }

  base::ListValue* list = new base::ListValue();

  for (ModuleEnumerator::ModulesVector::const_iterator module =
           enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    base::DictionaryValue* data = new base::DictionaryValue();
    data->SetInteger("type", module->type);
    base::string16 type_string;
    if ((module->type & ModuleEnumerator::LOADED_MODULE) == 0) {
      // Module is not loaded, denote type of module.
      if (module->type & ModuleEnumerator::SHELL_EXTENSION)
        type_string = L"Shell Extension";
      if (module->type & ModuleEnumerator::WINSOCK_MODULE_REGISTRATION) {
        if (!type_string.empty())
          type_string += L", ";
        type_string += L"Winsock";
      }
      // Must be one of the above type.
      DCHECK(!type_string.empty());
      if (!limited_mode_) {
        type_string += L" -- ";
        type_string += l10n_util::GetStringUTF16(IDS_CONFLICTS_NOT_LOADED_YET);
      }
    }
    data->SetString("type_description", type_string);
    data->SetInteger("status", module->status);
    data->SetString("location", module->location);
    data->SetString("name", module->name);
    data->SetString("product_name", module->product_name);
    data->SetString("description", module->description);
    data->SetString("version", module->version);
    data->SetString("digital_signer", module->digital_signer);

    if (!limited_mode_) {
      // Figure out the possible resolution help string.
      base::string16 actions;
      base::string16 separator = L" " +
          l10n_util::GetStringUTF16(
              IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_SEPARATOR) +
          L" ";

      if (module->recommended_action & ModuleEnumerator::INVESTIGATING) {
        actions = l10n_util::GetStringUTF16(
            IDS_CONFLICTS_CHECK_INVESTIGATING);
      } else {
        if (module->recommended_action & ModuleEnumerator::UNINSTALL) {
          if (!actions.empty())
            actions += separator;
          actions = l10n_util::GetStringUTF16(
              IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_UNINSTALL);
        }
        if (module->recommended_action & ModuleEnumerator::UPDATE) {
          if (!actions.empty())
            actions += separator;
          actions += l10n_util::GetStringUTF16(
              IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_UPDATE);
        }
        if (module->recommended_action & ModuleEnumerator::DISABLE) {
          if (!actions.empty())
            actions += separator;
          actions += l10n_util::GetStringUTF16(
              IDS_CONFLICTS_CHECK_POSSIBLE_ACTION_DISABLE);
        }
      }
      base::string16 possible_resolution;
      if (!actions.empty()) {
        possible_resolution =
            l10n_util::GetStringUTF16(IDS_CONFLICTS_CHECK_POSSIBLE_ACTIONS) +
            L" " + actions;
      }
      data->SetString("possibleResolution", possible_resolution);
      data->SetString("help_url",
                      ConstructHelpCenterUrl(*module).spec().c_str());
    }

    list->Append(data);
  }

  lock->Release();
  return list;
}

GURL EnumerateModulesModel::GetFirstNotableConflict() {
  lock->Acquire();
  GURL url;

  if (enumerated_modules_.empty()) {
    lock->Release();
    return GURL();
  }

  for (ModuleEnumerator::ModulesVector::const_iterator module =
           enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    if (!(module->recommended_action & ModuleEnumerator::NOTIFY_USER))
      continue;

    url = ConstructHelpCenterUrl(*module);
    DCHECK(url.is_valid());
    break;
  }

  lock->Release();
  return url;
}

EnumerateModulesModel::EnumerateModulesModel()
    : limited_mode_(false),
      scanning_(false),
      conflict_notification_acknowledged_(false),
      confirmed_bad_modules_detected_(0),
      modules_to_notify_about_(0),
      suspected_bad_modules_detected_(0) {
  lock = new base::Lock();
}

EnumerateModulesModel::~EnumerateModulesModel() {
  delete lock;
}

void EnumerateModulesModel::MaybePostScanningTask() {
  static bool done = false;
  if (!done) {
    done = true;
    if (base::win::GetVersion() == base::win::VERSION_XP) {
      check_modules_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kModuleCheckDelayMs),
          this, &EnumerateModulesModel::ScanNow);
    }
  }
}

void EnumerateModulesModel::DoneScanning() {
  confirmed_bad_modules_detected_ = 0;
  suspected_bad_modules_detected_ = 0;
  modules_to_notify_about_ = 0;
  for (ModuleEnumerator::ModulesVector::const_iterator module =
       enumerated_modules_.begin();
       module != enumerated_modules_.end(); ++module) {
    if (module->status == ModuleEnumerator::CONFIRMED_BAD) {
      ++confirmed_bad_modules_detected_;
      if (module->recommended_action & ModuleEnumerator::NOTIFY_USER)
        ++modules_to_notify_about_;
    } else if (module->status == ModuleEnumerator::SUSPECTED_BAD) {
      ++suspected_bad_modules_detected_;
      if (module->recommended_action & ModuleEnumerator::NOTIFY_USER)
        ++modules_to_notify_about_;
    }
  }

  scanning_ = false;
  lock->Release();

  UMA_HISTOGRAM_COUNTS_100("Conflicts.SuspectedBadModules",
                           suspected_bad_modules_detected_);
  UMA_HISTOGRAM_COUNTS_100("Conflicts.ConfirmedBadModules",
                           confirmed_bad_modules_detected_);

  // Notifications are not available in limited mode.
  if (limited_mode_)
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_MODULE_LIST_ENUMERATED,
      content::Source<EnumerateModulesModel>(this),
      content::NotificationService::NoDetails());
}

GURL EnumerateModulesModel::ConstructHelpCenterUrl(
    const ModuleEnumerator::Module& module) const {
  if (!(module.recommended_action & ModuleEnumerator::SEE_LINK) &&
      !(module.recommended_action & ModuleEnumerator::NOTIFY_USER))
    return GURL();

  // Construct the needed hashes.
  std::string filename, location, description, signer;
  GenerateHash(base::WideToUTF8(module.name), &filename);
  GenerateHash(base::WideToUTF8(module.location), &location);
  GenerateHash(base::WideToUTF8(module.description), &description);
  GenerateHash(base::WideToUTF8(module.digital_signer), &signer);

  base::string16 url =
      l10n_util::GetStringFUTF16(IDS_HELP_CENTER_VIEW_CONFLICTS,
          base::ASCIIToUTF16(filename), base::ASCIIToUTF16(location),
          base::ASCIIToUTF16(description), base::ASCIIToUTF16(signer));
  return GURL(base::UTF16ToUTF8(url));
}
