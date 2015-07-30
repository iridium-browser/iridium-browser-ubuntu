// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.app.Activity;
import android.os.Handler;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.chrome.browser.ResourceId;
import org.chromium.ui.DropdownItem;
import org.chromium.ui.autofill.AutofillPopup;
import org.chromium.ui.autofill.AutofillPopup.AutofillPopupDelegate;
import org.chromium.ui.autofill.AutofillSuggestion;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillPopupBridge implements AutofillPopupDelegate{
    private final long mNativeAutofillPopup;
    private final AutofillPopup mAutofillPopup;

    public AutofillPopupBridge(long nativeAutofillPopupViewAndroid, WindowAndroid windowAndroid,
            ViewAndroidDelegate containerViewDelegate) {
        mNativeAutofillPopup = nativeAutofillPopupViewAndroid;
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            mAutofillPopup = null;
            // Clean up the native counterpart.  This is posted to allow the native counterpart
            // to fully finish the construction of this glue object before we attempt to delete it.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    dismissed();
                }
            });
        } else {
            mAutofillPopup = new AutofillPopup(activity, containerViewDelegate, this);
        }
    }

    @CalledByNative
    private static AutofillPopupBridge create(long nativeAutofillPopupViewAndroid,
            WindowAndroid windowAndroid, ViewAndroidDelegate viewAndroidDelegate) {
        return new AutofillPopupBridge(
                nativeAutofillPopupViewAndroid, windowAndroid, viewAndroidDelegate);
    }

    @Override
    public void dismissed() {
        nativePopupDismissed(mNativeAutofillPopup);
    }

    @Override
    public void suggestionSelected(int listIndex) {
        nativeSuggestionSelected(mNativeAutofillPopup, listIndex);
    }

    /**
     * Hides the Autofill Popup and removes its anchor from the ContainerView.
     */
    @CalledByNative
    private void dismiss() {
        if (mAutofillPopup != null) mAutofillPopup.dismiss();
    }

    /**
     * Shows an Autofill popup with specified suggestions.
     * @param suggestions Autofill suggestions to be displayed.
     */
    @CalledByNative
    private void show(AutofillSuggestion[] suggestions, boolean isRtl) {
        if (mAutofillPopup != null) mAutofillPopup.filterAndShow(suggestions, isRtl);
    }

    /**
     * Sets the location and size of the Autofill popup anchor (input field).
     * @param x X coordinate.
     * @param y Y coordinate.
     * @param width The width of the anchor.
     * @param height The height of the anchor.
     */
    @CalledByNative
    private void setAnchorRect(float x, float y, float width, float height) {
        if (mAutofillPopup != null) mAutofillPopup.setAnchorRect(x, y, width, height);
    }

    // Helper methods for AutofillSuggestion

    @CalledByNative
    private static AutofillSuggestion[] createAutofillSuggestionArray(int size) {
        return new AutofillSuggestion[size];
    }

    /**
     * @param array AutofillSuggestion array that should get a new suggestion added.
     * @param index Index in the array where to place a new suggestion.
     * @param label First line of the suggestion.
     * @param sublabel Second line of the suggestion.
     * @param iconId The resource ID for the icon associated with the suggestion, or 0 for no icon.
     * @param suggestionId Identifier for the suggestion type.
     */
    @CalledByNative
    private static void addToAutofillSuggestionArray(AutofillSuggestion[] array, int index,
            String label, String sublabel, int iconId, int suggestionId) {
        int drawableId = iconId == 0 ? DropdownItem.NO_ICON : ResourceId.mapToDrawableId(iconId);
        array[index] = new AutofillSuggestion(label, sublabel, drawableId, suggestionId);
    }

    private native void nativePopupDismissed(long nativeAutofillPopupViewAndroid);
    private native void nativeSuggestionSelected(long nativeAutofillPopupViewAndroid,
            int listIndex);
}
