// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/public/web/ConsoleMessageStructTraits.h"

#include "third_party/WebKit/public/platform/WebString.h"

namespace mojo {

// Ensure that the WebConsoleMessage::Level enum values stay in sync with the
// mojom::ConsoleMessageLevel.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enum : " #a)

STATIC_ASSERT_ENUM(::blink::WebConsoleMessage::Level::LevelDebug,
                   ::blink::mojom::ConsoleMessageLevel::Debug);
STATIC_ASSERT_ENUM(::blink::WebConsoleMessage::Level::LevelLog,
                   ::blink::mojom::ConsoleMessageLevel::Log);
STATIC_ASSERT_ENUM(::blink::WebConsoleMessage::Level::LevelInfo,
                   ::blink::mojom::ConsoleMessageLevel::Info);
STATIC_ASSERT_ENUM(::blink::WebConsoleMessage::Level::LevelWarning,
                   ::blink::mojom::ConsoleMessageLevel::Warning);
STATIC_ASSERT_ENUM(::blink::WebConsoleMessage::Level::LevelError,
                   ::blink::mojom::ConsoleMessageLevel::Error);

// static
::blink::mojom::ConsoleMessageLevel
EnumTraits<::blink::mojom::ConsoleMessageLevel,
           ::blink::WebConsoleMessage::Level>::
    ToMojom(::blink::WebConsoleMessage::Level level) {
  return static_cast<::blink::mojom::ConsoleMessageLevel>(level);
}

// static
bool EnumTraits<::blink::mojom::ConsoleMessageLevel,
                ::blink::WebConsoleMessage::Level>::
    FromMojom(::blink::mojom::ConsoleMessageLevel in,
              ::blink::WebConsoleMessage::Level* out) {
  *out = static_cast<::blink::WebConsoleMessage::Level>(in);
  return true;
}

}  // namespace mojo
