// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/scoped_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"
#include "device/bluetooth/bluetooth_socket_mac.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace {

// The frequency with which to poll the adapter for updates.
const int kPollIntervalMs = 500;

}  // namespace

namespace device {

// static
const NSTimeInterval BluetoothAdapterMac::kDiscoveryTimeoutSec =
    180;  // 3 minutes

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterMac::CreateAdapter();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapter() {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapterForTest(
    std::string name,
    std::string address,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->InitForTest(ui_task_runner);
  adapter->name_ = name;
  adapter->address_ = address;
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      classic_powered_(false),
      num_discovery_sessions_(0),
      classic_discovery_manager_(
          BluetoothDiscoveryManagerMac::CreateClassic(this)),
      weak_ptr_factory_(this) {
  if (IsLowEnergyAvailable()) {
    low_energy_discovery_manager_.reset(
        BluetoothLowEnergyDiscoveryManagerMac::Create(this));
    low_energy_central_manager_delegate_.reset(
        [[BluetoothLowEnergyCentralManagerDelegate alloc]
            initWithDiscoveryManager:low_energy_discovery_manager_.get()
                          andAdapter:this]);
    Class aClass = NSClassFromString(@"CBCentralManager");
    low_energy_central_manager_.reset([[aClass alloc]
        initWithDelegate:low_energy_central_manager_delegate_.get()
                   queue:dispatch_get_main_queue()]);
    low_energy_discovery_manager_->SetCentralManager(
        low_energy_central_manager_.get());
  }
  DCHECK(classic_discovery_manager_.get());
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
}

std::string BluetoothAdapterMac::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterMac::GetName() const {
  return name_;
}

void BluetoothAdapterMac::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsInitialized() const {
  return true;
}

bool BluetoothAdapterMac::IsPresent() const {
  bool is_present = !address_.empty();
  if (IsLowEnergyAvailable()) {
    is_present = is_present || ([low_energy_central_manager_ state] ==
                                CBCentralManagerStatePoweredOn);
  }
  return is_present;
}

bool BluetoothAdapterMac::IsPowered() const {
  bool is_powered = classic_powered_;
  if (IsLowEnergyAvailable()) {
    is_powered = is_powered || ([low_energy_central_manager_ state] ==
                                CBCentralManagerStatePoweredOn);
  }
  return is_powered;
}

void BluetoothAdapterMac::SetPowered(bool powered,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// TODO(krstnmnlsn): If this information is retrievable form IOBluetooth we
// should return the discoverable status.
bool BluetoothAdapterMac::IsDiscoverable() const {
  return false;
}

void BluetoothAdapterMac::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscovering() const {
  bool is_discovering = classic_discovery_manager_->IsDiscovering();
  if (IsLowEnergyAvailable())
    is_discovering =
        is_discovering || low_energy_discovery_manager_->IsDiscovering();
  return is_discovering;
}

void BluetoothAdapterMac::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingRfcomm(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingL2cap(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::RegisterAudioSink(
    const BluetoothAudioSink::Options& options,
    const AcquiredCallback& callback,
    const BluetoothAudioSink::ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAudioSink::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterMac::RegisterAdvertisement(
    scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const CreateAdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterMac::ClassicDeviceFound(IOBluetoothDevice* device) {
  ClassicDeviceAdded(device);
}

void BluetoothAdapterMac::ClassicDiscoveryStopped(bool unexpected) {
  if (unexpected) {
    DVLOG(1) << "Discovery stopped unexpectedly";
    num_discovery_sessions_ = 0;
    MarkDiscoverySessionsAsInactive();
  }
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, false));
}

void BluetoothAdapterMac::DeviceConnected(IOBluetoothDevice* device) {
  // TODO(isherman): Investigate whether this method can be replaced with a call
  // to +registerForConnectNotifications:selector:.
  DVLOG(1) << "Adapter registered a new connection from device with address: "
           << BluetoothClassicDeviceMac::GetDeviceAddress(device);
  ClassicDeviceAdded(device);
}

// static
bool BluetoothAdapterMac::IsLowEnergyAvailable() {
  return base::mac::IsOSYosemiteOrLater();
}

void BluetoothAdapterMac::SetCentralManagerForTesting(
    CBCentralManager* central_manager) {
  CHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  [central_manager performSelector:@selector(setDelegate:)
                        withObject:low_energy_central_manager_delegate_];
  low_energy_central_manager_.reset(central_manager);
  low_energy_discovery_manager_->SetCentralManager(
      low_energy_central_manager_.get());
}

void BluetoothAdapterMac::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
}

void BluetoothAdapterMac::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DVLOG(1) << __func__;
  if (num_discovery_sessions_ > 0) {
    DCHECK(IsDiscovering());
    num_discovery_sessions_++;
    // We are already running a discovery session, notify the system if the
    // filter has changed.
    if (!StartDiscovery(discovery_filter)) {
      error_callback.Run();
      return;
    }
    callback.Run();
    return;
  }

  DCHECK_EQ(0, num_discovery_sessions_);

  if (!StartDiscovery(discovery_filter)) {
    error_callback.Run();
    return;
  }

  DVLOG(1) << "Added a discovery session";
  num_discovery_sessions_++;
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, true));
  callback.Run();
}

void BluetoothAdapterMac::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  DVLOG(1) << __func__;

  if (num_discovery_sessions_ > 1) {
    // There are active sessions other than the one currently being removed.
    DCHECK(IsDiscovering());
    num_discovery_sessions_--;
    callback.Run();
    return;
  }

  if (num_discovery_sessions_ == 0) {
    DVLOG(1) << "No active discovery sessions. Returning error.";
    error_callback.Run();
    return;
  }

  // Default to dual discovery if |discovery_filter| is NULL.
  BluetoothDiscoveryFilter::TransportMask transport =
      BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if (transport & BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC) {
    if (!classic_discovery_manager_->StopDiscovery()) {
      DVLOG(1) << "Failed to stop classic discovery";
      error_callback.Run();
      return;
    }
  }
  if (transport & BluetoothDiscoveryFilter::Transport::TRANSPORT_LE) {
    if (IsLowEnergyAvailable())
      low_energy_discovery_manager_->StopDiscovery();
  }

  DVLOG(1) << "Discovery stopped";
  num_discovery_sessions_--;
  callback.Run();
}

void BluetoothAdapterMac::SetDiscoveryFilter(
    scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run();
}

bool BluetoothAdapterMac::StartDiscovery(
    BluetoothDiscoveryFilter* discovery_filter) {
  // Default to dual discovery if |discovery_filter| is NULL.  IOBluetooth seems
  // allow starting low energy and classic discovery at once.
  BluetoothDiscoveryFilter::TransportMask transport =
      BluetoothDiscoveryFilter::Transport::TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if ((transport & BluetoothDiscoveryFilter::Transport::TRANSPORT_CLASSIC) &&
      !classic_discovery_manager_->IsDiscovering()) {
    // TODO(krstnmnlsn): If a classic discovery session is already running then
    // we should update its filter. crbug.com/498056
    if (!classic_discovery_manager_->StartDiscovery()) {
      DVLOG(1) << "Failed to add a classic discovery session";
      return false;
    }
  }
  if (transport & BluetoothDiscoveryFilter::Transport::TRANSPORT_LE) {
    // Begin a low energy discovery session or update it if one is already
    // running.
    if (IsLowEnergyAvailable())
      low_energy_discovery_manager_->StartDiscovery(
          BluetoothDevice::UUIDList());
  }
  return true;
}

void BluetoothAdapterMac::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PollAdapter();
}

