// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "net/base/backoff_entry.h"
#include "net/log/capturing_net_log.h"
#include "testing/gmock/include/gmock/gmock.h"

class TestingPrefServiceSimple;

namespace base {
class MessageLoopForUI;
}

namespace net {
class MockClientSocketFactory;
class NetLog;
class URLRequestContext;
class URLRequestContextGetter;
class URLRequestContextStorage;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyEventStore;
class DataReductionProxyMutableConfigValues;
class DataReductionProxyRequestOptions;
class DataReductionProxySettings;
class DataReductionProxyCompressionStats;
class MockDataReductionProxyConfig;
class TestDataReductionProxyConfig;
class TestDataReductionProxyConfigurator;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyRequestOptions|.
class TestDataReductionProxyRequestOptions
    : public DataReductionProxyRequestOptions {
 public:
  TestDataReductionProxyRequestOptions(
      Client client,
      const std::string& version,
      DataReductionProxyConfig* config,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Overrides of DataReductionProxyRequestOptions.
  std::string GetDefaultKey() const override;
  base::Time Now() const override;
  void RandBytes(void* output, size_t length) const override;

  // Time after the unix epoch that Now() reports.
  void set_offset(const base::TimeDelta& now_offset);

 private:
  base::TimeDelta now_offset_;
};

// Mock version of |DataReductionProxyRequestOptions|.
class MockDataReductionProxyRequestOptions
    : public DataReductionProxyRequestOptions {
 public:
  MockDataReductionProxyRequestOptions(
      Client client,
      const std::string& version,
      DataReductionProxyConfig* config,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~MockDataReductionProxyRequestOptions();

  MOCK_CONST_METHOD1(PopulateConfigResponse,
                     void(base::DictionaryValue* response));
};

// Test version of |DataReductionProxyConfigServiceClient|, which permits
// finely controlling the backoff timer.
class TestDataReductionProxyConfigServiceClient
    : public DataReductionProxyConfigServiceClient {
 public:
  TestDataReductionProxyConfigServiceClient(
      scoped_ptr<DataReductionProxyParams> params,
      DataReductionProxyRequestOptions* request_options,
      DataReductionProxyMutableConfigValues* config_values,
      DataReductionProxyConfig* config,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  ~TestDataReductionProxyConfigServiceClient() override;

  void SetNow(const base::Time& time);

  void SetCustomReleaseTime(const base::TimeTicks& release_time);

  base::TimeDelta GetDelay() const;

 protected:
  // Overrides of DataReductionProxyConfigServiceClient
  base::Time Now() const override;
  net::BackoffEntry* GetBackoffEntry() override;

 private:
  // A clock which returns a fixed value in both base::Time and base::TimeTicks.
  class TestTickClock {
   public:
    TestTickClock(const base::Time& initial_time);

    // Returns the current base::TimeTicks
    base::TimeTicks NowTicks() const;

    // Returns the current base::Time
    base::Time Now() const;

    // Sets the current time.
    void SetTime(const base::Time& time);

   private:
    base::Time time_;
  };

  // A net::BackoffEntry which uses an injected base::TickClock to control
  // the backoff expiration time.
  class TestBackoffEntry : public net::BackoffEntry {
   public:
    TestBackoffEntry(const BackoffEntry::Policy* const policy,
                     const TestTickClock* tick_clock);

   protected:
    // Override of net::BackoffEntry.
    base::TimeTicks ImplGetTimeNow() const override;

   private:
    const TestTickClock* tick_clock_;
  };

  TestTickClock tick_clock_;
  TestBackoffEntry test_backoff_entry_;
};

// Test version of |DataReductionProxyService|, which permits mocking of various
// methods.
class MockDataReductionProxyService : public DataReductionProxyService {
 public:
  MockDataReductionProxyService(
      scoped_ptr<DataReductionProxyCompressionStats> compression_stats,
      DataReductionProxySettings* settings,
      net::URLRequestContextGetter* request_context);
  ~MockDataReductionProxyService() override;

  MOCK_METHOD2(SecureProxyCheck,
      void(const GURL& secure_proxy_check_url,
           FetcherResponseCallback fetcher_callback));
};

// Test version of |DataReductionProxyIOData|, which bypasses initialization in
// the constructor in favor of explicitly passing in its owned classes. This
// permits the use of test/mock versions of those classes.
class TestDataReductionProxyIOData : public DataReductionProxyIOData {
 public:
  TestDataReductionProxyIOData(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<DataReductionProxyConfig> config,
      scoped_ptr<DataReductionProxyEventStore> event_store,
      scoped_ptr<DataReductionProxyRequestOptions> request_options,
      scoped_ptr<DataReductionProxyConfigurator> configurator,
      scoped_ptr<DataReductionProxyConfigServiceClient> config_client);
  ~TestDataReductionProxyIOData() override;

  DataReductionProxyConfigurator* configurator() const {
    return configurator_.get();
  }

  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }
};

// Builds a test version of the Data Reduction Proxy stack for use in tests.
// Takes in various |TestContextOptions| which controls the behavior of the
// underlying objects.
class DataReductionProxyTestContext {
 public:
  // Allows for a fluent builder interface to configure what kind of objects
  // (test vs mock vs real) are used by the |DataReductionProxyTestContext|.
  class Builder {
   public:
    Builder();

    // |DataReductionProxyParams| flags to use.
    Builder& WithParamsFlags(int params_flags);

    // |TestDataReductionProxyParams| flags to use.
    Builder& WithParamsDefinitions(unsigned int params_definitions);

    // The |Client| enum to use for |DataReductionProxyRequestOptions|.
    Builder& WithClient(Client client);

    // Specifies a |net::URLRequestContext| to use. The |request_context| is
    // owned by the caller.
    Builder& WithURLRequestContext(net::URLRequestContext* request_context);

    // Specifies a |net::MockClientSocketFactory| to use. The
    // |mock_socket_factory| is owned by the caller. If a non-NULL
    // |request_context_| is also specified, then the caller is responsible for
    // attaching |mock_socket_factory| to |request_context_|. Otherwise,
    // |mock_socket_factory| will be attached to the dummy
    // |net::URLRequestContext| generated during Build().
    Builder& WithMockClientSocketFactory(
        net::MockClientSocketFactory* mock_socket_factory);

    // Specifies the use of |MockDataReductionProxyConfig| instead of
    // |TestDataReductionProxyConfig|.
    Builder& WithMockConfig();

    // Specifies the use of |TestDataReductionProxyConfigurator| instead of
    // |DataReductionProxyConfigurator|.
    Builder& WithTestConfigurator();

    // Specifies the use of |MockDataReductionProxyService| instead of
    // |DataReductionProxyService|.
    Builder& WithMockDataReductionProxyService();

    // Specifies the use of |MockDataReductionProxyRequestOptions| instead of
    // |DataReductionProxyRequestOptions|.
    Builder& WithMockRequestOptions();

    // Specifies the use of the |DataReductionProxyConfigServiceClient|.
    Builder& WithConfigClient();

    // Specifies the use of the a |TestDataReductionProxyConfigServiceClient|
    // instead of a |DataReductionProxyConfigServiceClient|.
    Builder& WithTestConfigClient();

    // Construct, but do not initialize the |DataReductionProxySettings| object.
    Builder& SkipSettingsInitialization();

    // Creates a |DataReductionProxyTestContext|. Owned by the caller.
    scoped_ptr<DataReductionProxyTestContext> Build();

   private:
    int params_flags_;
    unsigned int params_definitions_;
    Client client_;
    net::URLRequestContext* request_context_;
    net::MockClientSocketFactory* mock_socket_factory_;

    bool use_mock_config_;
    bool use_test_configurator_;
    bool use_mock_service_;
    bool use_mock_request_options_;
    bool use_config_client_;
    bool use_test_config_client_;
    bool skip_settings_initialization_;
  };

  virtual ~DataReductionProxyTestContext();

  // Waits while executing all tasks on the current SingleThreadTaskRunner.
  void RunUntilIdle();

  // Initializes the |DataReductionProxySettings| object. Can only be called if
  // built with SkipSettingsInitialization.
  void InitSettings();

  // Creates a |DataReductionProxyService| object, or a
  // |MockDataReductionProxyService| if built with
  // WithMockDataReductionProxyService. Can only be called if built with
  // SkipSettingsInitialization.
  scoped_ptr<DataReductionProxyService> CreateDataReductionProxyService();

  // This creates a |DataReductionProxyNetworkDelegate| and
  // |DataReductionProxyInterceptor|, using them in the |net::URLRequestContext|
  // for |request_context_storage|. |request_context_storage| takes ownership of
  // the created objects.
  void AttachToURLRequestContext(
      net::URLRequestContextStorage* request_context_storage) const;

  // Enable the Data Reduction Proxy, simulating a successful secure proxy
  // check. This can only be called if not built with WithTestConfigurator,
  // |settings_| has been initialized, and |this| was built with a
  // |net::MockClientSocketFactory| specified.
  void EnableDataReductionProxyWithSecureProxyCheckSuccess();

  // Returns the underlying |TestDataReductionProxyConfigurator|. This can only
  // be called if built with WithTestConfigurator.
  TestDataReductionProxyConfigurator* test_configurator() const;

  // Returns the underlying |MockDataReductionProxyConfig|. This can only be
  // called if built with WithMockConfig.
  MockDataReductionProxyConfig* mock_config() const;

  DataReductionProxyService* data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyService|. This can only
  // be called if built with WithMockDataReductionProxyService.
  MockDataReductionProxyService* mock_data_reduction_proxy_service() const;

  // Returns the underlying |MockDataReductionProxyRequestOptions|. This can
  // only be called if built with WithMockRequestOptions.
  MockDataReductionProxyRequestOptions* mock_request_options() const;

  // Returns the underlying |TestDataReductionProxyConfig|.
  TestDataReductionProxyConfig* config() const;

  // Returns the underlying |DataReductionProxyMutableConfigValues|. This can
  // only be called if built with WithConfigClient.
  DataReductionProxyMutableConfigValues* mutable_config_values();

  // Returns the underlying |TestDataReductionProxyConfigServiceClient|. This
  // can only be called if built with WithTestConfigClient.
  TestDataReductionProxyConfigServiceClient* test_config_client();

  // Obtains a callback for notifying that the Data Reduction Proxy is no
  // longer reachable.
  DataReductionProxyBypassStats::UnreachableCallback
  unreachable_callback() const;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  TestingPrefServiceSimple* pref_service() {
    return simple_pref_service_.get();
  }

  net::NetLog* net_log() {
    return net_log_.get();
  }

  net::URLRequestContextGetter* request_context_getter() const {
    return request_context_getter_.get();
  }

  DataReductionProxyEventStore* event_store() const {
    return io_data_->event_store();
  }

  DataReductionProxyConfigurator* configurator() const {
    return io_data_->configurator();
  }

  TestDataReductionProxyIOData* io_data() const {
    return io_data_.get();
  }

  DataReductionProxySettings* settings() const {
    return settings_.get();
  }

  TestDataReductionProxyParams* test_params() const {
    return params_;
  }

 private:
  enum TestContextOptions {
    // Permits mocking of the underlying |DataReductionProxyConfig|.
    USE_MOCK_CONFIG = 0x1,
    // Uses a |TestDataReductionProxyConfigurator| to record proxy configuration
    // changes.
    USE_TEST_CONFIGURATOR = 0x2,
    // Construct, but do not initialize the |DataReductionProxySettings| object.
    // Primarily used for testing of the |DataReductionProxySettings| object
    // itself.
    SKIP_SETTINGS_INITIALIZATION = 0x4,
    // Permits mocking of the underlying |DataReductionProxyService|.
    USE_MOCK_SERVICE = 0x8,
    // Permits mocking of the underlying |DataReductionProxyRequestOptions|.
    USE_MOCK_REQUEST_OPTIONS = 0x10,
    // Specifies the use of the |DataReductionProxyConfigServiceClient|.
    USE_CONFIG_CLIENT = 0x20,
    // Specifies the use of the |TESTDataReductionProxyConfigServiceClient|.
    USE_TEST_CONFIG_CLIENT = 0x40,
  };

  DataReductionProxyTestContext(
      scoped_ptr<base::MessageLoop> loop,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_ptr<TestingPrefServiceSimple> simple_pref_service,
      scoped_ptr<net::CapturingNetLog> net_log,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      net::MockClientSocketFactory* mock_socket_factory,
      scoped_ptr<TestDataReductionProxyIOData> io_data,
      scoped_ptr<DataReductionProxySettings> settings,
      TestDataReductionProxyParams* params,
      unsigned int test_context_flags);

  void InitSettingsWithoutCheck();

  scoped_ptr<DataReductionProxyService>
      CreateDataReductionProxyServiceInternal();

  unsigned int test_context_flags_;

  scoped_ptr<base::MessageLoop> loop_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<TestingPrefServiceSimple> simple_pref_service_;
  scoped_ptr<net::CapturingNetLog> net_log_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  // Non-owned pointer. Will be NULL if |this| was built without specifying a
  // |net::MockClientSocketFactory|.
  net::MockClientSocketFactory* mock_socket_factory_;

  scoped_ptr<TestDataReductionProxyIOData> io_data_;
  scoped_ptr<DataReductionProxySettings> settings_;

  TestDataReductionProxyParams* params_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyTestContext);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_TEST_UTILS_H_
