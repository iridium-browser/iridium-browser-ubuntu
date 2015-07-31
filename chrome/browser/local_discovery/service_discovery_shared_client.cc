// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/service_discovery_shared_client.h"

#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/firewall_manager_win.h"
#endif  // OS_WIN

#if defined(OS_MACOSX)
#include "chrome/browser/local_discovery/service_discovery_client_mac_factory.h"
#endif

#if defined(ENABLE_MDNS)
#include "chrome/browser/local_discovery/service_discovery_client_mdns.h"
#include "chrome/browser/local_discovery/service_discovery_client_utility.h"
#endif  // ENABLE_MDNS

namespace {

#if defined(OS_WIN)

bool g_is_firewall_ready = false;
bool g_is_firewall_state_reported = false;

void ReportFirewallStats() {
  if (g_is_firewall_state_reported)
    return;
  g_is_firewall_state_reported = true;
  base::FilePath exe_path;
  if (!PathService::Get(base::FILE_EXE, &exe_path))
    return;
  base::ElapsedTimer timer;
  scoped_ptr<installer::FirewallManager> manager =
      installer::FirewallManager::Create(BrowserDistribution::GetDistribution(),
                                         exe_path);
  if (!manager)
    return;
  g_is_firewall_ready = manager->CanUseLocalPorts();
  UMA_HISTOGRAM_TIMES("LocalDiscovery.FirewallAccessTime", timer.Elapsed());
  UMA_HISTOGRAM_BOOLEAN("LocalDiscovery.IsFirewallReady", g_is_firewall_ready);
}
#endif  // OS_WIN

}  // namespace


namespace local_discovery {

using content::BrowserThread;

namespace {
ServiceDiscoverySharedClient* g_service_discovery_client = NULL;
}  // namespace

ServiceDiscoverySharedClient::ServiceDiscoverySharedClient() {
  DCHECK(!g_service_discovery_client);
  g_service_discovery_client = this;
}

ServiceDiscoverySharedClient::~ServiceDiscoverySharedClient() {
  DCHECK_EQ(g_service_discovery_client, this);
  g_service_discovery_client = NULL;
}

#if defined(ENABLE_MDNS) || defined(OS_MACOSX)

scoped_refptr<ServiceDiscoverySharedClient>
    ServiceDiscoverySharedClient::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (g_service_discovery_client)
    return g_service_discovery_client;

#if defined(OS_WIN)
  if (!g_is_firewall_state_reported) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&ReportFirewallStats));
  }
#endif  // OS_WIN

#if defined(OS_MACOSX)
  return ServiceDiscoveryClientMacFactory::CreateInstance();
#else  // OS_MACOSX

  return new ServiceDiscoveryClientMdns();
#endif  // OS_MACOSX
}

// static
void ServiceDiscoverySharedClient::GetInstanceWithoutAlert(
    const GetInstanceCallback& callback) {
#if !defined(OS_WIN)

  scoped_refptr<ServiceDiscoverySharedClient> result = GetInstance();
  return callback.Run(result);

#else   // OS_WIN
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(vitalybuka): Switch to |ServiceDiscoveryClientMdns| after we find what
  // to do with firewall for user-level installs. crbug.com/366408
  scoped_refptr<ServiceDiscoverySharedClient> result =
      g_service_discovery_client;
  if (result.get())
    return callback.Run(result);

  if (!g_is_firewall_state_reported) {
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&ReportFirewallStats),
        base::Bind(&ServiceDiscoverySharedClient::GetInstanceWithoutAlert,
                   callback));
    return;
  }

  result =
      g_is_firewall_ready ? GetInstance() : new ServiceDiscoveryClientUtility();
  callback.Run(result);
#endif  // OS_WIN
}

#else

scoped_refptr<ServiceDiscoverySharedClient>
    ServiceDiscoverySharedClient::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NOTIMPLEMENTED();
  return NULL;
}

#endif  // ENABLE_MDNS

}  // namespace local_discovery
