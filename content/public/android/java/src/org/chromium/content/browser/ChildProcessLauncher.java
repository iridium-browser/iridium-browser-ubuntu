// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.os.RemoteException;
import android.util.Pair;
import android.view.Surface;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.Linker;
import org.chromium.content.app.ChildProcessService;
import org.chromium.content.app.ChromiumLinkerParams;
import org.chromium.content.app.PrivilegedProcessService;
import org.chromium.content.app.SandboxedProcessService;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.SurfaceWrapper;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;

/**
 * This class provides the method to start/stop ChildProcess called by native.
 */
@JNINamespace("content")
public class ChildProcessLauncher {
    private static final String TAG = "ChildProcessLauncher";

    static final int CALLBACK_FOR_UNKNOWN_PROCESS = 0;
    static final int CALLBACK_FOR_GPU_PROCESS = 1;
    static final int CALLBACK_FOR_RENDERER_PROCESS = 2;
    static final int CALLBACK_FOR_UTILITY_PROCESS = 3;

    private static final String SWITCH_PROCESS_TYPE = "type";
    private static final String SWITCH_RENDERER_PROCESS = "renderer";
    private static final String SWITCH_UTILITY_PROCESS = "utility";
    private static final String SWITCH_GPU_PROCESS = "gpu-process";

    private static class ChildConnectionAllocator {
        // Connections to services. Indices of the array correspond to the service numbers.
        private final ChildProcessConnection[] mChildProcessConnections;

        // The list of free (not bound) service indices.
        // SHOULD BE ACCESSED WITH mConnectionLock.
        private final ArrayList<Integer> mFreeConnectionIndices;
        private final Object mConnectionLock = new Object();

        private Class<? extends ChildProcessService> mChildClass;
        private final boolean mInSandbox;

        public ChildConnectionAllocator(boolean inSandbox, int numChildServices) {
            mChildProcessConnections = new ChildProcessConnectionImpl[numChildServices];
            mFreeConnectionIndices = new ArrayList<Integer>(numChildServices);
            for (int i = 0; i < numChildServices; i++) {
                mFreeConnectionIndices.add(i);
            }
            mChildClass =
                inSandbox ? SandboxedProcessService.class : PrivilegedProcessService.class;
            mInSandbox = inSandbox;
        }

        public ChildProcessConnection allocate(
                Context context, ChildProcessConnection.DeathCallback deathCallback,
                ChromiumLinkerParams chromiumLinkerParams,
                boolean alwaysInForeground) {
            synchronized (mConnectionLock) {
                if (mFreeConnectionIndices.isEmpty()) {
                    Log.d(TAG, "Ran out of services to allocate.");
                    return null;
                }
                int slot = mFreeConnectionIndices.remove(0);
                assert mChildProcessConnections[slot] == null;
                mChildProcessConnections[slot] = new ChildProcessConnectionImpl(context, slot,
                        mInSandbox, deathCallback, mChildClass, chromiumLinkerParams,
                        alwaysInForeground);
                Log.d(TAG, "Allocator allocated a connection, sandbox: " + mInSandbox
                        + ", slot: " + slot);
                return mChildProcessConnections[slot];
            }
        }

        public void free(ChildProcessConnection connection) {
            synchronized (mConnectionLock) {
                int slot = connection.getServiceNumber();
                if (mChildProcessConnections[slot] != connection) {
                    int occupier = mChildProcessConnections[slot] == null
                            ? -1 : mChildProcessConnections[slot].getServiceNumber();
                    Log.e(TAG, "Unable to find connection to free in slot: " + slot
                            + " already occupied by service: " + occupier);
                    assert false;
                } else {
                    mChildProcessConnections[slot] = null;
                    assert !mFreeConnectionIndices.contains(slot);
                    mFreeConnectionIndices.add(slot);
                    Log.d(TAG, "Allocator freed a connection, sandbox: " + mInSandbox
                            + ", slot: " + slot);
                }
            }
        }

