// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CalledByNative;
import org.chromium.base.CommandLine;
import org.chromium.base.JNINamespace;
import org.chromium.base.annotations.SuppressFBWarnings;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.content.browser.ChildProcessConnection;
import org.chromium.content.browser.ChildProcessLauncher;
import org.chromium.content.common.IChildProcessCallback;
import org.chromium.content.common.IChildProcessService;

import java.util.ArrayList;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicReference;

/**
 * This is the base class for child services; the [Non]SandboxedProcessService0, 1.. etc
 * subclasses provide the concrete service entry points, to enable the browser to connect
 * to more than one distinct process (i.e. one process per service number, up to limit of N).
 * The embedding application must declare these service instances in the application section
 * of its AndroidManifest.xml, for example with N entries of the form:-
 *     <service android:name="org.chromium.content.app.[Non]SandboxedProcessServiceX"
 *              android:process=":[non]sandboxed_processX" />
 * for X in 0...N-1 (where N is {@link ChildProcessLauncher#MAX_REGISTERED_SERVICES})
 */
@JNINamespace("content")
public class ChildProcessService extends Service {
    private static final String MAIN_THREAD_NAME = "ChildProcessMain";
    private static final String TAG = "ChildProcessService";
    private IChildProcessCallback mCallback;

    // This is the native "Main" thread for the renderer / utility process.
    private Thread mMainThread;
    // Parameters received via IPC, only accessed while holding the mMainThread monitor.
    private String[] mCommandLineParams;
    private int mCpuCount;
    private long mCpuFeatures;
    // Pairs IDs and file descriptors that should be registered natively.
    private ArrayList<Integer> mFileIds;
    private ArrayList<ParcelFileDescriptor> mFileFds;
    // Linker-specific parameters for this child process service.
    private ChromiumLinkerParams mLinkerParams;

    private static AtomicReference<Context> sContext = new AtomicReference<Context>(null);
    private boolean mLibraryInitialized = false;
    // Becomes true once the service is bound. Access must synchronize around mMainThread.
    private boolean mIsBound = false;

    private final Semaphore mActivitySemaphore = new Semaphore(1);

    // Binder object used by clients for this service.
    private final IChildProcessService.Stub mBinder = new IChildProcessService.Stub() {
        // NOTE: Implement any IChildProcessService methods here.
        @Override
        public int setupConnection(Bundle args, IChildProcessCallback callback) {
            mCallback = callback;
            synchronized (mMainThread) {
                // Allow the command line to be set via bind() intent or setupConnection, but
                // the FD can only be transferred here.
                if (mCommandLineParams == null) {
                    mCommandLineParams = args.getStringArray(
                            ChildProcessConnection.EXTRA_COMMAND_LINE);
                }
                // We must have received the command line by now
                assert mCommandLineParams != null;
                mCpuCount = args.getInt(ChildProcessConnection.EXTRA_CPU_COUNT);
                mCpuFeatures = args.getLong(ChildProcessConnection.EXTRA_CPU_FEATURES);
                assert mCpuCount > 0;
                mFileIds = new ArrayList<Integer>();
                mFileFds = new ArrayList<ParcelFileDescriptor>();
                for (int i = 0;; i++) {
                    String fdName = ChildProcessConnection.EXTRA_FILES_PREFIX + i
                            + ChildProcessConnection.EXTRA_FILES_FD_SUFFIX;
                    ParcelFileDescriptor parcel = args.getParcelable(fdName);
                    if (parcel == null) {
                        // End of the file list.
                        break;
                    }
                    mFileFds.add(parcel);
                    String idName = ChildProcessConnection.EXTRA_FILES_PREFIX + i
                            + ChildProcessConnection.EXTRA_FILES_ID_SUFFIX;
                    mFileIds.add(args.getInt(idName));
                }
                Bundle sharedRelros = args.getBundle(Linker.EXTRA_LINKER_SHARED_RELROS);
                if (sharedRelros != null) {
                    Linker.useSharedRelros(sharedRelros);
                    sharedRelros = null;
                }
                mMainThread.notifyAll();
            }
            return Process.myPid();
        }

        @Override
        public void crashIntentionallyForTesting() {
            Process.killProcess(Process.myPid());
        }
    };

    /* package */ static Context getContext() {
        return sContext.get();
    }

