// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include <vector>

#include "apps/custom_launcher_page_contents.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/user_metrics.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/launcher_page_event_dispatcher.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_resource_manager.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/launcher_page_info.h"
#include "grit/theme_resources.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_controller.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/webview/webview.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/webview/webview.h"
#endif

#if defined(USE_AURA)
#include "ui/keyboard/keyboard_util.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/web_applications/web_app_win.h"
#endif


namespace chrome {
const char kAppLauncherCategoryTag[] = "AppLauncher";
}  // namespace chrome

namespace {

const int kAutoLaunchDefaultTimeoutMilliSec = 50;

#if defined(OS_WIN)
void CreateShortcutInWebAppDir(
    const base::FilePath& app_data_dir,
    base::Callback<void(const base::FilePath&)> callback,
    scoped_ptr<web_app::ShortcutInfo> info) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(web_app::CreateShortcutInWebAppDir, app_data_dir,
                 base::Passed(&info)),
      callback);
}
#endif

void PopulateUsers(const ProfileInfoCache& profile_info,
                   const base::FilePath& active_profile_path,
                   app_list::AppListViewDelegate::Users* users) {
  users->clear();
  const size_t count = profile_info.GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    app_list::AppListViewDelegate::User user;
    user.name = profile_info.GetNameOfProfileAtIndex(i);
    user.email = profile_info.GetUserNameOfProfileAtIndex(i);
    user.profile_path = profile_info.GetPathOfProfileAtIndex(i);
    user.active = active_profile_path == user.profile_path;
    users->push_back(user);
  }
}

// Gets a list of URLs of the custom launcher pages to show in the launcher.
// Returns a URL for each installed launcher page. If --custom-launcher-page is
// specified and valid, also includes that URL.
void GetCustomLauncherPageUrls(content::BrowserContext* browser_context,
                               std::vector<GURL>* urls) {
  // First, check the command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (app_list::switches::IsExperimentalAppListEnabled() &&
      command_line->HasSwitch(app_list::switches::kCustomLauncherPage)) {
    GURL custom_launcher_page_url(command_line->GetSwitchValueASCII(
        app_list::switches::kCustomLauncherPage));

    if (custom_launcher_page_url.SchemeIs(extensions::kExtensionScheme)) {
      urls->push_back(custom_launcher_page_url);
    } else {
      LOG(ERROR) << "Invalid custom launcher page URL: "
                 << custom_launcher_page_url.possibly_invalid_spec();
    }
  }

  // Prevent launcher pages from loading unless the pref is enabled.
  // (Command-line specified pages are exempt from this rule).
  PrefService* profile_prefs = user_prefs::UserPrefs::Get(browser_context);
  if (profile_prefs &&
      profile_prefs->HasPrefPath(prefs::kGoogleNowLauncherEnabled) &&
      !profile_prefs->GetBoolean(prefs::kGoogleNowLauncherEnabled)) {
    return;
  }

  // Search the list of installed extensions for ones with 'launcher_page'.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator it = enabled_extensions.begin();
       it != enabled_extensions.end();
       ++it) {
    const extensions::Extension* extension = it->get();
    extensions::LauncherPageInfo* info =
        extensions::LauncherPageHandler::GetInfo(extension);
    if (!info)
      continue;

    urls->push_back(extension->GetResourceURL(info->page));
  }
}

}  // namespace

AppListViewDelegate::AppListViewDelegate(AppListControllerDelegate* controller)
    : controller_(controller),
      profile_(NULL),
      model_(NULL),
      is_voice_query_(false),
      template_url_service_observer_(this),
      scoped_observer_(this) {
  CHECK(controller_);
  // The SigninManagerFactor and the SigninManagers are observed to keep the
  // profile switcher menu up to date, with the correct list of profiles and the
  // correct email address (or none for signed out users) for each.
  SigninManagerFactory::GetInstance()->AddObserver(this);

  // Start observing all already-created SigninManagers.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();

  for (std::vector<Profile*>::iterator i = profiles.begin();
       i != profiles.end();
       ++i) {
    SigninManagerBase* manager =
        SigninManagerFactory::GetForProfileIfExists(*i);
    if (manager) {
      DCHECK(!scoped_observer_.IsObserving(manager));
      scoped_observer_.Add(manager);
    }
  }

  profile_manager->GetProfileInfoCache().AddObserver(this);
  speech_ui_.reset(new app_list::SpeechUIModel);

#if defined(GOOGLE_CHROME_BUILD)
  gfx::ImageSkia* image;
  {
    // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 GetImageSkiaNamed()"));
    image = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_APP_LIST_GOOGLE_LOGO_VOICE_SEARCH);
  }

  speech_ui_->set_logo(*image);
