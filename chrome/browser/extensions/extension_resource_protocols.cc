// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_resource_protocols.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/file_util.h"
#include "net/url_request/url_request_file_job.h"

namespace {

base::FilePath ResolvePath(const GURL& url) {
  base::FilePath root_path;
  PathService::Get(chrome::DIR_RESOURCES_EXTENSION, &root_path);
  return extensions::file_util::ExtensionResourceURLToFilePath(url, root_path);
}

class ExtensionResourcesJob : public net::URLRequestFileJob {
 public:
  ExtensionResourcesJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate)
      : net::URLRequestFileJob(
            request, network_delegate, base::FilePath(),
            content::BrowserThread::GetBlockingPool()->
                GetTaskRunnerWithShutdownBehavior(
                    base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
        weak_ptr_factory_(this) {}

  void Start() override;

  bool IsRedirectResponse(GURL* location, int* http_status_code) override;

 protected:
  ~ExtensionResourcesJob() override {}

  void ResolvePathDone(const base::FilePath& resolved_path);

 private:
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ExtensionResourcesJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionResourcesJob);
};

void ExtensionResourcesJob::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&ResolvePath, request()->url()),
      base::Bind(&ExtensionResourcesJob::ResolvePathDone,
          weak_ptr_factory_.GetWeakPtr()));
}

bool ExtensionResourcesJob::IsRedirectResponse(GURL* location,
                                               int* http_status_code) {
  return false;
}

void ExtensionResourcesJob::ResolvePathDone(
    const base::FilePath& resolved_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_path_ = resolved_path;
  net::URLRequestFileJob::Start();
}

class ExtensionResourceProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionResourceProtocolHandler() {}
  ~ExtensionResourceProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionResourceProtocolHandler);
};

// Creates URLRequestJobs for chrome-extension-resource:// URLs.
net::URLRequestJob*
ExtensionResourceProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new ExtensionResourcesJob(request, network_delegate);
}

}  // namespace

std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
CreateExtensionResourceProtocolHandler() {
  return base::WrapUnique(new ExtensionResourceProtocolHandler());
}
