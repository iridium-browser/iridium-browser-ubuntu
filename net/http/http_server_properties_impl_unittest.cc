// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_impl.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class ListValue;
}

namespace net {

const int kMaxSupportsSpdyServerHosts = 500;

class HttpServerPropertiesImplPeer {
 public:
  static void AddBrokenAlternativeServiceWithExpirationTime(
      HttpServerPropertiesImpl& impl,
      AlternativeService alternative_service,
      base::TimeTicks when) {
    impl.broken_alternative_services_.insert(
        std::make_pair(alternative_service, when));
    ++impl.recently_broken_alternative_services_[alternative_service];
  }

  static void ExpireBrokenAlternateProtocolMappings(
      HttpServerPropertiesImpl& impl) {
    impl.ExpireBrokenAlternateProtocolMappings();
  }
};

namespace {

class HttpServerPropertiesImplTest : public testing::Test {
 protected:
  bool HasAlternativeService(const HostPortPair& origin) {
    const AlternativeService alternative_service =
        impl_.GetAlternativeService(origin);
    return alternative_service.protocol != UNINITIALIZED_ALTERNATE_PROTOCOL;
  }

  HttpServerPropertiesImpl impl_;
};

typedef HttpServerPropertiesImplTest SpdyServerPropertiesTest;

TEST_F(SpdyServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();

  HostPortPair spdy_server_docs("docs.google.com", 443);
  std::string spdy_server_d = spdy_server_docs.ToString();

  // Check by initializing NULL spdy servers.
  impl_.InitializeSpdyServers(NULL, true);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing empty spdy servers.
  std::vector<std::string> spdy_servers;
  impl_.InitializeSpdyServers(&spdy_servers, true);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing with www.google.com:443 spdy server.
  std::vector<std::string> spdy_servers1;
  spdy_servers1.push_back(spdy_server_g);
  impl_.InitializeSpdyServers(&spdy_servers1, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));

  // Check by initializing with www.google.com:443 and docs.google.com:443 spdy
  // servers.
  std::vector<std::string> spdy_servers2;
  spdy_servers2.push_back(spdy_server_g);
  spdy_servers2.push_back(spdy_server_d);
  impl_.InitializeSpdyServers(&spdy_servers2, true);

  // Verify spdy_server_g and spdy_server_d are in the list in the same order.
  base::ListValue spdy_server_list;
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(2U, spdy_server_list.GetSize());
  std::string string_value_g;
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  std::string string_value_d;
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_d));
  ASSERT_EQ(spdy_server_d, string_value_d);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
}

TEST_F(SpdyServerPropertiesTest, SupportsRequestPriorityTest) {
  HostPortPair spdy_server_empty(std::string(), 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_empty));

  // Add www.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));

  // Add mail.google.com:443 as not supporting SPDY.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));

  // Add docs.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_docs("docs.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_docs, true);
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));

  // Add www.youtube.com:443 as supporting QUIC.
  HostPortPair quic_server_youtube("www.youtube.com", 443);
  const AlternativeService alternative_service(QUIC, "www.youtube.com", 443);
  impl_.SetAlternativeService(quic_server_youtube, alternative_service, 1.0);
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_youtube));

  // Verify all the entries are the same after additions.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_docs));
  EXPECT_TRUE(impl_.SupportsRequestPriority(quic_server_youtube));
}

TEST_F(SpdyServerPropertiesTest, Clear) {
  // Add www.google.com:443 and mail.google.com:443 as supporting SPDY.
  HostPortPair spdy_server_google("www.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_google, true);
  HostPortPair spdy_server_mail("mail.google.com", 443);
  impl_.SetSupportsSpdy(spdy_server_mail, true);

  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_mail));

  impl_.Clear();
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_google));
  EXPECT_FALSE(impl_.SupportsRequestPriority(spdy_server_mail));
}