void BluetoothAdapterMac::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

void BluetoothAdapterMac::PollAdapter() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::Start"));
  bool was_present = IsPresent();
  std::string address;
  bool classic_powered = false;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::GetControllerStats"));
  if (controller != nil) {
    address = BluetoothDevice::CanonicalizeAddress(
        base::SysNSStringToUTF8([controller addressAsString]));
    classic_powered = ([controller powerState] == kBluetoothHCIPowerStateON);

    // For performance reasons, cache the adapter's name. It's not uncommon for
    // a call to [controller nameAsString] to take tens of milliseconds. Note
    // that this caching strategy might result in clients receiving a stale
    // name. If this is a significant issue, then some more sophisticated
    // workaround for the performance bottleneck will be needed. For additional
    // context, see http://crbug.com/461181 and http://crbug.com/467316
    if (address != address_ || (!address.empty() && name_.empty()))
      name_ = base::SysNSStringToUTF8([controller nameAsString]);
  }

  bool is_present = !address.empty();
  address_ = address;

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AdapterPresentChanged"));
  if (was_present != is_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, is_present));
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AdapterPowerChanged"));
  if (classic_powered_ != classic_powered) {
    classic_powered_ = classic_powered;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPoweredChanged(this, classic_powered_));
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::RemoveTimedOutDevices"));
  RemoveTimedOutDevices();

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile6(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AddPairedDevices"));
  AddPairedDevices();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterMac::PollAdapter,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::ClassicDeviceAdded(IOBluetoothDevice* device) {
  std::string device_address =
      BluetoothClassicDeviceMac::GetDeviceAddress(device);

  // Only notify observers once per device.
  if (devices_.count(device_address))
    return;

  devices_[device_address] = new BluetoothClassicDeviceMac(device);
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    DeviceAdded(this, devices_[device_address]));
}

