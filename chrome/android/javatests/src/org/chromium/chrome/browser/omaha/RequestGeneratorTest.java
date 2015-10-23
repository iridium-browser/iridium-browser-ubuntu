// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.omaha.AttributeFinder;
import org.chromium.chrome.test.omaha.MockRequestGenerator;
import org.chromium.chrome.test.omaha.MockRequestGenerator.DeviceType;

/**
 * Unit tests for the RequestGenerator class.
 */
public class RequestGeneratorTest extends InstrumentationTestCase {
    private static final String INSTALL_SOURCE = "install_source";

    @SmallTest
    @Feature({"Omaha"})
    public void testInstallAgeNewInstallation() {
        long currentTimestamp = 201207310000L;
        long installTimestamp = 198401160000L;
        boolean installing = true;
        long expectedAge = RequestGenerator.INSTALL_AGE_IMMEDIATELY_AFTER_INSTALLING;
        checkInstallAge(currentTimestamp, installTimestamp, installing, expectedAge);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testInstallAge() {
        long currentTimestamp = 201207310000L;
        long installTimestamp = 198401160000L;
        boolean installing = false;
        long expectedAge = 32;
        checkInstallAge(currentTimestamp, installTimestamp, installing, expectedAge);
    }

    /**
     * Checks whether the install age function is behaving according to spec.
     */
    void checkInstallAge(long currentTimestamp, long installTimestamp, boolean installing,
            long expectedAge) {
        long actualAge = RequestGenerator.installAge(currentTimestamp, installTimestamp,
                installing);
        assertEquals("Install ages differed.", expectedAge, actualAge);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testHandsetXMLCreationWithInstall() {
        createAndCheckXML(DeviceType.HANDSET, true);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testHandsetXMLCreationWithoutInstall() {
        createAndCheckXML(DeviceType.HANDSET, false);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testTabletXMLCreationWithInstall() {
        createAndCheckXML(DeviceType.TABLET, true);
    }

    @SmallTest
    @Feature({"Omaha"})
    public void testTabletXMLCreationWithoutInstall() {
        createAndCheckXML(DeviceType.TABLET, false);
    }

    /**
     * Checks that the XML is being created properly.
     */
    private RequestGenerator createAndCheckXML(DeviceType deviceType, boolean sendInstallEvent) {
        Context targetContext = getInstrumentation().getTargetContext();
        AdvancedMockContext context = new AdvancedMockContext(targetContext);

        String sessionId = "random_session_id";
        String requestId = "random_request_id";
        String version = "1.2.3.4";
        long installAge = 42;

        MockRequestGenerator generator = new MockRequestGenerator(context, deviceType);

        String xml = null;
        try {
            RequestData data = new RequestData(sendInstallEvent, 0, requestId, INSTALL_SOURCE);
            xml = generator.generateXML(sessionId, version, installAge, data);
        } catch (RequestFailureException e) {
            fail("XML generation failed.");
        }

        checkForAttributeAndValue(xml, "request", "sessionid", "{" + sessionId + "}");
        checkForAttributeAndValue(xml, "request", "requestid", "{" + requestId + "}");
        checkForAttributeAndValue(xml, "request", "installsource", INSTALL_SOURCE);
        checkForAttributeAndValue(xml, "request",
                MockRequestGenerator.REQUEST_ATTRIBUTE_1, MockRequestGenerator.REQUEST_VALUE_1);
        checkForAttributeAndValue(xml, "request",
                MockRequestGenerator.REQUEST_ATTRIBUTE_2, MockRequestGenerator.REQUEST_VALUE_2);

        checkForAttributeAndValue(xml, "app", "version", version);
        checkForAttributeAndValue(xml, "app", "lang", generator.getLanguage());
        checkForAttributeAndValue(xml, "app", "brand", generator.getBrand());
        checkForAttributeAndValue(xml, "app", "client", generator.getClient());
        checkForAttributeAndValue(xml, "app", "appid", generator.getAppId());
        checkForAttributeAndValue(xml, "app", "installage", String.valueOf(installAge));
        checkForAttributeAndValue(xml, "app", "ap", generator.getAdditionalParameters());
        checkForAttributeAndValue(xml, "app",
                MockRequestGenerator.APP_ATTRIBUTE_1, MockRequestGenerator.APP_VALUE_1);
        checkForAttributeAndValue(xml, "app",
                MockRequestGenerator.APP_ATTRIBUTE_2, MockRequestGenerator.APP_VALUE_2);

        if (sendInstallEvent) {
            checkForAttributeAndValue(xml, "event", "eventtype", "2");
            checkForAttributeAndValue(xml, "event", "eventresult", "1");
            assertFalse("Ping and install event are mutually exclusive",
                    checkForTag(xml, "ping"));
            assertFalse("Update check and install event are mutually exclusive",
                    checkForTag(xml, "updatecheck"));
        } else {
            assertFalse("Update check and install event are mutually exclusive",
                    checkForTag(xml, "event"));
            checkForAttributeAndValue(xml, "ping", "active", "1");
            assertTrue("Update check and install event are mutually exclusive",
                    checkForTag(xml, "updatecheck"));
        }

        return generator;
    }

    private boolean checkForTag(String xml, String tag) {
        return new AttributeFinder(xml, tag, null).isTagFound();
    }

    private void checkForAttributeAndValue(
            String xml, String tag, String attribute, String expectedValue) {
        // Check that the attribute exists for the tag and that the value matches.
        AttributeFinder finder = new AttributeFinder(xml, tag, attribute);
        assertTrue("Couldn't find tag '" + tag + "'", finder.isTagFound());
        assertEquals("Bad value found for tag '" + tag + "' and attribute '" + attribute + "'",
                expectedValue, finder.getValue());
    }
}
