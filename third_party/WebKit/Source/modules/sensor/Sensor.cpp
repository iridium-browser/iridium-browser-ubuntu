// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/Sensor.h"

#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessage.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "modules/sensor/SensorErrorEvent.h"
#include "modules/sensor/SensorProviderProxy.h"
#include "modules/sensor/SensorReading.h"

using namespace device::mojom::blink;

namespace blink {

Sensor::Sensor(ExecutionContext* executionContext,
               const SensorOptions& sensorOptions,
               ExceptionState& exceptionState,
               SensorType type)
    : ContextLifecycleObserver(executionContext),
      m_sensorOptions(sensorOptions),
      m_type(type),
      m_state(Sensor::SensorState::Idle),
      m_lastUpdateTimestamp(0.0) {
  // Check secure context.
  String errorMessage;
  if (!executionContext->isSecureContext(errorMessage)) {
    exceptionState.throwDOMException(SecurityError, errorMessage);
    return;
  }

  // Check top-level browsing context.
  if (!toDocument(executionContext)->domWindow()->frame() ||
      !toDocument(executionContext)->frame()->isMainFrame()) {
    exceptionState.throwSecurityError(
        "Must be in a top-level browsing context");
    return;
  }

  // Check the given frequency value.
  if (m_sensorOptions.hasFrequency()) {
    double frequency = m_sensorOptions.frequency();
    if (frequency <= 0.0) {
      exceptionState.throwRangeError("Frequency must be positive.");
      return;
    }

    if (frequency > SensorConfiguration::kMaxAllowedFrequency) {
      m_sensorOptions.setFrequency(SensorConfiguration::kMaxAllowedFrequency);
      ConsoleMessage* consoleMessage = ConsoleMessage::create(
          JSMessageSource, InfoMessageLevel, "Frequency is limited to 60 Hz.");
      executionContext->addConsoleMessage(consoleMessage);
    }
  }
}

Sensor::~Sensor() = default;

void Sensor::start(ScriptState* scriptState, ExceptionState& exceptionState) {
  if (m_state != Sensor::SensorState::Idle &&
      m_state != Sensor::SensorState::Errored) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot start because SensorState is not Idle or errored");
    return;
  }

  initSensorProxyIfNeeded();

  if (!m_sensorProxy) {
    exceptionState.throwDOMException(
        InvalidStateError, "The Sensor is no longer associated to a frame.");
    return;
  }
  m_lastUpdateTimestamp = WTF::monotonicallyIncreasingTime();
  startListening();
}

void Sensor::stop(ScriptState*, ExceptionState& exceptionState) {
  if (m_state == Sensor::SensorState::Idle ||
      m_state == Sensor::SensorState::Errored) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "Cannot stop because SensorState is either Idle or errored");
    return;
  }

  stopListening();
}

static String ToString(Sensor::SensorState state) {
  switch (state) {
    case Sensor::SensorState::Idle:
      return "idle";
    case Sensor::SensorState::Activating:
      return "activating";
    case Sensor::SensorState::Activated:
      return "activated";
    case Sensor::SensorState::Errored:
      return "errored";
    default:
      NOTREACHED();
  }
  return "idle";
}

// Getters
String Sensor::state() const {
  return ToString(m_state);
}

SensorReading* Sensor::reading() const {
  if (m_state != Sensor::SensorState::Activated)
    return nullptr;
  DCHECK(m_sensorProxy);
  return m_sensorProxy->sensorReading();
}

DEFINE_TRACE(Sensor) {
  visitor->trace(m_sensorProxy);
  ActiveScriptWrappable::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
}

bool Sensor::hasPendingActivity() const {
  if (m_state == Sensor::SensorState::Idle ||
      m_state == Sensor::SensorState::Errored)
    return false;
  return getExecutionContext() && hasEventListeners();
}

auto Sensor::createSensorConfig() -> SensorConfigurationPtr {
  auto result = SensorConfiguration::New();

  double defaultFrequency = m_sensorProxy->defaultConfig()->frequency;
  double maximumFrequency = m_sensorProxy->maximumFrequency();

  double frequency = m_sensorOptions.hasFrequency()
                         ? m_sensorOptions.frequency()
                         : defaultFrequency;

  if (frequency > maximumFrequency)
    frequency = maximumFrequency;

  result->frequency = frequency;
  return result;
}

