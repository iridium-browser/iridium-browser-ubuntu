// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/api/bluetooth_socket/bluetooth_socket_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

// Implementations for chrome.easyUnlockPrivate API functions.

namespace content {
class BrowserContext;
}

namespace extensions {

class EasyUnlockPrivateCryptoDelegate;

class EasyUnlockPrivateAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>*
      GetFactoryInstance();

  static const bool kServiceRedirectedInIncognito = true;

  explicit EasyUnlockPrivateAPI(content::BrowserContext* context);
  ~EasyUnlockPrivateAPI() override;

  EasyUnlockPrivateCryptoDelegate* GetCryptoDelegate();

 private:
  friend class BrowserContextKeyedAPIFactory<EasyUnlockPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "EasyUnlockPrivate"; }

  scoped_ptr<EasyUnlockPrivateCryptoDelegate> crypto_delegate_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateAPI);
};

// TODO(tbarzic): Replace SyncExtensionFunction/AsyncExtensionFunction overrides
// with UIThreadExtensionFunction throughout the file.
class EasyUnlockPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateGetStringsFunction();

 protected:
  ~EasyUnlockPrivateGetStringsFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getStrings",
                             EASYUNLOCKPRIVATE_GETSTRINGS)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetStringsFunction);
};

class EasyUnlockPrivatePerformECDHKeyAgreementFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivatePerformECDHKeyAgreementFunction();

 protected:
  ~EasyUnlockPrivatePerformECDHKeyAgreementFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& secret_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.performECDHKeyAgreement",
                             EASYUNLOCKPRIVATE_PERFORMECDHKEYAGREEMENT)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivatePerformECDHKeyAgreementFunction);
};

class EasyUnlockPrivateGenerateEcP256KeyPairFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateGenerateEcP256KeyPairFunction();

 protected:
  ~EasyUnlockPrivateGenerateEcP256KeyPairFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& public_key,
              const std::string& private_key);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.generateEcP256KeyPair",
                             EASYUNLOCKPRIVATE_GENERATEECP256KEYPAIR)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGenerateEcP256KeyPairFunction);
};

class EasyUnlockPrivateCreateSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateCreateSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateCreateSecureMessageFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& message);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.createSecureMessage",
                             EASYUNLOCKPRIVATE_CREATESECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCreateSecureMessageFunction);
};

class EasyUnlockPrivateUnwrapSecureMessageFunction
    : public AsyncExtensionFunction {
 public:
  EasyUnlockPrivateUnwrapSecureMessageFunction();

 protected:
  ~EasyUnlockPrivateUnwrapSecureMessageFunction() override;

  bool RunAsync() override;

 private:
  void OnData(const std::string& data);

  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.unwrapSecureMessage",
                             EASYUNLOCKPRIVATE_UNWRAPSECUREMESSAGE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUnwrapSecureMessageFunction);
};

class EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.seekBluetoothDeviceByAddress",
                             EASYUNLOCKPRIVATE_SEEKBLUETOOTHDEVICEBYADDRESS)
  EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction();

 private:
  ~EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Callbacks that are called when the seek operation succeeds or fails.
  void OnSeekSuccess();
  void OnSeekFailure(const std::string& error_message);

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateSeekBluetoothDeviceByAddressFunction);
};

class EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction
    : public core_api::BluetoothSocketAbstractConnectFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "easyUnlockPrivate.connectToBluetoothServiceInsecurely",
      EASYUNLOCKPRIVATE_CONNECTTOBLUETOOTHSERVICEINSECURELY)
  EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction();

 private:
  ~EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction() override;

  // BluetoothSocketAbstractConnectFunction:
  void ConnectToService(device::BluetoothDevice* device,
                        const device::BluetoothUUID& uuid) override;

  DISALLOW_COPY_AND_ASSIGN(
      EasyUnlockPrivateConnectToBluetoothServiceInsecurelyFunction);
};

class EasyUnlockPrivateUpdateScreenlockStateFunction
    : public SyncExtensionFunction {
 public:
  EasyUnlockPrivateUpdateScreenlockStateFunction();

 protected:
  ~EasyUnlockPrivateUpdateScreenlockStateFunction() override;

  bool RunSync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.updateScreenlockState",
                             EASYUNLOCKPRIVATE_UPDATESCREENLOCKSTATE)

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateUpdateScreenlockStateFunction);
};

class EasyUnlockPrivateSetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setPermitAccess",
                             EASYUNLOCKPRIVATE_SETPERMITACCESS)
  EasyUnlockPrivateSetPermitAccessFunction();

 private:
  ~EasyUnlockPrivateSetPermitAccessFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetPermitAccessFunction);
};

class EasyUnlockPrivateGetPermitAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getPermitAccess",
                             EASYUNLOCKPRIVATE_GETPERMITACCESS)
  EasyUnlockPrivateGetPermitAccessFunction();

 private:
  ~EasyUnlockPrivateGetPermitAccessFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetPermitAccessFunction);
};

class EasyUnlockPrivateClearPermitAccessFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.clearPermitAccess",
                             EASYUNLOCKPRIVATE_CLEARPERMITACCESS)
  EasyUnlockPrivateClearPermitAccessFunction();

 private:
  ~EasyUnlockPrivateClearPermitAccessFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateClearPermitAccessFunction);
};

class EasyUnlockPrivateSetRemoteDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setRemoteDevices",
                             EASYUNLOCKPRIVATE_SETREMOTEDEVICES)
  EasyUnlockPrivateSetRemoteDevicesFunction();

 private:
  ~EasyUnlockPrivateSetRemoteDevicesFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetRemoteDevicesFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getRemoteDevices",
                             EASYUNLOCKPRIVATE_GETREMOTEDEVICES)
  EasyUnlockPrivateGetRemoteDevicesFunction();

 private:
  ~EasyUnlockPrivateGetRemoteDevicesFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetRemoteDevicesFunction);
};

class EasyUnlockPrivateGetSignInChallengeFunction :
    public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getSignInChallenge",
                             EASYUNLOCKPRIVATE_GETSIGNINCHALLENGE)
  EasyUnlockPrivateGetSignInChallengeFunction();

 private:
  ~EasyUnlockPrivateGetSignInChallengeFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

  // Called when the challenge and the signed nonce have been generated.
  void OnDone(const std::string& challenge, const std::string& signed_nonce);

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetSignInChallengeFunction);
};

class EasyUnlockPrivateTrySignInSecretFunction :
    public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.trySignInSecret",
                             EASYUNLOCKPRIVATE_TRYSIGNINSECRET)
  EasyUnlockPrivateTrySignInSecretFunction();

 private:
  ~EasyUnlockPrivateTrySignInSecretFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateTrySignInSecretFunction);
};

class EasyUnlockPrivateGetUserInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getUserInfo",
                             EASYUNLOCKPRIVATE_GETUSERINFO)
  EasyUnlockPrivateGetUserInfoFunction();

 private:
  ~EasyUnlockPrivateGetUserInfoFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetUserInfoFunction);
};

class EasyUnlockPrivateGetConnectionInfoFunction
    : public core_api::BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.getConnectionInfo",
                             EASYUNLOCKPRIVATE_GETCONNECTIONINFO)
  EasyUnlockPrivateGetConnectionInfoFunction();

 private:
  ~EasyUnlockPrivateGetConnectionInfoFunction() override;

  // BluetoothExtensionFunction:
  bool DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

  void OnConnectionInfo(
      const device::BluetoothDevice::ConnectionInfo& connection_info);

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateGetConnectionInfoFunction);
};

class EasyUnlockPrivateShowErrorBubbleFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.showErrorBubble",
                             EASYUNLOCKPRIVATE_SHOWERRORBUBBLE)
  EasyUnlockPrivateShowErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateShowErrorBubbleFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateShowErrorBubbleFunction);
};

class EasyUnlockPrivateHideErrorBubbleFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.hideErrorBubble",
                             EASYUNLOCKPRIVATE_HIDEERRORBUBBLE)
  EasyUnlockPrivateHideErrorBubbleFunction();

 private:
  ~EasyUnlockPrivateHideErrorBubbleFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateHideErrorBubbleFunction);
};

class EasyUnlockPrivateSetAutoPairingResultFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("easyUnlockPrivate.setAutoPairingResult",
                             EASYUNLOCKPRIVATE_SETAUTOPAIRINGRESULT)
  EasyUnlockPrivateSetAutoPairingResultFunction();

 private:
  ~EasyUnlockPrivateSetAutoPairingResultFunction() override;

  // SyncExtensionFunction:
  bool RunSync() override;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateSetAutoPairingResultFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EASY_UNLOCK_PRIVATE_EASY_UNLOCK_PRIVATE_API_H_