TEST_F(SpdyServerPropertiesTest, GetSpdyServerList) {
  base::ListValue spdy_server_list;

  // Check there are no spdy_servers.
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Check empty server is not added.
  HostPortPair spdy_server_empty(std::string(), 443);
  impl_.SetSupportsSpdy(spdy_server_empty, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.ToString();

  // Add www.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  EXPECT_EQ(0U, spdy_server_list.GetSize());

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as not supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, false);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());

  // Verify www.google.com:443 and mail.google.com:443 are in the list.
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Request for only one server and verify that we get only one server.
  impl_.GetSpdyServerList(&spdy_server_list, 1);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

TEST_F(SpdyServerPropertiesTest, MRUOfGetSpdyServerList) {
  base::ListValue spdy_server_list;

  std::string string_value_g;
  std::string string_value_m;
  HostPortPair spdy_server_google("www.google.com", 443);
  std::string spdy_server_g = spdy_server_google.ToString();
  HostPortPair spdy_server_mail("mail.google.com", 443);
  std::string spdy_server_m = spdy_server_mail.ToString();

  // Add www.google.com:443 as supporting SPDY.
  impl_.SetSupportsSpdy(spdy_server_google, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(1U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Add mail.google.com:443 as supporting SPDY. Verify mail.google.com:443 and
  // www.google.com:443 are in the list.
  impl_.SetSupportsSpdy(spdy_server_mail, true);
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);

  // Get www.google.com:443 should reorder SpdyServerHostPortMap. Verify that it
  // is www.google.com:443 is the MRU server.
  EXPECT_TRUE(impl_.SupportsRequestPriority(spdy_server_google));
  impl_.GetSpdyServerList(&spdy_server_list, kMaxSupportsSpdyServerHosts);
  ASSERT_EQ(2U, spdy_server_list.GetSize());
  ASSERT_TRUE(spdy_server_list.GetString(0, &string_value_g));
  ASSERT_EQ(spdy_server_g, string_value_g);
  ASSERT_TRUE(spdy_server_list.GetString(1, &string_value_m));
  ASSERT_EQ(spdy_server_m, string_value_m);
}

typedef HttpServerPropertiesImplTest AlternateProtocolServerPropertiesTest;

TEST_F(AlternateProtocolServerPropertiesTest, Basic) {
  HostPortPair test_host_port_pair("foo", 80);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));

  AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service, 1.0);
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  alternative_service = impl_.GetAlternativeService(test_host_port_pair);
  EXPECT_EQ(443, alternative_service.port);
  EXPECT_EQ(NPN_SPDY_4, alternative_service.protocol);

  impl_.Clear();
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, DefaultProbabilityExcluded) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service, 0.99);

  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Probability) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.25);

  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service, 0.5);
  EXPECT_TRUE(HasAlternativeService(test_host_port_pair));

  AlternativeServiceMap::const_iterator it =
      impl_.alternative_service_map().Peek(test_host_port_pair);
  ASSERT_TRUE(it != impl_.alternative_service_map().end());
  EXPECT_EQ(443, it->second.alternative_service.port);
  EXPECT_EQ(NPN_SPDY_4, it->second.alternative_service.protocol);
  EXPECT_EQ(0.5, it->second.probability);
}

TEST_F(AlternateProtocolServerPropertiesTest, ProbabilityExcluded) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.75);

  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service, 0.5);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, Initialize) {
  HostPortPair test_host_port_pair1("foo1", 80);
  const AlternativeService alternative_service1(NPN_SPDY_4, "foo1", 443);
  impl_.SetAlternativeService(test_host_port_pair1, alternative_service1, 1.0);
  impl_.MarkAlternativeServiceBroken(alternative_service1);

  HostPortPair test_host_port_pair2("foo2", 80);
  const AlternativeService alternative_service2(NPN_SPDY_4, "foo2", 443);
  impl_.SetAlternativeService(test_host_port_pair2, alternative_service2, 1.0);

  AlternativeServiceMap alternative_service_map(
      AlternativeServiceMap::NO_AUTO_EVICT);
  AlternativeServiceInfo alternative_service_info(NPN_SPDY_4, "bar", 123, 1.0);
  alternative_service_map.Put(test_host_port_pair2, alternative_service_info);
  HostPortPair test_host_port_pair3("foo3", 80);
  alternative_service_info.alternative_service.port = 1234;
  alternative_service_map.Put(test_host_port_pair3, alternative_service_info);
  impl_.InitializeAlternativeServiceServers(&alternative_service_map);

  // Verify test_host_port_pair3 is the MRU server.
  const AlternativeServiceMap& map = impl_.alternative_service_map();
  AlternativeServiceMap::const_iterator it = map.begin();
  ASSERT_TRUE(it != map.end());
  EXPECT_TRUE(it->first.Equals(test_host_port_pair3));
  EXPECT_EQ(NPN_SPDY_4, it->second.alternative_service.protocol);
  EXPECT_EQ(1234, it->second.alternative_service.port);

  ASSERT_TRUE(HasAlternativeService(test_host_port_pair1));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
  const AlternativeService alternative_service =
      impl_.GetAlternativeService(test_host_port_pair2);
  EXPECT_EQ(NPN_SPDY_4, alternative_service.protocol);
  EXPECT_EQ(123, alternative_service.port);
}

