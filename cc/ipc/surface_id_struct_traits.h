// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_

#include "cc/surfaces/surface_id.h"

namespace mojo {

// This template is fully specialized as cc::mojom::SurfaceIdDataView and
// as cc::mojom::blink::SurfaceIdDataView, in generated .mojom.h and
// .mojom-blink.h respectively.
template <typename T>
struct StructTraits<T, cc::SurfaceId> {
  static uint32_t client_id(const cc::SurfaceId& id) { return id.client_id(); }

  static uint32_t local_id(const cc::SurfaceId& id) { return id.local_id(); }

  static uint64_t nonce(const cc::SurfaceId& id) { return id.nonce(); }

  static bool Read(T data, cc::SurfaceId* out) {
    *out = cc::SurfaceId(data.client_id(), data.local_id(), data.nonce());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_ID_STRUCT_TRAITS_H_
