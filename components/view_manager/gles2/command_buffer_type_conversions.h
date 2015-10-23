// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
#define COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_

#include "components/view_manager/public/interfaces/command_buffer.mojom.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

class CommandBufferState;

template <>
struct TypeConverter<CommandBufferStatePtr, gpu::CommandBuffer::State> {
  static CommandBufferStatePtr Convert(const gpu::CommandBuffer::State& input);
};

template <>
struct TypeConverter<gpu::CommandBuffer::State, CommandBufferStatePtr> {
  static gpu::CommandBuffer::State Convert(const CommandBufferStatePtr& input);
};

template <>
struct TypeConverter<GpuShaderPrecisionPtr,
                     gpu::Capabilities::ShaderPrecision> {
  static GpuShaderPrecisionPtr Convert(
      const gpu::Capabilities::ShaderPrecision& input);
};

template <>
struct TypeConverter<gpu::Capabilities::ShaderPrecision,
                     GpuShaderPrecisionPtr> {
  static gpu::Capabilities::ShaderPrecision Convert(
      const GpuShaderPrecisionPtr& input);
};

template <>
struct TypeConverter<GpuPerStagePrecisionsPtr,
                     gpu::Capabilities::PerStagePrecisions> {
  static GpuPerStagePrecisionsPtr Convert(
      const gpu::Capabilities::PerStagePrecisions& input);
};

template <>
struct TypeConverter<gpu::Capabilities::PerStagePrecisions,
                     GpuPerStagePrecisionsPtr> {
  static gpu::Capabilities::PerStagePrecisions Convert(
      const GpuPerStagePrecisionsPtr& input);
};

template <>
struct TypeConverter<GpuCapabilitiesPtr, gpu::Capabilities> {
  static GpuCapabilitiesPtr Convert(const gpu::Capabilities& input);
};

template <>
struct TypeConverter<gpu::Capabilities, GpuCapabilitiesPtr> {
  static gpu::Capabilities Convert(const GpuCapabilitiesPtr& input);
};

}  // namespace gles2

#endif  // COMPONENTS_VIEW_MANAGER_GLES2_COMMAND_BUFFER_TYPE_CONVERSIONS_H_
