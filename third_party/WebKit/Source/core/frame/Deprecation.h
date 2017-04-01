// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Deprecation_h
#define Deprecation_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "wtf/BitVector.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Deprecation {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(Deprecation);

 public:
  Deprecation();
  ~Deprecation();

  static void warnOnDeprecatedProperties(const LocalFrame*,
                                         CSSPropertyID unresolvedProperty);
  void clearSuppression();

  void muteForInspector();
  void unmuteForInspector();

  // "countDeprecation" sets the bit for this feature to 1, and sends a
  // deprecation warning to the console. Repeated calls are ignored.
  //
  // Be considerate to developers' consoles: features should only send
  // deprecation warnings when we're actively interested in removing them from
  // the platform.
  //
  // For shared workers and service workers, the ExecutionContext* overload
  // doesn't count the usage but only sends a console warning.
  static void countDeprecation(const LocalFrame*, UseCounter::Feature);
  static void countDeprecation(ExecutionContext*, UseCounter::Feature);
  static void countDeprecation(const Document&, UseCounter::Feature);

  // Count only features if they're being used in an iframe which does not
  // have script access into the top level document.
  static void countDeprecationCrossOriginIframe(const LocalFrame*,
                                                UseCounter::Feature);
  static void countDeprecationCrossOriginIframe(const Document&,
                                                UseCounter::Feature);
  static String deprecationMessage(UseCounter::Feature);

  // Note: this is only public for tests.
  bool isSuppressed(CSSPropertyID unresolvedProperty);

 protected:
  void suppress(CSSPropertyID unresolvedProperty);
  // CSSPropertyIDs that aren't deprecated return an empty string.
  static String deprecationMessage(CSSPropertyID unresolvedProperty);

  BitVector m_cssPropertyDeprecationBits;
  unsigned m_muteCount;
};

}  // namespace blink

#endif  // Deprecation_h