#endif

  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

AppListViewDelegate::~AppListViewDelegate() {
  // Note that the destructor is not always called. E.g. on Mac, this is owned
  // by a leaky singleton. Essential shutdown work must be done by observing
  // chrome::NOTIFICATION_APP_TERMINATING.
  SetProfile(NULL);
  g_browser_process->profile_manager()->GetProfileInfoCache().RemoveObserver(
      this);

  SigninManagerFactory* factory = SigninManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);
}

void AppListViewDelegate::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    // Note: |search_resource_manager_| has a reference to |speech_ui_| so must
    // be destroyed first.
    search_resource_manager_.reset();
    search_controller_.reset();
    launcher_page_event_dispatcher_.reset();
    custom_page_contents_.clear();
    app_list::StartPageService* start_page_service =
        app_list::StartPageService::Get(profile_);
    if (start_page_service)
      start_page_service->RemoveObserver(this);
#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset();
#endif
    model_ = NULL;
  }

  profile_ = new_profile;
  if (!profile_) {
    speech_ui_->SetSpeechRecognitionState(app_list::SPEECH_RECOGNITION_OFF,
                                          false);
    return;
  }

  // If we are in guest mode, the new profile should be an incognito profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use incognito profile";

  {
    // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 AppListViewDelegate TemplateURL etc."));
    template_url_service_observer_.RemoveAll();
    if (app_list::switches::IsExperimentalAppListEnabled()) {
      TemplateURLService* template_url_service =
          TemplateURLServiceFactory::GetForProfile(profile_);
      template_url_service_observer_.Add(template_url_service);
    }

    model_ = app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
                 ->GetModel();

#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset(
        new AppSyncUIStateWatcher(profile_, model_));
#endif

    SetUpSearchUI();
    SetUpProfileSwitcher();
    SetUpCustomLauncherPages();
    OnTemplateURLServiceChanged();
  }

  // Clear search query.
  model_->search_box()->SetText(base::string16());
}

void AppListViewDelegate::SetUpSearchUI() {
  app_list::StartPageService* start_page_service =
      app_list::StartPageService::Get(profile_);
  if (start_page_service)
    start_page_service->AddObserver(this);

  speech_ui_->SetSpeechRecognitionState(start_page_service
                                            ? start_page_service->state()
                                            : app_list::SPEECH_RECOGNITION_OFF,
                                        false);

  search_resource_manager_.reset(new app_list::SearchResourceManager(
      profile_,
      model_->search_box(),
      speech_ui_.get()));

  search_controller_ = CreateSearchController(profile_, model_, controller_);
}

