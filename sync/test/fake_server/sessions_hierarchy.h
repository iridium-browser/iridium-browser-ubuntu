// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_SESSIONS_HIERARCHY_H_
#define SYNC_TEST_FAKE_SERVER_SESSIONS_HIERARCHY_H_

#include <set>
#include <string>

#include "base/macros.h"

namespace fake_server {

class SessionsHierarchy;

// A representation of the Sync Sessions hierarchy (windows and the URLs of
// their tabs).
class SessionsHierarchy {
 public:
  // Creates an empty (no windows) SessionsHierachy.
  SessionsHierarchy();

  ~SessionsHierarchy();

  // Add a window to the builder with one tab.
  void AddWindow(const std::string& tab);

  // Add a window to the builder with multiple tabs.
  void AddWindow(const std::multiset<std::string>& tabs);

  // Creates and returns a human-readable string version of this object's data.
  std::string ToString() const;

  // Returns true when this object and |other| have equivalent data.
  //
  // Two SessionHierarchy objects A and B have equivalent data iff:
  // 1) A and B contain the same number of Windows, and
  // 2) Each Window of A is equal (as a multiset) to exactly one Window of B
  //    (and vice versa).
  //
  // Examples of equivalent hierarchies:
  //   {} and {}, {{X}} and {{X}}, {{X,Y}} and {{Y,X}}, {{X},{Y}} and {{Y},{X}}
  // Examples of nonequivalent hierarchies:
  //   {{X}} and {{Y}}, {{X}} and {{X,X}}, {{X}} and {{X},{X}}
  bool Equals(const SessionsHierarchy& other) const;

 private:
  // A collection of tab URLs.
  typedef std::multiset<std::string> Window;

  // A collection of Windows (an instance of this collection represents a
  // sessions hierarchy).
  typedef std::multiset<Window> WindowContainer;

  // The windows of the sessions hierarchy.
  WindowContainer windows_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_SESSIONS_HIERARCHY_H_