        /** @return the count of connections managed by the allocator */
        @VisibleForTesting
        int allocatedConnectionsCountForTesting() {
            return mChildProcessConnections.length - mFreeConnectionIndices.size();
        }
    }

    private static class PendingSpawnData {
        private final Context mContext;
        private final String[] mCommandLine;
        private final int mChildProcessId;
        private final FileDescriptorInfo[] mFilesToBeMapped;
        private final long mClientContext;
        private final int mCallbackType;
        private final boolean mInSandbox;

        private PendingSpawnData(
                Context context,
                String[] commandLine,
                int childProcessId,
                FileDescriptorInfo[] filesToBeMapped,
                long clientContext,
                int callbackType,
                boolean inSandbox) {
            mContext = context;
            mCommandLine = commandLine;
            mChildProcessId = childProcessId;
            mFilesToBeMapped = filesToBeMapped;
            mClientContext = clientContext;
            mCallbackType = callbackType;
            mInSandbox = inSandbox;
        }

        private Context context() {
            return mContext;
        }
        private String[] commandLine() {
            return mCommandLine;
        }
        private int childProcessId() {
            return mChildProcessId;
        }
        private FileDescriptorInfo[] filesToBeMapped() {
            return mFilesToBeMapped;
        }
        private long clientContext() {
            return mClientContext;
        }
        private int callbackType() {
            return mCallbackType;
        }
        private boolean inSandbox() {
            return mInSandbox;
        }
    }

    private static class PendingSpawnQueue {
        // The list of pending process spawn requests and its lock.
        private static Queue<PendingSpawnData> sPendingSpawns =
                new LinkedList<PendingSpawnData>();
        static final Object sPendingSpawnsLock = new Object();

        /**
         * Queue up a spawn requests to be processed once a free service is available.
         * Called when a spawn is requested while we are at the capacity.
         */
        public void enqueue(final PendingSpawnData pendingSpawn) {
            synchronized (sPendingSpawnsLock) {
                sPendingSpawns.add(pendingSpawn);
            }
        }

        /**
         * Pop the next request from the queue. Called when a free service is available.
         * @return the next spawn request waiting in the queue.
         */
        public PendingSpawnData dequeue() {
            synchronized (sPendingSpawnsLock) {
                return sPendingSpawns.poll();
            }
        }

        /** @return the count of pending spawns in the queue */
        public int size() {
            synchronized (sPendingSpawnsLock) {
                return sPendingSpawns.size();
            }
        }
    }

    private static final PendingSpawnQueue sPendingSpawnQueue = new PendingSpawnQueue();

    // Service class for child process. As the default value it uses SandboxedProcessService0 and
    // PrivilegedProcessService0.
    private static ChildConnectionAllocator sSandboxedChildConnectionAllocator;
    private static ChildConnectionAllocator sPrivilegedChildConnectionAllocator;

    private static final String NUM_SANDBOXED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_SANDBOXED_SERVICES";
    private static final String NUM_PRIVILEGED_SERVICES_KEY =
            "org.chromium.content.browser.NUM_PRIVILEGED_SERVICES";

    private static int getNumberOfServices(Context context, boolean inSandbox) {
        try {
            PackageManager packageManager = context.getPackageManager();
            ApplicationInfo appInfo = packageManager.getApplicationInfo(context.getPackageName(),
                    PackageManager.GET_META_DATA);
            int numServices = appInfo.metaData.getInt(inSandbox ? NUM_SANDBOXED_SERVICES_KEY
                    : NUM_PRIVILEGED_SERVICES_KEY);
            if (numServices <= 0) {
                throw new RuntimeException("Illegal meta data value for number of child services");
            }
            return numServices;
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException("Could not get application info");
        }
    }

