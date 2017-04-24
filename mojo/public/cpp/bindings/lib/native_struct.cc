// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/native_struct.h"

#include "mojo/public/cpp/bindings/lib/hash_util.h"

namespace mojo {

// static
NativeStructPtr NativeStruct::New() {
  return NativeStructPtr(base::in_place);
}

NativeStruct::NativeStruct() {}

NativeStruct::~NativeStruct() {}

NativeStructPtr NativeStruct::Clone() const {
  NativeStructPtr rv(New());
  rv->data = data;
  return rv;
}

bool NativeStruct::Equals(const NativeStruct& other) const {
  return data == other.data;
}

size_t NativeStruct::Hash(size_t seed) const {
  return internal::Hash(seed, data);
}

}  // namespace mojo
