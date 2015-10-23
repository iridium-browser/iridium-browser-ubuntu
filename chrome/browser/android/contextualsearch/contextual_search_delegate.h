// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "content/public/browser/android/content_view_core.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

class Profile;
class TemplateURLService;

// Handles tasks for the ContextualSearchManager in a separable and testable
// way, without the complication of being connected to a Java object.
class ContextualSearchDelegate
    : public net::URLFetcherDelegate,
      public base::SupportsWeakPtr<ContextualSearchDelegate> {
 public:
  typedef base::Callback<void(bool,
                              int,
                              const std::string&,
                              const std::string&,
                              const std::string&,
                              bool,
                              int,
                              int)> SearchTermResolutionCallback;
  typedef base::Callback<
      void(const std::string&, const std::string&)> SurroundingTextCallback;
  typedef base::Callback<
      void(const base::string16&, int, int)>
      HandleSurroundingsCallback;
  typedef base::Callback<
      void(const std::string&, const base::string16&, size_t, size_t)>
      IcingCallback;

  // ID used in creating URLFetcher for Contextual Search results.
  static const int kContextualSearchURLFetcherID;

  // Constructs a delegate that will always call back to the given callbacks
  // when search term resolution or surrounding text responses are available.
  ContextualSearchDelegate(
      net::URLRequestContextGetter* url_request_context,
      TemplateURLService* template_url_service,
      const SearchTermResolutionCallback& search_term_callback,
      const SurroundingTextCallback& surrounding_callback,
      const IcingCallback& icing_callback);
  ~ContextualSearchDelegate() override;

  // Gathers surrounding text and starts an asynchronous search term resolution
  // request. The "search term" is the best query to issue for a section of text
  // in the context of a web page. When the response is available the callback
  // specified in the constructor is run.
  void StartSearchTermResolutionRequest(
      const std::string& selection,
      bool use_resolved_search_term,
      content::ContentViewCore* content_view_core,
      bool may_send_base_page_url);

  // Gathers surrounding text and saves it locally for a future query.
  void GatherAndSaveSurroundingText(const std::string& selection,
                                    bool use_resolved_search_term,
                                    content::ContentViewCore* content_view_core,
                                    bool may_send_base_page_url);

  // Continues making a Search Term Resolution request, once the surrounding
  // text has been gathered.
  void ContinueSearchTermResolutionRequest();

  // For testing.
  void set_context_for_testing(ContextualSearchContext* context) {
    context_.reset(context);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextHighMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextLowMaximum);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextNoBeforeText);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           ExtractMentionsStartEnd);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextForIcing);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           SurroundingTextForIcingNegativeLimit);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchDelegateTest,
                           DecodeSearchTermsFromJsonResponse);

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Builds the search term resolution request URL from the current context.
  GURL BuildRequestUrl();

  // Uses the TemplateURL service to construct a search term resolution URL from
  // the given parameters.
  std::string GetSearchTermResolutionUrlString(
      const std::string& selected_text,
      const std::string& base_page_url,
      const bool may_send_base_page_url);

  // Will gather the surrounding text from the |content_view_core| and call the
  // |callback|.
  void GatherSurroundingTextWithCallback(
      const std::string& selection,
      bool use_resolved_search_term,
      content::ContentViewCore* content_view_core,
      bool may_send_base_page_url,
      HandleSurroundingsCallback callback);

  // Callback for GatherSurroundingTextWithCallback(). Will start the search
  // term resolution request.
  void StartSearchTermRequestFromSelection(
      const base::string16& surrounding_text,
      int start_offset,
      int end_offset);

  void SaveSurroundingText(
    const base::string16& surrounding_text,
    int start_offset,
    int end_offset);

  // Will call back to the manager with the proper surrounding text to be shown
  // in the UI. Will return a maximum of |max_surrounding_chars| characters for
  // each of the segments.
  void SendSurroundingText(int max_surrounding_chars);

  // Populates the discourse context and adds it to the HTTP header of the
  // search term resolution request.
  void SetDiscourseContextAndAddToHeader(
      const ContextualSearchContext& context);

  // Checks if we can send the URL for this user. Several conditions are checked
  // to make sure it's OK to send the URL.  These fall into two categories:
  // 1) check if it's allowed by our policy, and 2) ensure that the user is
  // already sending their URL browsing activity to Google.
  bool CanSendPageURL(const GURL& current_page_url,
                      Profile* profile,
                      TemplateURLService* template_url_service);

  // Decodes the given json response string and extracts parameters.
  void DecodeSearchTermsFromJsonResponse(const std::string& response,
                                         std::string* search_term,
                                         std::string* display_text,
                                         std::string* alternate_term,
                                         std::string* prevent_preload,
                                         int* mention_start,
                                         int* mention_end);

  void ExtractMentionsStartEnd(const base::ListValue& mentions_list,
                               int* startResult,
                               int* endResult);

  // Returns the surrounding size to use for the search term resolution
  // request.
  int GetSearchTermSurroundingSize();

  // Returns the size of the surroundings to be sent to Icing.
  int GetIcingSurroundingSize();

  // Generates a subset of the given surrounding_text string, for Icing
  // integration.
  // |surrounding_text| the entire text context that contains the selection.
  // |padding_each_side| the number of characters of padding desired on each
  // side of the selection (negative values treated as 0).
  // |start| the start offset of the selection, updated to reflect the new
  // position
  // of the selection in the function result.
  // |end| the end offset of the selection, updated to reflect the new position
  // of the selection in the function result.
  // |return| the trimmed surrounding text with selection at the
  // updated start/end offsets.
  base::string16 SurroundingTextForIcing(const base::string16& surrounding_text,
                                         int padding_each_side,
                                         size_t* start,
                                         size_t* end);

  // The current request in progress, or NULL.
  scoped_ptr<net::URLFetcher> search_term_fetcher_;

  // Holds the URL request context. Not owned.
  net::URLRequestContextGetter* url_request_context_;

  // Holds the TemplateURLService. Not owned.
  TemplateURLService* template_url_service_;

  // The callback for notifications of completed URL fetches.
  SearchTermResolutionCallback search_term_callback_;

  // The callback for notifications of surrounding text being available.
  SurroundingTextCallback surrounding_callback_;

  // The callback for notifications of Icing selection being available.
  IcingCallback icing_callback_;

  // Used to hold the context until an upcoming search term request is started.
  scoped_ptr<ContextualSearchContext> context_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_DELEGATE_H_
