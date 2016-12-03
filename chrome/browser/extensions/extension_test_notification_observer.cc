// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_notification_observer.h"

#include <stddef.h>

#include "base/callback_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/extension.h"

using extensions::Extension;

namespace {

// A callback that returns true if the condition has been met and takes no
// arguments.
typedef base::Callback<bool(void)> ConditionCallback;

bool HasPageActionVisibilityReachedTarget(
    Browser* browser, size_t target_visible_page_action_count) {
  return extensions::extension_action_test_util::GetVisiblePageActionCount(
      browser->tab_strip_model()->GetActiveWebContents()) ==
      target_visible_page_action_count;
}

bool HaveAllExtensionRenderFrameHostsFinishedLoading(
    extensions::ProcessManager* manager) {
  extensions::ProcessManager::FrameSet all_views = manager->GetAllFrames();
  for (content::RenderFrameHost* host : manager->GetAllFrames()) {
    if (content::WebContents::FromRenderFrameHost(host)->IsLoading())
      return false;
  }
  return true;
}

bool IsExtensionNotIdle(const std::string& extension_id,
                        content::BrowserContext* context) {
  return !extensions::util::IsExtensionIdle(extension_id, context);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver::NotificationSet

class ExtensionTestNotificationObserver::NotificationSet
    : public content::NotificationObserver,
      public extensions::ProcessManagerObserver {
 public:
  NotificationSet() : process_manager_observer_(this) {}
  ~NotificationSet() override {}

  void Add(int type, const content::NotificationSource& source);
  void Add(int type);
  void AddExtensionFrameUnregistration(extensions::ProcessManager* manager);

  // Notified any time an Add()ed notification is received.
  // The details of the notification are dropped.
  base::CallbackList<void()>& callback_list() {
    return callback_list_;
  }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::ProcessManagerObserver:
  void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;

  content::NotificationRegistrar notification_registrar_;
  base::CallbackList<void()> callback_list_;
  ScopedObserver<extensions::ProcessManager, extensions::ProcessManagerObserver>
      process_manager_observer_;
};

void ExtensionTestNotificationObserver::NotificationSet::Add(
    int type,
    const content::NotificationSource& source) {
  notification_registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::NotificationSet::Add(int type) {
  Add(type, content::NotificationService::AllSources());
}

void ExtensionTestNotificationObserver::NotificationSet::
    AddExtensionFrameUnregistration(extensions::ProcessManager* manager) {
  process_manager_observer_.Add(manager);
}

void ExtensionTestNotificationObserver::NotificationSet::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  callback_list_.Notify();
}

void ExtensionTestNotificationObserver::NotificationSet::
    OnExtensionFrameUnregistered(const std::string& extension_id,
                                 content::RenderFrameHost* render_frame_host) {
  callback_list_.Notify();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ExtensionTestNotificationObserver::ExtensionTestNotificationObserver(
    Browser* browser)
    : browser_(browser),
      profile_(NULL),
      extension_installs_observed_(0),
      extension_load_errors_observed_(0),
      crx_installers_done_observed_(0) {
}

ExtensionTestNotificationObserver::~ExtensionTestNotificationObserver() {}

Profile* ExtensionTestNotificationObserver::GetProfile() {
  if (!profile_) {
    if (browser_)
      profile_ = browser_->profile();
    else
      profile_ = ProfileManager::GetActiveUserProfile();
  }
  return profile_;
}

void ExtensionTestNotificationObserver::WaitForNotification(
    int notification_type) {
  // TODO(bauerb): Using a WindowedNotificationObserver like this can break
  // easily, if the notification we're waiting for is sent before this method.
  // Change it so that the WindowedNotificationObserver is constructed earlier.
  content::NotificationRegistrar registrar;
  registrar.Add(
      this, notification_type, content::NotificationService::AllSources());
  content::WindowedNotificationObserver(
      notification_type, content::NotificationService::AllSources()).Wait();
}

bool ExtensionTestNotificationObserver::WaitForPageActionVisibilityChangeTo(
    int count) {
  extensions::ExtensionActionAPI::Get(GetProfile())->AddObserver(this);
  WaitForCondition(
      base::Bind(&HasPageActionVisibilityReachedTarget, browser_, count),
      NULL);
  extensions::ExtensionActionAPI::Get(GetProfile())->
      RemoveObserver(this);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionViewsToLoad() {
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(GetProfile());
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  notification_set.Add(content::NOTIFICATION_LOAD_STOP);
  notification_set.AddExtensionFrameUnregistration(manager);
  WaitForCondition(
      base::Bind(&HaveAllExtensionRenderFrameHostsFinishedLoading, manager),
      &notification_set);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionIdle(
    const std::string& extension_id) {
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  WaitForCondition(base::Bind(&extensions::util::IsExtensionIdle, extension_id,
                              GetProfile()),
                   &notification_set);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionNotIdle(
    const std::string& extension_id) {
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_LOAD_STOP);
  WaitForCondition(base::Bind(&IsExtensionNotIdle, extension_id, GetProfile()),
                   &notification_set);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllSources()).Wait();
  return extension_installs_observed_ == before;
}

void ExtensionTestNotificationObserver::WaitForExtensionLoad() {
  WaitForNotification(extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED);
}

void ExtensionTestNotificationObserver::WaitForExtensionAndViewLoad() {
  this->WaitForExtensionLoad();
  WaitForExtensionViewsToLoad();
}

bool ExtensionTestNotificationObserver::WaitForExtensionLoadError() {
  int before = extension_load_errors_observed_;
  WaitForNotification(extensions::NOTIFICATION_EXTENSION_LOAD_ERROR);
  return extension_load_errors_observed_ != before;
}

bool ExtensionTestNotificationObserver::WaitForExtensionCrash(
    const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      GetProfile())->extension_service();

  if (!service->GetExtensionById(extension_id, true)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources()).Wait();
  return (service->GetExtensionById(extension_id, true) == NULL);
}

bool ExtensionTestNotificationObserver::WaitForCrxInstallerDone() {
  int before = crx_installers_done_observed_;
  WaitForNotification(extensions::NOTIFICATION_CRX_INSTALLER_DONE);
  return crx_installers_done_observed_ == (before + 1);
}

void ExtensionTestNotificationObserver::Watch(
    int type,
    const content::NotificationSource& source) {
  CHECK(!observer_);
  observer_.reset(new content::WindowedNotificationObserver(type, source));
  registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::Wait() {
  observer_->Wait();

  registrar_.RemoveAll();
  observer_.reset();
}

void ExtensionTestNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
      last_loaded_extension_id_ =
        content::Details<const Extension>(details).ptr()->id();
      VLOG(1) << "Got EXTENSION_LOADED notification.";
      break;

    case extensions::NOTIFICATION_CRX_INSTALLER_DONE:
      VLOG(1) << "Got CRX_INSTALLER_DONE notification.";
      {
          const Extension* extension =
            content::Details<const Extension>(details).ptr();
          if (extension)
            last_loaded_extension_id_ = extension->id();
          else
            last_loaded_extension_id_.clear();
      }
      ++crx_installers_done_observed_;
      break;

    case extensions::NOTIFICATION_EXTENSION_LOAD_ERROR:
      VLOG(1) << "Got EXTENSION_LOAD_ERROR notification.";
      ++extension_load_errors_observed_;
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ExtensionTestNotificationObserver::OnPageActionsUpdated(
    content::WebContents* web_contents) {
  MaybeQuit();
}

void ExtensionTestNotificationObserver::WaitForCondition(
    const ConditionCallback& condition,
    NotificationSet* notification_set) {
  if (condition.Run())
    return;
  condition_ = condition;

  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  quit_closure_ = runner->QuitClosure();

  std::unique_ptr<base::CallbackList<void()>::Subscription> subscription;
  if (notification_set) {
    subscription = notification_set->callback_list().Add(
        base::Bind(&ExtensionTestNotificationObserver::MaybeQuit,
                   base::Unretained(this)));
  }
  runner->Run();

  condition_.Reset();
  quit_closure_.Reset();
}

void ExtensionTestNotificationObserver::MaybeQuit() {
  if (condition_.Run())
    quit_closure_.Run();
}