TEST_F(AlternateProtocolServerPropertiesTest, MRUOfGetAlternateProtocol) {
  HostPortPair test_host_port_pair1("foo1", 80);
  const AlternativeService alternative_service1(NPN_SPDY_4, "foo1", 443);
  impl_.SetAlternativeService(test_host_port_pair1, alternative_service1, 1.0);
  HostPortPair test_host_port_pair2("foo2", 80);
  const AlternativeService alternative_service2(NPN_SPDY_4, "foo2", 1234);
  impl_.SetAlternativeService(test_host_port_pair2, alternative_service2, 1.0);

  const AlternativeServiceMap& map = impl_.alternative_service_map();
  AlternativeServiceMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair2));
  EXPECT_EQ(NPN_SPDY_4, it->second.alternative_service.protocol);
  EXPECT_EQ(1234, it->second.alternative_service.port);

  // GetAlternativeService should reorder the AlternateProtocol map.
  const AlternativeService alternative_service =
      impl_.GetAlternativeService(test_host_port_pair1);
  EXPECT_EQ(443, alternative_service.port);
  EXPECT_EQ(NPN_SPDY_4, alternative_service.protocol);
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(test_host_port_pair1));
  EXPECT_EQ(NPN_SPDY_4, it->second.alternative_service.protocol);
  EXPECT_EQ(443, it->second.alternative_service.port);
}

TEST_F(AlternateProtocolServerPropertiesTest, SetBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service1(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service1, 1.0);
  impl_.MarkAlternativeServiceBroken(alternative_service1);
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));

  const AlternativeService alternative_service2(NPN_SPDY_4, "foo", 1234);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service2, 1.0);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service1));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service2));
  EXPECT_EQ(1234, impl_.GetAlternativeService(test_host_port_pair).port);
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearBroken) {
  HostPortPair test_host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(test_host_port_pair, alternative_service, 1.0);
  impl_.MarkAlternativeServiceBroken(alternative_service);
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  impl_.ClearAlternativeService(test_host_port_pair);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, MarkRecentlyBroken) {
  HostPortPair host_port_pair("foo", 80);
  const AlternativeService alternative_service(NPN_SPDY_4, "foo", 443);
  impl_.SetAlternativeService(host_port_pair, alternative_service, 1.0);

  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.MarkAlternativeServiceRecentlyBroken(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  impl_.ConfirmAlternativeService(alternative_service);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));
}