void AppListViewDelegate::SetUpProfileSwitcher() {
  // If a profile change is observed when there is no app list, there is nothing
  // to update until SetProfile() calls this function again.
  if (!profile_)
    return;

  // Don't populate the app list users if we are on the ash desktop.
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  if (desktop == chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  // Populate the app list users.
  PopulateUsers(g_browser_process->profile_manager()->GetProfileInfoCache(),
                profile_->GetPath(),
                &users_);

  FOR_EACH_OBSERVER(
      app_list::AppListViewDelegateObserver, observers_, OnProfilesChanged());
}

void AppListViewDelegate::SetUpCustomLauncherPages() {
  std::vector<GURL> custom_launcher_page_urls;
  GetCustomLauncherPageUrls(profile_, &custom_launcher_page_urls);
  if (custom_launcher_page_urls.empty())
    return;

  for (std::vector<GURL>::const_iterator it = custom_launcher_page_urls.begin();
       it != custom_launcher_page_urls.end();
       ++it) {
    std::string extension_id = it->host();
    apps::CustomLauncherPageContents* page_contents =
        new apps::CustomLauncherPageContents(
            scoped_ptr<extensions::AppDelegate>(
                new ChromeAppDelegate(scoped_ptr<ScopedKeepAlive>())),
            extension_id);
    page_contents->Initialize(profile_, *it);
    custom_page_contents_.push_back(page_contents);
  }

  std::string first_launcher_page_app_id = custom_launcher_page_urls[0].host();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->GetExtensionById(first_launcher_page_app_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  model_->set_custom_launcher_page_name(extension->name());
  // Only the first custom launcher page gets events dispatched to it.
  launcher_page_event_dispatcher_.reset(
      new app_list::LauncherPageEventDispatcher(profile_,
                                                first_launcher_page_app_id));
}

void AppListViewDelegate::OnHotwordStateChanged(bool started) {
  if (started) {
    if (speech_ui_->state() == app_list::SPEECH_RECOGNITION_READY) {
      OnSpeechRecognitionStateChanged(
          app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING);
    }
  } else {
    if (speech_ui_->state() == app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING)
      OnSpeechRecognitionStateChanged(app_list::SPEECH_RECOGNITION_READY);
  }
}

void AppListViewDelegate::OnHotwordRecognized(
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  DCHECK_EQ(app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING,
            speech_ui_->state());
  ToggleSpeechRecognitionForHotword(preamble);
}

void AppListViewDelegate::SigninManagerCreated(SigninManagerBase* manager) {
  scoped_observer_.Add(manager);
}

void AppListViewDelegate::SigninManagerShutdown(SigninManagerBase* manager) {
  if (scoped_observer_.IsObserving(manager))
    scoped_observer_.Remove(manager);
}

void AppListViewDelegate::GoogleSigninFailed(
    const GoogleServiceAuthError& error) {
  SetUpProfileSwitcher();
}

void AppListViewDelegate::GoogleSigninSucceeded(const std::string& account_id,
                                                const std::string& username,
                                                const std::string& password) {
  SetUpProfileSwitcher();
}

void AppListViewDelegate::GoogleSignedOut(const std::string& account_id,
                                          const std::string& username) {
  SetUpProfileSwitcher();
}

void AppListViewDelegate::OnProfileAdded(const base::FilePath& profile_path) {
  SetUpProfileSwitcher();
}

void AppListViewDelegate::OnProfileWasRemoved(
    const base::FilePath& profile_path,
    const base::string16& profile_name) {
  SetUpProfileSwitcher();
}

void AppListViewDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  SetUpProfileSwitcher();
}

bool AppListViewDelegate::ForceNativeDesktop() const {
  return controller_->ForceNativeDesktop();
}

void AppListViewDelegate::SetProfileByPath(const base::FilePath& profile_path) {
  DCHECK(model_);
  // The profile must be loaded before this is called.
  SetProfile(
      g_browser_process->profile_manager()->GetProfileByPath(profile_path));
}

app_list::AppListModel* AppListViewDelegate::GetModel() {
  return model_;
}

app_list::SpeechUIModel* AppListViewDelegate::GetSpeechUI() {
  return speech_ui_.get();
}

void AppListViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
#if defined(OS_WIN)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    callback.Run(base::FilePath());
    return;
  }

  base::FilePath app_data_dir(
      web_app::GetWebAppDataDirectory(profile_->GetPath(),
                                      extension->id(),
                                      GURL()));

  web_app::GetShortcutInfoForApp(
      extension,
      profile_,
      base::Bind(CreateShortcutInWebAppDir, app_data_dir, callback));
#else
  callback.Run(base::FilePath());
#endif
}

void AppListViewDelegate::StartSearch() {
  if (search_controller_) {
    search_controller_->Start(is_voice_query_);
    controller_->OnSearchStarted();
  }
}

void AppListViewDelegate::StopSearch() {
  if (search_controller_)
    search_controller_->Stop();
}

void AppListViewDelegate::OpenSearchResult(
    app_list::SearchResult* result,
    bool auto_launch,
    int event_flags) {
  if (auto_launch)
    base::RecordAction(base::UserMetricsAction("AppList_AutoLaunched"));
  search_controller_->OpenResult(result, event_flags);
  is_voice_query_ = false;
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  search_controller_->InvokeResultAction(result, action_index, event_flags);
}

base::TimeDelta AppListViewDelegate::GetAutoLaunchTimeout() {
  return auto_launch_timeout_;
}

void AppListViewDelegate::AutoLaunchCanceled() {
  if (is_voice_query_) {
    base::RecordAction(base::UserMetricsAction("AppList_AutoLaunchCanceled"));
    // Cancelling the auto launch means we are no longer in a voice query.
    is_voice_query_ = false;
  }
  auto_launch_timeout_ = base::TimeDelta();
}

