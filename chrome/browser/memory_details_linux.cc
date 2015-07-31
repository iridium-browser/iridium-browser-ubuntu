// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_details.h"

#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/process/process_iterator.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "ui/base/l10n/l10n_util.h"

using base::ProcessEntry;
using content::BrowserThread;

namespace {

// Known browsers which we collect details for.
enum BrowserType {
  CHROME = 0,
  FIREFOX,
  ICEWEASEL,
  OPERA,
  KONQUEROR,
  EPIPHANY,
  MIDORI,
  MAX_BROWSERS
};

// The pretty printed names of those browsers. Matches up with enum
// BrowserType.
const char kBrowserPrettyNames[][10] = {
  "Chrome",
  "Firefox",
  "Iceweasel",
  "Opera",
  "Konqueror",
  "Epiphany",
  "Midori",
};

// A mapping from process name to the type of browser.
const struct {
  const char process_name[17];
  BrowserType browser;
} kBrowserBinaryNames[] = {
  { "firefox", FIREFOX },
  { "firefox-3.5", FIREFOX },
  { "firefox-3.0", FIREFOX },
  { "firefox-bin", FIREFOX },
  { "iceweasel", ICEWEASEL },
  { "opera", OPERA },
  { "konqueror", KONQUEROR },
  { "epiphany-browser", EPIPHANY },
  { "epiphany", EPIPHANY },
  { "midori", MIDORI },
  { "", MAX_BROWSERS },
};

struct Process {
  pid_t pid;
  pid_t parent;
  std::string name;
};

typedef std::map<pid_t, Process> ProcessMap;

// Get information on all the processes running on the system.
ProcessMap GetProcesses() {
  ProcessMap map;

  base::ProcessIterator process_iter(NULL);
  while (const ProcessEntry* process_entry = process_iter.NextProcessEntry()) {
    Process process;
    process.pid = process_entry->pid();
    process.parent = process_entry->parent_pid();
    process.name = process_entry->exe_file();
    map.insert(std::make_pair(process.pid, process));
  }
  return map;
}

// Given a process name, return the type of the browser which created that
// process, or |MAX_BROWSERS| if we don't know about it.
BrowserType GetBrowserType(const std::string& process_name) {
  for (unsigned i = 0; kBrowserBinaryNames[i].process_name[0]; ++i) {
    if (strcmp(process_name.c_str(), kBrowserBinaryNames[i].process_name) == 0)
      return kBrowserBinaryNames[i].browser;
  }

  return MAX_BROWSERS;
}

// For each of a list of pids, collect memory information about that process.
ProcessData GetProcessDataMemoryInformation(
    const std::vector<pid_t>& pids) {
  ProcessData process_data;
  for (pid_t pid : pids) {
    ProcessMemoryInformation pmi;

    pmi.pid = pid;
    pmi.num_processes = 1;

    if (pmi.pid == base::GetCurrentProcId())
      pmi.process_type = content::PROCESS_TYPE_BROWSER;
    else
      pmi.process_type = content::PROCESS_TYPE_UNKNOWN;

    scoped_ptr<base::ProcessMetrics> metrics(
        base::ProcessMetrics::CreateProcessMetrics(pid));
    metrics->GetWorkingSetKBytes(&pmi.working_set);

    process_data.processes.push_back(pmi);
  }
  return process_data;
}

// Find all children of the given process with pid |root|.
std::vector<pid_t> GetAllChildren(const ProcessMap& processes, pid_t root) {
  std::vector<pid_t> children;
  children.push_back(root);

  std::set<pid_t> wavefront, next_wavefront;
  wavefront.insert(root);

  while (wavefront.size()) {
    for (const auto& entry : processes) {
      const Process& process = entry.second;
      if (wavefront.count(process.parent)) {
        children.push_back(process.pid);
        next_wavefront.insert(process.pid);
      }
    }

    wavefront.clear();
    wavefront.swap(next_wavefront);
  }
  return children;
}

}  // namespace

MemoryDetails::MemoryDetails() {
}

ProcessData* MemoryDetails::ChromeBrowser() {
  return &process_data_[0];
}

void MemoryDetails::CollectProcessData(
    CollectionMode mode,
    const std::vector<ProcessMemoryInformation>& child_info) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  ProcessMap process_map = GetProcesses();
  std::set<pid_t> browsers_found;

  // For each process on the system, if it appears to be a browser process and
  // it's parent isn't a browser process, then record it in |browsers_found|.
  for (const auto& entry : process_map) {
    const Process& current_process = entry.second;
    const BrowserType type = GetBrowserType(current_process.name);
    if (type == MAX_BROWSERS)
      continue;
    if (type != CHROME && mode == FROM_CHROME_ONLY)
      continue;

    ProcessMap::const_iterator parent_iter =
        process_map.find(current_process.parent);
    if (parent_iter == process_map.end()) {
      browsers_found.insert(current_process.pid);
      continue;
    }

    if (GetBrowserType(parent_iter->second.name) != type) {
      // We found a process whose type is different from its parent's type.
      // That means it is the root process of the browser.
      browsers_found.insert(current_process.pid);
      continue;
    }
  }

  ProcessData current_browser =
      GetProcessDataMemoryInformation(GetAllChildren(process_map, getpid()));
  current_browser.name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  current_browser.process_name = base::ASCIIToUTF16("chrome");

  for (std::vector<ProcessMemoryInformation>::iterator
       i = current_browser.processes.begin();
       i != current_browser.processes.end(); ++i) {
    // Check if this is one of the child processes whose data we collected
    // on the IO thread, and if so copy over that data.
    for (size_t child = 0; child < child_info.size(); child++) {
      if (child_info[child].pid != i->pid)
        continue;
      i->titles = child_info[child].titles;
      i->process_type = child_info[child].process_type;
      break;
    }
  }

  process_data_.push_back(current_browser);

  // For each browser process, collect a list of its children and get the
  // memory usage of each.
  for (pid_t pid : browsers_found) {
    std::vector<pid_t> browser_processes = GetAllChildren(process_map, pid);
    ProcessData browser = GetProcessDataMemoryInformation(browser_processes);

    ProcessMap::const_iterator process_iter = process_map.find(pid);
    if (process_iter == process_map.end())
      continue;
    BrowserType type = GetBrowserType(process_iter->second.name);
    if (type != MAX_BROWSERS)
      browser.name = base::ASCIIToUTF16(kBrowserPrettyNames[type]);
    process_data_.push_back(browser);
  }

#if defined(OS_CHROMEOS)
  base::GetSwapInfo(&swap_info_);
#endif

  // Finally return to the browser thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MemoryDetails::CollectChildInfoOnUIThread, this));
}
