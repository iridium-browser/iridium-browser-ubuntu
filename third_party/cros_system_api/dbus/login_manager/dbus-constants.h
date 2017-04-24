// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_LOGIN_MANAGER_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_LOGIN_MANAGER_DBUS_CONSTANTS_H_

namespace login_manager {
const char kSessionManagerInterface[] = "org.chromium.SessionManagerInterface";
const char kSessionManagerServicePath[] = "/org/chromium/SessionManager";
const char kSessionManagerServiceName[] = "org.chromium.SessionManager";
// Methods
const char kSessionManagerEmitLoginPromptVisible[] = "EmitLoginPromptVisible";
const char kSessionManagerStartSession[] = "StartSession";
const char kSessionManagerStopSession[] = "StopSession";
const char kSessionManagerRestartJob[] = "RestartJob";
const char kSessionManagerStorePolicy[] = "StorePolicy";
const char kSessionManagerStoreUnsignedPolicy[] = "StoreUnsignedPolicy";
const char kSessionManagerRetrievePolicy[] = "RetrievePolicy";
const char kSessionManagerStorePolicyForUser[] = "StorePolicyForUser";
const char kSessionManagerStoreUnsignedPolicyForUser[] =
    "StoreUnsignedPolicyForUser";
const char kSessionManagerRetrievePolicyForUser[] = "RetrievePolicyForUser";
const char kSessionManagerStoreDeviceLocalAccountPolicy[] =
    "StoreDeviceLocalAccountPolicy";
const char kSessionManagerRetrieveDeviceLocalAccountPolicy[] =
    "RetrieveDeviceLocalAccountPolicy";
const char kSessionManagerRetrieveSessionState[] = "RetrieveSessionState";
const char kSessionManagerRetrieveActiveSessions[] = "RetrieveActiveSessions";
const char kSessionManagerStartDeviceWipe[] = "StartDeviceWipe";
const char kSessionManagerHandleSupervisedUserCreationStarting[] =
    "HandleSupervisedUserCreationStarting";
const char kSessionManagerHandleSupervisedUserCreationFinished[] =
    "HandleSupervisedUserCreationFinished";
const char kSessionManagerLockScreen[] = "LockScreen";
const char kSessionManagerHandleLockScreenShown[] = "HandleLockScreenShown";
const char kSessionManagerHandleLockScreenDismissed[] =
    "HandleLockScreenDismissed";
const char kSessionManagerSetFlagsForUser[] = "SetFlagsForUser";
const char kSessionManagerGetServerBackedStateKeys[] =
    "GetServerBackedStateKeys";
const char kSessionManagerInitMachineInfo[] = "InitMachineInfo";
const char kSessionManagerCheckArcAvailability[] = "CheckArcAvailability";
const char kSessionManagerStartArcInstance[] = "StartArcInstance";
const char kSessionManagerStopArcInstance[] = "StopArcInstance";
const char kSessionManagerSetArcCpuRestriction[] = "SetArcCpuRestriction";
const char kSessionManagerEmitArcBooted[] = "EmitArcBooted";
const char kSessionManagerGetArcStartTimeTicks[] = "GetArcStartTimeTicks";
const char kSessionManagerRemoveArcData[] = "RemoveArcData";
const char kSessionManagerStartContainer[] = "StartContainer";
const char kSessionManagerStopContainer[] = "StopContainer";
// Signals
const char kLoginPromptVisibleSignal[] = "LoginPromptVisible";
const char kSessionStateChangedSignal[] = "SessionStateChanged";
// ScreenLock signals.
const char kScreenIsLockedSignal[] = "ScreenIsLocked";
const char kScreenIsUnlockedSignal[] = "ScreenIsUnlocked";
// Ownership API signals.
const char kOwnerKeySetSignal[] = "SetOwnerKeyComplete";
const char kPropertyChangeCompleteSignal[] = "PropertyChangeComplete";
// ARC instance signals.
const char kArcInstanceStopped[] = "ArcInstanceStopped";
const char kArcInstanceRebooted[] = "ArcInstanceRebooted";

// Values
enum ContainerCpuRestrictionState {
  CONTAINER_CPU_RESTRICTION_FOREGROUND = 0,
  CONTAINER_CPU_RESTRICTION_BACKGROUND = 1,
  NUM_CONTAINER_CPU_RESTRICTION_STATES = 2,
};

}  // namespace login_manager

#endif  // SYSTEM_API_DBUS_LOGIN_MANAGER_DBUS_CONSTANTS_H_