void AppListViewDelegate::ViewInitialized() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListShown();
    if (service->HotwordEnabled()) {
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile_);
      if (hotword_service)
        hotword_service->RequestHotwordSession(this);
    }
    OnHotwordStateChanged(service->HotwordEnabled());
  }
}

void AppListViewDelegate::Dismiss()  {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();

  if (!profile_)
    return;

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListHidden();
    if (service->HotwordEnabled()) {
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile_);
      if (hotword_service) {
        hotword_service->StopHotwordSession(this);

        // If we're in always-on mode, we always want to restart hotwording
        // after closing the launcher window. So, in always-on mode, hotwording
        // is stopped, and then started again right away. Note that hotwording
        // may already be stopped. The call to StopHotwordSession() above both
        // explicitly stops hotwording, if it's running, and clears the
        // association between the hotword service and |this|.  When starting up
        // hotwording, pass nullptr as the client so that hotword triggers cause
        // the launcher to open.
        // TODO(amistry): This only works on ChromeOS since Chrome hides the
        // launcher instead of destroying it. Make this work on Chrome.
        if (hotword_service->IsAlwaysOnEnabled())
          hotword_service->RequestHotwordSession(nullptr);
      }
    }
  }
}

gfx::ImageSkia AppListViewDelegate::GetWindowIcon() {
  return controller_->GetWindowIcon();
}

void AppListViewDelegate::OpenSettings() {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_misc::kSettingsAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  DCHECK(extension);
  controller_->ActivateApp(profile_,
                           extension,
                           AppListControllerDelegate::LAUNCH_FROM_UNKNOWN,
                           0);
}

void AppListViewDelegate::OpenHelp() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile_, desktop);
  content::OpenURLParams params(GURL(chrome::kAppLauncherHelpURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                false);
  displayer.browser()->OpenURL(params);
}

void AppListViewDelegate::OpenFeedback() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  Browser* browser = chrome::FindTabbedBrowser(profile_, false, desktop);
  chrome::ShowFeedbackPage(browser, std::string(),
                           chrome::kAppLauncherCategoryTag);
}

void AppListViewDelegate::ToggleSpeechRecognition() {
  ToggleSpeechRecognitionForHotword(nullptr);
}

void AppListViewDelegate::ToggleSpeechRecognitionForHotword(
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);

  // Don't start the recognizer or stop the hotword session if there is a
  // network error. Show the network error message instead.
  if (service) {
    if (service->state() == app_list::SPEECH_RECOGNITION_NETWORK_ERROR) {
      speech_ui_->SetSpeechRecognitionState(
          app_list::SPEECH_RECOGNITION_NETWORK_ERROR, true);
      return;
    }
    service->ToggleSpeechRecognition(preamble);
  }

  // With the new hotword extension, stop the hotword session. With the launcher
  // and NTP, this is unnecessary since the hotwording is implicitly stopped.
  // However, for always on, hotword triggering launches the launcher which
  // starts a session and hence starts the hotword detector. This results in the
  // hotword detector and the speech-to-text engine running in parallel, which
  // will conflict with each other (i.e. saying 'Ok Google' twice in a row
  // should cause a search to happen for 'Ok Google', not two hotword triggers).
  // To get around this, always stop the session when switching to speech
  // recognition.
  if (service && service->HotwordEnabled()) {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    if (hotword_service)
      hotword_service->StopHotwordSession(this);
  }
}

void AppListViewDelegate::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  controller_->ShowForProfileByPath(profile_path);
}

void AppListViewDelegate::OnSpeechResult(const base::string16& result,
                                         bool is_final) {
  speech_ui_->SetSpeechResult(result, is_final);
  if (is_final) {
    auto_launch_timeout_ = base::TimeDelta::FromMilliseconds(
        kAutoLaunchDefaultTimeoutMilliSec);
    is_voice_query_ = true;
    model_->search_box()->SetText(result);
  }
}

void AppListViewDelegate::OnSpeechSoundLevelChanged(int16 level) {
  speech_ui_->UpdateSoundLevel(level);
}

