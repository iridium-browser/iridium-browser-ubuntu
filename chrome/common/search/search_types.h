// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_SEARCH_TYPES_H_
#define CHROME_COMMON_SEARCH_SEARCH_TYPES_H_

// The Mode structure encodes the visual states encountered when interacting
// with the NTP and the Omnibox.
struct SearchMode {
  // The visual state that applies to the current interaction.
  enum Type {
    // The default state means anything but the following states.
    MODE_DEFAULT,

    // On the NTP page and the NTP is ready to be displayed.
    MODE_NTP,

    // The Omnibox is modified in some way, either on the NTP or not.
    MODE_SEARCH_SUGGESTIONS,

    // On a search results page.
    // TODO(treib): Remove this; it's not used anymore. crbug.com/627747
    MODE_SEARCH_RESULTS,
  };

  // The kind of page from which the user initiated the current search.
  enum Origin {
    // The user is searching from some random page.
    ORIGIN_DEFAULT = 0,

    // The user is searching from the NTP.
    ORIGIN_NTP,

    // The user is searching from a search results page.
    // TODO(treib): Remove this; it's not used anymore. crbug.com/627747
    ORIGIN_SEARCH,
  };

  SearchMode() : mode(MODE_DEFAULT), origin(ORIGIN_DEFAULT) {
  }

  SearchMode(Type in_mode, Origin in_origin)
      : mode(in_mode),
        origin(in_origin) {
  }

  bool operator==(const SearchMode& rhs) const {
    return mode == rhs.mode && origin == rhs.origin;
  }

  bool operator!=(const SearchMode& rhs) const {
    return !(*this == rhs);
  }

  bool is_default() const {
    return mode == MODE_DEFAULT;
  }

  bool is_ntp() const {
    return mode == MODE_NTP;
  }

  bool is_search() const {
    return mode == MODE_SEARCH_SUGGESTIONS || mode == MODE_SEARCH_RESULTS;
  }

  bool is_search_results() const {
    return mode == MODE_SEARCH_RESULTS;
  }

  bool is_search_suggestions() const {
    return mode == MODE_SEARCH_SUGGESTIONS;
  }

  bool is_origin_default() const {
    return origin == ORIGIN_DEFAULT;
  }

  bool is_origin_search() const {
    return origin == ORIGIN_SEARCH;
  }

  bool is_origin_ntp() const {
    return origin == ORIGIN_NTP;
  }

  Type mode;
  Origin origin;
};

#endif  // CHROME_COMMON_SEARCH_SEARCH_TYPES_H_
