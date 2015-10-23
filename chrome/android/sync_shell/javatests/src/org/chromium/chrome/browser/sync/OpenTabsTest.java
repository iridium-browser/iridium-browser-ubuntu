// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.test.suitebuilder.annotation.LargeTest;
import android.util.Pair;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.content.browser.test.util.Criteria;
import org.chromium.content.browser.test.util.CriteriaHelper;
import org.chromium.sync.protocol.EntitySpecifics;
import org.chromium.sync.protocol.SessionHeader;
import org.chromium.sync.protocol.SessionSpecifics;
import org.chromium.sync.protocol.SessionTab;
import org.chromium.sync.protocol.SessionWindow;
import org.chromium.sync.protocol.SyncEnums;
import org.chromium.sync.protocol.TabNavigation;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Test suite for the open tabs (sessions) sync data type.
 */
public class OpenTabsTest extends SyncTestBase {
    private static final String TAG = "OpenTabsTest";

    private static final String OPEN_TABS_TYPE = "Sessions";

    // TestHttpServerClient is preferred here but it can't be used. The test server
    // serves pages on localhost and Chrome doesn't sync localhost URLs as typed URLs.
    // This type of URL requires no external data connection or resources.
    private static final String URL = "data:text,OpenTabsTestURL";
    private static final String URL2 = "data:text,OpenTabsTestURL2";
    private static final String URL3 = "data:text,OpenTabsTestURL3";

    private static final String SESSION_TAG_PREFIX = "FakeSessionTag";
    private static final String FAKE_CLIENT = "FakeClient";

    // The client name for tabs generated locally will vary based on the device the test is
    // running on, so it is determined once in the setUp() method and cached here.
    private String mClientName;

    // A counter used for generating unique session tags. Resets to 0 in setUp().
    private int mSessionTagCounter;

    // A container to store OpenTabs information for data verification.
    private static class OpenTabs {
        public final String headerId;
        public final List<String> tabIds;
        public final List<String> urls;

        private OpenTabs(String headerId, List<String> tabIds, List<String> urls) {
            this.headerId = headerId;
            this.tabIds = tabIds;
            this.urls = urls;
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        setupTestAccountAndSignInToSync(CLIENT_ID);
        mClientName = getClientName();
        mSessionTagCounter = 0;
    }

    // Test syncing an open tab from client to server.
    @LargeTest
    @Feature({"Sync"})
    public void testUploadOpenTab() throws Exception {
        loadUrl(URL);
        waitForLocalTabsForClient(mClientName, URL);
        waitForServerTabs(URL);
    }

    // Test syncing multiple open tabs from client to server.
    @LargeTest
    @Feature({"Sync"})
    public void testUploadMultipleOpenTabs() throws Exception {
        loadUrl(URL);
        openNewTab(URL2);
        openNewTab(URL3);
        waitForLocalTabsForClient(mClientName, URL, URL2, URL3);
        waitForServerTabs(URL, URL2, URL3);
    }

    // Test syncing an open tab from client to server.
    @LargeTest
    @Feature({"Sync"})
    public void testUploadAndCloseOpenTab() throws Exception {
        loadUrl(URL);
        // Can't have zero tabs, so we have to open two to test closing one.
        openNewTab(URL2);
        waitForLocalTabsForClient(mClientName, URL, URL2);
        waitForServerTabs(URL, URL2);
        closeActiveTab();
        waitForLocalTabsForClient(mClientName, URL);
        waitForServerTabs(URL);
    }

    // Test syncing an open tab from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadOpenTab() throws Exception {
        addFakeServerTabs(FAKE_CLIENT, URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        // Verify data synced to client.
        OpenTabs openTabs = getLocalTabsForClient(FAKE_CLIENT);
        assertEquals("Only the injected session should exist on the client.",
                1, openTabs.urls.size());
        assertEquals("The wrong URL was found for the session.", URL, openTabs.urls.get(0));
    }

    // Test syncing multiple open tabs from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadMultipleOpenTabs() throws Exception {
        addFakeServerTabs(FAKE_CLIENT, URL, URL2, URL3);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);

