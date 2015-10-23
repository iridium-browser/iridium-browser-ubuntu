// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_connection_handler.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chromeos/cert_loader.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/managed_network_configuration_handler_impl.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/net_errors.h"
#include "net/base/test_data_directory.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char* kSuccessResult = "success";
const char* kUsernameHash = "userhash";

void ConfigureCallback(const dbus::ObjectPath& result) {
}

void ConfigureErrorCallback(const std::string& error_name,
                            const std::string& error_message) {
}

class TestNetworkConnectionObserver : public NetworkConnectionObserver {
 public:
  TestNetworkConnectionObserver() {}
  ~TestNetworkConnectionObserver() override {}

  // NetworkConnectionObserver
  void ConnectToNetworkRequested(const std::string& service_path) override {
    requests_.insert(service_path);
  }

  void ConnectSucceeded(const std::string& service_path) override {
    results_[service_path] = kSuccessResult;
  }

  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override {
    results_[service_path] = error_name;
  }

  void DisconnectRequested(const std::string& service_path) override {
    requests_.insert(service_path);
  }

  bool GetRequested(const std::string& service_path) {
    return requests_.count(service_path) != 0;
  }

  std::string GetResult(const std::string& service_path) {
    auto iter = results_.find(service_path);
    if (iter == results_.end())
      return "";
    return iter->second;
  }

 private:
  std::set<std::string> requests_;
  std::map<std::string, std::string> results_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkConnectionObserver);
};

}  // namespace

class NetworkConnectionHandlerTest : public testing::Test {
 public:
  NetworkConnectionHandlerTest()
      : test_manager_client_(nullptr), test_service_client_(nullptr) {}

  ~NetworkConnectionHandlerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(test_nssdb_.is_open());