void AppListViewDelegate::OnSpeechRecognitionStateChanged(
    app_list::SpeechRecognitionState new_state) {
  speech_ui_->SetSpeechRecognitionState(new_state, false);

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  // With the new hotword extension, we need to re-request hotwording after
  // speech recognition has stopped. Do not request hotwording after the app
  // list has already closed.
  if (new_state == app_list::SPEECH_RECOGNITION_READY &&
      service && service->HotwordEnabled() &&
      controller_->GetAppListWindow()) {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    if (hotword_service) {
      hotword_service->RequestHotwordSession(this);
    }
  }
}

#if defined(TOOLKIT_VIEWS)
views::View* AppListViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (!service)
    return NULL;

  service->LoadContentsIfNeeded();

  content::WebContents* web_contents = service->GetStartPageContents();
  if (!web_contents)
    return NULL;

  DCHECK_EQ(profile_, web_contents->GetBrowserContext());
  views::WebView* web_view = new views::WebView(
      web_contents->GetBrowserContext());
  web_view->SetPreferredSize(size);
  web_view->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetWebContents(web_contents);
  return web_view;
}

std::vector<views::View*> AppListViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  std::vector<views::View*> web_views;

  for (ScopedVector<apps::CustomLauncherPageContents>::const_iterator it =
           custom_page_contents_.begin();
       it != custom_page_contents_.end();
       ++it) {
    content::WebContents* web_contents = (*it)->web_contents();

    // The web contents should belong to the current profile.
    DCHECK_EQ(profile_, web_contents->GetBrowserContext());

    // Make the webview transparent.
    content::RenderWidgetHostView* render_view_host_view =
        web_contents->GetRenderViewHost()->GetView();
    // The RenderWidgetHostView may be null if the renderer has crashed.
    if (render_view_host_view)
      render_view_host_view->SetBackgroundColor(SK_ColorTRANSPARENT);

    views::WebView* web_view =
        new views::WebView(web_contents->GetBrowserContext());
    web_view->SetPreferredSize(size);
    web_view->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
    web_view->SetWebContents(web_contents);
    web_views.push_back(web_view);
  }

  return web_views;
}

void AppListViewDelegate::CustomLauncherPageAnimationChanged(double progress) {
  if (launcher_page_event_dispatcher_)
    launcher_page_event_dispatcher_->ProgressChanged(progress);
}

void AppListViewDelegate::CustomLauncherPagePopSubpage() {
  if (launcher_page_event_dispatcher_)
    launcher_page_event_dispatcher_->PopSubpage();
}
#endif

bool AppListViewDelegate::IsSpeechRecognitionEnabled() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  return service && service->GetSpeechRecognitionContents();
}

const app_list::AppListViewDelegate::Users&
AppListViewDelegate::GetUsers() const {
  return users_;
}

bool AppListViewDelegate::ShouldCenterWindow() const {
  // Some ChromeOS devices (those that support TouchView mode) turn this flag on
  // by default, which ensures that the app list is consistently centered on
  // those devices. This avoids having the app list change shape and position as
  // the user enters and exits TouchView mode.
  if (app_list::switches::IsCenteredAppListEnabled())
    return true;

  // keyboard depends upon Aura.
#if defined(USE_AURA)
  // If the virtual keyboard is enabled, use the new app list position. The old
  // position is too tall, and doesn't fit in the left-over screen space.
  if (keyboard::IsKeyboardEnabled())
    return true;
#endif

  return false;
}

void AppListViewDelegate::AddObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListViewDelegate::RemoveObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListViewDelegate::OnTemplateURLServiceChanged() {
  if (!app_list::switches::IsExperimentalAppListEnabled())
    return;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  bool is_google =
      TemplateURLPrepopulateData::GetEngineType(
          *default_provider, template_url_service->search_terms_data()) ==
      SEARCH_ENGINE_GOOGLE;

  model_->SetSearchEngineIsGoogle(is_google);

  app_list::StartPageService* start_page_service =
      app_list::StartPageService::Get(profile_);
  if (start_page_service)
    start_page_service->set_search_engine_is_google(is_google);
}

void AppListViewDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      FOR_EACH_OBSERVER(
          app_list::AppListViewDelegateObserver, observers_, OnShutdown());

      SetProfile(NULL);  // Ensures launcher page web contents are torn down.

      // SigninManagerFactory is not a leaky singleton (unlike this class), and
      // its destructor will check that it has no remaining observers.
      scoped_observer_.RemoveAll();
      SigninManagerFactory::GetInstance()->RemoveObserver(this);
      break;
    default:
      NOTREACHED();
  }
}
