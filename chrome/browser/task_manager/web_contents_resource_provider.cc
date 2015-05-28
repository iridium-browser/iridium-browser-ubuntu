// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/web_contents_resource_provider.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/task_manager/renderer_resource.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_util.h"
#include "chrome/browser/task_manager/web_contents_information.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

using content::RenderFrameHost;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace task_manager {

// A resource for a process hosting out-of-process iframes.
class SubframeResource : public RendererResource {
 public:
  explicit SubframeResource(WebContents* web_contents,
                            SiteInstance* site_instance,
                            RenderFrameHost* example_rfh);
  ~SubframeResource() override {}

  // Resource methods:
  Type GetType() const override;
  base::string16 GetTitle() const override;
  gfx::ImageSkia GetIcon() const override;
  WebContents* GetWebContents() const override;

 private:
  WebContents* web_contents_;
  base::string16 title_;
  DISALLOW_COPY_AND_ASSIGN(SubframeResource);
};

SubframeResource::SubframeResource(WebContents* web_contents,
                                   SiteInstance* subframe_site_instance,
                                   RenderFrameHost* example_rfh)
    : RendererResource(subframe_site_instance->GetProcess()->GetHandle(),
                       example_rfh->GetRenderViewHost()),
      web_contents_(web_contents) {
  int message_id = subframe_site_instance->GetBrowserContext()->IsOffTheRecord()
                       ? IDS_TASK_MANAGER_SUBFRAME_INCOGNITO_PREFIX
                       : IDS_TASK_MANAGER_SUBFRAME_PREFIX;
  title_ = l10n_util::GetStringFUTF16(
      message_id,
      base::UTF8ToUTF16(subframe_site_instance->GetSiteURL().spec()));
}

Resource::Type SubframeResource::GetType() const {
  return RENDERER;
}

base::string16 SubframeResource::GetTitle() const {
  return title_;
}

gfx::ImageSkia SubframeResource::GetIcon() const {
  return gfx::ImageSkia();
}

WebContents* SubframeResource::GetWebContents() const {
  return web_contents_;
}

// Tracks changes to one WebContents, and manages task manager resources for
// that WebContents, on behalf of a WebContentsResourceProvider.
class TaskManagerWebContentsEntry : public content::WebContentsObserver,
                                    public content::RenderProcessHostObserver {
 public:
  typedef std::multimap<SiteInstance*, RendererResource*> ResourceMap;
  typedef std::pair<ResourceMap::iterator, ResourceMap::iterator> ResourceRange;

  TaskManagerWebContentsEntry(WebContents* web_contents,
                              WebContentsResourceProvider* provider)
      : content::WebContentsObserver(web_contents),
        provider_(provider),
        main_frame_site_instance_(NULL) {}

  ~TaskManagerWebContentsEntry() override { ClearAllResources(false); }

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    ClearResourceForFrame(render_frame_host);
  }

  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override {
    if (old_host)
      ClearResourceForFrame(old_host);
    CreateResourceForFrame(new_host);
  }

  void RenderViewReady() override {
    ClearAllResources(true);
    CreateAllResources();
  }

  void WebContentsDestroyed() override {
    ClearAllResources(true);
    provider_->DeleteEntry(web_contents(), this);  // Deletes |this|.
  }

  // content::RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* process_host,
                           base::TerminationStatus status,
                           int exit_code) override {
    ClearResourcesForProcess(process_host);
  }

  void RenderProcessHostDestroyed(RenderProcessHost* process_host) override {
    tracked_process_hosts_.erase(process_host);
  }

  // Called by WebContentsResourceProvider.
  RendererResource* GetResourceForSiteInstance(SiteInstance* site_instance) {
    ResourceMap::iterator i = resources_by_site_instance_.find(site_instance);
    if (i == resources_by_site_instance_.end())
      return NULL;
    return i->second;
  }

  void CreateAllResources() {
    // We'll show one row per SiteInstance in the task manager.
    DCHECK(web_contents()->GetMainFrame() != NULL);
    web_contents()->ForEachFrame(
        base::Bind(&TaskManagerWebContentsEntry::CreateResourceForFrame,
                   base::Unretained(this)));
  }

  void ClearAllResources(bool update_task_manager) {
    RendererResource* last_resource = NULL;
    for (auto& x : resources_by_site_instance_) {
      RendererResource* resource = x.second;
      if (resource == last_resource)
        continue;  // Skip multiset duplicates.
      if (update_task_manager)
        task_manager()->RemoveResource(resource);
      delete resource;
      last_resource = resource;
    }
    resources_by_site_instance_.clear();

    RenderProcessHost* last_process_host = NULL;
    for (RenderProcessHost* process_host : tracked_process_hosts_) {
      if (last_process_host == process_host)
        continue;  // Skip multiset duplicates.
      process_host->RemoveObserver(this);
      last_process_host = process_host;
    }
    tracked_process_hosts_.clear();
    tracked_frame_hosts_.clear();
  }

  void ClearResourceForFrame(RenderFrameHost* render_frame_host) {
    SiteInstance* site_instance = render_frame_host->GetSiteInstance();
    std::set<RenderFrameHost*>::iterator frame_set_iterator =
        tracked_frame_hosts_.find(render_frame_host);
    if (frame_set_iterator == tracked_frame_hosts_.end()) {
      // We weren't tracking this RenderFrameHost.
      return;
    }
    tracked_frame_hosts_.erase(frame_set_iterator);
    ResourceRange resource_range =
        resources_by_site_instance_.equal_range(site_instance);
    if (resource_range.first == resource_range.second) {
      NOTREACHED();
      return;
    }
    RendererResource* resource = resource_range.first->second;
    resources_by_site_instance_.erase(resource_range.first++);
    if (resource_range.first == resource_range.second) {
      // The removed entry was the sole remaining reference to that resource, so
      // actually destroy it.
      task_manager()->RemoveResource(resource);
      delete resource;
      DecrementProcessWatch(site_instance->GetProcess());
      if (site_instance == main_frame_site_instance_) {
        main_frame_site_instance_ = NULL;
      }
    }
  }

  void ClearResourcesForProcess(RenderProcessHost* crashed_process) {
    std::vector<RenderFrameHost*> frame_hosts_to_delete;
    for (RenderFrameHost* frame_host : tracked_frame_hosts_) {
      if (frame_host->GetProcess() == crashed_process) {
        frame_hosts_to_delete.push_back(frame_host);
      }
    }
    for (RenderFrameHost* frame_host : frame_hosts_to_delete) {
      ClearResourceForFrame(frame_host);
    }
  }

  void CreateResourceForFrame(RenderFrameHost* render_frame_host) {
    SiteInstance* site_instance = render_frame_host->GetSiteInstance();

    DCHECK_EQ(0u, tracked_frame_hosts_.count(render_frame_host));

    if (!site_instance->GetProcess()->HasConnection())
      return;

    tracked_frame_hosts_.insert(render_frame_host);

    ResourceRange existing_resource_range =
        resources_by_site_instance_.equal_range(site_instance);
    bool existing_resource =
        (existing_resource_range.first != existing_resource_range.second);
    bool is_main_frame = (render_frame_host == web_contents()->GetMainFrame());
    bool site_instance_is_main = (site_instance == main_frame_site_instance_);
    scoped_ptr<RendererResource> new_resource;
    if (!existing_resource || (is_main_frame && !site_instance_is_main)) {
      if (is_main_frame) {
        new_resource = info()->MakeResource(web_contents());
        main_frame_site_instance_ = site_instance;
      } else {
        new_resource.reset(new SubframeResource(
            web_contents(), site_instance, render_frame_host));
      }
    }

    if (existing_resource) {
      RendererResource* old_resource = existing_resource_range.first->second;
      if (!new_resource) {
        resources_by_site_instance_.insert(
            std::make_pair(site_instance, old_resource));
      } else {
        for (ResourceMap::iterator it = existing_resource_range.first;
             it != existing_resource_range.second;
             ++it) {
          it->second = new_resource.get();
        }
        task_manager()->RemoveResource(old_resource);
        delete old_resource;
        DecrementProcessWatch(site_instance->GetProcess());
      }
    }

    if (new_resource) {
      task_manager()->AddResource(new_resource.get());
      resources_by_site_instance_.insert(
          std::make_pair(site_instance, new_resource.release()));
      IncrementProcessWatch(site_instance->GetProcess());
    }
  }

  // Add ourself as an observer of |process|, if we aren't already. Must be
  // balanced by a call to DecrementProcessWatch().
  // TODO(nick): Move away from RenderProcessHostObserver once
  // WebContentsObserver supports per-frame process death notices.
  void IncrementProcessWatch(RenderProcessHost* process) {
    auto range = tracked_process_hosts_.equal_range(process);
    if (range.first == range.second) {
      process->AddObserver(this);
    }
    tracked_process_hosts_.insert(range.first, process);
  }

  void DecrementProcessWatch(RenderProcessHost* process) {
    auto range = tracked_process_hosts_.equal_range(process);
    if (range.first == range.second) {
      NOTREACHED();
      return;
    }

    auto element = range.first++;
    if (range.first == range.second) {
      process->RemoveObserver(this);
    }
    tracked_process_hosts_.erase(element, range.first);
  }

 private:
  TaskManager* task_manager() { return provider_->task_manager(); }

  WebContentsInformation* info() { return provider_->info(); }

  WebContentsResourceProvider* const provider_;

  // Every RenderFrameHost that we're watching.
  std::set<RenderFrameHost*> tracked_frame_hosts_;

  // The set of processes we're currently observing. There is one entry here per
  // RendererResource we create. A multimap because we may request observation
  // more than once, say if two resources happen to share a process.
  std::multiset<RenderProcessHost*> tracked_process_hosts_;

  // Maps SiteInstances to the RendererResources. A multimap, this contains one
  // entry per tracked RenderFrameHost, so we can tell when we're done reusing.
  ResourceMap resources_by_site_instance_;

  // The site instance of the main frame.
  SiteInstance* main_frame_site_instance_;
};

