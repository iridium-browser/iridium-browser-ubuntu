// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.content.browser.test.util.DOMUtils;
import org.chromium.content.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content.browser.test.util.TestInputMethodManagerWrapper.Range;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_shell_apk.ContentShellTestBase;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for text input using cases based on fixed regressions.
 */
public class ImeTest extends ContentShellTestBase {

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=2.0, maximum-scale=2.0\" /></head>"
            + "<body><form action=\"about:blank\">"
            + "<input id=\"input_text\" type=\"text\" /><br/>"
            + "<input id=\"input_radio\" type=\"radio\" style=\"width:50px;height:50px\" />"
            + "<br/><textarea id=\"textarea\" rows=\"4\" cols=\"20\"></textarea>"
            + "<br/><textarea id=\"textarea2\" rows=\"4\" cols=\"20\" autocomplete=\"off\">"
            + "</textarea>"
            + "<br/><p><span id=\"plain_text\">This is Plain Text One</span></p>"
            + "</form></body></html>");
    private static final int COMPOSITION_KEY_CODE = 229;

    private TestAdapterInputConnection mConnection;
    private ImeAdapter mImeAdapter;

    private ContentViewCore mContentViewCore;
    private WebContents mWebContents;
    private TestCallbackHelperContainer mCallbackContainer;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        launchContentShellWithUrl(DATA_URL);
        assertTrue("Page failed to load", waitForActiveShellToBeDoneLoading());
        mContentViewCore = getContentViewCore();
        mWebContents = getWebContents();

        mInputMethodManagerWrapper = new TestInputMethodManagerWrapper(mContentViewCore);
        getImeAdapter().setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        assertEquals(0, mInputMethodManagerWrapper.getShowSoftInputCounter());
        mContentViewCore.setAdapterInputConnectionFactory(
                new TestAdapterInputConnectionFactory());

        mCallbackContainer = new TestCallbackHelperContainer(mContentViewCore);
        // TODO(aurimas) remove this wait once crbug.com/179511 is fixed.
        assertWaitForPageScaleFactorMatch(2);
        assertTrue(DOMUtils.waitForNonZeroNodeBounds(
                mWebContents, "input_text"));
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);

        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        mImeAdapter = getImeAdapter();

        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelStart);
        assertEquals(0, mInputMethodManagerWrapper.getEditorInfo().initialSelEnd);
    }

    @MediumTest
    @Feature({"TextInput", "Main"})
    public void testKeyboardDismissedAfterClickingGo() throws Throwable {
        setComposingText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, 0, 5);

        performGo(getAdapterInputConnection(), mCallbackContainer);

        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "", 0, 0, -1, -1);
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    @RerunWithUpdatedContainerView
    public void testGetTextUpdatesAfterEnteringText() throws Throwable {
        setComposingText(mConnection, "h", 1);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 1, "h", 1, 1, 0, 1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText(mConnection, "he", 1);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 2, "he", 2, 2, 0, 2);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        setComposingText(mConnection, "hel", 1);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 3, "hel", 3, 3, 0, 3);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());

        commitText(mConnection, "hel", 1);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 4, "hel", 3, 3, -1, -1);
        assertEquals(1, mInputMethodManagerWrapper.getShowSoftInputCounter());
    }

    @SmallTest
    @Feature({"TextInput"})
    @RerunWithUpdatedContainerView
    public void testImeCopy() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        setSelection(mConnection, 2, 5);
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 2, "hello", 2, 5, -1, -1);

        copy(mImeAdapter);
        assertClipboardContents(getActivity(), "llo");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testEnterTextAndRefocus() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        DOMUtils.clickNode(this, mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertEquals(5, mInputMethodManagerWrapper.getEditorInfo().initialSelStart);
        assertEquals(5, mInputMethodManagerWrapper.getEditorInfo().initialSelEnd);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testKeyboardNotDismissedAfterCopySelection() throws Exception {
        commitText(mConnection, "Sample Text", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "Sample Text", 11, 11, -1, -1);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        selectAll(mImeAdapter);
        copy(mImeAdapter);
        assertWaitForKeyboardStatus(true);
        assertEquals(11, Selection.getSelectionEnd(mContentViewCore.getEditableForTest()));
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotDismissedAfterCutSelection() throws Exception {
        commitText(mConnection, "Sample Text", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "Sample Text", 11, 11, -1, -1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
        assertWaitForKeyboardStatus(true);
        cut(mImeAdapter);
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingEmptyInput() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_radio");
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarShownOnLongPressingInput() throws Exception {
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(true);
    }

    /*
    @SmallTest
    @Feature({"TextInput"})
    */
    @DisabledTest
    public void testSelectActionBarClearedOnTappingInput() throws Exception {
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_text");
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testSelectActionBarClearedOnTappingOutsideInput() throws Exception {
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        assertWaitForSelectActionBarStatus(true);
        DOMUtils.clickNode(this, mContentViewCore, "input_radio");
        assertWaitForKeyboardStatus(false);
        assertWaitForSelectActionBarStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeNotShownOnLongPressingDifferentEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_radio");
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(false);
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeStaysOnLongPressingDifferentNonEmptyInputs() throws Exception {
        DOMUtils.focusNode(mWebContents, "input_text");
        assertWaitForKeyboardStatus(true);
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.focusNode(mWebContents, "textarea");
        commitText(mConnection, "Sample Text", 1);
        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        assertWaitForKeyboardStatus(true);
        DOMUtils.longPressNode(this, mContentViewCore, "textarea");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeCut() throws Exception {
        commitText(mConnection, "snarful", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "snarful", 7, 7, -1, -1);

        setSelection(mConnection, 1, 5);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "snarful", 1, 5, -1, -1);

        cut(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "sul", 1, 1, -1, -1);

        assertClipboardContents(getActivity(), "narf");
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImePaste() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClipboardManager clipboardManager =
                        (ClipboardManager) getActivity().getSystemService(
                                Context.CLIPBOARD_SERVICE);
                clipboardManager.setPrimaryClip(ClipData.newPlainText("blarg", "blarg"));
            }
        });

        paste(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "blarg", 5, 5, -1, -1);

        setSelection(mConnection, 3, 5);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "blarg", 3, 5, -1, -1);

        paste(mImeAdapter);
        // Paste is a two step process when there is a non-zero selection.
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 3, "bla", 3, 3, -1, -1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 4, "blablarg", 8, 8, -1, -1);

        paste(mImeAdapter);
        waitAndVerifyStatesAndCalls(
                mConnection.mImeUpdateQueue, 5, "blablargblarg", 13, 13, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testImeSelectAndUnSelectAll() throws Exception {
        commitText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        selectAll(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "hello", 0, 5, -1, -1);

        unselect(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "", 0, 0, -1, -1);

        assertWaitForKeyboardStatus(false);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testShowImeIfNeeded() throws Throwable {
        // showImeIfNeeded() is now implicitly called by the updated focus
        // heuristic so no need to call explicitly. http://crbug.com/371927
        DOMUtils.focusNode(mWebContents, "input_radio");
        assertWaitForKeyboardStatus(false);

        DOMUtils.focusNode(mWebContents, "input_text");
        assertWaitForKeyboardStatus(true);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testFinishComposingText() throws Throwable {
        DOMUtils.focusNode(mWebContents, "input_radio");
        assertWaitForKeyboardStatus(false);

        focusElement("textarea");

        commitText(mConnection, "hllo", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hllo", 4, 4, -1, -1);

        commitText(mConnection, " ", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "hllo ", 5, 5, -1, -1);

        setSelection(mConnection, 1, 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "hllo ", 1, 1, -1, -1);

        setComposingRegion(mConnection, 0, 4);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 4, "hllo ", 1, 1, 0, 4);

        finishComposingText(mConnection);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 5, "hllo ", 1, 1, -1, -1);

        commitText(mConnection, "\n", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 6, "h\nllo ", 2, 2, -1, -1);
    }

    private int getTypedKeycodeGuess(String before, String after) {
        KeyEvent ev = ImeAdapter.getTypedKeyEventGuess(before, after);
        if (ev == null) return -1;
        return ev.getKeyCode();
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testGuessedKeyCodeFromTyping() throws Throwable {
        assertEquals(-1, getTypedKeycodeGuess(null, ""));
        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess(null, "x"));
        assertEquals(-1, getTypedKeycodeGuess(null, "xyz"));

        assertEquals(-1, getTypedKeycodeGuess("abc", "abc"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("abc", ""));

        assertEquals(KeyEvent.KEYCODE_H, getTypedKeycodeGuess("", "h"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("h", ""));
        assertEquals(KeyEvent.KEYCODE_E, getTypedKeycodeGuess("h", "he"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("he", "hel"));
        assertEquals(KeyEvent.KEYCODE_O, getTypedKeycodeGuess("hel", "helo"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("helo", "hel"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("hel", "hell"));
        assertEquals(KeyEvent.KEYCODE_L, getTypedKeycodeGuess("hell", "helll"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("helll", "hell"));
        assertEquals(KeyEvent.KEYCODE_O, getTypedKeycodeGuess("hell", "hello"));

        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess("xxx", "xxxx"));
        assertEquals(KeyEvent.KEYCODE_X, getTypedKeycodeGuess("xxx", "xxxxx"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("xxx", "xx"));
        assertEquals(KeyEvent.KEYCODE_DEL, getTypedKeycodeGuess("xxx", "x"));

        assertEquals(KeyEvent.KEYCODE_Y, getTypedKeycodeGuess("xxx", "xxxy"));
        assertEquals(KeyEvent.KEYCODE_Y, getTypedKeycodeGuess("xxx", "xxxxy"));
        assertEquals(-1, getTypedKeycodeGuess("xxx", "xy"));
        assertEquals(-1, getTypedKeycodeGuess("xxx", "y"));

        assertEquals(-1, getTypedKeycodeGuess("foo", "bar"));
        assertEquals(-1, getTypedKeycodeGuess("foo", "bars"));
        assertEquals(-1, getTypedKeycodeGuess("foo", "ba"));

        // Some characters also require modifiers so we have to check the full event.
        KeyEvent ev = ImeAdapter.getTypedKeyEventGuess(null, "!");
        assertEquals(KeyEvent.KEYCODE_1, ev.getKeyCode());
        assertTrue(ev.isShiftPressed());
    }

    /*
    @SmallTest
    @Feature({"TextInput", "Main"})
    http://crbug.com/445499
    */
    @DisabledTest
    public void testKeyCodesWhileComposingText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the noted key is touched on screen.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "ho", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        setComposingRegion(mConnection, 0, 1);  // DEL calls cancelComposition() then restarts
        setComposingText(mConnection, "h", 1);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // I
        setComposingText(mConnection, "hi", 1);
        assertEquals(KeyEvent.KEYCODE_I, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // SPACE
        commitText(mConnection, "hi", 1);
        assertEquals(-1, mImeAdapter.mLastSyntheticKeyCode);
        commitText(mConnection, " ", 1);
        assertEquals(KeyEvent.KEYCODE_SPACE, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        setComposingRegion(mConnection, 0, 2);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        setComposingText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        commitText(mConnection, "", 1);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(mConnection, 1, 0);  // DEL on empty still sends 1,0
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    /*
    @SmallTest
    @Feature({"TextInput", "Main"})
    http://crbug.com/445499
    */
    @DisabledTest
    public void testKeyCodesWhileSwipingText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the stock Google Keyboard (Android 4.4) sends
        // when the word is swiped on the soft keyboard.  Exercise care when altering to make sure
        // that the test reflects reality.  If this test breaks, it's possible that code has
        // changed and different calls need to be made instead.
        // "three"
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "three", 1);
        assertEquals(KeyEvent.KEYCODE_UNKNOWN, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("three", mConnection.getTextBeforeCursor(99, 0));

        // "word"
        commitText(mConnection, "three", 1);
        commitText(mConnection, " ", 1);
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "word", 1);
        assertEquals(KeyEvent.KEYCODE_UNKNOWN, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("three word", mConnection.getTextBeforeCursor(99, 0));

        // "test"
        commitText(mConnection, "word", 1);
        commitText(mConnection, " ", 1);
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "test", 1);
        assertEquals(KeyEvent.KEYCODE_UNKNOWN, mImeAdapter.mLastSyntheticKeyCode);
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("three word test", mConnection.getTextBeforeCursor(99, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDeleteMultiCharacterCodepoint() throws Throwable {
        // This smiley is a multi character codepoint.
        final String smiley = "\uD83D\uDE0A";

        commitText(mConnection, smiley, 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, smiley, 2, 2, -1, -1);

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "", 0, 0, -1, -1);

        // Make sure that we accept further typing after deleting the smiley.
        setComposingText(mConnection, "s", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "s", 1, 1, 0, 1);
        setComposingText(mConnection, "sm", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 4, "sm", 2, 2, 0, 2);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "o", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL, sent via dispatchKeyEvent like it is in Android WebView or a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        expectUpdateStateCall(mConnection);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testRepeatBackspaceKeycode() throws Throwable {
        focusElement("textarea");

        // H
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "o", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // Multiple keydowns should each delete one character (this is for physical keyboard
        // key-repeat).
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));

        // DEL
        expectUpdateStateCall(mConnection);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testKeyCodesWhileTypingText() throws Throwable {
        focusElement("textarea");

        // The calls below are a reflection of what the Hacker's Keyboard sends when the noted
        // key is touched on screen.  Exercise care when altering to make sure that the test
        // reflects reality.
        // H
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "h", 1);
        assertEquals(KeyEvent.KEYCODE_H, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // O
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "o", 1);
        assertEquals(KeyEvent.KEYCODE_O, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("ho", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // I
        expectUpdateStateCall(mConnection);
        commitText(mConnection, "i", 1);
        assertEquals(KeyEvent.KEYCODE_I, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // SPACE
        expectUpdateStateCall(mConnection);
        commitText(mConnection, " ", 1);
        assertEquals(KeyEvent.KEYCODE_SPACE, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi ", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hi", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));

        // DEL
        expectUpdateStateCall(mConnection);
        deleteSurroundingText(mConnection, 1, 0);
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));

        // DEL (on empty input)
        deleteSurroundingText(mConnection, 1, 0);  // DEL on empty still sends 1,0
        assertEquals(KeyEvent.KEYCODE_DEL, mImeAdapter.mLastSyntheticKeyCode);
        assertEquals("", mConnection.getTextBeforeCursor(9, 0));
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard() throws Throwable {
        focusElement("textarea");
        // Type 'a' using a physical keyboard.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_A));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "a", 1, 1, -1, -1);

        // Type 'enter' key.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "a\n\n", 2, 2, -1, -1);

        // Type 'b'.
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "a\nb", 3, 3, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testPhysicalKeyboard_AccentKeyCodes() throws Throwable {
        focusElement("textarea");

        // h
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_H));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_H));
        assertEquals("h", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "h", 1, 1, -1, -1);

        // ALT-i  (circumflex accent key on virtual keyboard)
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hˆ", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "hˆ", 2, 2, 1, 2);

        // o
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_O));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hô", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_O));
        assertEquals("hô", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "hô", 2, 2, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(mConnection, 1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 4, "hôˆ", 3, 3, 2, 3);

        // ALT-i again should have no effect
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(mConnection, 1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆ", mConnection.getTextBeforeCursor(9, 0));

        // b (cannot be accented, should just appear after)
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_B));
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        // A transitional state due to finishComposingText.
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 5, "hôˆ", 3, 3, -1, -1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 6, "hôˆb", 4, 4, -1, -1);

        // ALT-i
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertUpdateStateCall(mConnection, 1000);
        dispatchKeyEvent(new KeyEvent(
                0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_I, 0, KeyEvent.META_ALT_ON));
        assertEquals("hôˆbˆ", mConnection.getTextBeforeCursor(9, 0));
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 7, "hôˆbˆ", 5, 5, 4, 5);

        // Backspace
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DEL));
        assertUpdateStateCall(mConnection, 1000);
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DEL));
        assertEquals("hôˆb", mConnection.getTextBeforeCursor(9, 0));
        // A transitional state due to finishComposingText in deleteSurroundingTextImpl.
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 8, "hôˆbˆ", 5, 5, -1, -1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 9, "hôˆb", 4, 4, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testSetComposingRegionOutOfBounds() throws Throwable {
        focusElement("textarea");
        setComposingText(mConnection, "hello", 1);

        setComposingRegion(mConnection, 0, 0);
        setComposingRegion(mConnection, 0, 9);
        setComposingRegion(mConnection, 9, 0);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_AfterCommitText() throws Throwable {
        focusElement("textarea");

        commitText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
        // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "hello\n\n", 6, 6, -1, -1);

        commitText(mConnection, "world", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "hello\nworld", 11, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testEnterKey_WhileComposingText() throws Throwable {
        focusElement("textarea");

        setComposingText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, 0, 5);

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));

        // TODO(aurimas): remove this workaround when crbug.com/278584 is fixed.
        // A transitional state due to finishComposingText.
        waitAndVerifyStates(mConnection.mImeUpdateQueue, 2, "hello", 5, 5, -1, -1);
        // The second new line is not a user visible/editable one, it is a side-effect of Blink
        // using <br> internally. This only happens when \n is at the end.
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 3, "hello\n\n", 6, 6, -1, -1);

        commitText(mConnection, "world", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 4, "hello\nworld", 11, 11, -1, -1);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testDpadKeyCodesWhileSwipingText() throws Throwable {
        focusElement("textarea");

        // DPAD_CENTER should cause keyboard to appear
        expectUpdateStateCall(mConnection);
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_CENTER));
        assertUpdateStateCall(mConnection, 1000);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testTransitionsWhileComposingText() throws Throwable {
        focusElement("textarea"); // Default with autocomplete="on"

        // H
        // Since autocomplete="on" by default, COMPOSITION_KEY_CODE is emitted as key code
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(COMPOSITION_KEY_CODE, mImeAdapter.mLastSyntheticKeyCode);

        // Simulate switch of input fields.
        finishComposingText(mConnection);

        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(COMPOSITION_KEY_CODE, mImeAdapter.mLastSyntheticKeyCode);
    }

    @SmallTest
    @Feature({"TextInput", "Main"})
    public void testTransitionsWhileEmittingKeyCode() throws Throwable {
        focusElement("textarea2"); // Default with autocomplete="off"

        // H
        // Although autocomplete="off", we still emit COMPOSITION_KEY_CODE since synthesized
        // keycodes are disabled.
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(COMPOSITION_KEY_CODE, mImeAdapter.mLastSyntheticKeyCode);

        // Simulate switch of input fields.
        finishComposingText(mConnection);

        // H
        expectUpdateStateCall(mConnection);
        setComposingText(mConnection, "h", 1);
        assertEquals(COMPOSITION_KEY_CODE, mImeAdapter.mLastSyntheticKeyCode);
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testPastePopupShowOnLongPress() throws Throwable {
        commitText(mConnection, "hello", 1);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 1, "hello", 5, 5, -1, -1);

        selectAll(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 2, "hello", 0, 5, -1, -1);

        cut(mImeAdapter);
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);

        DOMUtils.longPressNode(this, mContentViewCore, "input_text");
        final PastePopupMenu pastePopup = mContentViewCore.getPastePopupForTest();
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return pastePopup.isShowing();
            }
        }));
    }

    @SmallTest
    @Feature({"TextInput"})
    public void testTextHandlesPreservedWithDpadNavigation() throws Throwable {
        DOMUtils.longPressNode(this, mContentViewCore, "plain_text");
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mContentViewCore.hasSelection());

        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_DOWN));
        assertWaitForSelectActionBarStatus(true);
        assertTrue(mContentViewCore.hasSelection());
    }

    private void performGo(final AdapterInputConnection inputConnection,
            TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                    }
                });
    }

    private void assertWaitForKeyboardStatus(final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == getImeAdapter().mIsShowWithoutHideOutstanding
                        && (!show || getAdapterInputConnection() != null);
            }
        }));
    }

    private void assertWaitForSelectActionBarStatus(
            final boolean show) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return show == mContentViewCore.isSelectActionBarShowing();
            }
        }));
    }

    private void waitAndVerifyStates(final ArrayList<TestImeState> states, final int index,
            String text, final int selectionStart, final int selectionEnd,
            final int compositionStart, final int compositionEnd) throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return states.size() > index;
            }
        }));
        states.get(index).assertEqualState(
                text, selectionStart, selectionEnd, compositionStart, compositionEnd);
    }

    private void waitAndVerifyStatesAndCalls(final ArrayList<TestImeState> states, final int index,
            String text, final int selectionStart, final int selectionEnd,
            final int compositionStart, final int compositionEnd) throws InterruptedException {
        waitAndVerifyStates(states, index, text, selectionStart, selectionEnd, compositionStart,
                compositionEnd);

        // Wait and verify calls to InputMethodManager.
        final Range selection = new Range(selectionStart, selectionEnd);
        final Range composition = new Range(compositionStart, compositionEnd);
        assertTrue("Actual selection was: " + mInputMethodManagerWrapper.getSelection()
                        + ", and actual composition was: "
                        + mInputMethodManagerWrapper.getComposition(),
                CriteriaHelper.pollForCriteria(new Criteria() {
                    @Override
                    public boolean isSatisfied() {
                        return mInputMethodManagerWrapper.getSelection().equals(selection)
                                && mInputMethodManagerWrapper.getComposition().equals(composition);
                    }
                }));
    }

    private void expectUpdateStateCall(final TestAdapterInputConnection connection) {
        connection.mImeUpdateQueue.clear();
    }

    private void assertUpdateStateCall(final TestAdapterInputConnection connection, int maxms)
            throws Exception {
        while (connection.mImeUpdateQueue.size() == 0 && maxms > 0) {
            try {
                Thread.sleep(50);
            } catch (Exception e) {
                // Not really a problem since we're just going to sleep again.
            }
            maxms -= 50;
        }
        assertTrue(connection.mImeUpdateQueue.size() > 0);
    }

    private void assertClipboardContents(final Activity activity, final String expectedContents)
            throws InterruptedException {
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        ClipboardManager clipboardManager =
                                (ClipboardManager) activity.getSystemService(
                                        Context.CLIPBOARD_SERVICE);
                        ClipData clip = clipboardManager.getPrimaryClip();
                        return clip != null && clip.getItemCount() == 1
                                && TextUtils.equals(clip.getItemAt(0).getText(), expectedContents);
                    }
                });
            }
        }));
    }

    private ImeAdapter getImeAdapter() {
        return mContentViewCore.getImeAdapterForTest();
    }

    private AdapterInputConnection getAdapterInputConnection() {
        return mContentViewCore.getInputConnectionForTest();
    }

    private void copy(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.copy();
            }
        });
    }

    private void cut(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.cut();
            }
        });
    }

    private void paste(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.paste();
            }
        });
    }

    private void selectAll(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.selectAll();
            }
        });
    }

    private void unselect(final ImeAdapter adapter) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                adapter.unselect();
            }
        });
    }

    private void commitText(final AdapterInputConnection connection, final CharSequence text,
            final int newCursorPosition) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.commitText(text, newCursorPosition);
            }
        });
    }

    private void setSelection(final AdapterInputConnection connection, final int start,
            final int end) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setSelection(start, end);
            }
        });
    }

    private void setComposingRegion(final AdapterInputConnection connection, final int start,
            final int end) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingRegion(start, end);
            }
        });
    }

    private void setComposingText(final AdapterInputConnection connection, final CharSequence text,
            final int newCursorPosition) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.setComposingText(text, newCursorPosition);
            }
        });
    }

    private void finishComposingText(final AdapterInputConnection connection) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.finishComposingText();
            }
        });
    }

    private void deleteSurroundingText(final AdapterInputConnection connection, final int before,
            final int after) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                connection.deleteSurroundingText(before, after);
            }
        });
    }

    private void dispatchKeyEvent(final KeyEvent event) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mImeAdapter.dispatchKeyEvent(event);
            }
        });
    }

    private void focusElement(final String id) throws InterruptedException, TimeoutException {
        DOMUtils.focusNode(mWebContents, id);
        assertWaitForKeyboardStatus(true);
        assertTrue(CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return id.equals(DOMUtils.getFocusedNode(mWebContents));
                } catch (Exception e) {
                    return false;
                }
            }
        }));
        // When we focus another element, the connection may be recreated.
        mConnection = (TestAdapterInputConnection) getAdapterInputConnection();
        waitAndVerifyStatesAndCalls(mConnection.mImeUpdateQueue, 0, "", 0, 0, -1, -1);
    }

    private static class TestAdapterInputConnectionFactory extends
            ImeAdapter.AdapterInputConnectionFactory {
        @Override
        public AdapterInputConnection get(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            return new TestAdapterInputConnection(view, imeAdapter, editable, outAttrs);
        }
    }

    private static class TestAdapterInputConnection extends AdapterInputConnection {
        private final ArrayList<TestImeState> mImeUpdateQueue = new ArrayList<TestImeState>();

        public TestAdapterInputConnection(View view, ImeAdapter imeAdapter,
                Editable editable, EditorInfo outAttrs) {
            super(view, imeAdapter, editable, outAttrs);
        }

        @Override
        public void updateState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd, boolean requiredAck) {
            mImeUpdateQueue.add(new TestImeState(text, selectionStart, selectionEnd,
                    compositionStart, compositionEnd));
            super.updateState(text, selectionStart, selectionEnd, compositionStart,
                    compositionEnd, requiredAck);
        }
    }

    private static class TestImeState {
        private final String mText;
        private final int mSelectionStart;
        private final int mSelectionEnd;
        private final int mCompositionStart;
        private final int mCompositionEnd;

        public TestImeState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mCompositionStart = compositionStart;
            mCompositionEnd = compositionEnd;
        }

        public void assertEqualState(String text, int selectionStart, int selectionEnd,
                int compositionStart, int compositionEnd) {
            assertEquals("Text did not match", text, mText);
            assertEquals("Selection start did not match", selectionStart, mSelectionStart);
            assertEquals("Selection end did not match", selectionEnd, mSelectionEnd);
            assertEquals("Composition start did not match", compositionStart, mCompositionStart);
            assertEquals("Composition end did not match", compositionEnd, mCompositionEnd);
        }
    }
}
