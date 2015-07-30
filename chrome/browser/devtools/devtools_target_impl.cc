// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_target_impl.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

const char DevToolsTargetImpl::kTargetTypeApp[] = "app";
const char DevToolsTargetImpl::kTargetTypeBackgroundPage[] = "background_page";
const char DevToolsTargetImpl::kTargetTypePage[] = "page";
const char DevToolsTargetImpl::kTargetTypeWorker[] = "worker";
const char DevToolsTargetImpl::kTargetTypeWebView[] = "webview";
const char DevToolsTargetImpl::kTargetTypeIFrame[] = "iframe";
const char DevToolsTargetImpl::kTargetTypeOther[] = "other";
const char DevToolsTargetImpl::kTargetTypeServiceWorker[] = "service_worker";

namespace {

// WebContentsTarget --------------------------------------------------------

class WebContentsTarget : public DevToolsTargetImpl {
 public:
  WebContentsTarget(WebContents* web_contents, bool is_tab);

  // DevToolsTargetImpl overrides.
  int GetTabId() const override;
  std::string GetExtensionId() const override;
  void Inspect(Profile* profile) const override;

 private:
  int tab_id_;
  std::string extension_id_;
};

WebContentsTarget::WebContentsTarget(WebContents* web_contents, bool is_tab)
    : DevToolsTargetImpl(DevToolsAgentHost::GetOrCreateFor(web_contents)),
      tab_id_(-1) {
  set_type(kTargetTypeOther);

  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(web_contents);
  WebContents* guest_contents = guest ? guest->embedder_web_contents() : NULL;
  if (guest_contents) {
    set_type(kTargetTypeWebView);
    set_parent_id(DevToolsAgentHost::GetOrCreateFor(guest_contents)->GetId());
    return;
  }

  if (is_tab) {
    set_type(kTargetTypePage);
    tab_id_ = extensions::ExtensionTabUtil::GetTabId(web_contents);
    return;
  }

  const extensions::Extension* extension = extensions::ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          GetURL().host());
  if (!extension)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return;
  set_title(extension->name());
  extensions::ExtensionHost* extension_host =
      extensions::ProcessManager::Get(profile)
          ->GetBackgroundHostForExtension(extension->id());
  if (extension_host &&
      extension_host->host_contents() == web_contents) {
    set_type(kTargetTypeBackgroundPage);
    extension_id_ = extension->id();
  } else if (extension->is_hosted_app()
             || extension->is_legacy_packaged_app()
             || extension->is_platform_app()) {
    set_type(kTargetTypeApp);
  }
  set_favicon_url(extensions::ExtensionIconSource::GetIconURL(
      extension, extension_misc::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_BIGGER, false, NULL));
}

int WebContentsTarget::GetTabId() const {
  return tab_id_;
}

std::string WebContentsTarget::GetExtensionId() const {
  return extension_id_;
}

void WebContentsTarget::Inspect(Profile* profile) const {
  WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  DevToolsWindow::OpenDevToolsWindow(web_contents);
}

// FrameTarget ----------------------------------------------------------------

class FrameTarget : public DevToolsTargetImpl {
 public:
  explicit FrameTarget(scoped_refptr<DevToolsAgentHost> agent_host);

  // DevToolsTargetImpl overrides:
  void Inspect(Profile* profile) const override;
};

FrameTarget::FrameTarget(scoped_refptr<DevToolsAgentHost> agent_host)
    : DevToolsTargetImpl(agent_host) {
  set_type(kTargetTypeIFrame);
  WebContents* wc = agent_host->GetWebContents();
  DCHECK(DevToolsAgentHost::GetOrCreateFor(wc).get() != agent_host.get());
  set_parent_id(DevToolsAgentHost::GetOrCreateFor(wc)->GetId());
}

void FrameTarget::Inspect(Profile* profile) const {
  DevToolsWindow::OpenDevToolsWindow(profile, GetAgentHost());
}

// WorkerTarget ----------------------------------------------------------------

class WorkerTarget : public DevToolsTargetImpl {
 public:
  explicit WorkerTarget(scoped_refptr<DevToolsAgentHost> agent_host);

  // DevToolsTargetImpl overrides:
  void Inspect(Profile* profile) const override;
};

WorkerTarget::WorkerTarget(scoped_refptr<DevToolsAgentHost> agent_host)
    : DevToolsTargetImpl(agent_host) {
  switch (agent_host->GetType()) {
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      set_type(kTargetTypeWorker);
      break;
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      set_type(kTargetTypeServiceWorker);
      break;
    default:
      NOTREACHED();
  }
}

void WorkerTarget::Inspect(Profile* profile) const {
  DevToolsWindow::OpenDevToolsWindowForWorker(profile, GetAgentHost());
}

}  // namespace

// DevToolsTargetImpl ----------------------------------------------------------

DevToolsTargetImpl::~DevToolsTargetImpl() {
}

DevToolsTargetImpl::DevToolsTargetImpl(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : devtools_discovery::BasicTargetDescriptor(agent_host) {
}

int DevToolsTargetImpl::GetTabId() const {
  return -1;
}

WebContents* DevToolsTargetImpl::GetWebContents() const {
  return GetAgentHost()->GetWebContents();
}

std::string DevToolsTargetImpl::GetExtensionId() const {
  return std::string();
}

void DevToolsTargetImpl::Inspect(Profile* /*profile*/) const {
}

void DevToolsTargetImpl::Reload() const {
}

// static
scoped_ptr<DevToolsTargetImpl> DevToolsTargetImpl::CreateForTab(
    content::WebContents* web_contents) {
  // TODO(dgozman): these checks should not be necessary. See
  // http://crbug.com/489664.
  if (!web_contents)
    return nullptr;
  if (!DevToolsAgentHost::GetOrCreateFor(web_contents))
    return nullptr;
  return scoped_ptr<DevToolsTargetImpl>(
      new WebContentsTarget(web_contents, true));
}

// static
std::vector<DevToolsTargetImpl*> DevToolsTargetImpl::EnumerateAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::set<WebContents*> tab_web_contents;
  for (TabContentsIterator it; !it.done(); it.Next())
    tab_web_contents.insert(*it);

  std::vector<DevToolsTargetImpl*> result;
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    DevToolsAgentHost* agent_host = (*it).get();
    switch (agent_host->GetType()) {
      case DevToolsAgentHost::TYPE_WEB_CONTENTS:
        if (WebContents* web_contents = agent_host->GetWebContents()) {
          const bool is_tab =
              tab_web_contents.find(web_contents) != tab_web_contents.end();
          result.push_back(new WebContentsTarget(web_contents, is_tab));
        }
        break;
      case DevToolsAgentHost::TYPE_FRAME:
        result.push_back(new FrameTarget(agent_host));
        break;
      case DevToolsAgentHost::TYPE_SHARED_WORKER:
        result.push_back(new WorkerTarget(agent_host));
        break;
      case DevToolsAgentHost::TYPE_SERVICE_WORKER:
        result.push_back(new WorkerTarget(agent_host));
        break;
      default:
        break;
    }
  }
  return result;
}
