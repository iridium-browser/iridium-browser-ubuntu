// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.signin.AccountManagementFragment;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;
import java.util.EnumSet;

/**
 * Modal dialog with options for selection the type of browsing data
 * to clear (history, cookies), triggered from a preference.
 */
public class ClearBrowsingDataDialogFragment extends DialogFragment implements
        PrefServiceBridge.OnClearBrowsingDataListener,
        DialogInterface.OnMultiChoiceClickListener, DialogInterface.OnClickListener {

    /** The tag used when showing the clear browsing fragment. */
    public static final String FRAGMENT_TAG = "ClearBrowsingDataDialogFragment";

    /**
     * Enum for Dialog options to be displayed in the dialog.
     */
    public enum DialogOption {
        CLEAR_HISTORY(R.string.clear_history_title),
        CLEAR_CACHE(R.string.clear_cache_title),
        CLEAR_COOKIES_AND_SITE_DATA(R.string.clear_cookies_and_site_data_title),
        CLEAR_PASSWORDS(R.string.clear_passwords_title),
        CLEAR_FORM_DATA(R.string.clear_formdata_title),
        // Clear bookmarks is only used by ClearSyncData dialog.
        CLEAR_BOOKMARKS_DATA(R.string.clear_bookmarks_title);

        private final int mResourceId;

        private DialogOption(int resourceId) {
            mResourceId = resourceId;
        }

        /**
         * @return resource id of the Dialog option.
         */
        public int getResourceId() {
            return mResourceId;
        }
    }

    private EnumSet<DialogOption> mSelectedOptions;
    private DialogOption[] mOptions;
    private AlertDialog mDialog;
    private ProgressDialog mProgressDialog;

    protected final void clearBrowsingData(EnumSet<DialogOption> selectedOptions) {
        PrefServiceBridge.getInstance().clearBrowsingData(this,
                selectedOptions.contains(DialogOption.CLEAR_HISTORY),
                selectedOptions.contains(DialogOption.CLEAR_CACHE),
                selectedOptions.contains(DialogOption.CLEAR_COOKIES_AND_SITE_DATA),
                selectedOptions.contains(DialogOption.CLEAR_PASSWORDS),
                selectedOptions.contains(DialogOption.CLEAR_FORM_DATA));
    }

    protected void dismissProgressDialog() {
        android.util.Log.i(FRAGMENT_TAG, "in dismissProgressDialog");
        if (mProgressDialog != null && mProgressDialog.isShowing()) {
            android.util.Log.i(FRAGMENT_TAG, "progress dialog dismissed");
            mProgressDialog.dismiss();
        }
        mProgressDialog = null;
    }

    /**
     * Returns the Array of dialog options. Options are displayed in the same
     * order as they appear in the array.
     */
    protected DialogOption[] getDialogOptions() {
        return new DialogOption[] {
            DialogOption.CLEAR_HISTORY,
            DialogOption.CLEAR_CACHE,
            DialogOption.CLEAR_COOKIES_AND_SITE_DATA,
            DialogOption.CLEAR_PASSWORDS,
            DialogOption.CLEAR_FORM_DATA};
    }

    /**
     * Get the default selections for the dialog.
     * @return EnumSet containing dialog options to be selected.
     */
    protected EnumSet<DialogOption> getDefaultDialogOptionsSelections() {
        return EnumSet.of(DialogOption.CLEAR_HISTORY, DialogOption.CLEAR_CACHE,
                DialogOption.CLEAR_COOKIES_AND_SITE_DATA);
    }

    // Called when "clear browsing data" completes.
    // Implements the ChromePreferences.OnClearBrowsingDataListener interface.
    @Override
    public void onBrowsingDataCleared() {
        dismissProgressDialog();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        if (which == AlertDialog.BUTTON_POSITIVE) {
            dismissProgressDialog();
            onOptionSelected(mSelectedOptions);
        }
    }

    /**
     * Disable the "Clear" button if none of the options are selected. Otherwise, enable it.
     */
    private void updateButtonState() {
        Button clearButton = mDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        if (clearButton != null) clearButton.setEnabled(!mSelectedOptions.isEmpty());
    }

    @Override
    public void onClick(DialogInterface dialog, int whichButton, boolean isChecked) {
        if (isChecked) {
            mSelectedOptions.add(mOptions[whichButton]);
        } else {
            mSelectedOptions.remove(mOptions[whichButton]);
        }
        updateButtonState();
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        DialogOption[] options = getDialogOptions();
        mOptions = Arrays.copyOf(options, options.length);
        mSelectedOptions = getDefaultDialogOptionsSelections();

        String[] items = new String[mOptions.length];
        boolean[] itemsChecked = new boolean[mOptions.length];
        Resources resources = getResources();
        for (int i = 0; i < mOptions.length; i++) {
            items[i] = resources.getString(mOptions[i].getResourceId());
            itemsChecked[i] = mSelectedOptions.contains(mOptions[i]);
        }

        final AlertDialog.Builder builder = new AlertDialog.Builder(getActivity())
                .setTitle(R.string.clear_browsing_data_title)
                .setPositiveButton(R.string.clear_data_delete, this)
                .setNegativeButton(R.string.cancel, this)
                .setMultiChoiceItems(items, itemsChecked, this);

        if (ChromeSigninController.get(getActivity()).isSignedIn()) {
            final String message = getString(R.string.clear_cookies_no_sign_out_summary);
            final SpannableString messageWithLink = SpanApplier.applySpans(message,
                    new SpanApplier.SpanInfo("<link>", "</link>", new ClickableSpan() {
                        @Override
                        public void onClick(View widget) {
                            dismiss();
                            Preferences prefActivity = (Preferences) getActivity();
                            prefActivity.startFragment(AccountManagementFragment.class.getName(),
                                    null);
                        }

                        // Change link formatting to use no underline
                        @Override
                        public void updateDrawState(TextPaint textPaint) {
                            textPaint.setColor(textPaint.linkColor);
                            textPaint.setUnderlineText(false);
                        }
                    }));

            View view = getActivity().getLayoutInflater().inflate(
                    R.layout.single_line_bottom_text_dialog, null);
            TextView summaryView = (TextView) view.findViewById(R.id.summary);
            summaryView.setText(messageWithLink);
            summaryView.setMovementMethod(LinkMovementMethod.getInstance());
            builder.setView(view);
        }

        mDialog = builder.create();
        return mDialog;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        // Now that the dialog's view has been created, update the button state.
        updateButtonState();
    }

    /**
     * Called when PositiveButton is clicked for the dialog.
     *
     * @param selectedOptions options which were selected.
     */
    protected void onOptionSelected(final EnumSet<DialogOption> selectedOptions) {
        showProgressDialog();
        clearBrowsingData(selectedOptions);
    }

    protected final void showProgressDialog() {
        if (getActivity() == null) return;

        android.util.Log.i(FRAGMENT_TAG, "progress dialog shown");
        mProgressDialog = ProgressDialog.show(getActivity(),
                getActivity().getString(R.string.clear_browsing_data_progress_title),
                getActivity().getString(R.string.clear_browsing_data_progress_message), true,
                false);
    }
}
