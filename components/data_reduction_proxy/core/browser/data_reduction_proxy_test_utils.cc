// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"

#include "base/prefs/testing_pref_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_experiments_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_storage_delegate_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_test_util.h"
#include "url/gurl.h"

namespace {

const char kTestKey[] = "test-key";

const net::BackoffEntry::Policy kTestBackoffPolicy = {
    0,               // num_errors_to_ignore
    10 * 1000,       // initial_delay_ms
    2,               // multiply_factor
    0,               // jitter_factor
    30 * 60 * 1000,  // maximum_backoff_ms
    -1,              // entry_lifetime_ms
    true,            // always_use_initial_delay
};

}  // namespace

namespace data_reduction_proxy {

TestDataReductionProxyRequestOptions::TestDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : DataReductionProxyRequestOptions(client, version, config) {
}

std::string TestDataReductionProxyRequestOptions::GetDefaultKey() const {
  return kTestKey;
}

base::Time TestDataReductionProxyRequestOptions::Now() const {
  return base::Time::UnixEpoch() + now_offset_;
}

void TestDataReductionProxyRequestOptions::RandBytes(void* output,
                                                     size_t length) const {
  char* c = static_cast<char*>(output);
  for (size_t i = 0; i < length; ++i) {
    c[i] = 'a';
  }
}

// Time after the unix epoch that Now() reports.
void TestDataReductionProxyRequestOptions::set_offset(
    const base::TimeDelta& now_offset) {
  now_offset_ = now_offset;
}

MockDataReductionProxyRequestOptions::MockDataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : TestDataReductionProxyRequestOptions(client, version, config) {
}

MockDataReductionProxyRequestOptions::~MockDataReductionProxyRequestOptions() {
}

TestDataReductionProxyConfigServiceClient::
    TestDataReductionProxyConfigServiceClient(
        scoped_ptr<DataReductionProxyParams> params,
        DataReductionProxyRequestOptions* request_options,
        DataReductionProxyMutableConfigValues* config_values,
        DataReductionProxyConfig* config,
        DataReductionProxyEventCreator* event_creator,
        net::NetLog* net_log,
        ConfigStorer config_storer)
    : DataReductionProxyConfigServiceClient(params.Pass(),
                                            kTestBackoffPolicy,
                                            request_options,
                                            config_values,
                                            config,
                                            event_creator,
                                            net_log,
                                            config_storer),
      tick_clock_(base::Time::UnixEpoch()),
      test_backoff_entry_(&kTestBackoffPolicy, &tick_clock_) {
}

TestDataReductionProxyConfigServiceClient::
    ~TestDataReductionProxyConfigServiceClient() {
}

void TestDataReductionProxyConfigServiceClient::SetNow(const base::Time& time) {
  tick_clock_.SetTime(time);
}

void TestDataReductionProxyConfigServiceClient::SetCustomReleaseTime(
    const base::TimeTicks& release_time) {
  test_backoff_entry_.SetCustomReleaseTime(release_time);
}

base::TimeDelta TestDataReductionProxyConfigServiceClient::GetDelay() const {
  return config_refresh_timer_.GetCurrentDelay();
}

int TestDataReductionProxyConfigServiceClient::GetBackoffErrorCount() {
  return test_backoff_entry_.failure_count();
}

void TestDataReductionProxyConfigServiceClient::SetConfigServiceURL(
    const GURL& service_url) {
  config_service_url_ = service_url;
  use_local_config_ = !config_service_url_.is_valid();
}

base::Time TestDataReductionProxyConfigServiceClient::Now() {
  return tick_clock_.Now();
}

net::BackoffEntry*
TestDataReductionProxyConfigServiceClient::GetBackoffEntry() {
  return &test_backoff_entry_;
}

TestDataReductionProxyConfigServiceClient::TestTickClock::TestTickClock(
    const base::Time& initial_time)
    : time_(initial_time) {
}

base::TimeTicks
TestDataReductionProxyConfigServiceClient::TestTickClock::NowTicks() {
  return base::TimeTicks::UnixEpoch() + (time_ - base::Time::UnixEpoch());
}

base::Time
TestDataReductionProxyConfigServiceClient::TestTickClock::Now() {
  return time_;
}

void TestDataReductionProxyConfigServiceClient::TestTickClock::SetTime(
    const base::Time& time) {
  time_ = time;
}

MockDataReductionProxyService::MockDataReductionProxyService(
    DataReductionProxySettings* settings,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : DataReductionProxyService(settings,
                                prefs,
                                request_context,
                                make_scoped_ptr(new DataStore()),
                                task_runner,
                                task_runner,
                                task_runner,
                                base::TimeDelta()) {
}

MockDataReductionProxyService::~MockDataReductionProxyService() {
}

TestDataReductionProxyIOData::TestDataReductionProxyIOData(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<DataReductionProxyConfig> config,
    scoped_ptr<DataReductionProxyEventCreator> event_creator,
    scoped_ptr<DataReductionProxyRequestOptions> request_options,
    scoped_ptr<DataReductionProxyConfigurator> configurator,
    scoped_ptr<DataReductionProxyConfigServiceClient> config_client,
    scoped_ptr<DataReductionProxyExperimentsStats> experiments_stats,
    net::NetLog* net_log,
    bool enabled)
    : DataReductionProxyIOData(), service_set_(false) {
  io_task_runner_ = task_runner;
  ui_task_runner_ = task_runner;
  config_ = config.Pass();
  event_creator_ = event_creator.Pass();
  request_options_ = request_options.Pass();
  configurator_ = configurator.Pass();
  config_client_ = config_client.Pass();
  experiments_stats_ = experiments_stats.Pass();
  net_log_ = net_log;
  bypass_stats_.reset(new DataReductionProxyBypassStats(
      config_.get(), base::Bind(&DataReductionProxyIOData::SetUnreachable,
                                base::Unretained(this))));
  enabled_ = enabled;
}

TestDataReductionProxyIOData::~TestDataReductionProxyIOData() {
}

void TestDataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  if (!service_set_)
    DataReductionProxyIOData::SetDataReductionProxyService(
        data_reduction_proxy_service);

  service_set_ = true;
}

DataReductionProxyTestContext::Builder::Builder()
    : params_flags_(DataReductionProxyParams::kAllowed |
                    DataReductionProxyParams::kFallbackAllowed |
                    DataReductionProxyParams::kPromoAllowed),
      params_definitions_(
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_SSL_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN),
      client_(Client::UNKNOWN),
      request_context_(nullptr),
      mock_socket_factory_(nullptr),
      use_mock_config_(false),
      use_test_configurator_(false),
      use_mock_service_(false),
      use_mock_request_options_(false),
      use_config_client_(false),
      use_test_config_client_(false),
      skip_settings_initialization_(false) {
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsFlags(int params_flags) {
  params_flags_ = params_flags;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithParamsDefinitions(
    unsigned int params_definitions) {
  params_definitions_ = params_definitions;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithURLRequestContext(
    net::URLRequestContext* request_context) {
  request_context_ = request_context;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockClientSocketFactory(
    net::MockClientSocketFactory* mock_socket_factory) {
  mock_socket_factory_ = mock_socket_factory;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithClient(Client client) {
  client_ = client;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockConfig() {
  use_mock_config_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigurator() {
  use_test_configurator_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockDataReductionProxyService() {
  use_mock_service_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithMockRequestOptions() {
  use_mock_request_options_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithConfigClient() {
  use_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::WithTestConfigClient() {
  use_config_client_ = true;
  use_test_config_client_ = true;
  return *this;
}

DataReductionProxyTestContext::Builder&
DataReductionProxyTestContext::Builder::SkipSettingsInitialization() {
  skip_settings_initialization_ = true;
  return *this;
}

scoped_ptr<DataReductionProxyTestContext>
DataReductionProxyTestContext::Builder::Build() {
  // Check for invalid builder combinations.
  DCHECK(!(use_mock_config_ && use_config_client_));

  unsigned int test_context_flags = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
  scoped_ptr<TestingPrefServiceSimple> pref_service(
      new TestingPrefServiceSimple());
  scoped_ptr<net::TestNetLog> net_log(new net::TestNetLog());
  scoped_ptr<TestConfigStorer> config_storer(
      new TestConfigStorer(pref_service.get()));
  if (request_context_) {
    request_context_getter = new net::TrivialURLRequestContextGetter(
        request_context_, task_runner);
  } else {
    scoped_ptr<net::TestURLRequestContext> test_request_context(
        new net::TestURLRequestContext(true));
    if (mock_socket_factory_)
      test_request_context->set_client_socket_factory(mock_socket_factory_);
    test_request_context->Init();
    request_context_getter = new net::TestURLRequestContextGetter(
        task_runner, test_request_context.Pass());
  }

  scoped_ptr<TestDataReductionProxyEventStorageDelegate> storage_delegate(
      new TestDataReductionProxyEventStorageDelegate());
  scoped_ptr<DataReductionProxyEventCreator> event_creator(
      new DataReductionProxyEventCreator(storage_delegate.get()));
  scoped_ptr<DataReductionProxyConfigurator> configurator;
  if (use_test_configurator_) {
    test_context_flags |= USE_TEST_CONFIGURATOR;
    configurator.reset(new TestDataReductionProxyConfigurator(
        net_log.get(), event_creator.get()));
  } else {
    configurator.reset(
        new DataReductionProxyConfigurator(net_log.get(), event_creator.get()));
  }

  scoped_ptr<TestDataReductionProxyConfig> config;
  scoped_ptr<DataReductionProxyConfigServiceClient> config_client;
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  scoped_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams(params_flags_, params_definitions_));
  TestDataReductionProxyParams* raw_params = params.get();
  if (use_config_client_) {
    test_context_flags |= USE_CONFIG_CLIENT;
    scoped_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        DataReductionProxyMutableConfigValues::CreateFromParams(params.get());
    raw_mutable_config = mutable_config.get();
    config.reset(new TestDataReductionProxyConfig(
        mutable_config.Pass(), net_log.get(), configurator.get(),
        event_creator.get()));
  } else if (use_mock_config_) {
    test_context_flags |= USE_MOCK_CONFIG;
    config.reset(new MockDataReductionProxyConfig(
        params.Pass(), net_log.get(), configurator.get(), event_creator.get()));
  } else {
    config.reset(new TestDataReductionProxyConfig(
        params.Pass(), net_log.get(), configurator.get(), event_creator.get()));
  }

  scoped_ptr<DataReductionProxyRequestOptions> request_options;
  if (use_mock_request_options_) {
    test_context_flags |= USE_MOCK_REQUEST_OPTIONS;
    request_options.reset(new MockDataReductionProxyRequestOptions(
        client_, std::string(), config.get()));
  } else {
    request_options.reset(
        new DataReductionProxyRequestOptions(client_, config.get()));
  }

  if (use_test_config_client_) {
    test_context_flags |= USE_TEST_CONFIG_CLIENT;
    config_client.reset(new TestDataReductionProxyConfigServiceClient(
        params.Pass(), request_options.get(), raw_mutable_config, config.get(),
        event_creator.get(), net_log.get(),
        base::Bind(&TestConfigStorer::StoreSerializedConfig,
                   base::Unretained(config_storer.get()))));
  } else if (use_config_client_) {
    config_client.reset(new DataReductionProxyConfigServiceClient(
        params.Pass(), GetBackoffPolicy(), request_options.get(),
        raw_mutable_config, config.get(), event_creator.get(), net_log.get(),
        base::Bind(&TestConfigStorer::StoreSerializedConfig,
                   base::Unretained(config_storer.get()))));
  }

  scoped_ptr<DataReductionProxySettings> settings(
      new DataReductionProxySettings());
  if (skip_settings_initialization_)
    test_context_flags |= SKIP_SETTINGS_INITIALIZATION;

  if (use_mock_service_)
    test_context_flags |= USE_MOCK_SERVICE;

  RegisterSimpleProfilePrefs(pref_service->registry());

  scoped_ptr<DataReductionProxyExperimentsStats> experiments_stats(
      new DataReductionProxyExperimentsStats(base::Bind(
          &PrefService::SetInt64, base::Unretained(pref_service.get()))));
  scoped_ptr<TestDataReductionProxyIOData> io_data(
      new TestDataReductionProxyIOData(
          task_runner, config.Pass(), event_creator.Pass(),
          request_options.Pass(), configurator.Pass(), config_client.Pass(),
          experiments_stats.Pass(), net_log.get(), true /* enabled */));
  io_data->SetSimpleURLRequestContextGetter(request_context_getter);

  scoped_ptr<DataReductionProxyTestContext> test_context(
      new DataReductionProxyTestContext(
          task_runner, pref_service.Pass(), net_log.Pass(),
          request_context_getter, mock_socket_factory_, io_data.Pass(),
          settings.Pass(), storage_delegate.Pass(), config_storer.Pass(),
          raw_params, test_context_flags));

  if (!skip_settings_initialization_)
    test_context->InitSettingsWithoutCheck();

  return test_context.Pass();
}

DataReductionProxyTestContext::DataReductionProxyTestContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<TestingPrefServiceSimple> simple_pref_service,
    scoped_ptr<net::TestNetLog> net_log,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    net::MockClientSocketFactory* mock_socket_factory,
    scoped_ptr<TestDataReductionProxyIOData> io_data,
    scoped_ptr<DataReductionProxySettings> settings,
    scoped_ptr<TestDataReductionProxyEventStorageDelegate> storage_delegate,
    scoped_ptr<TestConfigStorer> config_storer,
    TestDataReductionProxyParams* params,
    unsigned int test_context_flags)
    : test_context_flags_(test_context_flags),
      task_runner_(task_runner),
      simple_pref_service_(simple_pref_service.Pass()),
      net_log_(net_log.Pass()),
      request_context_getter_(request_context_getter),
      mock_socket_factory_(mock_socket_factory),
      io_data_(io_data.Pass()),
      settings_(settings.Pass()),
      storage_delegate_(storage_delegate.Pass()),
      config_storer_(config_storer.Pass()),
      params_(params) {
}

DataReductionProxyTestContext::~DataReductionProxyTestContext() {
  DestroySettings();
}

void DataReductionProxyTestContext::RunUntilIdle() {
  base::MessageLoop::current()->RunUntilIdle();
}

void DataReductionProxyTestContext::InitSettings() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  InitSettingsWithoutCheck();
}

void DataReductionProxyTestContext::DestroySettings() {
  // Force destruction of |DBDataOwner|, which lives on DB task runner and is
  // indirectly owned by |settings_|.
  if (settings_.get()) {
    settings_.reset();
    RunUntilIdle();
  }
}

void DataReductionProxyTestContext::InitSettingsWithoutCheck() {
  settings_->InitDataReductionProxySettings(
      simple_pref_service_.get(), io_data_.get(),
      CreateDataReductionProxyServiceInternal());
  storage_delegate_->SetStorageDelegate(
      settings_->data_reduction_proxy_service()->event_store());
  io_data_->SetDataReductionProxyService(
      settings_->data_reduction_proxy_service()->GetWeakPtr());
  if (io_data_->config_client())
    io_data_->config_client()->InitializeOnIOThread(
        request_context_getter_.get());
  settings_->data_reduction_proxy_service()->SetIOData(io_data_->GetWeakPtr());
}

scoped_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyService() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION);
  return CreateDataReductionProxyServiceInternal();
}

scoped_ptr<DataReductionProxyService>
DataReductionProxyTestContext::CreateDataReductionProxyServiceInternal() {
  if (test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE) {
    return make_scoped_ptr(new MockDataReductionProxyService(
        settings_.get(), simple_pref_service_.get(),
        request_context_getter_.get(), task_runner_));
  } else {
    return make_scoped_ptr(new DataReductionProxyService(
        settings_.get(), simple_pref_service_.get(),
        request_context_getter_.get(), make_scoped_ptr(new DataStore()),
        task_runner_, task_runner_, task_runner_, base::TimeDelta()));
  }
}

void DataReductionProxyTestContext::AttachToURLRequestContext(
      net::URLRequestContextStorage* request_context_storage) const {
  DCHECK(request_context_storage);

  // |request_context_storage| takes ownership of the network delegate.
  request_context_storage->set_network_delegate(
      io_data()->CreateNetworkDelegate(
          scoped_ptr<net::NetworkDelegate>(new net::TestNetworkDelegate()),
          true).release());

  // |request_context_storage| takes ownership of the job factory.
  request_context_storage->set_job_factory(
      new net::URLRequestInterceptingJobFactory(
          scoped_ptr<net::URLRequestJobFactory>(
              new net::URLRequestJobFactoryImpl()),
          io_data()->CreateInterceptor().Pass()));
}

void DataReductionProxyTestContext::
    EnableDataReductionProxyWithSecureProxyCheckSuccess() {
  DCHECK(mock_socket_factory_);
  // This won't actually update the proxy config when using a test configurator.
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::USE_TEST_CONFIGURATOR));
  // |settings_| needs to have been initialized, since a
  // |DataReductionProxyService| is needed in order to issue the secure proxy
  // check.
  DCHECK(data_reduction_proxy_service());

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check.
  net::MockRead mock_reads[] = {
      net::MockRead("HTTP/1.1 200 OK\r\n\r\n"),
      net::MockRead("OK"),
      net::MockRead(net::SYNCHRONOUS, net::OK),
  };
  net::StaticSocketDataProvider socket_data_provider(
      mock_reads, arraysize(mock_reads), nullptr, 0);
  mock_socket_factory_->AddSocketDataProvider(&socket_data_provider);

  // Set the pref to cause the secure proxy check to be issued.
  pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled, true);
  RunUntilIdle();
}

TestDataReductionProxyConfigurator*
DataReductionProxyTestContext::test_configurator() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIGURATOR);
  return reinterpret_cast<TestDataReductionProxyConfigurator*>(
      io_data_->configurator());
}

MockDataReductionProxyConfig* DataReductionProxyTestContext::mock_config()
    const {
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_CONFIG);
  return reinterpret_cast<MockDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyService*
DataReductionProxyTestContext::data_reduction_proxy_service() const {
  return settings_->data_reduction_proxy_service();
}

MockDataReductionProxyService*
DataReductionProxyTestContext::mock_data_reduction_proxy_service()
    const {
  DCHECK(!(test_context_flags_ &
           DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));
  DCHECK(test_context_flags_ & DataReductionProxyTestContext::USE_MOCK_SERVICE);
  return reinterpret_cast<MockDataReductionProxyService*>(
      data_reduction_proxy_service());
}

MockDataReductionProxyRequestOptions*
DataReductionProxyTestContext::mock_request_options() const {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_MOCK_REQUEST_OPTIONS);
  return reinterpret_cast<MockDataReductionProxyRequestOptions*>(
      io_data_->request_options());
}

TestDataReductionProxyConfig* DataReductionProxyTestContext::config() const {
  return reinterpret_cast<TestDataReductionProxyConfig*>(io_data_->config());
}

DataReductionProxyMutableConfigValues*
DataReductionProxyTestContext::mutable_config_values() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_CONFIG_CLIENT);
  return reinterpret_cast<DataReductionProxyMutableConfigValues*>(
      config()->config_values());
}

TestDataReductionProxyConfigServiceClient*
DataReductionProxyTestContext::test_config_client() {
  DCHECK(test_context_flags_ &
         DataReductionProxyTestContext::USE_TEST_CONFIG_CLIENT);
  return reinterpret_cast<TestDataReductionProxyConfigServiceClient*>(
      io_data_->config_client());
}

DataReductionProxyBypassStats::UnreachableCallback
DataReductionProxyTestContext::unreachable_callback() const {
  return base::Bind(&DataReductionProxySettings::SetUnreachable,
                    base::Unretained(settings_.get()));
}

DataReductionProxyTestContext::TestConfigStorer::TestConfigStorer(
    PrefService* prefs)
    : prefs_(prefs) {
  DCHECK(prefs);
}

void DataReductionProxyTestContext::TestConfigStorer::StoreSerializedConfig(
    const std::string& serialized_config) {
  prefs_->SetString(prefs::kDataReductionProxyConfig, serialized_config);
}

}  // namespace data_reduction_proxy
