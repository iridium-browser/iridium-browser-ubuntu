// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
class Version;
}

namespace net {
class URLRequestContextGetter;
}

namespace update_client {

class OutOfProcessPatcher;

// Controls the component updater behavior.
// TODO(sorin): this class will be split soon in two. One class controls
// the behavior of the update client, and the other class controls the
// behavior of the component updater.
class Configurator : public base::RefCountedThreadSafe<Configurator> {
 public:
  // Delay in seconds from calling Start() to the first update check.
  virtual int InitialDelay() const = 0;

  // Delay in seconds to every subsequent update check. 0 means don't check.
  virtual int NextCheckDelay() const = 0;

  // Delay in seconds from each task step. Used to smooth out CPU/IO usage.
  virtual int StepDelay() const = 0;

  // Minimum delta time in seconds before an on-demand check is allowed
  // for the same component.
  virtual int OnDemandDelay() const = 0;

  // The time delay in seconds between applying updates for different
  // components.
  virtual int UpdateDelay() const = 0;

  // The URLs for the update checks. The URLs are tried in order, the first one
  // that succeeds wins.
  virtual std::vector<GURL> UpdateUrl() const = 0;

  // The URLs for pings. Returns an empty vector if and only if pings are
  // disabled. Similarly, these URLs have a fall back behavior too.
  virtual std::vector<GURL> PingUrl() const = 0;

  // Version of the application. Used to compare the component manifests.
  virtual base::Version GetBrowserVersion() const = 0;

  // Returns the value we use for the "updaterchannel=" and "prodchannel="
  // parameters. Possible return values include: "canary", "dev", "beta", and
  // "stable".
  virtual std::string GetChannel() const = 0;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  virtual std::string GetLang() const = 0;

  // Returns the OS's long name like "Windows", "Mac OS X", etc.
  virtual std::string GetOSLongName() const = 0;

  // Parameters added to each url request. It can be empty if none are needed.
  // The return string must be safe for insertion as an attribute in an
  // XML element.
  virtual std::string ExtraRequestParams() const = 0;

  // The source of contexts for all the url requests.
  virtual net::URLRequestContextGetter* RequestContext() const = 0;

  // Returns a new out of process patcher. May be NULL for implementations
  // that patch in-process.
  virtual scoped_refptr<update_client::OutOfProcessPatcher>
  CreateOutOfProcessPatcher() const = 0;

  // True means that this client can handle delta updates.
  virtual bool DeltasEnabled() const = 0;

  // True means that the background downloader can be used for downloading
  // non on-demand components.
  virtual bool UseBackgroundDownloader() const = 0;

  // Gets a task runner to a blocking pool of threads suitable for worker jobs.
  virtual scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner()
      const = 0;

  // Gets a task runner for worker jobs guaranteed to run on a single thread.
  // This thread must be capable of IO. On Windows, this thread must be
  // initialized for use of COM objects.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetSingleThreadTaskRunner() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<Configurator>;

  virtual ~Configurator() {}
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
