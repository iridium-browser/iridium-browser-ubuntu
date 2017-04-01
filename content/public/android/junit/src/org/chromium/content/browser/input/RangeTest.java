// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.chromium.base.test.util.Feature;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/**
 * Unit tests for {@Range}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class RangeTest {
    @Test
    @Feature({"TextInput"})
    public void testClamp() {
        // Overlap case #1
        Range range = new Range(1, 4);
        range.clamp(2, 5);
        assertEquals(new Range(2, 4), range);
        // Overlap case #2
        range.set(1, 4);
        range.clamp(0, 2);
        assertEquals(new Range(1, 2), range);
        // Clamp on both ends
        range.set(1, 4);
        range.clamp(2, 3);
        assertEquals(new Range(2, 3), range);
        // No-op
        range.set(1, 4);
        range.clamp(0, 5);
        assertEquals(new Range(1, 4), range);
    }

    @Test
    @Feature({"TextInput"})
    public void testEquals() {
        assertTrue(new Range(1, 3).equals(new Range(1, 3)));
        assertFalse(new Range(1, 2).equals(new Range(1, 3)));
        Range range = new Range(1, 4);
        assertTrue(range.equals(range));
    }
}