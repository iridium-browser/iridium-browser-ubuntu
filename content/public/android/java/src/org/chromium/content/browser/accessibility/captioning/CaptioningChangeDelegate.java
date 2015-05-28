// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.graphics.Color;
import android.graphics.Typeface;
import android.view.accessibility.CaptioningManager.CaptionStyle;

import org.chromium.base.VisibleForTesting;
import org.chromium.content.browser.ContentViewCore;

import java.lang.ref.WeakReference;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.util.Locale;

/**
 * API level agnostic delegate for getting updates about caption styles.
 *
 * This class is based on CaptioningManager.CaptioningChangeListener except it uses internal
 * classes instead of the API level dependent versions. Here is the documentation for that class:
 *
 * @link https://developer.android.com/reference/android/view/accessibility/CaptioningManager.CaptioningChangeListener.html
 */
public class CaptioningChangeDelegate {
    private static final String FONT_STYLE_ITALIC = "italic";

    @VisibleForTesting
    public static final String DEFAULT_CAPTIONING_PREF_VALUE = "";

    // Using a weak reference avoids cycles that might prevent GC of WebView's WebContents.
    private final WeakReference<ContentViewCore> mWeakContentViewCore;

    private boolean mTextTrackEnabled;

    private String mTextTrackBackgroundColor;
    private String mTextTrackFontFamily;
    private String mTextTrackFontStyle;
    private String mTextTrackFontVariant;
    private String mTextTrackTextColor;
    private String mTextTrackTextShadow;
    private String mTextTrackTextSize;

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onEnabledChanged
     */
    public void onEnabledChanged(boolean enabled) {
        mTextTrackEnabled = enabled;
        notifySettingsChanged();
    }

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onFontScaleChanged
     */
    public void onFontScaleChanged(float fontScale) {
        if (mWeakContentViewCore.get() == null) return;
        mTextTrackTextSize = androidFontScaleToPercentage(fontScale);
        notifySettingsChanged();
    }

    /**
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onLocaleChanged
     */
    public void onLocaleChanged(Locale locale) {}

    /**
     * Unlike the original method, use Chromium's CaptioningStyle since CaptionStyle is only
     * available on KitKat or higher. userStyle will never be null.
     *
     * @see android.view.accessibility.CaptioningManager.CaptioningChangeListener#onUserStyleChanged
     */
    public void onUserStyleChanged(CaptioningStyle userStyle) {
        if (mWeakContentViewCore.get() == null) return;

        mTextTrackTextColor = androidColorToCssColor(userStyle.getForegroundColor());
        mTextTrackBackgroundColor = androidColorToCssColor(userStyle.getBackgroundColor());

        final ClosedCaptionEdgeAttribute edge = ClosedCaptionEdgeAttribute.fromSystemEdgeAttribute(
                userStyle.getEdgeType(),
                androidColorToCssColor(userStyle.getEdgeColor()));

        mTextTrackTextShadow = edge.getTextShadow();

        final Typeface typeFace = userStyle.getTypeface();
        final ClosedCaptionFont font = ClosedCaptionFont.fromSystemFont(typeFace);
        mTextTrackFontFamily = font.getFontFamily();
        if (typeFace != null && typeFace.isItalic()) {
            mTextTrackFontStyle = FONT_STYLE_ITALIC;
        } else {
            mTextTrackFontStyle = DEFAULT_CAPTIONING_PREF_VALUE;
        }

        mTextTrackFontVariant = DEFAULT_CAPTIONING_PREF_VALUE;

        notifySettingsChanged();
    }

    /**
     * Construct a new CaptioningChangeDelegate object.
     *
     * @param contentViewCore the ContentViewCore to associate with
     *        this delegate
     */
    public CaptioningChangeDelegate(ContentViewCore contentViewCore) {
        mWeakContentViewCore = new WeakReference<ContentViewCore>(contentViewCore);
    }

    /**
     * Describes a character edge attribute for closed captioning
     */
    public static enum ClosedCaptionEdgeAttribute {
        NONE (""),
        OUTLINE ("%2$s %2$s 0 %1$s, -%2$s -%2$s 0 %1$s, %2$s -%2$s 0 %1$s, -%2$s %2$s 0 %1$s"),
        DROP_SHADOW ("%1$s %2$s %2$s 0.1em"),
        RAISED ("-%2$s -%2$s 0 %1$s"),
        DEPRESSED ("%2$s %2$s 0 %1$s");

        private static String sDefaultEdgeColor = "silver";
        private static String sShadowOffset = "0.05em";
        private static String sEdgeColor;
        private final String mTextShadow;

        private ClosedCaptionEdgeAttribute(String textShadow) {
            mTextShadow = textShadow;
        }

