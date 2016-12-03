// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.InterfaceRegistry.ImplementationFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.device.battery.BatteryMonitorFactory;
import org.chromium.device.nfc.NfcImpl;
import org.chromium.device.vibration.VibrationManagerImpl;
import org.chromium.mojom.device.BatteryMonitor;
import org.chromium.mojom.device.VibrationManager;
import org.chromium.mojom.device.nfc.Nfc;
import org.chromium.ui.base.WindowAndroid;

/**
 * Registers interfaces exposed by the browser in the given registry.
 */
@JNINamespace("content")
class InterfaceRegistrar {
    // BatteryMonitorFactory can't implement ImplementationFactory itself, as we don't depend on
    // /content in /device. Hence we use BatteryMonitorImplementationFactory as a wrapper.
    private static class BatteryMonitorImplementationFactory
            implements ImplementationFactory<BatteryMonitor> {
        private final BatteryMonitorFactory mFactory;

        BatteryMonitorImplementationFactory(Context applicationContext) {
            mFactory = new BatteryMonitorFactory(applicationContext);
        }

        @Override
        public BatteryMonitor createImpl() {
            return mFactory.createMonitor();
        }
    }

    private static class VibrationManagerImplementationFactory
            implements ImplementationFactory<VibrationManager> {
        private final Context mApplicationContext;

        VibrationManagerImplementationFactory(Context applicationContext) {
            mApplicationContext = applicationContext;
        }

        @Override
        public VibrationManager createImpl() {
            return new VibrationManagerImpl(mApplicationContext);
        }
    }

    private static class NfcImplementationFactory implements ImplementationFactory<Nfc> {
        private final Context mContext;
        private final WebContents mContents;

        NfcImplementationFactory(Context context, WebContents contents) {
            mContext = context;
            mContents = contents;
        }

        private static class ContextAwareNfcImpl extends NfcImpl implements
                WindowAndroidChangedObserver {
            private final ContentViewCore mContextViewCore;

            ContextAwareNfcImpl(Context context, ContentViewCore contextViewCore) {
                super(context);
                mContextViewCore = contextViewCore;
                if (mContextViewCore != null) {
                    mContextViewCore.addWindowAndroidChangedObserver(this);
                    if (mContextViewCore.getWindowAndroid() != null) {
                        setActivity(mContextViewCore.getWindowAndroid().getActivity().get());
                    }
                }
            }

            @Override
            public void close() {
                super.close();
                if (mContextViewCore != null) {
                    mContextViewCore.removeWindowAndroidChangedObserver(this);
                }
            }

            @Override
            public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
                Activity activity = null;
                if (newWindowAndroid != null) {
                    activity = newWindowAndroid.getActivity().get();
                }
                setActivity(activity);
            }
        }

        @Override
        public Nfc createImpl() {
            return new ContextAwareNfcImpl(mContext, ContentViewCore.fromWebContents(mContents));
        }
    }

    @CalledByNative
    static void exposeInterfacesToRenderer(InterfaceRegistry registry, Context applicationContext) {
        assert applicationContext != null;
        registry.addInterface(BatteryMonitor.MANAGER,
                new BatteryMonitorImplementationFactory(applicationContext));
    }

    @CalledByNative
    static void exposeInterfacesToFrame(InterfaceRegistry registry,
            Context applicationContext, WebContents contents) {
        assert applicationContext != null;
        registry.addInterface(VibrationManager.MANAGER,
                new VibrationManagerImplementationFactory(applicationContext));
        registry.addInterface(Nfc.MANAGER,
                new NfcImplementationFactory(applicationContext, contents));
        // TODO(avayvod): Register the PresentationService implementation here.
    }
}
