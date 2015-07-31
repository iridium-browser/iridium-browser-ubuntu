// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_BASIC_TYPE_CONVERTERS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_BASIC_TYPE_CONVERTERS_H_

#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/array.h"
#include "ui/mojo/geometry/geometry.mojom.h"

namespace blink {
struct WebRect;
class WebString;
}

namespace mojo {
class String;

template<>
struct TypeConverter<String, blink::WebString> {
  static String Convert(const blink::WebString& str);
};
template<>
struct TypeConverter<blink::WebString, String> {
  static blink::WebString Convert(const String& str);
};
template <>
struct TypeConverter<Array<uint8_t>, blink::WebString> {
  static Array<uint8_t> Convert(const blink::WebString& input);
};

template <>
struct TypeConverter<RectPtr, blink::WebRect> {
  static RectPtr Convert(const blink::WebRect& input);
};

template<typename T, typename U>
struct TypeConverter<Array<T>, blink::WebVector<U> > {
  static Array<T> Convert(const blink::WebVector<U>& vector) {
    Array<T> array(vector.size());
    for (size_t i = 0; i < vector.size(); ++i)
      array[i] = TypeConverter<T, U>::Convert(vector[i]);
    return array.Pass();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_HTML_VIEWER_BLINK_BASIC_TYPE_CONVERTERS_H_