    // Use the same DB for public and private slot.
    test_nsscertdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(test_nssdb_.slot()))));
    test_nsscertdb_->SetSlowTaskRunnerForTest(message_loop_.task_runner());

    CertLoader::Initialize();
    CertLoader::ForceHardwareBackedForTesting();

    DBusThreadManager::Initialize();
    DBusThreadManager* dbus_manager = DBusThreadManager::Get();
    test_manager_client_ =
        dbus_manager->GetShillManagerClient()->GetTestInterface();
    test_service_client_ =
        dbus_manager->GetShillServiceClient()->GetTestInterface();

    test_manager_client_->AddTechnology(shill::kTypeWifi, true /* enabled */);
    dbus_manager->GetShillDeviceClient()->GetTestInterface()->AddDevice(
        "/device/wifi1", shill::kTypeWifi, "wifi_device1");
    test_manager_client_->AddTechnology(shill::kTypeCellular,
                                        true /* enabled */);
    dbus_manager->GetShillProfileClient()->GetTestInterface()->AddProfile(
        "shared_profile_path", std::string() /* shared profile */);
    dbus_manager->GetShillProfileClient()->GetTestInterface()->AddProfile(
        "user_profile_path", kUsernameHash);

    base::RunLoop().RunUntilIdle();
    LoginState::Initialize();
    network_state_handler_.reset(NetworkStateHandler::InitializeForTest());
    network_config_handler_.reset(
        NetworkConfigurationHandler::InitializeForTest(
            network_state_handler_.get(),
            nullptr /* network_device_handler */));

    network_profile_handler_.reset(new NetworkProfileHandler());
    network_profile_handler_->Init();

    managed_config_handler_.reset(new ManagedNetworkConfigurationHandlerImpl());
    managed_config_handler_->Init(
        network_state_handler_.get(), network_profile_handler_.get(),
        network_config_handler_.get(), nullptr /* network_device_handler */);

    network_connection_handler_.reset(new NetworkConnectionHandler);
    network_connection_handler_->Init(network_state_handler_.get(),
                                      network_config_handler_.get(),
                                      managed_config_handler_.get());
    network_connection_observer_.reset(new TestNetworkConnectionObserver);
    network_connection_handler_->AddObserver(
        network_connection_observer_.get());

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    managed_config_handler_.reset();
    network_profile_handler_.reset();
    network_connection_handler_->RemoveObserver(
        network_connection_observer_.get());
    network_connection_observer_.reset();
    network_connection_handler_.reset();
    network_config_handler_.reset();
    network_state_handler_.reset();
    CertLoader::Shutdown();
    LoginState::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  bool Configure(const std::string& json_string) {
    scoped_ptr<base::DictionaryValue> json_dict =
        onc::ReadDictionaryFromJson(json_string);
    if (!json_dict) {
      LOG(ERROR) << "Error parsing json: " << json_string;
      return false;
    }
    DBusThreadManager::Get()->GetShillManagerClient()->ConfigureService(
        *json_dict,
        base::Bind(&ConfigureCallback),
        base::Bind(&ConfigureErrorCallback));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void Connect(const std::string& service_path) {
    const bool check_error_state = true;
    network_connection_handler_->ConnectToNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerTest::ErrorCallback,
                   base::Unretained(this)),
        check_error_state);
    base::RunLoop().RunUntilIdle();
  }

  void Disconnect(const std::string& service_path) {
    network_connection_handler_->DisconnectNetwork(
        service_path,
        base::Bind(&NetworkConnectionHandlerTest::SuccessCallback,
                   base::Unretained(this)),
        base::Bind(&NetworkConnectionHandlerTest::ErrorCallback,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void SuccessCallback() {
    result_ = kSuccessResult;
  }

  void ErrorCallback(const std::string& error_name,
                     scoped_ptr<base::DictionaryValue> error_data) {
    result_ = error_name;
  }

  std::string GetResultAndReset() {
    std::string result;
    result.swap(result_);
    return result;
  }

  std::string GetServiceStringProperty(const std::string& service_path,
                                       const std::string& key) {
    std::string result;
    const base::DictionaryValue* properties =
        test_service_client_->GetServiceProperties(service_path);
    if (properties)
      properties->GetStringWithoutPathExpansion(key, &result);
    return result;
  }

  void StartCertLoader() {
    CertLoader::Get()->StartWithNSSDB(test_nsscertdb_.get());
    base::RunLoop().RunUntilIdle();
  }

  void LoginToRegularUser() {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_ACTIVE,
                                        LoginState::LOGGED_IN_USER_REGULAR);
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<net::X509Certificate> ImportTestClientCert() {
    net::CertificateList ca_cert_list(net::CreateCertificateListFromFile(
        net::GetTestCertsDirectory(), "client_1_ca.pem",
        net::X509Certificate::FORMAT_AUTO));
    if (ca_cert_list.empty()) {
      LOG(ERROR) << "No CA cert loaded.";
      return nullptr;
    }
    net::NSSCertDatabase::ImportCertFailureList failures;
    EXPECT_TRUE(test_nsscertdb_->ImportCACerts(
        ca_cert_list, net::NSSCertDatabase::TRUST_DEFAULT, &failures));
    if (!failures.empty()) {
      LOG(ERROR) << net::ErrorToString(failures[0].net_error);
      return nullptr;
    }

    // Import a client cert signed by that CA.
    scoped_refptr<net::X509Certificate> client_cert(
        net::ImportClientCertAndKeyFromFile(net::GetTestCertsDirectory(),
                                            "client_1.pem", "client_1.pk8",
                                            test_nssdb_.slot()));
    return client_cert;
  }

  void SetupPolicy(const std::string& network_configs_json,
                   const base::DictionaryValue& global_config,
                   bool user_policy) {
    std::string error;
    scoped_ptr<base::Value> network_configs_value(
        base::JSONReader::DeprecatedReadAndReturnError(
            network_configs_json, base::JSON_ALLOW_TRAILING_COMMAS, nullptr,
            &error));
    ASSERT_TRUE(network_configs_value) << error;

    base::ListValue* network_configs = nullptr;
    ASSERT_TRUE(network_configs_value->GetAsList(&network_configs));

    if (user_policy) {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_USER_POLICY,
                                         kUsernameHash, *network_configs,
                                         global_config);
    } else {
      managed_config_handler_->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY,
                                         std::string(),  // no username hash
                                         *network_configs,
                                         global_config);
    }
    base::RunLoop().RunUntilIdle();
  }

  scoped_ptr<NetworkStateHandler> network_state_handler_;
  scoped_ptr<NetworkConfigurationHandler> network_config_handler_;
  scoped_ptr<NetworkConnectionHandler> network_connection_handler_;
  scoped_ptr<TestNetworkConnectionObserver> network_connection_observer_;
  scoped_ptr<ManagedNetworkConfigurationHandlerImpl> managed_config_handler_;
  scoped_ptr<NetworkProfileHandler> network_profile_handler_;
  ShillManagerClient::TestInterface* test_manager_client_;
  ShillServiceClient::TestInterface* test_service_client_;
  crypto::ScopedTestNSSDB test_nssdb_;
  scoped_ptr<net::NSSCertDatabaseChromeOS> test_nsscertdb_;
  base::MessageLoopForUI message_loop_;
  std::string result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnectionHandlerTest);
};

namespace {

const char* kNoNetwork = "no-network";
const char* kWifi0 = "wifi0";
const char* kWifi1 = "wifi1";
const char* kWifi2 = "wifi2";
const char* kWifi3 = "wifi3";

const char* kConfigConnectable =
    "{ \"GUID\": \"wifi0\", \"Type\": \"wifi\", \"State\": \"idle\", "
    "  \"Connectable\": true }";
const char* kConfigConnected =
    "{ \"GUID\": \"wifi1\", \"Type\": \"wifi\", \"State\": \"online\" }";
const char* kConfigConnecting =
    "{ \"GUID\": \"wifi2\", \"Type\": \"wifi\", \"State\": \"association\" }";
const char* kConfigRequiresPassphrase =
    "{ \"GUID\": \"wifi3\", \"Type\": \"wifi\", "
    "  \"PassphraseRequired\": true }";

}  // namespace