    @Override
    public void onCreate() {
        Log.i(TAG, "Creating new ChildProcessService pid=" + Process.myPid());
        if (sContext.get() != null) {
            Log.e(TAG, "ChildProcessService created again in process!");
        }
        sContext.set(this);
        super.onCreate();

        mMainThread = new Thread(new Runnable() {
            @Override
            @SuppressFBWarnings("DM_EXIT")
            public void run()  {
                try {
                    // CommandLine must be initialized before others, e.g., Linker.isUsed()
                    // may check the command line options.
                    synchronized (mMainThread) {
                        while (mCommandLineParams == null) {
                            mMainThread.wait();
                        }
                    }
                    CommandLine.init(mCommandLineParams);
                    boolean useLinker = Linker.isUsed();
                    boolean requestedSharedRelro = false;
                    if (useLinker) {
                        synchronized (mMainThread) {
                            while (!mIsBound) {
                                mMainThread.wait();
                            }
                        }
                        assert mLinkerParams != null;
                        if (mLinkerParams.mWaitForSharedRelro) {
                            requestedSharedRelro = true;
                            Linker.initServiceProcess(mLinkerParams.mBaseLoadAddress);
                        } else {
                            Linker.disableSharedRelros();
                        }
                        Linker.setTestRunnerClassName(mLinkerParams.mTestRunnerClassName);
                    }
                    boolean isLoaded = false;
                    if (CommandLine.getInstance().hasSwitch(
                            BaseSwitches.RENDERER_WAIT_FOR_JAVA_DEBUGGER)) {
                        android.os.Debug.waitForDebugger();
                    }

                    boolean loadAtFixedAddressFailed = false;
                    try {
                        LibraryLoader.get(LibraryProcessType.PROCESS_CHILD)
                                .loadNow(getApplicationContext(), false);
                        isLoaded = true;
                    } catch (ProcessInitException e) {
                        if (requestedSharedRelro) {
                            Log.w(TAG, "Failed to load native library with shared RELRO, "
                                    + "retrying without");
                            loadAtFixedAddressFailed = true;
                        } else {
                            Log.e(TAG, "Failed to load native library", e);
                        }
                    }
                    if (!isLoaded && requestedSharedRelro) {
                        Linker.disableSharedRelros();
                        try {
                            LibraryLoader.get(LibraryProcessType.PROCESS_CHILD)
                                    .loadNow(getApplicationContext(), false);
                            isLoaded = true;
                        } catch (ProcessInitException e) {
                            Log.e(TAG, "Failed to load native library on retry", e);
                        }
                    }
                    if (!isLoaded) {
                        System.exit(-1);
                    }
                    LibraryLoader.get(LibraryProcessType.PROCESS_CHILD)
                            .registerRendererProcessHistogram(requestedSharedRelro,
                                    loadAtFixedAddressFailed);
                    LibraryLoader.get(LibraryProcessType.PROCESS_CHILD).initialize();
                    synchronized (mMainThread) {
                        mLibraryInitialized = true;
                        mMainThread.notifyAll();
                        while (mFileIds == null) {
                            mMainThread.wait();
                        }
                    }
                    assert mFileIds.size() == mFileFds.size();
                    int[] fileIds = new int[mFileIds.size()];
                    int[] fileFds = new int[mFileFds.size()];
                    for (int i = 0; i < mFileIds.size(); ++i) {
                        fileIds[i] = mFileIds.get(i);
                        fileFds[i] = mFileFds.get(i).detachFd();
                    }
                    ContentMain.initApplicationContext(sContext.get().getApplicationContext());
                    nativeInitChildProcess(sContext.get().getApplicationContext(),
                            ChildProcessService.this, fileIds, fileFds,
                            mCpuCount, mCpuFeatures);
                    if (mActivitySemaphore.tryAcquire()) {
                        ContentMain.start();
                        nativeExitChildProcess();
                    }
                } catch (InterruptedException e) {
                    Log.w(TAG, MAIN_THREAD_NAME + " startup failed: " + e);
                } catch (ProcessInitException e) {
                    Log.w(TAG, MAIN_THREAD_NAME + " startup failed: " + e);
                }
            }
        }, MAIN_THREAD_NAME);
        mMainThread.start();
    }

