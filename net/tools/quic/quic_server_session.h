// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server specific QuicSession subclass.

#ifndef NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_
#define NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_session.h"

namespace net {

class QuicBlockedWriterInterface;
class QuicConfig;
class QuicConnection;
class QuicCryptoServerConfig;
class ReliableQuicStream;

namespace tools {

namespace test {
class QuicServerSessionPeer;
}  // namespace test

// An interface from the session to the entity owning the session.
// This lets the session notify its owner (the Dispatcher) when the connection
// is closed, blocked, or added/removed from the time-wait list.
class QuicServerSessionVisitor {
 public:
  virtual ~QuicServerSessionVisitor() {}

  virtual void OnConnectionClosed(QuicConnectionId connection_id,
                                  QuicErrorCode error) = 0;
  virtual void OnWriteBlocked(QuicBlockedWriterInterface* blocked_writer) = 0;
  // Called after the given connection is added to the time-wait list.
  virtual void OnConnectionAddedToTimeWaitList(QuicConnectionId connection_id) {
  }
  // Called after the given connection is removed from the time-wait list.
  virtual void OnConnectionRemovedFromTimeWaitList(
      QuicConnectionId connection_id) {}
};

class QuicServerSession : public QuicSession {
 public:
  QuicServerSession(const QuicConfig& config,
                    QuicConnection* connection,
                    QuicServerSessionVisitor* visitor);

  // Override the base class to notify the owner of the connection close.
  void OnConnectionClosed(QuicErrorCode error, bool from_peer) override;
  void OnWriteBlocked() override;

  // Sends a server config update to the client, containing new bandwidth
  // estimate.
  void OnCongestionWindowChange(QuicTime now) override;

  ~QuicServerSession() override;

  // |crypto_config| must outlive the session.
  virtual void InitializeSession(const QuicCryptoServerConfig* crypto_config);

  const QuicCryptoServerStream* crypto_stream() const {
    return crypto_stream_.get();
  }

  // Override base class to process FEC config received from client.
  void OnConfigNegotiated() override;

  void set_serving_region(std::string serving_region) {
    serving_region_ = serving_region;
  }

 protected:
  // QuicSession methods:
  QuicDataStream* CreateIncomingDataStream(QuicStreamId id) override;
  QuicDataStream* CreateOutgoingDataStream() override;
  QuicCryptoServerStream* GetCryptoStream() override;

  // If we should create an incoming stream, returns true. Otherwise
  // does error handling, including communicating the error to the client and
  // possibly closing the connection, and returns false.
  virtual bool ShouldCreateIncomingDataStream(QuicStreamId id);

  virtual QuicCryptoServerStream* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config);

 private:
  friend class test::QuicServerSessionPeer;

  scoped_ptr<QuicCryptoServerStream> crypto_stream_;
  QuicServerSessionVisitor* visitor_;

  // The most recent bandwidth estimate sent to the client.
  QuicBandwidth bandwidth_estimate_sent_to_client_;

  // Text describing server location. Sent to the client as part of the bandwith
  // estimate in the source-address token. Optional, can be left empty.
  std::string serving_region_;

  // Time at which we send the last SCUP to the client.
  QuicTime last_scup_time_;

  // Number of packets sent to the peer, at the time we last sent a SCUP.
  int64 last_scup_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(QuicServerSession);
};

}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_QUIC_SERVER_SESSION_H_
