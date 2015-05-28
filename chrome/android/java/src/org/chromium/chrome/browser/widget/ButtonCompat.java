// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.animation.AnimatorInflater;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.widget.Button;

import org.chromium.chrome.R;

/**
 * A Material-styled button with a customizable background color. On L devices, this is a true
 * Material button. On earlier devices, the button is similar but lacks ripples and a shadow.
 *
 * Create a button in Java:
 *
 *   new ButtonCompat(context, Color.RED);
 *
 * Create a button in XML:
 *
 *   <org.chromium.chrome.browser.widget.ButtonCompat
 *       android:layout_width="wrap_content"
 *       android:layout_height="wrap_content"
 *       android:text="Click me"
 *       chrome:buttonColor="#f00" />
 *
 * Note: To ensure the button's shadow is fully visible, you may need to set
 * android:clipToPadding="false" on the button's parent view.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ButtonCompat extends Button {

    private static final float PRE_L_PRESSED_BRIGHTNESS = 0.85f;
    private static final int DISABLED_COLOR = 0x1F000000;

    private int mColor;

    /**
     * Returns a new borderless material-style button.
     */
    public static Button createBorderlessButton(Context context) {
        return new Button(new ContextThemeWrapper(context, R.style.ButtonCompatBorderlessOverlay));
    }

    /**
     * Constructs a button with the given buttonColor as its background.
     */
    public ButtonCompat(Context context, int buttonColor) {
        this(context, buttonColor, null);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ButtonCompat(Context context, AttributeSet attrs) {
        this(context, getColorFromAttributeSet(context, attrs), attrs);
    }

    private ButtonCompat(Context context, int buttonColor, AttributeSet attrs) {
        // To apply the ButtonCompat style to this view, use a ContextThemeWrapper to overlay the
        // ButtonCompatThemeOverlay, which simply sets the buttonStyle to @style/ButtonCompat.
        super(new ContextThemeWrapper(context, R.style.ButtonCompatOverlay), attrs);

        getBackground().mutate();
        setButtonColor(buttonColor);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            // Use the StateListAnimator from the Widget.Material.Button style to animate the
            // elevation when the button is pressed.
            TypedArray a = getContext().obtainStyledAttributes(null,
                    new int[]{android.R.attr.stateListAnimator}, 0,
                    android.R.style.Widget_Material_Button);
            setStateListAnimator(AnimatorInflater.loadStateListAnimator(getContext(),
                    a.getResourceId(0, 0)));
            a.recycle();
        }
    }

    /**
     * Sets the background color of the button.
     */
    public void setButtonColor(int color) {
        if (color == mColor) return;
        mColor = color;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            updateButtonBackgroundL();
        } else {
            updateButtonBackgroundPreL();
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            updateButtonBackgroundPreL();
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updateButtonBackgroundL() {
        ColorStateList csl = new ColorStateList(
                new int[][] { { -android.R.attr.state_enabled }, {} },
                new int[] { DISABLED_COLOR, mColor });
        GradientDrawable shape = (GradientDrawable)
                ((RippleDrawable) getBackground()).getDrawable(0);
        shape.mutate();
        shape.setColor(csl);
    }

    private void updateButtonBackgroundPreL() {
        GradientDrawable background = (GradientDrawable) getBackground();
        background.setColor(getBackgroundColorPreL());
    }

    private int getBackgroundColorPreL() {
        for (int state : getDrawableState()) {
            if (state == android.R.attr.state_pressed
                    || state == android.R.attr.state_focused
                    || state == android.R.attr.state_selected) {
                return Color.rgb(
                        Math.round(Color.red(mColor) * PRE_L_PRESSED_BRIGHTNESS),
                        Math.round(Color.green(mColor) * PRE_L_PRESSED_BRIGHTNESS),
                        Math.round(Color.blue(mColor) * PRE_L_PRESSED_BRIGHTNESS));
            }
        }
        for (int state : getDrawableState()) {
            if (state == android.R.attr.state_enabled) {
                return mColor;
            }
        }
        return DISABLED_COLOR;
    }

    private static int getColorFromAttributeSet(Context context, AttributeSet attrs) {
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ButtonCompat, 0, 0);
        int color = a.getColor(R.styleable.ButtonCompat_buttonColor, Color.WHITE);
        a.recycle();
        return color;
    }
}