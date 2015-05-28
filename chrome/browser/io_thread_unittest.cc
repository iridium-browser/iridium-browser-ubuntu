// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/quic/quic_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

using ::testing::ElementsAre;

class BadEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  ~BadEntropyProvider() override {}

  double GetEntropyForTrial(const std::string& trial_name,
                            uint32 randomization_seed) const override {
    return 0.5;
  }
};

class IOThreadPeer {
 public:
  static void ConfigureQuicGlobals(
      const base::CommandLine& command_line,
      base::StringPiece quic_trial_group,
      const std::map<std::string, std::string>& quic_trial_params,
      bool is_quic_allowed_by_policy,
      IOThread::Globals* globals) {
    IOThread::ConfigureQuicGlobals(command_line, quic_trial_group,
                                   quic_trial_params, is_quic_allowed_by_policy,
                                   globals);
  }

  static void ConfigureSpdyGlobals(
      const base::CommandLine& command_line,
      base::StringPiece spdy_trial_group,
      const std::map<std::string, std::string>& spdy_trial_params,
      IOThread::Globals* globals) {
    IOThread::ConfigureSpdyGlobals(command_line, spdy_trial_group,
                                   spdy_trial_params, globals);
  }

  static void InitializeNetworkSessionParamsFromGlobals(
      const IOThread::Globals& globals,
      net::HttpNetworkSession::Params* params) {
    IOThread::InitializeNetworkSessionParamsFromGlobals(globals, params);
  }
};

class IOThreadTest : public testing::Test {
 public:
  IOThreadTest()
      : command_line_(base::CommandLine::NO_PROGRAM),
        is_quic_allowed_by_policy_(true) {
    globals_.http_server_properties.reset(new net::HttpServerPropertiesImpl());
  }

  void ConfigureQuicGlobals() {
    IOThreadPeer::ConfigureQuicGlobals(command_line_,
                                       field_trial_group_,
                                       field_trial_params_,
                                       is_quic_allowed_by_policy_,
                                       &globals_);
  }

  void ConfigureSpdyGlobals() {
    IOThreadPeer::ConfigureSpdyGlobals(command_line_, field_trial_group_,
                                       field_trial_params_, &globals_);
  }

  void InitializeNetworkSessionParams(net::HttpNetworkSession::Params* params) {
    IOThreadPeer::InitializeNetworkSessionParamsFromGlobals(globals_, params);
  }

  base::CommandLine command_line_;
  IOThread::Globals globals_;
  std::string field_trial_group_;
  bool is_quic_allowed_by_policy_;
  std::map<std::string, std::string> field_trial_params_;
};

TEST_F(IOThreadTest, InitializeNetworkSessionParamsFromGlobals) {
  globals_.quic_connection_options.push_back(net::kPACE);
  globals_.quic_connection_options.push_back(net::kTBBR);
  globals_.quic_connection_options.push_back(net::kTIME);

  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(globals_.quic_connection_options,
            params.quic_connection_options);
}

TEST_F(IOThreadTest, SpdyFieldTrialHoldbackEnabled) {
  net::HttpStreamFactory::set_spdy_enabled(true);
  field_trial_group_ = "SpdyDisabled";
  ConfigureSpdyGlobals();
  EXPECT_FALSE(net::HttpStreamFactory::spdy_enabled());
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy31Enabled) {
  bool use_alternate_protocols = false;
  field_trial_group_ = "Spdy31Enabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11,
                          net::kProtoSPDY31));
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyFieldTrialSpdy4Enabled) {
  bool use_alternate_protocols = false;
  field_trial_group_ = "Spdy4Enabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11, net::kProtoSPDY31,
                          net::kProtoSPDY4_14, net::kProtoSPDY4));
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyFieldTrialDefault) {
  field_trial_group_ = "";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11, net::kProtoSPDY31,
                          net::kProtoSPDY4_14, net::kProtoSPDY4));
  bool use_alternate_protocols = false;
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyFieldTrialParametrized) {
  field_trial_params_["enable_spdy31"] = "false";
  // Undefined parameter "enable_http2_14" should default to false.
  field_trial_params_["enable_http2"] = "true";
  field_trial_group_ = "ParametrizedHTTP2Only";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11, net::kProtoSPDY4));
  bool use_alternate_protocols = false;
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyCommandLineEnable) {
  command_line_.AppendSwitch("enable-spdy4");
  // Command line should overwrite field trial group.
  field_trial_group_ = "SpdyDisabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos,
              ElementsAre(net::kProtoHTTP11, net::kProtoSPDY31,
                          net::kProtoSPDY4_14, net::kProtoSPDY4));
  bool use_alternate_protocols = false;
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_TRUE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyCommandLineDisable) {
  command_line_.AppendSwitch("enable-npn-http");
  // Command line should overwrite field trial group.
  field_trial_group_ = "Spdy4Enabled";
  ConfigureSpdyGlobals();
  EXPECT_THAT(globals_.next_protos, ElementsAre(net::kProtoHTTP11));
  bool use_alternate_protocols = true;
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_FALSE(use_alternate_protocols);
}

