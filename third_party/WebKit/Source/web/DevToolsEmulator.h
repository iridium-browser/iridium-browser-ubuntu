// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DevToolsEmulator_h
#define DevToolsEmulator_h

#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"

namespace blink {

class InspectorEmulationAgent;
class IntPoint;
class WebInputEvent;
class WebViewImpl;

struct WebDeviceEmulationParams;

class DevToolsEmulator final : public NoBaseWillBeGarbageCollectedFinalized<DevToolsEmulator> {
public:
    ~DevToolsEmulator();
    static PassOwnPtrWillBeRawPtr<DevToolsEmulator> create(WebViewImpl*);
    DECLARE_TRACE();

    void setEmulationAgent(InspectorEmulationAgent*);
    void viewportChanged();

    // Settings overrides.
    void setTextAutosizingEnabled(bool);
    void setDeviceScaleAdjustment(float);
    void setPreferCompositingToLCDTextEnabled(bool);
    void setUseMobileViewportStyle(bool);
    void setPluginsEnabled(bool);
    void setScriptEnabled(bool);
    void setDoubleTapToZoomEnabled(bool);
    bool doubleTapToZoomEnabled() const;
    void setHidePinchScrollbarsNearMinScale(bool);

    // Emulation.
    void enableDeviceEmulation(const WebDeviceEmulationParams&);
    void disableDeviceEmulation();
    bool deviceEmulationEnabled() { return m_deviceMetricsEnabled; }
    void setTouchEventEmulationEnabled(bool);
    bool handleInputEvent(const WebInputEvent&);
    void setScriptExecutionDisabled(bool);

private:
    explicit DevToolsEmulator(WebViewImpl*);

    void enableMobileEmulation();
    void disableMobileEmulation();

    WebViewImpl* m_webViewImpl;
    RawPtrWillBeMember<InspectorEmulationAgent> m_emulationAgent;

    bool m_deviceMetricsEnabled;
    bool m_emulateMobileEnabled;
    bool m_isOverlayScrollbarsEnabled;
    float m_originalDefaultMinimumPageScaleFactor;
    float m_originalDefaultMaximumPageScaleFactor;
    bool m_embedderTextAutosizingEnabled;
    float m_embedderDeviceScaleAdjustment;
    bool m_embedderPreferCompositingToLCDTextEnabled;
    bool m_embedderUseMobileViewport;
    bool m_embedderPluginsEnabled;

    bool m_touchEventEmulationEnabled;
    bool m_doubleTapToZoomEnabled;
    bool m_originalTouchEnabled;
    bool m_originalDeviceSupportsMouse;
    bool m_originalDeviceSupportsTouch;
    int m_originalMaxTouchPoints;
    OwnPtr<IntPoint> m_lastPinchAnchorCss;
    OwnPtr<IntPoint> m_lastPinchAnchorDip;

    bool m_embedderScriptEnabled;
    bool m_scriptExecutionDisabled;
    bool m_hidePinchScrollbarsNearMinScale;
};

} // namespace blink

#endif
