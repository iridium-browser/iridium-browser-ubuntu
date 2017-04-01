// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AmbientLightSensor.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/sensor/AmbientLightSensorReading.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
AmbientLightSensor* AmbientLightSensor::create(
    ExecutionContext* executionContext,
    const SensorOptions& options,
    ExceptionState& exceptionState) {
  return new AmbientLightSensor(executionContext, options, exceptionState);
}

// static
AmbientLightSensor* AmbientLightSensor::create(
    ExecutionContext* executionContext,
    ExceptionState& exceptionState) {
  return create(executionContext, SensorOptions(), exceptionState);
}

AmbientLightSensor::AmbientLightSensor(ExecutionContext* executionContext,
                                       const SensorOptions& options,
                                       ExceptionState& exceptionState)
    : Sensor(executionContext,
             options,
             exceptionState,
             SensorType::AMBIENT_LIGHT) {}

AmbientLightSensorReading* AmbientLightSensor::reading() const {
  return static_cast<AmbientLightSensorReading*>(Sensor::reading());
}

std::unique_ptr<SensorReadingFactory>
AmbientLightSensor::createSensorReadingFactory() {
  return std::unique_ptr<SensorReadingFactory>(
      new SensorReadingFactoryImpl<AmbientLightSensorReading>());
}

DEFINE_TRACE(AmbientLightSensor) {
  Sensor::trace(visitor);
}

}  // namespace blink
