// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubresourceIntegrity_h
#define SubresourceIntegrity_h

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "platform/Crypto.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
class ExecutionContext;
class KURL;
class Resource;

class CORE_EXPORT SubresourceIntegrity {
  STATIC_ONLY(SubresourceIntegrity);

 public:
  enum IntegrityParseResult {
    kIntegrityParseValidResult,
    kIntegrityParseNoValidResult
  };

  // The versions with the IntegrityMetadataSet passed as the first argument
  // assume that the integrity attribute has already been parsed, and the
  // IntegrityMetadataSet represents the result of that parsing.
  static bool CheckSubresourceIntegrity(const String& integrity_attribute,
                                        Document&,  // the embedding document
                                        const char* content,
                                        size_t,
                                        const KURL& resource_url,
                                        const Resource&);
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        Document&,
                                        const char* content,
                                        size_t,
                                        const KURL& resource_url,
                                        const Resource&);
  static bool CheckSubresourceIntegrity(const String&,
                                        const char*,
                                        size_t,
                                        const KURL& resource_url,
                                        ExecutionContext&,
                                        WTF::String&);
  static bool CheckSubresourceIntegrity(const IntegrityMetadataSet&,
                                        const char*,
                                        size_t,
                                        const KURL& resource_url,
                                        ExecutionContext&,
                                        WTF::String&);

  // The IntegrityMetadataSet arguments are out parameters which contain the
  // set of all valid, parsed metadata from |attribute|.
  static IntegrityParseResult ParseIntegrityAttribute(
      const WTF::String& attribute,
      IntegrityMetadataSet&);
  static IntegrityParseResult ParseIntegrityAttribute(
      const WTF::String& attribute,
      IntegrityMetadataSet&,
      ExecutionContext*);

 private:
  friend class SubresourceIntegrityTest;
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Parsing);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, ParseAlgorithm);
  FRIEND_TEST_ALL_PREFIXES(SubresourceIntegrityTest, Prioritization);

  enum AlgorithmParseResult {
    kAlgorithmValid,
    kAlgorithmUnparsable,
    kAlgorithmUnknown
  };

  static HashAlgorithm GetPrioritizedHashFunction(HashAlgorithm, HashAlgorithm);
  static AlgorithmParseResult ParseAlgorithm(const UChar*& begin,
                                             const UChar* end,
                                             HashAlgorithm&);
  static bool ParseDigest(const UChar*& begin,
                          const UChar* end,
                          String& digest);
};

}  // namespace blink

#endif
