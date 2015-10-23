// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth;

import android.Manifest;
import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelUuid;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * Wrapper classes around android.bluetooth.* classes that provide an
 * indirection layer enabling fake implementations when running tests.
 *
 * Each Wrapper base class accepts an Android API object and passes through
 * calls to it. When under test, Fake subclasses override all methods that
 * pass through to the Android object and instead provide fake implementations.
 */
@JNINamespace("device")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class Wrappers {
    private static final String TAG = "cr.Bluetooth";

    /**
     * Wraps android.bluetooth.BluetoothAdapter.
     */
    static class BluetoothAdapterWrapper {
        private final BluetoothAdapter mAdapter;
        protected final BluetoothLeScannerWrapper mScanner;

        /**
         * Creates a BluetoothAdapterWrapper using the default
         * android.bluetooth.BluetoothAdapter. May fail if the default adapter
         * is not available or if the application does not have sufficient
         * permissions.
         */
        @CalledByNative("BluetoothAdapterWrapper")
        public static BluetoothAdapterWrapper createWithDefaultAdapter(Context context) {
            final boolean hasMinAPI = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
            if (!hasMinAPI) {
                Log.i(TAG, "BluetoothAdapterWrapper.create failed: SDK version (%d) too low.",
                        Build.VERSION.SDK_INT);
                return null;
            }

            final boolean hasPermissions =
                    context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH)
                            == PackageManager.PERMISSION_GRANTED
                    && context.checkCallingOrSelfPermission(Manifest.permission.BLUETOOTH_ADMIN)
                            == PackageManager.PERMISSION_GRANTED;
            if (!hasPermissions) {
                Log.w(TAG, "BluetoothAdapterWrapper.create failed: Lacking Bluetooth permissions.");
                return null;
            }

            // Only Low Energy currently supported, see BluetoothAdapterAndroid class note.
            final boolean hasLowEnergyFeature =
                    Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2
                    && context.getPackageManager().hasSystemFeature(
                               PackageManager.FEATURE_BLUETOOTH_LE);
            if (!hasLowEnergyFeature) {
                Log.i(TAG, "BluetoothAdapterWrapper.create failed: No Low Energy support.");
                return null;
            }

            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (adapter == null) {
                Log.i(TAG, "BluetoothAdapterWrapper.create failed: Default adapter not found.");
                return null;
            } else {
                return new BluetoothAdapterWrapper(
                        adapter, new BluetoothLeScannerWrapper(adapter.getBluetoothLeScanner()));
            }
        }

        public BluetoothAdapterWrapper(
                BluetoothAdapter adapter, BluetoothLeScannerWrapper scanner) {
            mAdapter = adapter;
            mScanner = scanner;
        }

        public BluetoothLeScannerWrapper getBluetoothLeScanner() {
            return mScanner;
        }

        public boolean isEnabled() {
            return mAdapter.isEnabled();
        }

        public String getAddress() {
            return mAdapter.getAddress();
        }

        public String getName() {
            return mAdapter.getName();
        }

        public int getScanMode() {
            return mAdapter.getScanMode();
        }

        public boolean isDiscovering() {
            return mAdapter.isDiscovering();
        }
    }

    /**
     * Wraps android.bluetooth.BluetoothLeScanner.
     */
    static class BluetoothLeScannerWrapper {
        private final BluetoothLeScanner mScanner;
        private final HashMap<ScanCallbackWrapper, ScanCallbackImpl> mCallbacks;

        public BluetoothLeScannerWrapper(BluetoothLeScanner scanner) {
            mScanner = scanner;
            mCallbacks = new HashMap<ScanCallbackWrapper, ScanCallbackImpl>();
        }

        public void startScan(
                List<ScanFilter> filters, int scanSettingsScanMode, ScanCallbackWrapper callback) {
            ScanSettings settings =
                    new ScanSettings.Builder().setScanMode(scanSettingsScanMode).build();

            ScanCallbackImpl callbackImpl = new ScanCallbackImpl(callback);
            mCallbacks.put(callback, callbackImpl);

            mScanner.startScan(filters, settings, callbackImpl);
        }

        public void stopScan(ScanCallbackWrapper callback) {
            ScanCallbackImpl callbackImpl = mCallbacks.remove(callback);
            mScanner.stopScan(callbackImpl);
        }
    }

    /**
     * Implements android.bluetooth.le.ScanCallback and passes calls through to a
     * provided ScanCallbackWrapper instance.
     *
     * This class is required so that Fakes can use ScanCallbackWrapper without
     * it extending from ScanCallback. Fakes must function even on Android
     * versions where ScanCallback class is not defined.
     */
    static class ScanCallbackImpl extends ScanCallback {
        final ScanCallbackWrapper mWrapperCallback;

        ScanCallbackImpl(ScanCallbackWrapper wrapperCallback) {
            mWrapperCallback = wrapperCallback;
        }

        @Override
        public void onBatchScanResults(List<ScanResult> results) {
            ArrayList<ScanResultWrapper> resultsWrapped =
                    new ArrayList<ScanResultWrapper>(results.size());
            for (ScanResult result : results) {
                resultsWrapped.add(new ScanResultWrapper(result));
            }
            mWrapperCallback.onBatchScanResult(resultsWrapped);
        }

        @Override
        public void onScanResult(int callbackType, ScanResult result) {
            mWrapperCallback.onScanResult(callbackType, new ScanResultWrapper(result));
        }

        @Override
        public void onScanFailed(int errorCode) {
            mWrapperCallback.onScanFailed(errorCode);
        }
    }

    /**
     * Wraps android.bluetooth.le.ScanCallback, being called by ScanCallbackImpl.
     */
    abstract static class ScanCallbackWrapper {
        public abstract void onBatchScanResult(List<ScanResultWrapper> results);
        public abstract void onScanResult(int callbackType, ScanResultWrapper result);
        public abstract void onScanFailed(int errorCode);
    }

    /**
     * Wraps android.bluetooth.le.ScanResult.
     */
    static class ScanResultWrapper {
        private final ScanResult mScanResult;

        public ScanResultWrapper(ScanResult scanResult) {
            mScanResult = scanResult;
        }

        public BluetoothDeviceWrapper getDevice() {
            return new BluetoothDeviceWrapper(mScanResult.getDevice());
        }

        public List<ParcelUuid> getScanRecord_getServiceUuids() {
            return mScanResult.getScanRecord().getServiceUuids();
        }
    }

    /**
     * Wraps android.bluetooth.BluetoothDevice.
     */
    static class BluetoothDeviceWrapper {
        private final BluetoothDevice mDevice;

        public BluetoothDeviceWrapper(BluetoothDevice device) {
            mDevice = device;
        }

        public String getAddress() {
            return mDevice.getAddress();
        }

        public int getBluetoothClass_getDeviceClass() {
            return mDevice.getBluetoothClass().getDeviceClass();
        }

        public int getBondState() {
            return mDevice.getBondState();
        }

        public String getName() {
            return mDevice.getName();
        }
    }
}