TEST_F(AlternateProtocolServerPropertiesTest, Canonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));

  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(canonical_port_pair));

  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);
  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 1.0);
  // Verify the forced protocol.
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  const AlternativeService alternative_service =
      impl_.GetAlternativeService(test_host_port_pair);
  EXPECT_EQ(canonical_altsvc.port, alternative_service.port);
  EXPECT_EQ(canonical_altsvc.protocol, alternative_service.protocol);

  // Verify the canonical suffix.
  EXPECT_EQ(".c.youtube.com",
            impl_.GetCanonicalSuffix(test_host_port_pair.host()));
  EXPECT_EQ(".c.youtube.com",
            impl_.GetCanonicalSuffix(canonical_port_pair.host()));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalDefaultHost) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));

  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  EXPECT_FALSE(HasAlternativeService(canonical_port_pair));

  AlternativeService canonical_altsvc(QUIC, "", 1234);
  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 1.0);
  ASSERT_TRUE(HasAlternativeService(test_host_port_pair));
  const AlternativeService alternative_service =
      impl_.GetAlternativeService(test_host_port_pair);
  EXPECT_EQ(canonical_altsvc.protocol, alternative_service.protocol);
  EXPECT_EQ(test_host_port_pair.host(), alternative_service.host);
  EXPECT_EQ(canonical_altsvc.port, alternative_service.port);
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBelowThreshold) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.02);

  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);

  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 0.01);
  EXPECT_FALSE(HasAlternativeService(canonical_port_pair));
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalAboveThreshold) {
  impl_.SetAlternativeServiceProbabilityThreshold(0.02);

  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);

  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 0.03);
  EXPECT_TRUE(HasAlternativeService(canonical_port_pair));
  EXPECT_TRUE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearCanonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);

  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 1.0);
  impl_.ClearAlternativeService(canonical_port_pair);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest, CanonicalBroken) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);

  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 1.0);
  impl_.MarkAlternativeServiceBroken(canonical_altsvc);
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

// Adding an alternative service for a new host overrides canonical host.
TEST_F(AlternateProtocolServerPropertiesTest, CanonicalOverride) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair bar_host_port_pair("bar.c.youtube.com", 80);
  AlternativeService bar_alternative_service(QUIC, "bar.c.youtube.com", 1234);
  impl_.SetAlternativeService(bar_host_port_pair, bar_alternative_service, 1.0);
  AlternativeService altsvc = impl_.GetAlternativeService(test_host_port_pair);
  EXPECT_EQ(1234, altsvc.port);

  HostPortPair qux_host_port_pair("qux.c.youtube.com", 80);
  AlternativeService qux_alternative_service(QUIC, "qux.c.youtube.com", 443);
  impl_.SetAlternativeService(qux_host_port_pair, qux_alternative_service, 1.0);
  altsvc = impl_.GetAlternativeService(test_host_port_pair);
  EXPECT_EQ(443, altsvc.port);
}

TEST_F(AlternateProtocolServerPropertiesTest, ClearWithCanonical) {
  HostPortPair test_host_port_pair("foo.c.youtube.com", 80);
  HostPortPair canonical_port_pair("bar.c.youtube.com", 80);
  AlternativeService canonical_altsvc(QUIC, "bar.c.youtube.com", 1234);

  impl_.SetAlternativeService(canonical_port_pair, canonical_altsvc, 1.0);
  impl_.Clear();
  EXPECT_FALSE(HasAlternativeService(test_host_port_pair));
}

TEST_F(AlternateProtocolServerPropertiesTest,
       ExpireBrokenAlternateProtocolMappings) {
  HostPortPair host_port_pair("foo", 443);
  AlternativeService alternative_service(QUIC, "foo", 443);
  impl_.SetAlternativeService(host_port_pair, alternative_service, 1.0);
  EXPECT_TRUE(HasAlternativeService(host_port_pair));
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_FALSE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  base::TimeTicks past =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(42);
  HttpServerPropertiesImplPeer::AddBrokenAlternativeServiceWithExpirationTime(
      impl_, alternative_service, past);
  EXPECT_TRUE(impl_.IsAlternativeServiceBroken(alternative_service));
  EXPECT_TRUE(impl_.WasAlternativeServiceRecentlyBroken(alternative_service));

  HttpServerPropertiesImplPeer::ExpireBrokenAlternateProtocolMappings(impl_);
  EXPECT_FALSE(impl_.IsAlternativeServiceBroken(alternative_service));
  // TODO(bnc): Test WasAlternativeServiceRecentlyBroken once it's changed to
  // take AlternativeService as argument.
}

typedef HttpServerPropertiesImplTest SpdySettingsServerPropertiesTest;