////////////////////////////////////////////////////////////////////////////////
// WebContentsResourceProvider class
////////////////////////////////////////////////////////////////////////////////

WebContentsResourceProvider::WebContentsResourceProvider(
    TaskManager* task_manager,
    scoped_ptr<WebContentsInformation> info)
    : task_manager_(task_manager), info_(info.Pass()) {
}

WebContentsResourceProvider::~WebContentsResourceProvider() {}

RendererResource* WebContentsResourceProvider::GetResource(int origin_pid,
                                                           int child_id,
                                                           int route_id) {
  RenderFrameHost* rfh = RenderFrameHost::FromID(child_id, route_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);

  // If an origin PID was specified then the request originated in a plugin
  // working on the WebContents's behalf, so ignore it.
  if (origin_pid)
    return NULL;

  EntryMap::const_iterator web_contents_it = entries_.find(web_contents);

  if (web_contents_it == entries_.end()) {
    // Can happen if the tab was closed while a network request was being
    // performed.
    return NULL;
  }

  return web_contents_it->second->GetResourceForSiteInstance(
      rfh->GetSiteInstance());
}

void WebContentsResourceProvider::StartUpdating() {
  WebContentsInformation::NewWebContentsCallback new_web_contents_callback =
      base::Bind(&WebContentsResourceProvider::OnWebContentsCreated, this);
  info_->GetAll(new_web_contents_callback);
  info_->StartObservingCreation(new_web_contents_callback);
}

