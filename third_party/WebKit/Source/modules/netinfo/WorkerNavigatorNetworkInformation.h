// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerNavigatorNetworkInformation_h
#define WorkerNavigatorNetworkInformation_h

#include "platform/Supplementable.h"

namespace blink {

class ExecutionContext;
class NetworkInformation;
class WorkerNavigator;

class WorkerNavigatorNetworkInformation final
    : public NoBaseWillBeGarbageCollectedFinalized<WorkerNavigatorNetworkInformation>
    , public WillBeHeapSupplement<WorkerNavigator> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WorkerNavigatorNetworkInformation);

public:
    virtual ~WorkerNavigatorNetworkInformation();
    static WorkerNavigatorNetworkInformation& from(WorkerNavigator&, ExecutionContext*);
    static WorkerNavigatorNetworkInformation* toWorkerNavigatorNetworkInformation(WorkerNavigator&, ExecutionContext*);
    static const char* supplementName();

    static NetworkInformation* connection(ExecutionContext*, WorkerNavigator&);

    DECLARE_VIRTUAL_TRACE();

private:
    WorkerNavigatorNetworkInformation(WorkerNavigator&, ExecutionContext*);
    NetworkInformation* connection(ExecutionContext*);

    PersistentWillBeMember<NetworkInformation> m_connection;
};

} // namespace blink

#endif // WorkerNavigatorNetworkInformation_h
