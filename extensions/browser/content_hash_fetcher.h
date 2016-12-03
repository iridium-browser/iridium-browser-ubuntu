// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_
#define EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"

namespace net {
class URLRequestContextGetter;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
class ContentHashFetcherJob;
class ContentVerifierDelegate;

// This class is responsible for getting signed expected hashes for use in
// extension content verification. As extensions are loaded it will fetch and
// parse/validate/cache this data as needed, including calculating expected
// hashes for each block of each file within an extension. (These unsigned leaf
// node block level hashes will always be checked at time of use use to make
// sure they match the signed treehash root hash).
class ContentHashFetcher {
 public:
  // A callback for when a fetch is complete. This reports back:
  // -extension id
  // -whether we were successful or not (have verified_contents.json and
  // -computed_hashes.json files)
  // -was it a forced check?
  // -a set of paths whose contents didn't match expected values
  typedef base::Callback<
      void(const std::string&, bool, bool, const std::set<base::FilePath>&)>
      FetchCallback;

  // The consumer of this class needs to ensure that context and delegate
  // outlive this object.
  ContentHashFetcher(net::URLRequestContextGetter* context_getter,
                     ContentVerifierDelegate* delegate,
                     const FetchCallback& callback);
  virtual ~ContentHashFetcher();

  // Explicitly ask to fetch hashes for |extension|. If |force| is true,
  // we will always check the validity of the verified_contents.json and
  // re-check the contents of the files in the filesystem.
  void DoFetch(const Extension* extension, bool force);

  // These should be called when an extension is loaded or unloaded.
  virtual void ExtensionLoaded(const Extension* extension);
  virtual void ExtensionUnloaded(const Extension* extension);

 private:
  // Callback for when a job getting content hashes has completed.
  void JobFinished(ContentHashFetcherJob* job);

  net::URLRequestContextGetter* context_getter_;
  ContentVerifierDelegate* delegate_;
  FetchCallback fetch_callback_;

  // We keep around pointers to in-progress jobs, both so we can avoid
  // scheduling duplicate work if fetching is already in progress, and so that
  // we can cancel in-progress work at shutdown time.
  typedef std::pair<ExtensionId, std::string> IdAndVersion;
  typedef std::map<IdAndVersion, scoped_refptr<ContentHashFetcherJob> > JobMap;
  JobMap jobs_;

  // Used for binding callbacks passed to jobs.
  base::WeakPtrFactory<ContentHashFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_HASH_FETCHER_H_