        // Verify data synced to client.
        OpenTabs openTabs = getLocalTabsForClient(FAKE_CLIENT);
        assertEquals("Only the injected tabs should exist on the client.",
                3, openTabs.urls.size());
        assertEquals("The wrong URL was found for tab 1.", URL, openTabs.urls.get(0));
        assertEquals("The wrong URL was found for tab 2.", URL2, openTabs.urls.get(1));
        assertEquals("The wrong URL was found for tab 3.", URL3, openTabs.urls.get(2));
    }

    // Test syncing a tab deletion from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadDeletedOpenTab() throws Exception {
        // Add the entity to test deleting.
        addFakeServerTabs(FAKE_CLIENT, URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForLocalTabsForClient(FAKE_CLIENT, URL);

        // Delete on server, sync, and verify deleted locally.
        deleteServerTabsForClient(FAKE_CLIENT);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForLocalTabsForClient(FAKE_CLIENT);
    }

    // Test syncing multiple tab deletions from server to client.
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadMultipleDeletedOpenTabs() throws Exception {
        // Add the entity to test deleting.
        addFakeServerTabs(FAKE_CLIENT, URL, URL2, URL3);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForLocalTabsForClient(FAKE_CLIENT, URL, URL2, URL3);

        // Delete on server, sync, and verify deleted locally.
        deleteServerTabsForClient(FAKE_CLIENT);
        SyncTestUtil.triggerSyncAndWaitForCompletion(mContext);
        waitForLocalTabsForClient(FAKE_CLIENT);
    }

    private void openNewTab(final String url) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().createTab(url);
            }
        });
    }

    private void closeActiveTab() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                getActivity().closeTab();
            }
        });
    }

    private String makeSessionTag() {
        return SESSION_TAG_PREFIX + (mSessionTagCounter++);
    }

    private void addFakeServerTabs(String clientName, String... urls)
            throws InterruptedException {
        String tag = makeSessionTag();
        EntitySpecifics header = makeSessionEntity(tag, clientName, urls.length);
        mFakeServerHelper.injectUniqueClientEntity(tag, header);
        for (int i = 0; i < urls.length; i++) {
            EntitySpecifics tab = makeTabEntity(tag, urls[i], i);
            // It is critical that the name here is "<tag> <tabNodeId>", otherwise sync crashes
            // when it tries to sync due to the use of TabIdToTag in sessions_sync_manager.cc.
            mFakeServerHelper.injectUniqueClientEntity(tag + " " + i, tab);
        }
    }

    private EntitySpecifics makeSessionEntity(String tag, String clientName, int numTabs) {
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.session = new SessionSpecifics();
        specifics.session.sessionTag = tag;
        specifics.session.header = new SessionHeader();
        specifics.session.header.clientName = clientName;
        specifics.session.header.deviceType = SyncEnums.TYPE_PHONE;
        SessionWindow window = new SessionWindow();
        window.windowId = 0;
        window.selectedTabIndex = 0;
        window.tab = new int[numTabs];
        for (int i = 0; i < numTabs; i++) {
            window.tab[i] = i;
        }
        specifics.session.header.window = new SessionWindow[] { window };
        return specifics;
    }

    private EntitySpecifics makeTabEntity(String tag, String url, int id) {
        EntitySpecifics specifics = new EntitySpecifics();
        specifics.session = new SessionSpecifics();
        specifics.session.sessionTag = tag;
        specifics.session.tabNodeId = id;
        SessionTab tab = new SessionTab();
        tab.tabId = id;
        tab.currentNavigationIndex = 0;
        TabNavigation nav = new TabNavigation();
        nav.virtualUrl = url;
        tab.navigation = new TabNavigation[] { nav };
        specifics.session.tab = tab;
        return specifics;
    }

    private void deleteServerTabsForClient(String clientName) throws JSONException {
        OpenTabs openTabs = getLocalTabsForClient(clientName);
        mFakeServerHelper.deleteEntity(openTabs.headerId);
        for (String tabId : openTabs.tabIds) {
            mFakeServerHelper.deleteEntity(tabId);
        }
    }

    private void waitForLocalTabsForClient(final String clientName, String... urls)
            throws InterruptedException {
        final List<String> urlList = new ArrayList<String>(urls.length);
        for (String url : urls) urlList.add(url);
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return getLocalTabsForClient(clientName).urls.equals(urlList);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected local open tabs for client " + clientName + ": "
                + Arrays.toString(urls), success);
    }

    private void waitForServerTabs(final String... urls)
            throws InterruptedException {
        boolean success = CriteriaHelper.pollForCriteria(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return mFakeServerHelper.verifySessions(urls);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            }
        }, SyncTestUtil.UI_TIMEOUT_MS, SyncTestUtil.CHECK_INTERVAL_MS);
        assertTrue("Expected server open tabs: " + Arrays.toString(urls), success);
    }

    private String getClientName() throws JSONException {
        List<Pair<String, JSONObject>> tabEntities = SyncTestUtil.getLocalData(
                mContext, OPEN_TABS_TYPE);
        assertEquals("Expected 2 entities when getting the client name.", 2, tabEntities.size());
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            if (tabEntity.second.has("header")) {
                return tabEntity.second.getJSONObject("header").getString("client_name");
            }
        }
        throw new IllegalStateException("No client name found.");
    }

    private static class HeaderInfo {
        public final String sessionTag;
        public final String headerId;
        public final List<String> tabIds;
        public HeaderInfo(String sessionTag, String headerId, List<String> tabIds) {
            this.sessionTag = sessionTag;
            this.headerId = headerId;
            this.tabIds = tabIds;
        }
    }

    // Distills the local session data into a simple data object for the given client.
    private OpenTabs getLocalTabsForClient(String clientName) throws JSONException {
        List<Pair<String, JSONObject>> tabEntities = SyncTestUtil.getLocalData(
                mContext, OPEN_TABS_TYPE);
        // Output lists.
        List<String> urls = new ArrayList<String>();
        List<String> tabEntityIds = new ArrayList<String>();
        HeaderInfo info = findHeaderInfoForClient(clientName, tabEntities);
        if (info.sessionTag == null) {
            // No client was found. Here we still want to return an empty list of urls.
            return new OpenTabs("", tabEntityIds, urls);
        }
        Map<String, String> tabIdsToUrls = new HashMap<String, String>();
        Map<String, String> tabIdsToEntityIds = new HashMap<String, String>();
        findTabMappings(info.sessionTag, tabEntities, tabIdsToUrls, tabIdsToEntityIds);
        // Convert the tabId list to the url list.
        for (String tabId : info.tabIds) {
            urls.add(tabIdsToUrls.get(tabId));
            tabEntityIds.add(tabIdsToEntityIds.get(tabId));
        }
        return new OpenTabs(info.headerId, tabEntityIds, urls);
    }

    // Find the header entity for clientName and extract its sessionTag and tabId list.
    private HeaderInfo findHeaderInfoForClient(
            String clientName, List<Pair<String, JSONObject>> tabEntities) throws JSONException {
        String sessionTag = null;
        String headerId = null;
        List<String> tabIds = new ArrayList<String>();
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            JSONObject header = tabEntity.second.optJSONObject("header");
            if (header != null && header.getString("client_name").equals(clientName)) {
                sessionTag = tabEntity.second.getString("session_tag");
                headerId = tabEntity.first;
                JSONArray windows = header.getJSONArray("window");
                if (windows.length() == 0) {
                    // The client was found but there are no tabs.
                    break;
                }
                assertEquals("Only single windows are supported.", 1, windows.length());
                JSONArray tabs = windows.getJSONObject(0).getJSONArray("tab");
                for (int i = 0; i < tabs.length(); i++) {
                    tabIds.add(tabs.getString(i));
                }
                break;
            }
        }
        return new HeaderInfo(sessionTag, headerId, tabIds);
    }

    // Find the associated tabs and record their tabId -> url and entityId mappings.
    private void findTabMappings(String sessionTag, List<Pair<String, JSONObject>> tabEntities,
            // Populating these maps is the output of this function.
            Map<String, String> tabIdsToUrls, Map<String, String> tabIdsToEntityIds)
            throws JSONException {
        for (Pair<String, JSONObject> tabEntity : tabEntities) {
            JSONObject json = tabEntity.second;
            if (json.has("tab") && json.getString("session_tag").equals(sessionTag)) {
                JSONObject tab = json.getJSONObject("tab");
                int i = tab.getInt("current_navigation_index");
                String tabId = tab.getString("tab_id");
                String url = tab.getJSONArray("navigation")
                        .getJSONObject(i).getString("virtual_url");
                tabIdsToUrls.put(tabId, url);
                tabIdsToEntityIds.put(tabId, tabEntity.first);
            }
        }
    }
}
