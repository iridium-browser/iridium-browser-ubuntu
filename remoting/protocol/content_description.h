// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
#define REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/session_config.h"

namespace buzz {
class XmlElement;
}  // namespace buzz

namespace remoting {
namespace protocol {

// ContentDescription used for chromoting sessions. It contains the information
// from the content description stanza in the session initialization handshake.
//
// This class also provides a type abstraction so that the Chromotocol Session
// interface does not need to depend on libjingle.
class ContentDescription {
 public:
  static const char kChromotingContentName[];

  ContentDescription(scoped_ptr<CandidateSessionConfig> config,
                     scoped_ptr<buzz::XmlElement> authenticator_message,
                     const std::string& quic_config_message);
  ~ContentDescription();

  const CandidateSessionConfig* config() const {
    return candidate_config_.get();
  }

  const buzz::XmlElement* authenticator_message() const {
    return authenticator_message_.get();
  }

  const std::string& quic_config_message() const {
    return quic_config_message_;
  }

  buzz::XmlElement* ToXml() const;

  static scoped_ptr<ContentDescription> ParseXml(
      const buzz::XmlElement* element);

 private:
  scoped_ptr<const CandidateSessionConfig> candidate_config_;
  scoped_ptr<const buzz::XmlElement> authenticator_message_;
  std::string quic_config_message_;

  static bool ParseChannelConfigs(const buzz::XmlElement* const element,
                                  const char tag_name[],
                                  bool codec_required,
                                  bool optional,
                                  std::list<ChannelConfig>* const configs);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONTENT_DESCRIPTION_H_