void WebContentsResourceProvider::StopUpdating() {
  info_->StopObservingCreation();

  // Delete all entries; this dissassociates them from the WebContents too.
  STLDeleteValues(&entries_);
}

void WebContentsResourceProvider::OnWebContentsCreated(
    WebContents* web_contents) {
  // Don't add dead tabs or tabs that haven't yet connected.
  if (!web_contents->GetRenderProcessHost()->GetHandle() ||
      !web_contents->WillNotifyDisconnection()) {
    return;
  }

  DCHECK(info_->CheckOwnership(web_contents));
  if (entries_.count(web_contents)) {
    // The case may happen that we have added a WebContents as part of the
    // iteration performed during StartUpdating() call but the notification that
    // it has connected was not fired yet. So when the notification happens, we
    // are already observing this WebContents and just ignore it.
    return;
  }
  scoped_ptr<TaskManagerWebContentsEntry> entry(
      new TaskManagerWebContentsEntry(web_contents, this));
  entry->CreateAllResources();
  entries_[web_contents] = entry.release();
}

void WebContentsResourceProvider::DeleteEntry(
    WebContents* web_contents,
    TaskManagerWebContentsEntry* entry) {
  if (!entries_.erase(web_contents)) {
    NOTREACHED();
    return;
  }
  delete entry;  // Typically, this is our caller. Deletion is okay.
}

}  // namespace task_manager
