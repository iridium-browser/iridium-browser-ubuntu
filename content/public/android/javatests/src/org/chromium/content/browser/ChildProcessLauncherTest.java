// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.RemoteException;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.MediumTest;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;

/**
 * Instrumentation tests for ChildProcessLauncher.
 */
public class ChildProcessLauncherTest extends InstrumentationTestCase {
    /**
     *  Tests cleanup for a connection that fails to connect in the first place.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceFailedToBind() throws InterruptedException, RemoteException {
        final Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Try to allocate a connection to service class in incorrect package. We can do that by
        // using the instrumentation context (getContext()) instead of the app context
        // (getTargetContext()).
        Context context = getInstrumentation().getContext();
        ChildProcessLauncher.allocateBoundConnectionForTesting(context);

        // Verify that the connection is not considered as allocated.
        assertTrue("Failed connection wasn't released from the allocator.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.allocatedConnectionsCountForTesting(
                                appContext) == 0;
                    }
                }));

        assertTrue("Failed connection wasn't released from ChildProcessLauncher.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 0;
                    }
                }));
    }

    /**
     * Tests cleanup for a connection that terminates before setup.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedBeforeSetup() throws InterruptedException, RemoteException {
        final Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));

        // Verify that the service is not yet set up.
        assertEquals(0, connection.getPid());
        assertEquals(0, ChildProcessLauncher.connectedServicesCountForTesting());

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

        // Verify that the connection gets cleaned-up.
        assertTrue("Crashed connection wasn't released from the allocator.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.allocatedConnectionsCountForTesting(
                                appContext) == 0;
                    }
                }));

        assertTrue("Crashed connection wasn't released from ChildProcessLauncher.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 0;
                    }
                }));
    }

    /**
     * Tests cleanup for a connection that terminates after setup.
     */
    @MediumTest
    @Feature({"ProcessManagement"})
    public void testServiceCrashedAfterSetup() throws InterruptedException, RemoteException {
        final Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));

        // Initiate the connection setup.
        ChildProcessLauncher.triggerConnectionSetup(connection, new String[0], 1,
                new FileDescriptorInfo[0], ChildProcessLauncher.CALLBACK_FOR_RENDERER_PROCESS, 0);

        // Verify that the connection completes the setup.
        assertTrue("The connection wasn't registered in ChildProcessLauncher after setup.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 1;
                    }
                }));

        assertTrue("The connection failed to get a pid in setup.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return connection.getPid() != 0;
                    }
                }));

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

        // Verify that the connection gets cleaned-up.
        assertTrue("Crashed connection wasn't released from the allocator.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.allocatedConnectionsCountForTesting(
                                appContext) == 0;
                    }
                }));

        assertTrue("Crashed connection wasn't released from ChildProcessLauncher.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 0;
                    }
                }));

        // Verify that the connection pid remains set after termination.
        assertTrue(connection.getPid() != 0);
    }

    /**
     * Tests spawning a pending process from queue.
     */
    /*
    @MediumTest
    @Feature({"ProcessManagement"})
    crbug.com/483089
    */
    @DisabledTest
    public void testPendingSpawnQueue() throws InterruptedException, RemoteException {
        final Context appContext = getInstrumentation().getTargetContext();
        assertEquals(0, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));

        // Start and connect to a new service.
        final ChildProcessConnectionImpl connection = startConnection();
        assertEquals(1, ChildProcessLauncher.allocatedConnectionsCountForTesting(appContext));

        // Queue up a a new spawn request.
        ChildProcessLauncher.enqueuePendingSpawnForTesting(appContext);
        assertEquals(1, ChildProcessLauncher.pendingSpawnsCountForTesting());

        // Initiate the connection setup.
        ChildProcessLauncher.triggerConnectionSetup(connection, new String[0], 1,
                new FileDescriptorInfo[0], ChildProcessLauncher.CALLBACK_FOR_RENDERER_PROCESS, 0);

        // Verify that the connection completes the setup.
        assertTrue("The connection wasn't registered in ChildProcessLauncher after setup.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 1;
                    }
                }));

        assertTrue("The connection failed to get a pid in setup.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return connection.getPid() != 0;
                    }
                }));

        // Crash the service.
        assertTrue(connection.crashServiceForTesting());

        // Verify that a new service is started for the pending spawn.
        assertTrue("Failed to spawn from queue.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.pendingSpawnsCountForTesting() == 0;
                    }
                }));

        assertTrue("The connection wasn't allocated for the pending spawn.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.allocatedConnectionsCountForTesting(
                                appContext) == 1;
                    }
                }));

        // Verify that the connection completes the setup for the pending spawn.
        assertTrue("The connection wasn't registered in ChildProcessLauncher after setup.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return ChildProcessLauncher.connectedServicesCountForTesting() == 1;
                    }
                }));
    }

    private ChildProcessConnectionImpl startConnection() throws InterruptedException {
        // Allocate a new connection.
        Context context = getInstrumentation().getTargetContext();
        final ChildProcessConnectionImpl connection = (ChildProcessConnectionImpl)
                ChildProcessLauncher.allocateBoundConnectionForTesting(context);

        // Wait for the service to connect.
        assertTrue("The connection wasn't established.",
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return connection.isConnected();
                    }
                }));
        return connection;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        LibraryLoader.get(LibraryProcessType.PROCESS_CHILD)
                .ensureInitialized(getInstrumentation().getContext());
    }
}