TEST_F(SpdySettingsServerPropertiesTest, Initialize) {
  HostPortPair spdy_server_google("www.google.com", 443);

  // Check by initializing empty spdy settings.
  SpdySettingsMap spdy_settings_map(SpdySettingsMap::NO_AUTO_EVICT);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);
  EXPECT_TRUE(impl_.GetSpdySettings(spdy_server_google).empty());

  // Check by initializing with www.google.com:443 spdy server settings.
  SettingsMap settings_map;
  const SpdySettingsIds id = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags = SETTINGS_FLAG_PERSISTED;
  const uint32 value = 31337;
  SettingsFlagsAndValue flags_and_value(flags, value);
  settings_map[id] = flags_and_value;
  spdy_settings_map.Put(spdy_server_google, settings_map);
  impl_.InitializeSpdySettingsServers(&spdy_settings_map);

  const SettingsMap& settings_map2 = impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map2.size());
  SettingsMap::const_iterator it = settings_map2.find(id);
  EXPECT_TRUE(it != settings_map2.end());
  SettingsFlagsAndValue flags_and_value2 = it->second;
  EXPECT_EQ(flags, flags_and_value2.first);
  EXPECT_EQ(value, flags_and_value2.second);
}

TEST_F(SpdySettingsServerPropertiesTest, SetSpdySetting) {
  HostPortPair spdy_server_empty(std::string(), 443);
  const SettingsMap& settings_map0 = impl_.GetSpdySettings(spdy_server_empty);
  EXPECT_EQ(0U, settings_map0.size());  // Returns kEmptySettingsMap.

  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add mail.google.com:443 as not persisting.
  HostPortPair spdy_server_mail("mail.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_DOWNLOAD_BANDWIDTH;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_NONE;
  const uint32 value2 = 62667;
  EXPECT_FALSE(impl_.SetSpdySetting(spdy_server_mail, id2, flags2, value2));
  const SettingsMap& settings_map2_ret =
      impl_.GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, settings_map2_ret.size());  // Returns kEmptySettingsMap.

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  SettingsFlagsAndValue flags_and_value3(flags3, value3);
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  // Check data for www.google.com:443 (id1).
  const SettingsMap& settings_map4_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map4_ret.size());
  SettingsMap::const_iterator it4_ret = settings_map4_ret.find(id1);
  EXPECT_TRUE(it4_ret != settings_map4_ret.end());
  SettingsFlagsAndValue flags_and_value4_ret = it4_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value4_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear www.google.com:443 as persisting.
  impl_.ClearSpdySettings(spdy_server_google);
  // Check the values.
  const SettingsMap& settings_map5_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(0U, settings_map5_ret.size());

  // Clear all settings.
  ASSERT_GT(impl_.spdy_settings_map().size(), 0U);
  impl_.ClearAllSpdySettings();
  ASSERT_EQ(0U, impl_.spdy_settings_map().size());
}

TEST_F(SpdySettingsServerPropertiesTest, Clear) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));
  // Check the values.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id3 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags3 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value3 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id3, flags3, value3));
  // Check the values.
  const SettingsMap& settings_map3_ret =
      impl_.GetSpdySettings(spdy_server_docs);
  ASSERT_EQ(1U, settings_map3_ret.size());
  SettingsMap::const_iterator it3_ret = settings_map3_ret.find(id3);
  EXPECT_TRUE(it3_ret != settings_map3_ret.end());
  SettingsFlagsAndValue flags_and_value3_ret = it3_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value3_ret.first);
  EXPECT_EQ(value3, flags_and_value3_ret.second);

  impl_.Clear();
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_google).size());
  EXPECT_EQ(0U, impl_.GetSpdySettings(spdy_server_docs).size());
}