    private static void initConnectionAllocatorsIfNecessary(Context context) {
        synchronized (ChildProcessLauncher.class) {
            if (sSandboxedChildConnectionAllocator == null) {
                sSandboxedChildConnectionAllocator =
                        new ChildConnectionAllocator(true, getNumberOfServices(context, true));
            }
            if (sPrivilegedChildConnectionAllocator == null) {
                sPrivilegedChildConnectionAllocator =
                        new ChildConnectionAllocator(false, getNumberOfServices(context, false));
            }
        }
    }

    private static ChildConnectionAllocator getConnectionAllocator(boolean inSandbox) {
        return inSandbox
                ? sSandboxedChildConnectionAllocator : sPrivilegedChildConnectionAllocator;
    }

    private static ChildProcessConnection allocateConnection(Context context, boolean inSandbox,
            ChromiumLinkerParams chromiumLinkerParams, boolean alwaysInForeground) {
        ChildProcessConnection.DeathCallback deathCallback =
                new ChildProcessConnection.DeathCallback() {
                    @Override
                    public void onChildProcessDied(ChildProcessConnection connection) {
                        if (connection.getPid() != 0) {
                            stop(connection.getPid());
                        } else {
                            freeConnection(connection);
                        }
                    }
                };
        initConnectionAllocatorsIfNecessary(context);
        return getConnectionAllocator(inSandbox).allocate(context, deathCallback,
                chromiumLinkerParams, alwaysInForeground);
    }

    private static boolean sLinkerInitialized = false;
    private static long sLinkerLoadAddress = 0;

    private static ChromiumLinkerParams getLinkerParamsForNewConnection() {
        if (!sLinkerInitialized) {
            if (Linker.isUsed()) {
                sLinkerLoadAddress = Linker.getBaseLoadAddress();
                if (sLinkerLoadAddress == 0) {
                    Log.i(TAG, "Shared RELRO support disabled!");
                }
            }
            sLinkerInitialized = true;
        }

        if (sLinkerLoadAddress == 0) return null;

        // Always wait for the shared RELROs in service processes.
        final boolean waitForSharedRelros = true;
        return new ChromiumLinkerParams(sLinkerLoadAddress,
                                waitForSharedRelros,
                                Linker.getTestRunnerClassName());
    }

    private static ChildProcessConnection allocateBoundConnection(Context context,
            String[] commandLine, boolean inSandbox, boolean alwaysInForeground) {
        ChromiumLinkerParams chromiumLinkerParams = getLinkerParamsForNewConnection();
        ChildProcessConnection connection =
                allocateConnection(context, inSandbox, chromiumLinkerParams, alwaysInForeground);
        if (connection != null) {
            connection.start(commandLine);
        }
        return connection;
    }

    private static final long FREE_CONNECTION_DELAY_MILLIS = 1;

    private static void freeConnection(ChildProcessConnection connection) {
        // Freeing a service should be delayed. This is so that we avoid immediately reusing the
        // freed service (see http://crbug.com/164069): the framework might keep a service process
        // alive when it's been unbound for a short time. If a new connection to the same service
        // is bound at that point, the process is reused and bad things happen (mostly static
        // variables are set when we don't expect them to).
        final ChildProcessConnection conn = connection;
        ThreadUtils.postOnUiThreadDelayed(new Runnable() {
            @Override
            public void run() {
                getConnectionAllocator(conn.isInSandbox()).free(conn);

                final PendingSpawnData pendingSpawn = sPendingSpawnQueue.dequeue();
                if (pendingSpawn != null) {
                    new Thread(new Runnable() {
                        @Override
                        public void run() {
                            startInternal(pendingSpawn.context(), pendingSpawn.commandLine(),
                                    pendingSpawn.childProcessId(), pendingSpawn.filesToBeMapped(),
                                    pendingSpawn.clientContext(), pendingSpawn.callbackType(),
                                    pendingSpawn.inSandbox());
                        }
                    }).start();
                }
            }
        }, FREE_CONNECTION_DELAY_MILLIS);
    }

    // Represents an invalid process handle; same as base/process/process.h kNullProcessHandle.
    private static final int NULL_PROCESS_HANDLE = 0;