        /**
         * Create a {@link ClosedCaptionEdgeAttribute} object based on the type number.
         *
         * @param type The edge type value specified by the user
         * @param color The color of the edge (e.g. "red")
         * @return The enum object
         */
        public static ClosedCaptionEdgeAttribute fromSystemEdgeAttribute(Integer type,
                String color) {
            if (type == null) {
                return NONE;
            }
            if (color == null || color.isEmpty()) {
                sEdgeColor = sDefaultEdgeColor;
            } else {
                sEdgeColor = color;
            }
            // Lollipop adds support for EDGE_TYPE_DEPRESSED and EDGE_TYPE_RAISED.
            switch (type) {
                case CaptionStyle.EDGE_TYPE_OUTLINE:
                    return OUTLINE;
                case CaptionStyle.EDGE_TYPE_DROP_SHADOW:
                    return DROP_SHADOW;
                case CaptionStyle.EDGE_TYPE_RAISED:
                    return RAISED;
                case CaptionStyle.EDGE_TYPE_DEPRESSED:
                    return DEPRESSED;
                default:
                    // CaptionStyle.EDGE_TYPE_NONE
                    // CaptionStyle.EDGE_TYPE_UNSPECIFIED
                    return NONE;
            }
        }

        /**
         * Specify a new shadow offset for the edge attribute. This
         * will be used as both the horizontal and vertical
         * offset.
         *
         * @param shadowOffset the offset to be applied
         */
        public static void setShadowOffset(String shadowOffset) {
            sShadowOffset = shadowOffset;
        }

        /**
         * Default color to use if no color is specified when
         * using this enumeration. "silver" is the initial default.
         *
         * @param color The Color to use if none is specified
         *        when calling fromSystemEdgeAttribute
         */
        public static void setDefaultEdgeColor(String color) {
            sDefaultEdgeColor = color;
        }

        /**
         * Get the Text Shadow CSS property from the edge attribute.
         *
         * @return the CSS-friendly String representation of the
         *         edge attribute.
         */
        public String getTextShadow() {
            return String.format(mTextShadow, sEdgeColor, sShadowOffset);
        }
    }

    /**
     * Describes a font available for Closed Captioning
     */
    public static enum ClosedCaptionFont {
        DEFAULT (""),
        MONOSPACE ("monospace"),
        SANS_SERIF ("sans-serif"),
        SERIF ("serif");

        private final String mFontFamily;

        private ClosedCaptionFont(String fontFamily) {
            mFontFamily = fontFamily;
        }

        /**
         * Create a {@link ClosedCaptionFont} object based on provided Typeface.
         *
         * @param typeFace a Typeface object
         * @return a string representation of the typeface
         */
        public static ClosedCaptionFont fromSystemFont(Typeface typeFace) {
            if (typeFace == null || typeFace.equals(Typeface.DEFAULT)) {
                return DEFAULT;
            } else if (typeFace.equals(Typeface.MONOSPACE)) {
                return MONOSPACE;
            } else if (typeFace.equals(Typeface.SANS_SERIF)) {
                return SANS_SERIF;
            } else if (typeFace.equals(Typeface.SERIF)) {
                return SERIF;
            } else {
                // This includes Typeface.DEFAULT_BOLD since font-weight
                // is not yet supported as a WebKit setting for a VTTCue.
                return DEFAULT;
            }
        }

        /**
         * Get the font family CSS property from the edge attribute.
         *
         * @return the CSS-friendly String representation of the
         *         typeFace.
         */
        public String getFontFamily() {
            return mFontFamily;
        }
    }

    /**
     * Convert an Integer color to a "rgba" CSS style string
     *
     * @param color The Integer color to convert
     * @return a "rgba" CSS style string
     */
    public static String androidColorToCssColor(Integer color) {
        if (color == null) {
            return DEFAULT_CAPTIONING_PREF_VALUE;
        }
        // CSS uses values between 0 and 1 for the alpha level
        final String alpha = new DecimalFormat("#.##", new DecimalFormatSymbols(Locale.US)).format(
                Color.alpha(color) / 255.0);
        // Return a CSS string in the form rgba(r,g,b,a)
        return String.format("rgba(%s, %s, %s, %s)",
                Color.red(color), Color.green(color), Color.blue(color), alpha);
    }

    /**
     * Convert a font scale to a percentage String
     *
     * @param fontScale the font scale value to convert
     * @return a percentage value as a String (eg 50%)
     */
    public static String androidFontScaleToPercentage(float fontScale) {
        return new DecimalFormat("#%", new DecimalFormatSymbols(Locale.US)).format(fontScale);
    }

    private void notifySettingsChanged() {
        final ContentViewCore contentViewCore = mWeakContentViewCore.get();
        if (contentViewCore == null) return;
        if (mTextTrackEnabled) {
            final TextTrackSettings settings = new TextTrackSettings(
                    mTextTrackBackgroundColor, mTextTrackFontFamily, mTextTrackFontStyle,
                    mTextTrackFontVariant, mTextTrackTextColor, mTextTrackTextShadow,
                    mTextTrackTextSize);
            contentViewCore.setTextTrackSettings(settings);
        } else {
            contentViewCore.setTextTrackSettings(new TextTrackSettings());
        }
    }
}
