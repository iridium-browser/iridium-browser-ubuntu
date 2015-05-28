// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_

#include <string>

#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace base {
namespace i18n {
class FixedPatternStringSearchIgnoringCaseAndAccents;
}  // namespace i18n
}  // namespace base

namespace drive {
namespace internal {

class ResourceMetadata;

typedef base::Callback<bool(const ResourceEntry&)> SearchMetadataPredicate;

// Searches the local resource metadata, and returns the entries
// |at_most_num_matches| that contain |query| in their base names. Search is
// done in a case-insensitive fashion. The eligible entries are selected based
// on the given |options|, which is a bit-wise OR of SearchMetadataOptions.
// |callback| must not be null. Must be called on UI thread. Empty |query|
// matches any base name. i.e. returns everything. |blocking_task_runner| must
// be the same one as |resource_metadata| uses.
void SearchMetadata(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    ResourceMetadata* resource_metadata,
    const std::string& query,
    const SearchMetadataPredicate& predicate,
    size_t at_most_num_matches,
    const SearchMetadataCallback& callback);

// Returns true if |entry| is eligible for the search |options| and should be
// tested for the match with the query.  If
// SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS is requested, the hosted documents
// are skipped. If SEARCH_METADATA_EXCLUDE_DIRECTORIES is requested, the
// directories are skipped. If SEARCH_METADATA_SHARED_WITH_ME is requested, only
// the entries with shared-with-me label will be tested. If
// SEARCH_METADATA_OFFLINE is requested, only hosted documents and cached files
// match with the query. This option can not be used with other options.
bool MatchesType(int options, const ResourceEntry& entry);

// Finds |query| in |text| while ignoring cases or accents. Cases of non-ASCII
// characters are also ignored; they are compared in the 'Primary Level' of
// http://userguide.icu-project.org/collation/concepts.
// Returns true if |query| is found. |highlighted_text| will have the original
// text with matched portions highlighted with <b> tag (only the first match
// is highlighted). Meta characters are escaped like &lt;. The original
// contents of |highlighted_text| will be lost.
bool FindAndHighlight(
    const std::string& text,
    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents* query,
    std::string* highlighted_text);

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_SEARCH_METADATA_H_
