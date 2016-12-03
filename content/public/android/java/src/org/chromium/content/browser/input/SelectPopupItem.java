// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.chromium.ui.DropdownItem;

/**
 * Select popup item containing the label, the type and the enabled state
 * of an item belonging to a select popup dialog.
 */
public class SelectPopupItem implements DropdownItem {
    private final String mLabel;
    private final int mType;

    public SelectPopupItem(String label, int type) {
        mLabel = label;
        mType = type;
    }

    @Override
    public String getLabel() {
        return mLabel;
    }

    @Override
    public String getSublabel() {
        return null;
    }

    @Override
    public int getIconId() {
        return NO_ICON;
    }

    @Override
    public boolean isEnabled() {
        return mType == PopupItemType.ENABLED || mType == PopupItemType.GROUP;
    }

    @Override
    public boolean isGroupHeader() {
        return mType == PopupItemType.GROUP;
    }

    @Override
    public boolean isMultilineLabel() {
        return false;
    }

    public int getType() {
        return mType;
    }
}
