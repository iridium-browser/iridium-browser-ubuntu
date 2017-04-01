// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ipc/latency_info_param_traits_macros.h"

#include "ui/gfx/ipc/geometry/gfx_param_traits.h"

// Generate param traits size methods.
#include "ipc/param_traits_size_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/events/ipc/latency_info_param_traits_macros.h"
}

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/events/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/events/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef UI_EVENTS_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#include "ui/events/ipc/latency_info_param_traits_macros.h"
}  // namespace IPC

// Implemetation for ParamTraits<ui::LatencyInfo>.
#include "ui/events/ipc/latency_info_param_traits.h"

namespace IPC {

void ParamTraits<ui::LatencyInfo>::GetSize(base::PickleSizer* s,
                                           const param_type& p) {
  GetParamSize(s, p.trace_name_);
  GetParamSize(s, p.latency_components_);
  GetParamSize(s, p.input_coordinates_size_);
  for (size_t i = 0; i < p.input_coordinates_size_; i++) {
    GetParamSize(s, p.input_coordinates_[i]);
  }
  GetParamSize(s, p.trace_id_);
  GetParamSize(s, p.terminated_);
  GetParamSize(s, p.source_event_type_);
}

void ParamTraits<ui::LatencyInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.trace_name_);
  WriteParam(m, p.latency_components_);
  WriteParam(m, p.input_coordinates_size_);
  for (size_t i = 0; i < p.input_coordinates_size_; i++) {
    WriteParam(m, p.input_coordinates_[i]);
  }
  WriteParam(m, p.trace_id_);
  WriteParam(m, p.terminated_);
  WriteParam(m, p.source_event_type_);
}

bool ParamTraits<ui::LatencyInfo>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  if (!ReadParam(m, iter, &p->trace_name_))
    return false;
  if (!ReadParam(m, iter, &p->latency_components_))
    return false;

  gfx::PointF input_coordinates;
  uint32_t input_coordinates_size;
  if (!ReadParam(m, iter, &input_coordinates_size))
    return false;
  for (size_t i = 0; i < input_coordinates_size; i++) {
    if (!ReadParam(m, iter, &input_coordinates))
        return false;
    if (!p->AddInputCoordinate(input_coordinates))
      return false;
  }

  if (!ReadParam(m, iter, &p->trace_id_))
    return false;
  if (!ReadParam(m, iter, &p->terminated_))
    return false;
  if (!ReadParam(m, iter, &p->source_event_type_))
    return false;

  return true;
}

void ParamTraits<ui::LatencyInfo>::Log(const param_type& p,
                                       std::string* l) {
  LogParam(p.trace_name_, l);
  l->append(" ");
  LogParam(p.latency_components_, l);
  l->append(" ");
  LogParam(p.input_coordinates_size_, l);
  l->append(" ");
  for (size_t i = 0; i < p.input_coordinates_size_; i++) {
    LogParam(p.input_coordinates_[i], l);
    l->append(" ");
  }
  LogParam(p.trace_id_, l);
  l->append(" ");
  LogParam(p.terminated_, l);
  l->append(" ");
  LogParam(p.source_event_type_, l);
}

}  // namespace IPC