    // Map from pid to ChildService connection.
    private static Map<Integer, ChildProcessConnection> sServiceMap =
            new ConcurrentHashMap<Integer, ChildProcessConnection>();

    // A pre-allocated and pre-bound connection ready for connection setup, or null.
    private static ChildProcessConnection sSpareSandboxedConnection = null;

    // Manages oom bindings used to bind chind services.
    private static BindingManager sBindingManager = BindingManagerImpl.createBindingManager();

    // Map from surface id to Surface.
    private static Map<Integer, Surface> sViewSurfaceMap =
            new ConcurrentHashMap<Integer, Surface>();

    // Map from surface texture id to Surface.
    private static Map<Pair<Integer, Integer>, Surface> sSurfaceTextureSurfaceMap =
            new ConcurrentHashMap<Pair<Integer, Integer>, Surface>();

    // Whether the main application is currently brought to the foreground.
    private static boolean sApplicationInForeground = true;

    @VisibleForTesting
    public static void setBindingManagerForTesting(BindingManager manager) {
        sBindingManager = manager;
    }

    /** @return true iff the child process is protected from out-of-memory killing */
    @CalledByNative
    private static boolean isOomProtected(int pid) {
        return sBindingManager.isOomProtected(pid);
    }

    @CalledByNative
    private static void registerViewSurface(int surfaceId, Surface surface) {
        sViewSurfaceMap.put(surfaceId, surface);
    }

    @CalledByNative
    private static void unregisterViewSurface(int surfaceId) {
        sViewSurfaceMap.remove(surfaceId);
    }

    private static void registerSurfaceTextureSurface(
            int surfaceTextureId, int clientId, Surface surface) {
        Pair<Integer, Integer> key = new Pair<Integer, Integer>(surfaceTextureId, clientId);
        sSurfaceTextureSurfaceMap.put(key, surface);
    }

    private static void unregisterSurfaceTextureSurface(int surfaceTextureId, int clientId) {
        Pair<Integer, Integer> key = new Pair<Integer, Integer>(surfaceTextureId, clientId);
        Surface surface = sSurfaceTextureSurfaceMap.remove(key);
        if (surface == null) return;

        assert surface.isValid();
        surface.release();
    }

    @CalledByNative
    private static void createSurfaceTextureSurface(
            int surfaceTextureId, int clientId, SurfaceTexture surfaceTexture) {
        registerSurfaceTextureSurface(surfaceTextureId, clientId, new Surface(surfaceTexture));
    }

    @CalledByNative
    private static void destroySurfaceTextureSurface(int surfaceTextureId, int clientId) {
        unregisterSurfaceTextureSurface(surfaceTextureId, clientId);
    }

    @CalledByNative
    private static SurfaceWrapper getSurfaceTextureSurface(
            int surfaceTextureId, int clientId) {
        Pair<Integer, Integer> key = new Pair<Integer, Integer>(surfaceTextureId, clientId);

        Surface surface = sSurfaceTextureSurfaceMap.get(key);
        if (surface == null) {
            Log.e(TAG, "Invalid Id for surface texture.");
            return null;
        }
        assert surface.isValid();
        return new SurfaceWrapper(surface);
    }

    /**
     * Sets the visibility of the child process when it changes or when it is determined for the
     * first time.
     */
    @CalledByNative
    public static void setInForeground(int pid, boolean inForeground) {
        sBindingManager.setInForeground(pid, inForeground);
    }

    /**
     * Called when the renderer commits a navigation. This signals a time at which it is safe to
     * rely on renderer visibility signalled through setInForeground. See http://crbug.com/421041.
     */
    public static void determinedVisibility(int pid) {
        sBindingManager.determinedVisibility(pid);
    }

    /**
     * Called when the embedding application is sent to background.
     */
    public static void onSentToBackground() {
        sApplicationInForeground = false;
        sBindingManager.onSentToBackground();
    }