void Sensor::initSensorProxyIfNeeded() {
  if (m_sensorProxy)
    return;

  Document* document = toDocument(getExecutionContext());
  if (!document || !document->frame())
    return;

  auto provider = SensorProviderProxy::from(document->frame());
  m_sensorProxy = provider->getSensorProxy(m_type);

  if (!m_sensorProxy) {
    m_sensorProxy = provider->createSensorProxy(m_type, document,
                                                createSensorReadingFactory());
  }
}

void Sensor::contextDestroyed(ExecutionContext*) {
  if (m_state == Sensor::SensorState::Activated ||
      m_state == Sensor::SensorState::Activating)
    stopListening();
}

void Sensor::onSensorInitialized() {
  if (m_state != Sensor::SensorState::Activating)
    return;

  startListening();
}

void Sensor::onSensorReadingChanged(double timestamp) {
  if (m_state != Sensor::SensorState::Activated)
    return;

  DCHECK_GT(m_configuration->frequency, 0.0);
  double period = 1 / m_configuration->frequency;
  if (timestamp - m_lastUpdateTimestamp >= period) {
    m_lastUpdateTimestamp = timestamp;
    notifySensorReadingChanged();
  }
}

void Sensor::onSensorError(ExceptionCode code,
                           const String& sanitizedMessage,
                           const String& unsanitizedMessage) {
  reportError(code, sanitizedMessage, unsanitizedMessage);
}

void Sensor::onStartRequestCompleted(bool result) {
  if (m_state != Sensor::SensorState::Activating)
    return;

  if (!result) {
    reportError(
        OperationError,
        "start() call has failed possibly due to inappropriate options.");
    return;
  }

  updateState(Sensor::SensorState::Activated);
}

void Sensor::startListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::Activating);

  m_sensorProxy->addObserver(this);
  if (!m_sensorProxy->isInitialized()) {
    m_sensorProxy->initialize();
    return;
  }

  if (!m_configuration) {
    m_configuration = createSensorConfig();
    DCHECK(m_configuration);
    DCHECK(m_configuration->frequency > 0 &&
           m_configuration->frequency <= m_sensorProxy->maximumFrequency());
  }

  auto startCallback =
      WTF::bind(&Sensor::onStartRequestCompleted, wrapWeakPersistent(this));
  m_sensorProxy->addConfiguration(m_configuration->Clone(),
                                  std::move(startCallback));
}

void Sensor::stopListening() {
  DCHECK(m_sensorProxy);
  updateState(Sensor::SensorState::Idle);

  if (m_sensorProxy->isInitialized()) {
    DCHECK(m_configuration);
    m_sensorProxy->removeConfiguration(m_configuration->Clone());
  }
  m_sensorProxy->removeObserver(this);
}

void Sensor::updateState(Sensor::SensorState newState) {
  if (newState == m_state)
    return;

  if (newState == SensorState::Activated && getExecutionContext()) {
    DCHECK_EQ(SensorState::Activating, m_state);
    // The initial value for m_lastUpdateTimestamp is set to current time,
    // so that the first reading update will be notified considering the given
    // frequency hint.
    m_lastUpdateTimestamp = WTF::monotonicallyIncreasingTime();
    getExecutionContext()->postTask(
        TaskType::Sensor, BLINK_FROM_HERE,
        createSameThreadTask(&Sensor::notifyOnActivate,
                             wrapWeakPersistent(this)));
  }

  m_state = newState;
}

void Sensor::reportError(ExceptionCode code,
                         const String& sanitizedMessage,
                         const String& unsanitizedMessage) {
  updateState(Sensor::SensorState::Errored);
  if (getExecutionContext()) {
    auto error =
        DOMException::create(code, sanitizedMessage, unsanitizedMessage);
    getExecutionContext()->postTask(
        TaskType::Sensor, BLINK_FROM_HERE,
        createSameThreadTask(&Sensor::notifyError, wrapWeakPersistent(this),
                             wrapPersistent(error)));
  }
}

void Sensor::notifySensorReadingChanged() {
  DCHECK(m_sensorProxy);
  DCHECK(m_sensorProxy->sensorReading());

  if (m_sensorProxy->sensorReading()->isReadingUpdated(m_storedData)) {
    m_storedData = m_sensorProxy->sensorReading()->data();
    dispatchEvent(Event::create(EventTypeNames::change));
  }
}

void Sensor::notifyOnActivate() {
  dispatchEvent(Event::create(EventTypeNames::activate));
}

void Sensor::notifyError(DOMException* error) {
  dispatchEvent(
      SensorErrorEvent::create(EventTypeNames::error, std::move(error)));
}

}  // namespace blink
