// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.autofill.AutofillProfileBridge.DropdownKeyValue;

import java.util.List;

/**
 * Helper class for creating a dropdown view with a label.
 */
class EditorDropdownField implements Validatable {
    private final EditorFieldModel mFieldModel;
    private final View mLayout;
    private final TextView mLabel;
    private final Spinner mDropdown;
    private int mSelectedIndex;

    /**
     * Builds a dropdown view.
     *
     * @param context         The application context to use when creating widgets.
     * @param root            The object that provides a set of LayoutParams values for the view.
     * @param fieldModel      The data model of the dropdown.
     * @param changedCallback The callback to invoke after user's dropdwn item selection has been
     *                        processed.
     */
    public EditorDropdownField(Context context, ViewGroup root, final EditorFieldModel fieldModel,
            final Runnable changedCallback) {
        assert fieldModel.getInputTypeHint() == EditorFieldModel.INPUT_TYPE_HINT_DROPDOWN;
        mFieldModel = fieldModel;

        mLayout = LayoutInflater.from(context).inflate(
                R.layout.payment_request_editor_dropdown, root, false);

        mLabel = (TextView) mLayout.findViewById(R.id.spinner_label);
        mLabel.setText(mFieldModel.isRequired()
                ? mFieldModel.getLabel() + EditorView.REQUIRED_FIELD_INDICATOR
                : mFieldModel.getLabel());

        final List<DropdownKeyValue> dropdownKeyValues = mFieldModel.getDropdownKeyValues();
        for (int j = 0; j < dropdownKeyValues.size(); j++) {
            if (dropdownKeyValues.get(j).getKey().equals(mFieldModel.getValue())) {
                mSelectedIndex = j;
                break;
            }
        }

        ArrayAdapter<DropdownKeyValue> adapter = new ArrayAdapter<DropdownKeyValue>(
                context, R.layout.multiline_spinner_item, dropdownKeyValues);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        mDropdown = (Spinner) mLayout.findViewById(R.id.spinner);
        mDropdown.setTag(this);
        mDropdown.setContentDescription(mFieldModel.getLabel());
        mDropdown.setAdapter(adapter);
        mDropdown.setSelection(mSelectedIndex);
        mDropdown.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (mSelectedIndex != position) {
                    mSelectedIndex = position;
                    mFieldModel.setDropdownKey(dropdownKeyValues.get(position).getKey(),
                            changedCallback);
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    /** @return The View containing everything. */
    public View getLayout() {
        return mLayout;
    }

    /** @return The EditorFieldModel that the EditorDropdownField represents. */
    public EditorFieldModel getFieldModel() {
        return mFieldModel;
    }

    /** @return The label view for the spinner. */
    public View getLabel() {
        return mLabel;
    }

    /** @return The dropdown view itself. */
    public Spinner getDropdown() {
        return mDropdown;
    }

    @Override
    public boolean isValid() {
        return mFieldModel.isValid();
    }

    @Override
    public void updateDisplayedError(boolean showError) {
        View view = mDropdown.getSelectedView();
        if (view != null && view instanceof TextView) {
            ((TextView) view).setError(showError ? mFieldModel.getErrorMessage() : null);
        }
    }

    @Override
    public void scrollToAndFocus() {
        ViewGroup parent = (ViewGroup) mDropdown.getParent();
        if (parent != null) parent.requestChildFocus(mDropdown, mDropdown);
        // Open the dropdown to prompt user selection.
        mDropdown.performClick();
        mDropdown.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