    /**
     * Called when the embedding application is brought to foreground.
     */
    public static void onBroughtToForeground() {
        sApplicationInForeground = true;
        sBindingManager.onBroughtToForeground();
    }

    /**
     * Returns whether the application is currently in the foreground.
     */
    static boolean isApplicationInForeground() {
        return sApplicationInForeground;
    }

    /**
     * Should be called early in startup so the work needed to spawn the child process can be done
     * in parallel to other startup work. Must not be called on the UI thread. Spare connection is
     * created in sandboxed child process.
     * @param context the application context used for the connection.
     */
    public static void warmUp(Context context) {
        synchronized (ChildProcessLauncher.class) {
            assert !ThreadUtils.runningOnUiThread();
            if (sSpareSandboxedConnection == null) {
                sSpareSandboxedConnection = allocateBoundConnection(context, null, true, false);
            }
        }
    }

    private static String getSwitchValue(final String[] commandLine, String switchKey) {
        if (commandLine == null || switchKey == null) {
            return null;
        }
        // This format should be matched with the one defined in command_line.h.
        final String switchKeyPrefix = "--" + switchKey + "=";
        for (String command : commandLine) {
            if (command != null && command.startsWith(switchKeyPrefix)) {
                return command.substring(switchKeyPrefix.length());
            }
        }
        return null;
    }

    /**
     * Spawns and connects to a child process. May be called on any thread. It will not block, but
     * will instead callback to {@link #nativeOnChildProcessStarted} when the connection is
     * established. Note this callback will not necessarily be from the same thread (currently it
     * always comes from the main thread).
     *
     * @param context Context used to obtain the application context.
     * @param commandLine The child process command line argv.
     * @param fileIds The ID that should be used when mapping files in the created process.
     * @param fileFds The file descriptors that should be mapped in the created process.
     * @param fileAutoClose Whether the file descriptors should be closed once they were passed to
     * the created process.
     * @param clientContext Arbitrary parameter used by the client to distinguish this connection.
     */
    @CalledByNative
    static void start(
            Context context,
            final String[] commandLine,
            int childProcessId,
            int[] fileIds,
            int[] fileFds,
            boolean[] fileAutoClose,
            long clientContext) {
        assert fileIds.length == fileFds.length && fileFds.length == fileAutoClose.length;
        FileDescriptorInfo[] filesToBeMapped = new FileDescriptorInfo[fileFds.length];
        for (int i = 0; i < fileFds.length; i++) {
            filesToBeMapped[i] =
                    new FileDescriptorInfo(fileIds[i], fileFds[i], fileAutoClose[i]);
        }
        assert clientContext != 0;

        int callbackType = CALLBACK_FOR_UNKNOWN_PROCESS;
        boolean inSandbox = true;
        String processType = getSwitchValue(commandLine, SWITCH_PROCESS_TYPE);
        if (SWITCH_RENDERER_PROCESS.equals(processType)) {
            callbackType = CALLBACK_FOR_RENDERER_PROCESS;
        } else if (SWITCH_GPU_PROCESS.equals(processType)) {
            callbackType = CALLBACK_FOR_GPU_PROCESS;
            inSandbox = false;
        } else if (SWITCH_UTILITY_PROCESS.equals(processType)) {
            // We only support sandboxed right now.
            callbackType = CALLBACK_FOR_UTILITY_PROCESS;
        } else {
            assert false;
        }

        startInternal(context, commandLine, childProcessId, filesToBeMapped, clientContext,
                callbackType, inSandbox);
    }