TEST_F(IOThreadTest, SpdyCommandLineUseSpdyOff) {
  command_line_.AppendSwitchASCII("use-spdy", "off");
  // Command line should overwrite field trial group.
  field_trial_group_ = "Spdy4Enabled";
  ConfigureSpdyGlobals();
  EXPECT_EQ(0u, globals_.next_protos.size());
}

TEST_F(IOThreadTest, SpdyCommandLineUseSpdySSL) {
  command_line_.AppendSwitchASCII("use-spdy", "ssl");
  // Command line should overwrite field trial group.
  field_trial_group_ = "SpdyDisabled";
  ConfigureSpdyGlobals();
  bool force_spdy_over_ssl = false;
  globals_.force_spdy_over_ssl.CopyToIfSet(&force_spdy_over_ssl);
  EXPECT_TRUE(force_spdy_over_ssl);
  bool force_spdy_always = false;
  globals_.force_spdy_always.CopyToIfSet(&force_spdy_always);
  EXPECT_TRUE(force_spdy_always);
}

TEST_F(IOThreadTest, SpdyCommandLineUseSpdyDisableAltProtocols) {
  command_line_.AppendSwitchASCII("use-spdy", "no-alt-protocols");
  ConfigureSpdyGlobals();
  bool use_alternate_protocols = true;
  globals_.use_alternate_protocols.CopyToIfSet(&use_alternate_protocols);
  EXPECT_FALSE(use_alternate_protocols);
}

TEST_F(IOThreadTest, DisableQuicByDefault) {
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_FALSE(params.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(IOThreadTest, EnableQuicFromFieldTrialGroup) {
  field_trial_group_ = "Enabled";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params default_params;
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_TRUE(params.enable_quic_for_proxies);
  EXPECT_EQ(1350u, params.quic_max_packet_length);
  EXPECT_EQ(1.0, params.alternate_protocol_probability_threshold);
  EXPECT_EQ(default_params.quic_supported_versions,
            params.quic_supported_versions);
  EXPECT_EQ(net::QuicTagVector(), params.quic_connection_options);
  EXPECT_FALSE(params.quic_always_require_handshake_confirmation);
  EXPECT_FALSE(params.quic_disable_connection_pooling);
  EXPECT_EQ(0.25f, params.quic_load_server_info_timeout_srtt_multiplier);
  EXPECT_FALSE(params.quic_enable_connection_racing);
  EXPECT_FALSE(params.quic_enable_non_blocking_io);
  EXPECT_FALSE(params.quic_disable_disk_cache);
  EXPECT_EQ(0, params.quic_max_number_of_lossy_connections);
  EXPECT_EQ(1.0f, params.quic_packet_loss_threshold);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(IOThreadTest, EnableQuicFromQuicProxyFieldTrialGroup) {
  base::FieldTrialList field_trial_list(new BadEntropyProvider());
  base::FieldTrialList::CreateFieldTrial(
      data_reduction_proxy::DataReductionProxyParams::GetQuicFieldTrialName(),
      "Enabled");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
  EXPECT_TRUE(params.enable_quic_for_proxies);
  EXPECT_TRUE(IOThread::ShouldEnableQuicForDataReductionProxy());
  EXPECT_EQ(256 * 1024, params.quic_socket_receive_buffer_size);
}

TEST_F(IOThreadTest, EnableQuicFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.enable_quic);
  EXPECT_TRUE(params.enable_quic_for_proxies);
  EXPECT_FALSE(IOThread::ShouldEnableQuicForDataReductionProxy());
}

TEST_F(IOThreadTest, EnablePacingFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitch("enable-quic-pacing");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicTagVector options;
  options.push_back(net::kPACE);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, EnablePacingFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_pacing"] = "true";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicTagVector options;
  options.push_back(net::kPACE);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, PacketLengthFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-max-packet-length", "1450");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1450u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, PacketLengthFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["max_packet_length"] = "1450";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1450u, params.quic_max_packet_length);
}

TEST_F(IOThreadTest, QuicVersionFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  std::string version =
      net::QuicVersionToString(net::QuicSupportedVersions().back());
  command_line_.AppendSwitchASCII("quic-version", version);

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params.quic_supported_versions);
}

TEST_F(IOThreadTest, QuicVersionFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["quic_version"] =
      net::QuicVersionToString(net::QuicSupportedVersions().back());

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  net::QuicVersionVector supported_versions;
  supported_versions.push_back(net::QuicSupportedVersions().back());
  EXPECT_EQ(supported_versions, params.quic_supported_versions);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromCommandLine) {
  command_line_.AppendSwitch("enable-quic");
  command_line_.AppendSwitchASCII("quic-connection-options",
                                  "PACE,TIME,TBBR,REJ");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kPACE);
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest, QuicConnectionOptionsFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["connection_options"] = "PACE,TIME,TBBR,REJ";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);

  net::QuicTagVector options;
  options.push_back(net::kPACE);
  options.push_back(net::kTIME);
  options.push_back(net::kTBBR);
  options.push_back(net::kREJ);
  EXPECT_EQ(options, params.quic_connection_options);
}

