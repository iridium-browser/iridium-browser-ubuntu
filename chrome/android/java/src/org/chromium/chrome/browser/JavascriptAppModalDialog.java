// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.TextView;

import org.chromium.base.CalledByNative;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.ui.base.WindowAndroid;

public class JavascriptAppModalDialog implements DialogInterface.OnClickListener {
    private static final String TAG = "JavascriptAppModalDialog";

    private final String mTitle;
    private final String mMessage;
    private final boolean mShouldShowSuppressCheckBox;
    private long mNativeDialogPointer;
    private AlertDialog mDialog;
    private CheckBox mSuppressCheckBox;
    private TextView mPromptTextView;

    private JavascriptAppModalDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        mTitle = title;
        mMessage = message;
        mShouldShowSuppressCheckBox = shouldShowSuppressCheckBox;
    }

    @CalledByNative
    public static JavascriptAppModalDialog createAlertDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppAlertDialog(title, message, shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createConfirmDialog(String title, String message,
            boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppConfirmDialog(title, message, shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createBeforeUnloadDialog(String title, String message,
            boolean isReload, boolean shouldShowSuppressCheckBox) {
        return new JavascriptAppBeforeUnloadDialog(title, message, isReload,
                shouldShowSuppressCheckBox);
    }

    @CalledByNative
    public static JavascriptAppModalDialog createPromptDialog(String title, String message,
            boolean shouldShowSuppressCheckBox, String defaultPromptText) {
        return new JavascriptAppPromptDialog(title, message, shouldShowSuppressCheckBox,
                defaultPromptText);
    }

    @CalledByNative
    void showJavascriptAppModalDialog(WindowAndroid window, long nativeDialogPointer) {
        assert window != null;
        Context context = window.getActivity().get();
        // If the activity has gone away, then just clean up the native pointer.
        if (context == null) {
            nativeDidCancelAppModalDialog(nativeDialogPointer, false);
            return;
        }

        // Cache the native dialog pointer so that we can use it to return the response.
        mNativeDialogPointer = nativeDialogPointer;

        LayoutInflater inflater =
                (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        ViewGroup layout = (ViewGroup) inflater.inflate(R.layout.js_modal_dialog, null);
        mSuppressCheckBox = (CheckBox) layout.findViewById(R.id.suppress_js_modal_dialogs);
        mPromptTextView = (TextView) layout.findViewById(R.id.js_modal_dialog_prompt);

        prepare(layout);

        AlertDialog.Builder builder = new AlertDialog.Builder(context)
                .setView(layout)
                .setTitle(mTitle)
                .setOnCancelListener(new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        cancel(false);
                    }
                });
        if (hasPositiveButton()) {
            builder.setPositiveButton(getPositiveButtonText(), this);
        }
        if (hasNegativeButton()) {
            builder.setNegativeButton(getNegativeButtonText(), this);
        }

        mDialog = builder.create();
        mDialog.setCanceledOnTouchOutside(false);
        mDialog.show();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                onPositiveButtonClicked();
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                onNegativeButtonClicked();
                break;
            default:
                Log.e(TAG, "Unexpected button pressed in dialog: " + which);
        }
    }

    @CalledByNative
    void dismiss() {
        mDialog.dismiss();
        mNativeDialogPointer = 0;
    }

    /**
     * Returns the currently showing dialog, null if none is showing.
     */
    @VisibleForTesting
    public static JavascriptAppModalDialog getCurrentDialogForTest() {
        return nativeGetCurrentModalDialog();
    }


    /**
     * Returns the AlertDialog associated with this JavascriptAppPromptDialog.
     */
    @VisibleForTesting
    public AlertDialog getDialogForTest() {
        return mDialog;
    }

    // Methods that subclasses should override to set buttons behavior.
    public boolean hasPositiveButton() {
        return false;
    }

    public int getPositiveButtonText() {
        return -1;
    }

    public boolean hasNegativeButton() {
        return false;
    }

    public int getNegativeButtonText() {
        return -1;
    }

    public void onPositiveButtonClicked() {
        confirm(mPromptTextView.getText().toString(), mSuppressCheckBox.isChecked());
        mDialog.dismiss();
    }

    public void onNegativeButtonClicked() {
        cancel(mSuppressCheckBox.isChecked());
        mDialog.dismiss();
    }

    void prepare(final ViewGroup layout) {
        // Display the checkbox for suppressing dialogs if necessary.
        layout.findViewById(R.id.suppress_js_modal_dialogs).setVisibility(
                mShouldShowSuppressCheckBox ? View.VISIBLE : View.GONE);

        // If the message is null or empty do not display the message text view.
        // Hide parent scroll view instead of text view in order to prevent ui discrepancies.
        if (TextUtils.isEmpty(mMessage)) {
            layout.findViewById(R.id.js_modal_dialog_scroll_view).setVisibility(View.GONE);
        } else {
            ((TextView) layout.findViewById(R.id.js_modal_dialog_message)).setText(mMessage);
        }
    }

    public void confirm(String promptResult, boolean suppressDialogs) {
        if (mNativeDialogPointer != 0) {
            nativeDidAcceptAppModalDialog(mNativeDialogPointer, promptResult, suppressDialogs);
        }
    }

    public void cancel(boolean suppressDialogs) {
        if (mNativeDialogPointer != 0) {
            nativeDidCancelAppModalDialog(mNativeDialogPointer, suppressDialogs);
        }
    }

    private static class JavascriptAppAlertDialog extends JavascriptAppModalDialog {
        public JavascriptAppAlertDialog(String title, String message,
                boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
        }

        @Override
        public boolean hasPositiveButton() {
            return true;
        }

        @Override
        public int getPositiveButtonText() {
            return R.string.js_modal_dialog_confirm;
        }
    }

    private static class JavascriptAppConfirmDialog extends JavascriptAppAlertDialog {
        public JavascriptAppConfirmDialog(String title, String message,
                boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
        }

        @Override
        public boolean hasNegativeButton() {
            return true;
        }

        @Override
        public int getNegativeButtonText() {
            return R.string.js_modal_dialog_cancel;
        }
    }

    private static class JavascriptAppBeforeUnloadDialog extends JavascriptAppConfirmDialog {
        private final boolean mIsReload;

        public JavascriptAppBeforeUnloadDialog(String title, String message,
                boolean isReload, boolean shouldShowSuppressCheckBox) {
            super(title, message, shouldShowSuppressCheckBox);
            mIsReload = isReload;
        }

        @Override
        public boolean hasPositiveButton() {
            return true;
        }

        @Override
        public int getPositiveButtonText() {
            return mIsReload ? R.string.reload_this_page : R.string.leave_this_page;
        }

        @Override
        public boolean hasNegativeButton() {
            return true;
        }

        @Override
        public int getNegativeButtonText() {
            return mIsReload ? R.string.dont_reload_this_page : R.string.stay_on_this_page;
        }
    }

    private static class JavascriptAppPromptDialog extends JavascriptAppConfirmDialog {
        private final String mDefaultPromptText;

        public JavascriptAppPromptDialog(String title, String message,
                boolean shouldShowSuppressCheckBox, String defaultPromptText) {
            super(title, message, shouldShowSuppressCheckBox);
            mDefaultPromptText = defaultPromptText;
        }

        @Override
        public void prepare(ViewGroup layout) {
            super.prepare(layout);
            EditText prompt = (EditText) layout.findViewById(R.id.js_modal_dialog_prompt);
            prompt.setVisibility(View.VISIBLE);

            if (mDefaultPromptText.length() > 0) {
                prompt.setText(mDefaultPromptText);
                prompt.selectAll();
            }
        }
    }

    private native void nativeDidAcceptAppModalDialog(long nativeJavascriptAppModalDialogAndroid,
            String prompt, boolean suppress);

    private native void nativeDidCancelAppModalDialog(long nativeJavascriptAppModalDialogAndroid,
            boolean suppress);

    private static native JavascriptAppModalDialog nativeGetCurrentModalDialog();
}