TEST_F(NetworkConnectionHandlerTest, NetworkConnectionHandlerConnectSuccess) {
  EXPECT_TRUE(Configure(kConfigConnectable));
  Connect(kWifi0);
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
  EXPECT_EQ(shill::kStateOnline,
            GetServiceStringProperty(kWifi0, shill::kStateProperty));
  // Observer expectations
  EXPECT_TRUE(network_connection_observer_->GetRequested(kWifi0));
  EXPECT_EQ(kSuccessResult, network_connection_observer_->GetResult(kWifi0));
}

// Handles basic failure cases.
TEST_F(NetworkConnectionHandlerTest, NetworkConnectionHandlerConnectFailure) {
  Connect(kNoNetwork);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer_->GetRequested(kNoNetwork));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            network_connection_observer_->GetResult(kNoNetwork));

  EXPECT_TRUE(Configure(kConfigConnected));
  Connect(kWifi1);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer_->GetRequested(kWifi1));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnected,
            network_connection_observer_->GetResult(kWifi1));

  EXPECT_TRUE(Configure(kConfigConnecting));
  Connect(kWifi2);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnecting, GetResultAndReset());
  EXPECT_TRUE(network_connection_observer_->GetRequested(kWifi2));
  EXPECT_EQ(NetworkConnectionHandler::kErrorConnecting,
            network_connection_observer_->GetResult(kWifi2));

  EXPECT_TRUE(Configure(kConfigRequiresPassphrase));
  Connect(kWifi3);
  EXPECT_EQ(NetworkConnectionHandler::kErrorPassphraseRequired,
            GetResultAndReset());
  EXPECT_TRUE(network_connection_observer_->GetRequested(kWifi3));
  EXPECT_EQ(NetworkConnectionHandler::kErrorPassphraseRequired,
            network_connection_observer_->GetResult(kWifi3));
}

namespace {

const char* kPolicyWithCertPatternTemplate =
    "[ { \"GUID\": \"wifi4\","
    "    \"Name\": \"wifi4\","
    "    \"Type\": \"WiFi\","
    "    \"WiFi\": {"
    "      \"Security\": \"WPA-EAP\","
    "      \"SSID\": \"wifi_ssid\","
    "      \"EAP\": {"
    "        \"Outer\": \"EAP-TLS\","
    "        \"ClientCertType\": \"Pattern\","
    "        \"ClientCertPattern\": {"
    "          \"Subject\": {"
    "            \"CommonName\" : \"%s\""
    "          }"
    "        }"
    "      }"
    "    }"
    "} ]";

}  // namespace

// Handle certificates.
TEST_F(NetworkConnectionHandlerTest, ConnectCertificateMissing) {
  StartCertLoader();
  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate, "unknown"),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect("wifi4");
  EXPECT_EQ(NetworkConnectionHandler::kErrorCertificateRequired,
            GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest, ConnectWithCertificateSuccess) {
  StartCertLoader();
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect("wifi4");
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

// Disabled, see http://crbug.com/396729.
TEST_F(NetworkConnectionHandlerTest,
       DISABLED_ConnectWithCertificateRequestedBeforeCertsAreLoaded) {
  scoped_refptr<net::X509Certificate> cert = ImportTestClientCert();
  ASSERT_TRUE(cert.get());

  SetupPolicy(base::StringPrintf(kPolicyWithCertPatternTemplate,
                                 cert->subject().common_name.c_str()),
              base::DictionaryValue(),  // no global config
              true);                    // load as user policy

  Connect("wifi4");

  // Connect request came before the cert loader loaded certificates, so the
  // connect request should have been throttled until the certificates are
  // loaded.
  EXPECT_EQ("", GetResultAndReset());

  StartCertLoader();

  // |StartCertLoader| should have triggered certificate loading.
  // When the certificates got loaded, the connection request should have
  // proceeded and eventually succeeded.
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest,
       NetworkConnectionHandlerDisconnectSuccess) {
  EXPECT_TRUE(Configure(kConfigConnected));
  Disconnect(kWifi1);
  EXPECT_TRUE(network_connection_observer_->GetRequested(kWifi1));
  EXPECT_EQ(kSuccessResult, GetResultAndReset());
}

TEST_F(NetworkConnectionHandlerTest,
       NetworkConnectionHandlerDisconnectFailure) {
  Connect(kNoNetwork);
  EXPECT_EQ(NetworkConnectionHandler::kErrorConfigureFailed,
            GetResultAndReset());

  EXPECT_TRUE(Configure(kConfigConnectable));
  Disconnect(kWifi0);
  EXPECT_EQ(NetworkConnectionHandler::kErrorNotConnected, GetResultAndReset());
}

}  // namespace chromeos