    private static void startInternal(
            Context context,
            final String[] commandLine,
            int childProcessId,
            FileDescriptorInfo[] filesToBeMapped,
            long clientContext,
            int callbackType,
            boolean inSandbox) {
        try {
            TraceEvent.begin("ChildProcessLauncher.startInternal");

            ChildProcessConnection allocatedConnection = null;
            synchronized (ChildProcessLauncher.class) {
                if (inSandbox) {
                    allocatedConnection = sSpareSandboxedConnection;
                    sSpareSandboxedConnection = null;
                }
            }
            if (allocatedConnection == null) {
                boolean alwaysInForeground = false;
                if (callbackType == CALLBACK_FOR_GPU_PROCESS) alwaysInForeground = true;
                allocatedConnection = allocateBoundConnection(
                        context, commandLine, inSandbox, alwaysInForeground);
                if (allocatedConnection == null) {
                    Log.d(TAG, "Allocation of new service failed. Queuing up pending spawn.");
                    sPendingSpawnQueue.enqueue(new PendingSpawnData(context, commandLine,
                            childProcessId, filesToBeMapped, clientContext, callbackType,
                            inSandbox));
                    return;
                }
            }

            Log.d(TAG, "Setting up connection to process: slot="
                    + allocatedConnection.getServiceNumber());
            triggerConnectionSetup(allocatedConnection, commandLine, childProcessId,
                    filesToBeMapped, callbackType, clientContext);
        } finally {
            TraceEvent.end("ChildProcessLauncher.startInternal");
        }
    }

    @VisibleForTesting
    static void triggerConnectionSetup(
            final ChildProcessConnection connection,
            String[] commandLine,
            int childProcessId,
            FileDescriptorInfo[] filesToBeMapped,
            final int callbackType,
            final long clientContext) {
        ChildProcessConnection.ConnectionCallback connectionCallback =
                new ChildProcessConnection.ConnectionCallback() {
                    @Override
                    public void onConnected(int pid) {
                        Log.d(TAG, "on connect callback, pid=" + pid + " context=" + clientContext
                                + " callbackType=" + callbackType);
                        if (pid != NULL_PROCESS_HANDLE) {
                            sBindingManager.addNewConnection(pid, connection);
                            sServiceMap.put(pid, connection);
                        }
                        // If the connection fails and pid == 0, the Java-side cleanup was already
                        // handled by DeathCallback. We still have to call back to native for
                        // cleanup there.
                        if (clientContext != 0) {  // Will be 0 in Java instrumentation tests.
                            nativeOnChildProcessStarted(clientContext, pid);
                        }
                    }
                };

        assert callbackType != CALLBACK_FOR_UNKNOWN_PROCESS;
        connection.setupConnection(commandLine,
                                   filesToBeMapped,
                                   createCallback(childProcessId, callbackType),
                                   connectionCallback,
                                   Linker.getSharedRelros());
    }

    /**
     * Terminates a child process. This may be called from any thread.
     *
     * @param pid The pid (process handle) of the service connection obtained from {@link #start}.
     */
    @CalledByNative
    static void stop(int pid) {
        Log.d(TAG, "stopping child connection: pid=" + pid);
        ChildProcessConnection connection = sServiceMap.remove(pid);
        if (connection == null) {
            logPidWarning(pid, "Tried to stop non-existent connection");
            return;
        }
        sBindingManager.clearConnection(pid);
        connection.stop();
        freeConnection(connection);
    }