    @Override
    @SuppressFBWarnings("DM_EXIT")
    public void onDestroy() {
        Log.i(TAG, "Destroying ChildProcessService pid=" + Process.myPid());
        super.onDestroy();
        if (mActivitySemaphore.tryAcquire()) {
            // TODO(crbug.com/457406): This is a bit hacky, but there is no known better solution
            // as this service will get reused (at least if not sandboxed).
            // In fact, we might really want to always exit() from onDestroy(), not just from
            // the early return here.
            System.exit(0);
            return;
        }
        synchronized (mMainThread) {
            try {
                while (!mLibraryInitialized) {
                    // Avoid a potential race in calling through to native code before the library
                    // has loaded.
                    mMainThread.wait();
                }
            } catch (InterruptedException e) {
                // Ignore
            }
        }
        // Try to shutdown the MainThread gracefully, but it might not
        // have chance to exit normally.
        nativeShutdownMainThread();
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We call stopSelf() to request that this service be stopped as soon as the client
        // unbinds. Otherwise the system may keep it around and available for a reconnect. The
        // child processes do not currently support reconnect; they must be initialized from
        // scratch every time.
        stopSelf();

        synchronized (mMainThread) {
            mCommandLineParams = intent.getStringArrayExtra(
                    ChildProcessConnection.EXTRA_COMMAND_LINE);
            // mLinkerParams is never used if Linker.isUsed() returns false.
            // See onCreate().
            mLinkerParams = new ChromiumLinkerParams(intent);
            mIsBound = true;
            mMainThread.notifyAll();
        }

        return mBinder;
    }

    /**
     * Called from native code to share a surface texture with another child process.
     * Through using the callback object the browser is used as a proxy to route the
     * call to the correct process.
     *
     * @param pid Process handle of the child process to share the SurfaceTexture with.
     * @param surfaceObject The Surface or SurfaceTexture to share with the other child process.
     * @param primaryID Used to route the call to the correct client instance.
     * @param secondaryID Used to route the call to the correct client instance.
     */
    @SuppressWarnings("unused")
    @CalledByNative
    private void establishSurfaceTexturePeer(
            int pid, Object surfaceObject, int primaryID, int secondaryID) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        Surface surface = null;
        boolean needRelease = false;
        if (surfaceObject instanceof Surface) {
            surface = (Surface) surfaceObject;
        } else if (surfaceObject instanceof SurfaceTexture) {
            surface = new Surface((SurfaceTexture) surfaceObject);
            needRelease = true;
        } else {
            Log.e(TAG, "Not a valid surfaceObject: " + surfaceObject);
            return;
        }
        try {
            mCallback.establishSurfacePeer(pid, surface, primaryID, secondaryID);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call establishSurfaceTexturePeer: " + e);
            return;
        } finally {
            if (needRelease) {
                surface.release();
            }
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Surface getViewSurface(int surfaceId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return null;
        }

        try {
            return mCallback.getViewSurface(surfaceId).getSurface();
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call establishSurfaceTexturePeer: " + e);
            return null;
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void createSurfaceTextureSurface(
            int surfaceTextureId, int clientId, SurfaceTexture surfaceTexture) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        Surface surface = new Surface(surfaceTexture);
        try {
            mCallback.registerSurfaceTextureSurface(surfaceTextureId, clientId, surface);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call registerSurfaceTextureSurface: " + e);
        }
        surface.release();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void destroySurfaceTextureSurface(int surfaceTextureId, int clientId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return;
        }

        try {
            mCallback.unregisterSurfaceTextureSurface(surfaceTextureId, clientId);
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call unregisterSurfaceTextureSurface: " + e);
        }
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private Surface getSurfaceTextureSurface(int surfaceTextureId) {
        if (mCallback == null) {
            Log.e(TAG, "No callback interface has been provided.");
            return null;
        }

        try {
            return mCallback.getSurfaceTextureSurface(surfaceTextureId).getSurface();
        } catch (RemoteException e) {
            Log.e(TAG, "Unable to call getSurfaceTextureSurface: " + e);
            return null;
        }
    }

    /**
     * The main entry point for a child process. This should be called from a new thread since
     * it will not return until the child process exits. See child_process_service.{h,cc}
     *
     * @param applicationContext The Application Context of the current process.
     * @param service The current ChildProcessService object.
     * @param fileIds A list of file IDs that should be registered for access by the renderer.
     * @param fileFds A list of file descriptors that should be registered for access by the
     * renderer.
     */
    private static native void nativeInitChildProcess(Context applicationContext,
            ChildProcessService service, int[] extraFileIds, int[] extraFileFds,
            int cpuCount, long cpuFeatures);

    /**
     * Force the child process to exit.
     */
    private static native void nativeExitChildProcess();

    private native void nativeShutdownMainThread();
}