TEST_F(SpdySettingsServerPropertiesTest, MRUOfGetSpdySettings) {
  // Add www.google.com:443 as persisting.
  HostPortPair spdy_server_google("www.google.com", 443);
  const SpdySettingsIds id1 = SETTINGS_UPLOAD_BANDWIDTH;
  const SpdySettingsFlags flags1 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_google, id1, flags1, value1));

  // Add docs.google.com:443 as persisting
  HostPortPair spdy_server_docs("docs.google.com", 443);
  const SpdySettingsIds id2 = SETTINGS_ROUND_TRIP_TIME;
  const SpdySettingsFlags flags2 = SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value2 = 93997;
  EXPECT_TRUE(impl_.SetSpdySetting(spdy_server_docs, id2, flags2, value2));

  // Verify the first element is docs.google.com:443.
  const SpdySettingsMap& map = impl_.spdy_settings_map();
  SpdySettingsMap::const_iterator it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_docs));
  const SettingsMap& settings_map2_ret = it->second;
  ASSERT_EQ(1U, settings_map2_ret.size());
  SettingsMap::const_iterator it2_ret = settings_map2_ret.find(id2);
  EXPECT_TRUE(it2_ret != settings_map2_ret.end());
  SettingsFlagsAndValue flags_and_value2_ret = it2_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value2_ret.first);
  EXPECT_EQ(value2, flags_and_value2_ret.second);

  // GetSpdySettings should reorder the SpdySettingsMap.
  const SettingsMap& settings_map1_ret =
      impl_.GetSpdySettings(spdy_server_google);
  ASSERT_EQ(1U, settings_map1_ret.size());
  SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Check the first entry is spdy_server_google by accessing it via iterator.
  it = map.begin();
  EXPECT_TRUE(it->first.Equals(spdy_server_google));
  const SettingsMap& settings_map1_it_ret = it->second;
  ASSERT_EQ(1U, settings_map1_it_ret.size());
  it1_ret = settings_map1_it_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_it_ret.end());
  flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);
}

typedef HttpServerPropertiesImplTest SupportsQuicServerPropertiesTest;

TEST_F(SupportsQuicServerPropertiesTest, Initialize) {
  HostPortPair quic_server_google("www.google.com", 443);

  // Check by initializing empty address.
  IPAddressNumber initial_address;
  impl_.InitializeSupportsQuic(&initial_address);

  IPAddressNumber address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  // Check by initializing with a valid address.
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &initial_address));
  impl_.InitializeSupportsQuic(&initial_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(initial_address, address);
}

TEST_F(SupportsQuicServerPropertiesTest, SetSupportsQuic) {
  IPAddressNumber address;
  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
  EXPECT_TRUE(address.empty());

  IPAddressNumber actual_address;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &actual_address));
  impl_.SetSupportsQuic(true, actual_address);

  EXPECT_TRUE(impl_.GetSupportsQuic(&address));
  EXPECT_EQ(actual_address, address);

  impl_.Clear();

  EXPECT_FALSE(impl_.GetSupportsQuic(&address));
}

typedef HttpServerPropertiesImplTest ServerNetworkStatsServerPropertiesTest;

TEST_F(ServerNetworkStatsServerPropertiesTest, Initialize) {
  HostPortPair google_server("www.google.com", 443);

  // Check by initializing empty ServerNetworkStats.
  ServerNetworkStatsMap server_network_stats_map(
      ServerNetworkStatsMap::NO_AUTO_EVICT);
  impl_.InitializeServerNetworkStats(&server_network_stats_map);
  const ServerNetworkStats* stats = impl_.GetServerNetworkStats(google_server);
  EXPECT_EQ(NULL, stats);

  // Check by initializing with www.google.com:443.
  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  stats1.bandwidth_estimate = QuicBandwidth::FromBitsPerSecond(100);
  server_network_stats_map.Put(google_server, stats1);
  impl_.InitializeServerNetworkStats(&server_network_stats_map);

  const ServerNetworkStats* stats2 = impl_.GetServerNetworkStats(google_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());
}

TEST_F(ServerNetworkStatsServerPropertiesTest, SetServerNetworkStats) {
  HostPortPair foo_server("foo", 80);
  const ServerNetworkStats* stats = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(NULL, stats);

  ServerNetworkStats stats1;
  stats1.srtt = base::TimeDelta::FromMicroseconds(10);
  stats1.bandwidth_estimate = QuicBandwidth::FromBitsPerSecond(100);
  impl_.SetServerNetworkStats(foo_server, stats1);

  const ServerNetworkStats* stats2 = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(10, stats2->srtt.ToInternalValue());
  EXPECT_EQ(100, stats2->bandwidth_estimate.ToBitsPerSecond());

  impl_.Clear();
  const ServerNetworkStats* stats3 = impl_.GetServerNetworkStats(foo_server);
  EXPECT_EQ(NULL, stats3);
}

}  // namespace

}  // namespace net
