// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.bindViewHolders;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.createDummySuggestions;
import static org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.registerCategory;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ntp.NewTabPage.DestructionObserver;
import org.chromium.chrome.browser.ntp.cards.ContentSuggestionsTestUtils.CategoryInfoBuilder;
import org.chromium.chrome.browser.ntp.snippets.FakeSuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsMetricsReporter;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.List;

/**
 * Unit tests for {@link SuggestionsSection}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SectionListTest {
    @Mock
    private SuggestionsUiDelegate mUiDelegate;
    @Mock
    private OfflinePageBridge mOfflinePageBridge;
    @Mock
    private SuggestionsMetricsReporter mMetricsReporter;
    private FakeSuggestionsSource mSuggestionSource;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSuggestionSource = new FakeSuggestionsSource();
        RecordHistogram.disableForTests();

        when(mUiDelegate.getSuggestionsSource()).thenReturn(mSuggestionSource);
        when(mUiDelegate.getMetricsReporter()).thenReturn(mMetricsReporter);
    }

    @Test
    @Feature({"Ntp"})
    public void testGetSuggestionRank() {
        // Setup the section list the following way:
        //
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // ARTICLE    | 0          | 0
        // ARTICLE    | 1          | 1
        // ARTICLE    | 2          | 2
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // BOOKMARK   | 0          | 3
        // BOOKMARK   | 1          | 4
        // BOOKMARK   | 2          | 5
        // BOOKMARK   | 3          | 6
        List<SnippetArticle> articles =
                registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 3);
        registerCategory(mSuggestionSource, KnownCategories.DOWNLOADS, 0);
        List<SnippetArticle> bookmarks =
                registerCategory(mSuggestionSource, KnownCategories.BOOKMARKS, 4);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);

        bindViewHolders(sectionList);

        assertThat(articles.get(0).getGlobalRank(), equalTo(0));
        assertThat(articles.get(0).getPerSectionRank(), equalTo(0));
        assertThat(articles.get(2).getGlobalRank(), equalTo(2));
        assertThat(articles.get(2).getPerSectionRank(), equalTo(2));
        assertThat(bookmarks.get(1).getGlobalRank(), equalTo(4));
        assertThat(bookmarks.get(1).getPerSectionRank(), equalTo(1));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetSuggestionRankWithChanges() {
        // Setup the section list the following way:
        //
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // ARTICLE    | 0          | 0
        // ARTICLE    | 1          | 1
        // ARTICLE    | 2          | 2
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // BOOKMARK   | 0          | 3
        // BOOKMARK   | 1          | 4
        // BOOKMARK   | 2          | 5
        // BOOKMARK   | 3          | 6
        List<SnippetArticle> articles =
                registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 3);
        registerCategory(mSuggestionSource, KnownCategories.DOWNLOADS, 0);
        List<SnippetArticle> bookmarks =
                registerCategory(mSuggestionSource, KnownCategories.BOOKMARKS, 4);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);

        bindViewHolders(sectionList, 0, 5); // Bind until after the third article.

        assertThat(articles.get(0).getGlobalRank(), equalTo(0));
        assertThat(articles.get(0).getPerSectionRank(), equalTo(0));
        assertThat(articles.get(2).getGlobalRank(), equalTo(2));
        assertThat(articles.get(2).getPerSectionRank(), equalTo(2));
        assertThat(bookmarks.get(1).getGlobalRank(), equalTo(-1));  // Not bound nor ranked yet.
        assertThat(bookmarks.get(1).getPerSectionRank(), equalTo(-1));


        // Test ranks after changes: remove then add some items.
        @SuppressWarnings("unchecked")
        Callback<String> cb = mock(Callback.class);
        sectionList.dismissItem(2, cb);

        // Using sublists to skip the first items and avoid using existing ones
        List<SnippetArticle> newArticles =
                createDummySuggestions(5, KnownCategories.ARTICLES).subList(3, 5);
        List<SnippetArticle> newBookmarks =
                createDummySuggestions(6, KnownCategories.BOOKMARKS).subList(4, 6);

        sectionList.onMoreSuggestions(KnownCategories.ARTICLES, newArticles.subList(0, 1));
        sectionList.onMoreSuggestions(KnownCategories.BOOKMARKS, newBookmarks);

        bindViewHolders(sectionList, 3, sectionList.getItemCount());

        // After the changes we should have:
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // ARTICLE    | 0          | 0
        // ARTICLE    | 1          | 1
        //            | -          | -  (deleted)
        // ARTICLE    | 3          | 3  (new)
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // BOOKMARK   | 0          | 4 (old but not seen until now)
        // BOOKMARK   | 1          | 5 (old but not seen until now)
        // BOOKMARK   | 2          | 6 (old but not seen until now)
        // BOOKMARK   | 3          | 7 (old but not seen until now)
        // BOOKMARK   | 4          | 8 (new)
        // BOOKMARK   | 5          | 9 (new)


        // The new article is seen first and gets the next global rank
        assertThat(newArticles.get(0).getGlobalRank(), equalTo(3));
        assertThat(newArticles.get(0).getPerSectionRank(), equalTo(3));

        // Bookmarks old and new are seen after the new article and have higher global ranks
        assertThat(bookmarks.get(1).getGlobalRank(), equalTo(5));
        assertThat(bookmarks.get(1).getPerSectionRank(), equalTo(1));
        assertThat(newBookmarks.get(1).getGlobalRank(), equalTo(9));
        assertThat(newBookmarks.get(1).getPerSectionRank(), equalTo(5));

        // Add one more article
        sectionList.onMoreSuggestions(KnownCategories.ARTICLES, newArticles.subList(1, 2));
        bindViewHolders(sectionList);

        // After the changes we should have:
        //  Item type | local rank | global rank
        // -----------+------------+-------------
        // HEADER     |            |
        // ARTICLE    | 0          | 0
        // ARTICLE    | 1          | 1
        //            | -          | -  (deleted)
        // ARTICLE    | 3          | 3
        // ARTICLE    | 4          | 10 (new)
        // HEADER     |            |
        // STATUS     |            |
        // ACTION     | 0          |
        // BOOKMARK   | 0          | 4
        // BOOKMARK   | 1          | 5
        // BOOKMARK   | 2          | 6
        // BOOKMARK   | 3          | 7
        // BOOKMARK   | 4          | 8
        // BOOKMARK   | 5          | 9

        // Old suggestions' ranks should not change.
        assertThat(articles.get(0).getGlobalRank(), equalTo(0));
        assertThat(articles.get(0).getPerSectionRank(), equalTo(0));
        assertThat(articles.get(2).getGlobalRank(), equalTo(2));
        assertThat(articles.get(2).getPerSectionRank(), equalTo(2));
        assertThat(bookmarks.get(1).getGlobalRank(), equalTo(5));
        assertThat(bookmarks.get(1).getPerSectionRank(), equalTo(1));

        // The new article should get the last global rank
        assertThat(newArticles.get(1).getGlobalRank(), equalTo(10));
        assertThat(newArticles.get(1).getPerSectionRank(), equalTo(4));
    }

    @Test
    @Feature({"Ntp"})
    public void testGetActionItemRank() {
        registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 0);
        registerCategory(mSuggestionSource,
                new CategoryInfoBuilder(KnownCategories.DOWNLOADS).withViewAllAction().build(), 3);

        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        bindViewHolders(sectionList);

        assertThat(sectionList.getSectionForTesting(KnownCategories.ARTICLES)
                           .getActionItemForTesting()
                           .getPerSectionRank(),
                equalTo(0));
        assertThat(sectionList.getSectionForTesting(KnownCategories.DOWNLOADS)
                           .getActionItemForTesting()
                           .getPerSectionRank(),
                equalTo(3));
    }

    @Test
    @Feature({"Ntp"})
    public void testRemovesSectionsWhenUiDelegateDestroyed() {
        registerCategory(mSuggestionSource, KnownCategories.ARTICLES, 1);
        registerCategory(mSuggestionSource,
                new CategoryInfoBuilder(KnownCategories.DOWNLOADS).withViewAllAction().build(), 3);
        SectionList sectionList = new SectionList(mUiDelegate, mOfflinePageBridge);
        bindViewHolders(sectionList);

        ArgumentCaptor<DestructionObserver> argument =
                ArgumentCaptor.forClass(DestructionObserver.class);
        verify(mUiDelegate).addDestructionObserver(argument.capture());

        assertFalse(sectionList.isEmpty());
        SuggestionsSection section = sectionList.getSectionForTesting(KnownCategories.ARTICLES);
        assertNotNull(section);

        // Now destroy the UI and thus notify the SectionList.
        argument.getValue().onDestroy();
        // The section should be removed.
        assertTrue(sectionList.isEmpty());
        // Verify that the section has been detached by notifying its parent about changes. If not
        // detached, it should crash.
        section.notifyItemRangeChanged(0, 1);
    }
}