    /**
     * This implementation is used to receive callbacks from the remote service.
     */
    private static IChildProcessCallback createCallback(
            final int childProcessId, final int callbackType) {
        return new IChildProcessCallback.Stub() {
            /**
             * This is called by the remote service regularly to tell us about new values. Note that
             * IPC calls are dispatched through a thread pool running in each process, so the code
             * executing here will NOT be running in our main thread -- so, to update the UI, we
             * need to use a Handler.
             */
            @Override
            public void establishSurfacePeer(
                    int pid, Surface surface, int primaryID, int secondaryID) {
                // Do not allow a malicious renderer to connect to a producer. This is only used
                // from stream textures managed by the GPU process.
                if (callbackType != CALLBACK_FOR_GPU_PROCESS) {
                    Log.e(TAG, "Illegal callback for non-GPU process.");
                    return;
                }

                nativeEstablishSurfacePeer(pid, surface, primaryID, secondaryID);
            }

            @Override
            public SurfaceWrapper getViewSurface(int surfaceId) {
                // Do not allow a malicious renderer to get to our view surface.
                if (callbackType != CALLBACK_FOR_GPU_PROCESS) {
                    Log.e(TAG, "Illegal callback for non-GPU process.");
                    return null;
                }

                Surface surface = sViewSurfaceMap.get(surfaceId);
                if (surface == null) {
                    Log.e(TAG, "Invalid surfaceId.");
                    return null;
                }
                assert surface.isValid();
                return new SurfaceWrapper(surface);
            }

            @Override
            public void registerSurfaceTextureSurface(
                    int surfaceTextureId, int clientId, Surface surface) {
                if (callbackType != CALLBACK_FOR_GPU_PROCESS) {
                    Log.e(TAG, "Illegal callback for non-GPU process.");
                    return;
                }

                ChildProcessLauncher.registerSurfaceTextureSurface(surfaceTextureId, clientId,
                        surface);
            }

            @Override
            public void unregisterSurfaceTextureSurface(
                    int surfaceTextureId, int clientId) {
                if (callbackType != CALLBACK_FOR_GPU_PROCESS) {
                    Log.e(TAG, "Illegal callback for non-GPU process.");
                    return;
                }

                ChildProcessLauncher.unregisterSurfaceTextureSurface(surfaceTextureId, clientId);
            }

            @Override
            public SurfaceWrapper getSurfaceTextureSurface(int surfaceTextureId) {
                if (callbackType != CALLBACK_FOR_RENDERER_PROCESS) {
                    Log.e(TAG, "Illegal callback for non-renderer process.");
                    return null;
                }

                return ChildProcessLauncher.getSurfaceTextureSurface(surfaceTextureId,
                        childProcessId);
            }
        };
    }

    static void logPidWarning(int pid, String message) {
        // This class is effectively a no-op in single process mode, so don't log warnings there.
        if (pid > 0 && !nativeIsSingleProcess()) {
            Log.w(TAG, message + ", pid=" + pid);
        }
    }

    @VisibleForTesting
    static ChildProcessConnection allocateBoundConnectionForTesting(Context context) {
        return allocateBoundConnection(context, null, true, false);
    }

    /**
     * Queue up a spawn requests for testing.
     */
    @VisibleForTesting
    static void enqueuePendingSpawnForTesting(Context context) {
        sPendingSpawnQueue.enqueue(new PendingSpawnData(context, new String[0], 1,
                new FileDescriptorInfo[0], 0, CALLBACK_FOR_RENDERER_PROCESS, true));
    }

    /** @return the count of sandboxed connections managed by the allocator */
    @VisibleForTesting
    static int allocatedConnectionsCountForTesting(Context context) {
        initConnectionAllocatorsIfNecessary(context);
        return sSandboxedChildConnectionAllocator.allocatedConnectionsCountForTesting();
    }

    /** @return the count of services set up and working */
    @VisibleForTesting
    static int connectedServicesCountForTesting() {
        return sServiceMap.size();
    }

    /** @return the count of pending spawns in the queue */
    @VisibleForTesting
    static int pendingSpawnsCountForTesting() {
        return sPendingSpawnQueue.size();
    }

    /**
     * Kills the child process for testing.
     * @return true iff the process was killed as expected
     */
    @VisibleForTesting
    public static boolean crashProcessForTesting(int pid) {
        if (sServiceMap.get(pid) == null) return false;

        try {
            ((ChildProcessConnectionImpl) sServiceMap.get(pid)).crashServiceForTesting();
        } catch (RemoteException ex) {
            return false;
        }

        return true;
    }

    private static native void nativeOnChildProcessStarted(long clientContext, int pid);
    private static native void nativeEstablishSurfacePeer(
            int pid, Surface surface, int primaryID, int secondaryID);
    private static native boolean nativeIsSingleProcess();
}
