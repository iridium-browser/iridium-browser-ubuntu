// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <psapi.h>
#include <TlHelp32.h>

#include "base/bind.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// Known browsers which we collect details for.
enum BrowserProcess {
  CHROME_BROWSER = 0,
  CHROME_NACL_PROCESS,
  IE_BROWSER,
  FIREFOX_BROWSER,
  OPERA_BROWSER,
  SAFARI_BROWSER,
  IE_64BIT_BROWSER,
  KONQUEROR_BROWSER,
  MAX_BROWSERS
};

MemoryDetails::MemoryDetails() {
  base::FilePath browser_process_path;
  PathService::Get(base::FILE_EXE, &browser_process_path);
  const base::string16 browser_process_name =
      browser_process_path.BaseName().value();
  const base::string16 google_browser_name =
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);

  struct {
    const wchar_t* name;
    const wchar_t* process_name;
  } process_template[MAX_BROWSERS] = {
    { google_browser_name.c_str(), browser_process_name.c_str(), },
    { google_browser_name.c_str(), L"nacl64.exe", },
    { L"IE", L"iexplore.exe", },
    { L"Firefox", L"firefox.exe", },
    { L"Opera", L"opera.exe", },
    { L"Safari", L"safari.exe", },
    { L"IE (64bit)", L"iexplore.exe", },
    { L"Konqueror", L"konqueror.exe", },
  };

  for (int index = 0; index < MAX_BROWSERS; ++index) {
    ProcessData process;
    process.name = process_template[index].name;
    process.process_name = process_template[index].process_name;
    process_data_.push_back(process);
  }
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[CHROME_BROWSER];
}

void MemoryDetails::CollectProcessData(
    CollectionMode mode,
    const std::vector<ProcessMemoryInformation>& child_info) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  // Clear old data.
  for (unsigned int index = 0; index < process_data_.size(); index++)
    process_data_[index].processes.clear();

  base::win::OSInfo::WindowsArchitecture windows_architecture =
      base::win::OSInfo::GetInstance()->architecture();

  base::win::ScopedHandle snapshot(
      ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  PROCESSENTRY32 process_entry = {sizeof(PROCESSENTRY32)};
  if (!snapshot.Get()) {
    LOG(ERROR) << "CreateToolhelp32Snaphot failed: " << GetLastError();
    return;
  }
  if (!::Process32First(snapshot.Get(), &process_entry)) {
    LOG(ERROR) << "Process32First failed: " << GetLastError();
    return;
  }
  do {
    base::ProcessId pid = process_entry.th32ProcessID;
    base::win::ScopedHandle process_handle(::OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid));
    if (!process_handle.IsValid())
      continue;
    bool is_64bit_process =
        ((windows_architecture == base::win::OSInfo::X64_ARCHITECTURE) ||
         (windows_architecture == base::win::OSInfo::IA64_ARCHITECTURE)) &&
        (base::win::OSInfo::GetWOW64StatusForProcess(process_handle.Get()) ==
            base::win::OSInfo::WOW64_DISABLED);
    const size_t browser_list_size =
        (mode == FROM_CHROME_ONLY ? 1 : process_data_.size());
    for (size_t index2 = 0; index2 < browser_list_size; ++index2) {
      if (_wcsicmp(process_data_[index2].process_name.c_str(),
                   process_entry.szExeFile) != 0)
        continue;
      if (index2 == IE_BROWSER && is_64bit_process)
        continue;  // Should use IE_64BIT_BROWSER
      // Get Memory Information.
      ProcessMemoryInformation info;
      info.pid = pid;
      if (info.pid == GetCurrentProcessId())
        info.process_type = content::PROCESS_TYPE_BROWSER;
      else
        info.process_type = content::PROCESS_TYPE_UNKNOWN;

      scoped_ptr<base::ProcessMetrics> metrics;
      metrics.reset(base::ProcessMetrics::CreateProcessMetrics(
                        process_handle.Get()));
      metrics->GetCommittedKBytes(&info.committed);
      metrics->GetWorkingSetKBytes(&info.working_set);

      // Get Version Information.
      TCHAR name[MAX_PATH];
      if (index2 == CHROME_BROWSER || index2 == CHROME_NACL_PROCESS) {
        info.version = base::ASCIIToUTF16(version_info::GetVersionNumber());
        // Check if this is one of the child processes whose data we collected
        // on the IO thread, and if so copy over that data.
        for (size_t child = 0; child < child_info.size(); child++) {
          if (child_info[child].pid != info.pid)
            continue;
          info.titles = child_info[child].titles;
          info.process_type = child_info[child].process_type;
          break;
        }
      } else if (GetModuleFileNameEx(process_handle.Get(), NULL, name,
                                     MAX_PATH - 1)) {
        std::wstring str_name(name);
        scoped_ptr<FileVersionInfo> version_info(
            FileVersionInfo::CreateFileVersionInfo(base::FilePath(str_name)));
        if (version_info != NULL) {
          info.version = version_info->product_version();
          info.product_name = version_info->product_name();
        }
      }

      // Add the process info to our list.
      if (index2 == CHROME_NACL_PROCESS) {
        // Add NaCl processes to Chrome's list
        process_data_[CHROME_BROWSER].processes.push_back(info);
      } else {
        process_data_[index2].processes.push_back(info);
      }
      break;
    }
  } while (::Process32Next(snapshot.Get(), &process_entry));

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
