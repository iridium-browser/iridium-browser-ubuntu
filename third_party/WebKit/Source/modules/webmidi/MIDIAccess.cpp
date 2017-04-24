/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webmidi/MIDIAccess.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIConnectionEvent.h"
#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIInputMap.h"
#include "modules/webmidi/MIDIOutput.h"
#include "modules/webmidi/MIDIOutputMap.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/AsyncMethodRunner.h"
#include <memory>

namespace blink {

namespace {

using midi::mojom::PortState;

// Since "open" status is separately managed per MIDIAccess instance, we do not
// expose service level PortState directly.
PortState ToDeviceState(PortState state) {
  if (state == PortState::OPENED)
    return PortState::CONNECTED;
  return state;
}

}  // namespace

MIDIAccess::MIDIAccess(
    std::unique_ptr<MIDIAccessor> accessor,
    bool sysexEnabled,
    const Vector<MIDIAccessInitializer::PortDescriptor>& ports,
    ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext),
      m_accessor(std::move(accessor)),
      m_sysexEnabled(sysexEnabled),
      m_hasPendingActivity(false) {
  m_accessor->setClient(this);
  for (size_t i = 0; i < ports.size(); ++i) {
    const MIDIAccessInitializer::PortDescriptor& port = ports[i];
    if (port.type == MIDIPort::TypeInput) {
      m_inputs.push_back(MIDIInput::create(this, port.id, port.manufacturer,
                                           port.name, port.version,
                                           ToDeviceState(port.state)));
    } else {
      m_outputs.push_back(MIDIOutput::create(
          this, m_outputs.size(), port.id, port.manufacturer, port.name,
          port.version, ToDeviceState(port.state)));
    }
  }
}

MIDIAccess::~MIDIAccess() {}

void MIDIAccess::dispose() {
  m_accessor.reset();
}

EventListener* MIDIAccess::onstatechange() {
  return getAttributeEventListener(EventTypeNames::statechange);
}

void MIDIAccess::setOnstatechange(EventListener* listener) {
  m_hasPendingActivity = listener;
  setAttributeEventListener(EventTypeNames::statechange, listener);
}

bool MIDIAccess::hasPendingActivity() const {
  return m_hasPendingActivity && getExecutionContext() &&
         !getExecutionContext()->isContextDestroyed();
}

MIDIInputMap* MIDIAccess::inputs() const {
  HeapVector<Member<MIDIInput>> inputs;
  HashSet<String> ids;
  for (size_t i = 0; i < m_inputs.size(); ++i) {
    MIDIInput* input = m_inputs[i];
    if (input->getState() != PortState::DISCONNECTED) {
      inputs.push_back(input);
      ids.insert(input->id());
    }
  }
  if (inputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    inputs.clear();
  }
  return new MIDIInputMap(inputs);
}

MIDIOutputMap* MIDIAccess::outputs() const {
  HeapVector<Member<MIDIOutput>> outputs;
  HashSet<String> ids;
  for (size_t i = 0; i < m_outputs.size(); ++i) {
    MIDIOutput* output = m_outputs[i];
    if (output->getState() != PortState::DISCONNECTED) {
      outputs.push_back(output);
      ids.insert(output->id());
    }
  }
  if (outputs.size() != ids.size()) {
    // There is id duplication that violates the spec.
    outputs.clear();
  }
  return new MIDIOutputMap(outputs);
}

void MIDIAccess::didAddInputPort(const String& id,
                                 const String& manufacturer,
                                 const String& name,
                                 const String& version,
                                 PortState state) {
  DCHECK(isMainThread());
  MIDIInput* port = MIDIInput::create(this, id, manufacturer, name, version,
                                      ToDeviceState(state));
  m_inputs.push_back(port);
  dispatchEvent(MIDIConnectionEvent::create(port));
}

void MIDIAccess::didAddOutputPort(const String& id,
                                  const String& manufacturer,
                                  const String& name,
                                  const String& version,
                                  PortState state) {
  DCHECK(isMainThread());
  unsigned portIndex = m_outputs.size();
  MIDIOutput* port = MIDIOutput::create(this, portIndex, id, manufacturer, name,
                                        version, ToDeviceState(state));
  m_outputs.push_back(port);
  dispatchEvent(MIDIConnectionEvent::create(port));
}

void MIDIAccess::didSetInputPortState(unsigned portIndex, PortState state) {
  DCHECK(isMainThread());
  if (portIndex >= m_inputs.size())
    return;

  PortState deviceState = ToDeviceState(state);
  if (m_inputs[portIndex]->getState() != deviceState)
    m_inputs[portIndex]->setState(deviceState);
}

void MIDIAccess::didSetOutputPortState(unsigned portIndex, PortState state) {
  DCHECK(isMainThread());
  if (portIndex >= m_outputs.size())
    return;

  PortState deviceState = ToDeviceState(state);
  if (m_outputs[portIndex]->getState() != deviceState)
    m_outputs[portIndex]->setState(deviceState);
}

void MIDIAccess::didReceiveMIDIData(unsigned portIndex,
                                    const unsigned char* data,
                                    size_t length,
                                    double timeStamp) {
  DCHECK(isMainThread());
  if (portIndex >= m_inputs.size())
    return;

  m_inputs[portIndex]->didReceiveMIDIData(portIndex, data, length, timeStamp);
}

void MIDIAccess::sendMIDIData(unsigned portIndex,
                              const unsigned char* data,
                              size_t length,
                              double timeStampInMilliseconds) {
  // Do not continue sending when document is going to be closed.
  Document* document = toDocument(getExecutionContext());
  DCHECK(document);
  DocumentLoader* loader = document->loader();
  if (!loader)
    return;

  if (!data || !length || portIndex >= m_outputs.size())
    return;

  // Convert from a time in milliseconds (a DOMHighResTimeStamp) according to
  // the same time coordinate system as performance.now() into a time in seconds
  // which is based on the time coordinate system of
  // monotonicallyIncreasingTime().
  double timeStamp;

  if (!timeStampInMilliseconds) {
    // We treat a value of 0 (which is the default value) as special, meaning
    // "now".  We need to translate it exactly to 0 seconds.
    timeStamp = 0;
  } else {
    double documentStartTime = loader->timing().referenceMonotonicTime();
    timeStamp = documentStartTime + 0.001 * timeStampInMilliseconds;
  }

  m_accessor->sendMIDIData(portIndex, data, length, timeStamp);
}

void MIDIAccess::contextDestroyed(ExecutionContext*) {
  m_accessor.reset();
}

DEFINE_TRACE(MIDIAccess) {
  visitor->trace(m_inputs);
  visitor->trace(m_outputs);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