TEST_F(IOThreadTest,
       QuicAlwaysRequireHandshakeConfirmationFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["always_require_handshake_confirmation"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_always_require_handshake_confirmation);
}

TEST_F(IOThreadTest,
       QuicDisableConnectionPoolingFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_connection_pooling"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_disable_connection_pooling);
}

TEST_F(IOThreadTest, QuicLoadServerInfoTimeToSmoothedRttFromFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["load_server_info_time_to_srtt"] = "0.5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0.5f, params.quic_load_server_info_timeout_srtt_multiplier);
}

TEST_F(IOThreadTest, QuicEnableConnectionRacing) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_connection_racing"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_enable_connection_racing);
}

TEST_F(IOThreadTest, QuicEnableNonBlockingIO) {
  field_trial_group_ = "Enabled";
  field_trial_params_["enable_non_blocking_io"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_enable_non_blocking_io);
}

TEST_F(IOThreadTest, QuicDisableDiskCache) {
  field_trial_group_ = "Enabled";
  field_trial_params_["disable_disk_cache"] = "true";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_TRUE(params.quic_disable_disk_cache);
}

TEST_F(IOThreadTest, QuicMaxNumberOfLossyConnectionsFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["max_number_of_lossy_connections"] = "5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(5, params.quic_max_number_of_lossy_connections);
}

TEST_F(IOThreadTest, QuicPacketLossThresholdFieldTrialParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["packet_loss_threshold"] = "0.5";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0.5f, params.quic_packet_loss_threshold);
}

TEST_F(IOThreadTest, QuicReceiveBufferSize) {
  field_trial_group_ = "Enabled";
  field_trial_params_["receive_buffer_size"] = "1048576";
  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(1048576, params.quic_socket_receive_buffer_size);
}

TEST_F(IOThreadTest,
       AlternateProtocolProbabilityThresholdFromFlag) {
  command_line_.AppendSwitchASCII("alternate-protocol-probability-threshold",
                                  ".5");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(.5, params.alternate_protocol_probability_threshold);
}

TEST_F(IOThreadTest,
       AlternateProtocolProbabilityThresholdFromEnableQuicFlag) {
  command_line_.AppendSwitch("enable-quic");

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(0, params.alternate_protocol_probability_threshold);
}

TEST_F(IOThreadTest,
       AlternateProtocolProbabilityThresholdFromParams) {
  field_trial_group_ = "Enabled";
  field_trial_params_["alternate_protocol_probability_threshold"] = ".5";

  ConfigureQuicGlobals();
  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_EQ(.5, params.alternate_protocol_probability_threshold);
}

TEST_F(IOThreadTest, QuicDisallowedByPolicy) {
  command_line_.AppendSwitch(switches::kEnableQuic);
  is_quic_allowed_by_policy_ = false;
  ConfigureQuicGlobals();

  net::HttpNetworkSession::Params params;
  InitializeNetworkSessionParams(&params);
  EXPECT_FALSE(params.enable_quic);
}

}  // namespace test
