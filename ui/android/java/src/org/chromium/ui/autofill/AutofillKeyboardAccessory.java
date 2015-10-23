// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.autofill;

import android.annotation.SuppressLint;
import android.graphics.Typeface;
import android.os.Build;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.R;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.gfx.DeviceDisplayInfo;

/**
 * The Autofill suggestion view that lists relevant suggestions. It sits above the keyboard and
 * below the content area.
 */
public class AutofillKeyboardAccessory extends LinearLayout
        implements WindowAndroid.KeyboardVisibilityListener, View.OnClickListener,
        View.OnLongClickListener {
    private final WindowAndroid mWindowAndroid;
    private final AutofillDelegate mAutofillDelegate;
    private final int mMaximumLabelWidthPx;
    private final int mMaximumSublabelWidthPx;

    /**
     * Creates an AutofillKeyboardAccessory with specified parameters.
     * @param windowAndroid The owning WindowAndroid.
     * @param autofillDelegate A object that handles the calls to the native
     *                         AutofillKeyboardAccessoryView.
     */
    public AutofillKeyboardAccessory(
            WindowAndroid windowAndroid, AutofillDelegate autofillDelegate) {
        super(windowAndroid.getActivity().get());
        assert autofillDelegate != null;
        assert windowAndroid.getActivity().get() != null;
        mWindowAndroid = windowAndroid;
        mAutofillDelegate = autofillDelegate;

        int deviceWidthPx = DeviceDisplayInfo.create(getContext()).getDisplayWidth();
        mMaximumLabelWidthPx = deviceWidthPx / 2;
        mMaximumSublabelWidthPx = deviceWidthPx / 4;

        mWindowAndroid.addKeyboardVisibilityListener(this);
        setContentDescription(getContext().getString(R.string.autofill_popup_content_description));

        int horizontalPaddingPx = getResources().getDimensionPixelSize(
                R.dimen.keyboard_accessory_half_padding);
        setPadding(horizontalPaddingPx, 0, horizontalPaddingPx, 0);
    }

    /**
     * Shows the given suggestions.
     * @param suggestions Autofill suggestion data.
     * @param isRtl Gives the layout direction for the <input> field.
     */
    @SuppressLint("InlinedApi")
    public void showWithSuggestions(AutofillSuggestion[] suggestions, boolean isRtl) {
        removeAllViews();
        int separatorPosition = -1;
        for (int i = 0; i < suggestions.length; i++) {
            AutofillSuggestion suggestion = suggestions[i];
            assert !TextUtils.isEmpty(suggestion.getLabel());

            // Negative suggestion ID indiciates a tool like "settings" or "scan credit card."
            // Non-negative suggestion ID indicates suggestions that can be filled into the form.
            View touchTarget;
            if (suggestion.getSuggestionId() < 0 && suggestion.getIconId() != 0) {
                touchTarget = LayoutInflater.from(getContext()).inflate(
                        R.layout.autofill_keyboard_accessory_icon, this, false);

                if (separatorPosition == -1) separatorPosition = i;

                ImageView icon = (ImageView) touchTarget;
                icon.setImageResource(suggestion.getIconId());
                icon.setContentDescription(suggestion.getLabel());
            } else {
                touchTarget = LayoutInflater.from(getContext()).inflate(
                        R.layout.autofill_keyboard_accessory_item, this, false);

                TextView label = (TextView) touchTarget.findViewById(
                        R.id.autofill_keyboard_accessory_item_label);
                label.setMaxWidth(mMaximumLabelWidthPx);
                label.setText(suggestion.getLabel());
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                    label.setTypeface(Typeface.DEFAULT_BOLD);
                }

                if (suggestion.getIconId() != 0) {
                    ApiCompatibilityUtils.setCompoundDrawablesRelativeWithIntrinsicBounds(
                            label, suggestion.getIconId(), 0, 0, 0);
                }

                if (!TextUtils.isEmpty(suggestion.getSublabel())) {
                    TextView sublabel = (TextView) touchTarget.findViewById(
                            R.id.autofill_keyboard_accessory_item_sublabel);
                    sublabel.setText(suggestion.getSublabel());
                    sublabel.setVisibility(View.VISIBLE);
                    sublabel.setMaxWidth(mMaximumSublabelWidthPx);
                }
            }

            touchTarget.setTag(i);
            touchTarget.setOnClickListener(this);
            if (suggestion.isDeletable()) {
                touchTarget.setOnLongClickListener(this);
            }

            addView(touchTarget);
        }

        if (separatorPosition != -1) {
            View separator = new View(getContext());
            separator.setLayoutParams(new LinearLayout.LayoutParams(0, 0, 1));
            addView(separator, separatorPosition);
        }

        ApiCompatibilityUtils.setLayoutDirection(
                this, isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        if (getParent() == null) {
            ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();
            container.addView(this);
            container.setVisibility(View.VISIBLE);
            sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
    }

    /**
     * Called to hide the suggestion view.
     */
    public void dismiss() {
        ViewGroup container = mWindowAndroid.getKeyboardAccessoryView();
        container.removeView(this);
        container.setVisibility(View.GONE);
        mWindowAndroid.removeKeyboardVisibilityListener(this);
        ((View) container.getParent()).requestLayout();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        if (!isShowing) {
            dismiss();
            mAutofillDelegate.dismissed();
        }
    }

    @Override
    public void onClick(View v) {
        mAutofillDelegate.suggestionSelected((int) v.getTag());
    }

    @Override
    public boolean onLongClick(View v) {
        mAutofillDelegate.deleteSuggestion((int) v.getTag());
        return true;
    }
}
