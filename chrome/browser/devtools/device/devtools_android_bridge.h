// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/gfx/geometry/size.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;

class MessageLoop;
class DictionaryValue;
class ListValue;
class Thread;
}  // namespace base

namespace content {
class BrowserContext;
}

class DevToolsTargetImpl;
class PortForwardingController;
class Profile;
class TCPDeviceProvider;

class DevToolsAndroidBridge : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of DevToolsAndroidBridge.
    static Factory* GetInstance();

    // Returns DevToolsAndroidBridge associated with |profile|.
    static DevToolsAndroidBridge* GetForProfile(Profile* profile);

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory overrides:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  using BrowserId = std::pair<std::string, std::string>;

  class RemotePage : public base::RefCounted<RemotePage> {
   public:
    const std::string& serial() { return browser_id_.first; }
    const std::string& socket() { return browser_id_.second; }
    const std::string& frontend_url() { return frontend_url_; }

   private:
    friend class base::RefCounted<RemotePage>;
    friend class DevToolsAndroidBridge;

    RemotePage(const BrowserId& browser_id, const base::DictionaryValue& dict);

    virtual ~RemotePage();

    BrowserId browser_id_;
    std::string frontend_url_;
    std::unique_ptr<base::DictionaryValue> dict_;

    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  using RemotePages = std::vector<scoped_refptr<RemotePage> >;
  using JsonRequestCallback = base::Callback<void(int, const std::string&)>;

  class RemoteBrowser : public base::RefCounted<RemoteBrowser> {
   public:
    const std::string& serial() { return browser_id_.first; }
    const std::string& socket() { return browser_id_.second; }
    const std::string& display_name() { return display_name_; }
    const std::string& user() { return user_; }
    const std::string& version() { return version_; }
    const RemotePages& pages() { return pages_; }

    bool IsChrome();
    std::string GetId();

    using ParsedVersion = std::vector<int>;
    ParsedVersion GetParsedVersion();

   private:
    friend class base::RefCounted<RemoteBrowser>;
    friend class DevToolsAndroidBridge;

    RemoteBrowser(const std::string& serial,
                  const AndroidDeviceManager::BrowserInfo& browser_info);

    virtual ~RemoteBrowser();

    BrowserId browser_id_;
    std::string display_name_;
    std::string user_;
    AndroidDeviceManager::BrowserInfo::Type type_;
    std::string version_;
    RemotePages pages_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  using RemoteBrowsers = std::vector<scoped_refptr<RemoteBrowser> >;

  class RemoteDevice : public base::RefCounted<RemoteDevice> {
   public:
    std::string serial() { return serial_; }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }

   private:
    friend class base::RefCounted<RemoteDevice>;
    friend class DevToolsAndroidBridge;

    RemoteDevice(const std::string& serial,
                 const AndroidDeviceManager::DeviceInfo& device_info);

    virtual ~RemoteDevice();

    std::string serial_;
    std::string model_;
    bool connected_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;

    DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
  };

  using RemoteDevices = std::vector<scoped_refptr<RemoteDevice> >;

  class DeviceListListener {
   public:
    virtual void DeviceListChanged(const RemoteDevices& devices) = 0;
   protected:
    virtual ~DeviceListListener() {}
  };

  explicit DevToolsAndroidBridge(Profile* profile);
  void AddDeviceListListener(DeviceListListener* listener);
  void RemoveDeviceListListener(DeviceListListener* listener);

  class DeviceCountListener {
   public:
    virtual void DeviceCountChanged(int count) = 0;
   protected:
    virtual ~DeviceCountListener() {}
  };

  void AddDeviceCountListener(DeviceCountListener* listener);
  void RemoveDeviceCountListener(DeviceCountListener* listener);

  using PortStatus = int;
  using PortStatusMap = std::map<int, PortStatus>;
  using BrowserStatus = std::pair<scoped_refptr<RemoteBrowser>, PortStatusMap>;
  using ForwardingStatus = std::vector<BrowserStatus>;

  class PortForwardingListener {
   public:
    using PortStatusMap = DevToolsAndroidBridge::PortStatusMap;
    using BrowserStatus = DevToolsAndroidBridge::BrowserStatus;
    using ForwardingStatus = DevToolsAndroidBridge::ForwardingStatus;

    virtual void PortStatusChanged(const ForwardingStatus&) = 0;
   protected:
    virtual ~PortForwardingListener() {}
  };

  void AddPortForwardingListener(PortForwardingListener* listener);
  void RemovePortForwardingListener(PortForwardingListener* listener);

  void set_device_providers_for_test(
      const AndroidDeviceManager::DeviceProviders& device_providers) {
    device_manager_->SetDeviceProviders(device_providers);
  }

  void set_task_scheduler_for_test(
      base::Callback<void(const base::Closure&)> scheduler) {
    task_scheduler_ = scheduler;
  }

  bool HasDevToolsWindow(const std::string& agent_id);

  // Creates new target instance owned by caller.
  DevToolsTargetImpl* CreatePageTarget(scoped_refptr<RemotePage> browser);

  using RemotePageCallback = base::Callback<void(scoped_refptr<RemotePage>)>;
  void OpenRemotePage(scoped_refptr<RemoteBrowser> browser,
                      const std::string& url);

  scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      scoped_refptr<RemoteBrowser> browser);

  void SendJsonRequest(const std::string& browser_id_str,
                       const std::string& url,
                       const JsonRequestCallback& callback);

  using TCPProviderCallback =
      base::Callback<void(scoped_refptr<TCPDeviceProvider>)>;
  void set_tcp_provider_callback_for_test(TCPProviderCallback callback);
 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAndroidBridge>;

  friend class PortForwardingController;

  class AgentHostDelegate;
  class DiscoveryRequest;
  class RemotePageTarget;

  ~DevToolsAndroidBridge() override;

  void StartDeviceListPolling();
  void StopDeviceListPolling();
  bool NeedsDeviceListPolling();

  using CompleteDevice = std::pair<scoped_refptr<AndroidDeviceManager::Device>,
                    scoped_refptr<RemoteDevice>>;
  using CompleteDevices = std::vector<CompleteDevice>;
  using DeviceListCallback = base::Callback<void(const CompleteDevices&)>;

  void RequestDeviceList(const DeviceListCallback& callback);
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  void StartDeviceCountPolling();
  void StopDeviceCountPolling();
  void RequestDeviceCount(const base::Callback<void(int)>& callback);
  void ReceivedDeviceCount(int count);

  static void ScheduleTaskDefault(const base::Closure& task);

  void CreateDeviceProviders();

  void SendJsonRequest(const BrowserId& browser_id,
                       const std::string& url,
                       const JsonRequestCallback& callback);

  void SendProtocolCommand(const BrowserId& browser_id,
                           const std::string& target_path,
                           const std::string& method,
                           std::unique_ptr<base::DictionaryValue> params,
                           const base::Closure callback);

  scoped_refptr<AndroidDeviceManager::Device> FindDevice(
      const std::string& serial);

  base::WeakPtr<DevToolsAndroidBridge> AsWeakPtr() {
      return weak_factory_.GetWeakPtr();
  }

  Profile* const profile_;
  const std::unique_ptr<AndroidDeviceManager> device_manager_;

  using DeviceMap =
      std::map<std::string, scoped_refptr<AndroidDeviceManager::Device> >;
  DeviceMap device_map_;

  using AgentHostDelegates = std::map<std::string, AgentHostDelegate*>;
  AgentHostDelegates host_delegates_;

  using DeviceListListeners = std::vector<DeviceListListener*>;
  DeviceListListeners device_list_listeners_;
  base::CancelableCallback<void(const CompleteDevices&)> device_list_callback_;

  using DeviceCountListeners = std::vector<DeviceCountListener*>;
  DeviceCountListeners device_count_listeners_;
  base::CancelableCallback<void(int)> device_count_callback_;
  base::Callback<void(const base::Closure&)> task_scheduler_;

  using PortForwardingListeners = std::vector<PortForwardingListener*>;
  PortForwardingListeners port_forwarding_listeners_;
  std::unique_ptr<PortForwardingController> port_forwarding_controller_;

  PrefChangeRegistrar pref_change_registrar_;

  TCPProviderCallback tcp_provider_callback_;

  base::WeakPtrFactory<DevToolsAndroidBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAndroidBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