void BluetoothAdapterMac::LowEnergyDeviceUpdated(
    CBPeripheral* peripheral,
    NSDictionary* advertisement_data,
    int rssi) {
  std::string device_address =
      BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  // Get a reference to the actual device pointer held by |devices_| (if
  // |device_address| has no entry in the map a NULL pointer is created by the
  // std::map [] operator).
  BluetoothDevice*& device_reference = devices_[device_address];
  if (!device_reference) {
    VLOG(1) << "LowEnergyDeviceUpdated new device";
    // A new device has been found.
    device_reference =
        new BluetoothLowEnergyDeviceMac(peripheral, advertisement_data, rssi);
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device_reference));
    return;
  }

  std::string stored_device_id = device_reference->GetIdentifier();
  std::string updated_device_id =
      BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(peripheral);
  if (stored_device_id != updated_device_id) {
    VLOG(1) << "LowEnergyDeviceUpdated stored_device_id != updated_device_id: "
            << std::endl
            << "  " << stored_device_id << std::endl
            << "  " << updated_device_id;
    // Collision, two identifiers map to the same hash address.  With a 48 bit
    // hash the probability of this occuring with 10,000 devices
    // simultaneously present is 1e-6 (see
    // https://en.wikipedia.org/wiki/Birthday_problem#Probability_table).  We
    // ignore the second device by returning.
    return;
  }

  // A device has an update.
  VLOG(2) << "LowEnergyDeviceUpdated";
  static_cast<BluetoothLowEnergyDeviceMac*>(device_reference)
      ->Update(peripheral, advertisement_data, rssi);
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceChanged(this, device_reference));
}

// TODO(krstnmnlsn): Implement. crbug.com/511025
void BluetoothAdapterMac::LowEnergyCentralManagerUpdatedState() {}

void BluetoothAdapterMac::RemoveTimedOutDevices() {
  // Notify observers if any previously seen devices are no longer available,
  // i.e. if they are no longer paired, connected, nor recently discovered via
  // an inquiry.
  std::set<std::string> removed_devices;
  for (DevicesMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    BluetoothDevice* device = it->second;
    if (device->IsPaired() || device->IsConnected())
      continue;

    NSDate* last_update_time =
        static_cast<BluetoothDeviceMac*>(device)->GetLastUpdateTime();
    if (last_update_time &&
        -[last_update_time timeIntervalSinceNow] < kDiscoveryTimeoutSec)
      continue;

    FOR_EACH_OBSERVER(
        BluetoothAdapter::Observer, observers_, DeviceRemoved(this, device));
    delete device;
    removed_devices.insert(it->first);
    // The device will be erased from the map in the loop immediately below.
  }
  for (const std::string& device_address : removed_devices) {
    size_t num_removed = devices_.erase(device_address);
    DCHECK_EQ(num_removed, 1U);
  }
}

void BluetoothAdapterMac::AddPairedDevices() {
  // Add any new paired devices.
  for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
    ClassicDeviceAdded(device);
  }
}

}  // namespace device
