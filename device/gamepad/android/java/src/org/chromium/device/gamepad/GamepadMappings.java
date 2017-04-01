// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.gamepad;

import android.view.KeyEvent;
import android.view.MotionEvent;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;

/**
 * Class to manage mapping information related to each supported gamepad controller device.
 */
@JNINamespace("content")
abstract class GamepadMappings {
    @VisibleForTesting
    static final String NVIDIA_SHIELD_DEVICE_NAME_PREFIX = "NVIDIA Corporation NVIDIA Controller";
    @VisibleForTesting
    static final String MICROSOFT_XBOX_PAD_DEVICE_NAME = "Microsoft X-Box 360 pad";
    @VisibleForTesting
    static final String PS3_SIXAXIS_DEVICE_NAME = "Sony PLAYSTATION(R)3 Controller";
    @VisibleForTesting
    static final String SAMSUNG_EI_GP20_DEVICE_NAME = "Samsung Game Pad EI-GP20";
    @VisibleForTesting
    static final String AMAZON_FIRE_DEVICE_NAME = "Amazon Fire Game Controller";

    public static GamepadMappings getMappings(String deviceName, int[] axes) {
        if (deviceName.startsWith(NVIDIA_SHIELD_DEVICE_NAME_PREFIX)
                || deviceName.equals(MICROSOFT_XBOX_PAD_DEVICE_NAME)) {
            return new XboxCompatibleGamepadMappings();
        } else if (deviceName.equals(PS3_SIXAXIS_DEVICE_NAME)) {
            return new PS3SixAxisGamepadMappings();
        } else if (deviceName.equals(SAMSUNG_EI_GP20_DEVICE_NAME)) {
            return new SamsungEIGP20GamepadMappings();
        } else if (deviceName.equals(AMAZON_FIRE_DEVICE_NAME)) {
            return new AmazonFireGamepadMappings();
        }

        return new UnknownGamepadMappings(axes);
    }

    /**
     * Method that specifies whether the mappings are standard or not.
     * It should be overridden in subclasses that don't provide standard
     * mappings.
     */
    public boolean isStandard() {
        return true;
    }

