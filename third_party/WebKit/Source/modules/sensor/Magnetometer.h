// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Magnetometer_h
#define Magnetometer_h

#include "modules/sensor/Sensor.h"

namespace blink {

class Magnetometer final : public Sensor {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Magnetometer* create(ExecutionContext*,
                              const SensorOptions&,
                              ExceptionState&);
  static Magnetometer* create(ExecutionContext*, ExceptionState&);

  double x(bool& isNull) const;
  double y(bool& isNull) const;
  double z(bool& isNull) const;

  DECLARE_VIRTUAL_TRACE();

 private:
  Magnetometer(ExecutionContext*, const SensorOptions&, ExceptionState&);
};

}  // namespace blink

#endif  // Magnetometer_h
