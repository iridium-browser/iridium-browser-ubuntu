// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_
#define COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_

#include <string>

#include "components/metrics/metrics_service_client.h"

namespace metrics {

// A simple concrete implementation of the MetricsServiceClient interface, for
// use in tests.
class TestMetricsServiceClient : public MetricsServiceClient {
 public:
  static const char kBrandForTesting[];

  TestMetricsServiceClient();
  ~TestMetricsServiceClient() override;

  // MetricsServiceClient:
  void SetMetricsClientId(const std::string& client_id) override;
  void OnRecordingDisabled() override;
  bool IsOffTheRecordSessionActive() override;
  int32_t GetProduct() override;
  std::string GetApplicationLocale() override;
  bool GetBrand(std::string* brand_code) override;
  SystemProfileProto::Channel GetChannel() override;
  std::string GetVersionString() override;
  void OnLogUploadComplete() override;
  void StartGatheringMetrics(const base::Closure& done_callback) override;
  void CollectFinalMetrics(const base::Closure& done_callback) override;
  scoped_ptr<MetricsLogUploader> CreateUploader(
      const base::Callback<void(int)>& on_upload_complete) override;
  base::TimeDelta GetStandardUploadInterval() override;

  const std::string& get_client_id() const { return client_id_; }
  void set_version_string(const std::string& str) { version_string_ = str; }
  void set_product(int32_t product) { product_ = product; }

 private:
  std::string client_id_;
  std::string version_string_;
  int32_t product_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsServiceClient);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_TEST_METRICS_SERVICE_CLIENT_H_