    /**
     * Method implemented by subclasses to perform mapping from raw axes and buttons
     * to canonical axes and buttons.
     */
    public abstract void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
            float[] rawAxes, float[] rawButtons);

    private static void mapCommonXYABButtons(float[] mappedButtons, float[] rawButtons) {
        float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
        float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
        float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
        float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
        mappedButtons[CanonicalButtonIndex.PRIMARY] = a;
        mappedButtons[CanonicalButtonIndex.SECONDARY] = b;
        mappedButtons[CanonicalButtonIndex.TERTIARY] = x;
        mappedButtons[CanonicalButtonIndex.QUATERNARY] = y;
    }

    private static void mapCommonStartSelectMetaButtons(float[] mappedButtons, float[] rawButtons) {
        float start = rawButtons[KeyEvent.KEYCODE_BUTTON_START];
        float select = rawButtons[KeyEvent.KEYCODE_BUTTON_SELECT];
        float mode = rawButtons[KeyEvent.KEYCODE_BUTTON_MODE];
        mappedButtons[CanonicalButtonIndex.START] = start;
        mappedButtons[CanonicalButtonIndex.BACK_SELECT] = select;
        mappedButtons[CanonicalButtonIndex.META] = mode;
    }

    private static void mapCommonThumbstickButtons(float[] mappedButtons, float[] rawButtons) {
        float thumbL = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBL];
        float thumbR = rawButtons[KeyEvent.KEYCODE_BUTTON_THUMBR];
        mappedButtons[CanonicalButtonIndex.LEFT_THUMBSTICK] = thumbL;
        mappedButtons[CanonicalButtonIndex.RIGHT_THUMBSTICK] = thumbR;
    }

    /**
     * Method for mapping the L1/R1 buttons to lower shoulder buttons, rather than
     * upper shoulder as the user would normally expect. Please think twice before
     * using this, as it can easily confuse the user. It is only really useful if
     * the controller completely lacks a second set of shoulder buttons.
     */
    private static void mapUpperTriggerButtonsToBottomShoulder(float[] mappedButtons,
            float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = r1;
    }

    private static void mapTriggerButtonsToTopShoulder(float[] mappedButtons, float[] rawButtons) {
        float l1 = rawButtons[KeyEvent.KEYCODE_BUTTON_L1];
        float r1 = rawButtons[KeyEvent.KEYCODE_BUTTON_R1];
        mappedButtons[CanonicalButtonIndex.LEFT_SHOULDER] = l1;
        mappedButtons[CanonicalButtonIndex.RIGHT_SHOULDER] = r1;
    }

    private static void mapCommonDpadButtons(float[] mappedButtons, float[] rawButtons) {
        float dpadDown = rawButtons[KeyEvent.KEYCODE_DPAD_DOWN];
        float dpadUp = rawButtons[KeyEvent.KEYCODE_DPAD_UP];
        float dpadLeft = rawButtons[KeyEvent.KEYCODE_DPAD_LEFT];
        float dpadRight = rawButtons[KeyEvent.KEYCODE_DPAD_RIGHT];
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = dpadDown;
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = dpadUp;
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = dpadLeft;
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = dpadRight;
    }

    private static void mapXYAxes(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_X] = rawAxes[MotionEvent.AXIS_X];
        mappedAxes[CanonicalAxisIndex.LEFT_STICK_Y] = rawAxes[MotionEvent.AXIS_Y];
    }

    private static void mapRXAndRYAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_RX];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RY];
    }

    private static void mapZAndRZAxesToRightStick(float[] mappedAxes, float[] rawAxes) {
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rawAxes[MotionEvent.AXIS_Z];
        mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rawAxes[MotionEvent.AXIS_RZ];
    }

    private static void mapPedalAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_BRAKE];
        float rTrigger = rawAxes[MotionEvent.AXIS_GAS];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    private static void mapTriggerAxesToBottomShoulder(float[] mappedButtons, float[] rawAxes) {
        float lTrigger = rawAxes[MotionEvent.AXIS_LTRIGGER];
        float rTrigger = rawAxes[MotionEvent.AXIS_RTRIGGER];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
    }

    private static void mapLowerTriggerButtonsToBottomShoulder(float[] mappedButtons,
            float[] rawButtons) {
        float l2 = rawButtons[KeyEvent.KEYCODE_BUTTON_L2];
        float r2 = rawButtons[KeyEvent.KEYCODE_BUTTON_R2];
        mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = l2;
        mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = r2;
    }

    @VisibleForTesting
    static float negativeAxisValueAsButton(float input) {
        return (input < -0.5f) ? 1.f : 0.f;
    }

    @VisibleForTesting
    static float positiveAxisValueAsButton(float input) {
        return (input > 0.5f) ? 1.f : 0.f;
    }

    private static void mapHatAxisToDpadButtons(float[] mappedButtons, float[] rawAxes) {
        float hatX = rawAxes[MotionEvent.AXIS_HAT_X];
        float hatY = rawAxes[MotionEvent.AXIS_HAT_Y];
        mappedButtons[CanonicalButtonIndex.DPAD_LEFT] = negativeAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_RIGHT] = positiveAxisValueAsButton(hatX);
        mappedButtons[CanonicalButtonIndex.DPAD_UP] = negativeAxisValueAsButton(hatY);
        mappedButtons[CanonicalButtonIndex.DPAD_DOWN] = positiveAxisValueAsButton(hatY);
    }

    private static class AmazonFireGamepadMappings extends GamepadMappings {

        /**
         * Method for mapping Amazon Fire gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
                float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapPedalAxesToBottomShoulder(mappedButtons, rawAxes);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class XboxCompatibleGamepadMappings extends GamepadMappings {

        /**
         * Method for mapping Xbox 360-compatible gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
                float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class PS3SixAxisGamepadMappings extends GamepadMappings {

        /**
         * Method for mapping PS3 gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
                float[] rawAxes, float[] rawButtons) {
            // On PS3 X/Y has higher priority.
            float a = rawButtons[KeyEvent.KEYCODE_BUTTON_A];
            float b = rawButtons[KeyEvent.KEYCODE_BUTTON_B];
            float x = rawButtons[KeyEvent.KEYCODE_BUTTON_X];
            float y = rawButtons[KeyEvent.KEYCODE_BUTTON_Y];
            mappedButtons[CanonicalButtonIndex.PRIMARY] = x;
            mappedButtons[CanonicalButtonIndex.SECONDARY] = y;
            mappedButtons[CanonicalButtonIndex.TERTIARY] = a;
            mappedButtons[CanonicalButtonIndex.QUATERNARY] = b;

            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonDpadButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapTriggerAxesToBottomShoulder(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapZAndRZAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class SamsungEIGP20GamepadMappings extends GamepadMappings {

        /**
         * Method for mapping PS3 gamepad axis and button values
         * to standard gamepad button and axes values.
         */
        @Override
        public void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
                float[] rawAxes, float[] rawButtons) {
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapUpperTriggerButtonsToBottomShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapHatAxisToDpadButtons(mappedButtons, rawAxes);

            mapXYAxes(mappedAxes, rawAxes);
            mapRXAndRYAxesToRightStick(mappedAxes, rawAxes);
        }
    }

    private static class UnknownGamepadMappings extends GamepadMappings {
        private int mLeftTriggerAxis = -1;
        private int mRightTriggerAxis = -1;
        private int mRightStickXAxis = -1;
        private int mRightStickYAxis = -1;
        private boolean mUseHatAxes;

        UnknownGamepadMappings(int[] axes) {
            int hatAxesFound = 0;

            for (int axis : axes) {
                switch (axis) {
                    case MotionEvent.AXIS_LTRIGGER:
                    case MotionEvent.AXIS_BRAKE:
                        mLeftTriggerAxis = axis;
                        break;
                    case MotionEvent.AXIS_RTRIGGER:
                    case MotionEvent.AXIS_GAS:
                    case MotionEvent.AXIS_THROTTLE:
                        mRightTriggerAxis = axis;
                        break;
                    case MotionEvent.AXIS_RX:
                    case MotionEvent.AXIS_Z:
                        mRightStickXAxis = axis;
                        break;
                    case MotionEvent.AXIS_RY:
                    case MotionEvent.AXIS_RZ:
                        mRightStickYAxis = axis;
                        break;
                    case MotionEvent.AXIS_HAT_X:
                        hatAxesFound++;
                        break;
                    case MotionEvent.AXIS_HAT_Y:
                        hatAxesFound++;
                        break;
                    default:
                        break;
                }
            }

            if (hatAxesFound == 2) {
                mUseHatAxes = true;
            }
        }

        @Override
        public boolean isStandard() {
            // These mappings should not be considered standard
            return false;
        }

        @Override
        public void mapToStandardGamepad(float[] mappedAxes, float[] mappedButtons,
                float[] rawAxes, float[] rawButtons) {
            // These are shared among all gamepads intended for use with Android
            // that we tested so far.
            mapCommonXYABButtons(mappedButtons, rawButtons);
            mapTriggerButtonsToTopShoulder(mappedButtons, rawButtons);
            mapCommonThumbstickButtons(mappedButtons, rawButtons);
            mapCommonStartSelectMetaButtons(mappedButtons, rawButtons);
            mapXYAxes(mappedAxes, rawAxes);

            if (mLeftTriggerAxis != -1 && mRightTriggerAxis != -1) {
                float lTrigger = rawAxes[mLeftTriggerAxis];
                float rTrigger = rawAxes[mRightTriggerAxis];
                mappedButtons[CanonicalButtonIndex.LEFT_TRIGGER] = lTrigger;
                mappedButtons[CanonicalButtonIndex.RIGHT_TRIGGER] = rTrigger;
            } else {
                // Devices without analog triggers use digital buttons
                mapLowerTriggerButtonsToBottomShoulder(mappedButtons, rawButtons);
            }

            if (mRightStickXAxis != -1 && mRightStickYAxis != -1) {
                float rX = rawAxes[mRightStickXAxis];
                float rY = rawAxes[mRightStickYAxis];
                mappedAxes[CanonicalAxisIndex.RIGHT_STICK_X] = rX;
                mappedAxes[CanonicalAxisIndex.RIGHT_STICK_Y] = rY;
            }

            if (mUseHatAxes) {
                mapHatAxisToDpadButtons(mappedButtons, rawAxes);
            } else {
                mapCommonDpadButtons(mappedButtons, rawButtons);
            }
        }
    }
}
