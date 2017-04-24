// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationInspectorAgent_h
#define DeviceOrientationInspectorAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/DeviceOrientation.h"
#include "modules/ModulesExport.h"

namespace blink {

class DeviceOrientationController;
class InspectedFrames;

class MODULES_EXPORT DeviceOrientationInspectorAgent final
    : public InspectorBaseAgent<protocol::DeviceOrientation::Metainfo> {
  WTF_MAKE_NONCOPYABLE(DeviceOrientationInspectorAgent);

 public:
  explicit DeviceOrientationInspectorAgent(InspectedFrames*);
  ~DeviceOrientationInspectorAgent() override;
  DECLARE_VIRTUAL_TRACE();

  // Protocol methods.
  Response setDeviceOrientationOverride(double, double, double) override;
  Response clearDeviceOrientationOverride() override;

  Response disable() override;
  void restore() override;
  void didCommitLoadForLocalFrame(LocalFrame*) override;

 private:
  DeviceOrientationController* controller();

  Member<InspectedFrames> m_inspectedFrames;
};

}  // namespace blink

#endif  // !defined(DeviceOrientationInspectorAgent_h)
