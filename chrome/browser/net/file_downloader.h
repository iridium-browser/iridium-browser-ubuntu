// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
#define CHROME_BROWSER_NET_FILE_DOWNLOADER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace base {
class FilePath;
}  // namespace base

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

class GURL;

// Helper class to download a file from a given URL and store it in a local
// file. If the local file already exists, reports success without downloading
// anything.
// TODO(treib): Add a "bool overwrite" param?
class FileDownloader : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(bool /* success */)> DownloadFinishedCallback;

  // Directly starts the download (if necessary) and runs |callback| when done.
  // If the instance is destroyed before it is finished, |callback| is not run.
  FileDownloader(const GURL& url,
                 const base::FilePath& path,
                 net::URLRequestContextGetter* request_context,
                 const DownloadFinishedCallback& callback);
  ~FileDownloader() override;

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnFileExistsCheckDone(bool exists);

  DownloadFinishedCallback callback_;

  scoped_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<FileDownloader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileDownloader);
};

#endif  // CHROME_BROWSER_NET_FILE_DOWNLOADER_H_
