// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_dialog_cloud.h"


#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/printing/print_dialog_cloud_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/google/core/browser/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/printing/common/print_messages.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/web_preferences.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/foreground_helper.h"
#endif

// This module implements the UI support in Chrome for cloud printing.
// This means hosting a dialog containing HTML/JavaScript and using
// the published cloud print user interface integration APIs to get
// page setup settings from the dialog contents and provide the
// generated print data to the dialog contents for uploading to the
// cloud print service.

// Currently, the flow between these classes is as follows:

// PrintDialogCloud::CreatePrintDialogForFile is called from
// resource_message_filter_gtk.cc once the renderer has informed the
// renderer host that print data generation into the renderer host provided
// temp file has been completed.  That call is on the FILE thread.
// That, in turn, hops over to the UI thread to create an instance of
// PrintDialogCloud.

// The constructor for PrintDialogCloud creates a
// CloudPrintWebDialogDelegate and asks the current active browser to
// show an HTML dialog using that class as the delegate. That class
// hands in the kChromeUICloudPrintResourcesURL as the URL to visit.  That is
// recognized by the GetWebUIFactoryFunction as a signal to create an
// ExternalWebDialogUI.

// CloudPrintWebDialogDelegate also temporarily owns a
// CloudPrintFlowHandler, a class which is responsible for the actual
// interactions with the dialog contents, including handing in the
// print data and getting any page setup parameters that the dialog
// contents provides.  As part of bringing up the dialog,
// WebDialogUI::RenderViewCreated is called (an override of
// WebUI::RenderViewCreated).  That routine, in turn, calls the
// delegate's GetWebUIMessageHandlers routine, at which point the
// ownership of the CloudPrintFlowHandler is handed over.  A pointer
// to the flow handler is kept to facilitate communication back and
// forth between the two classes.

// The WebUI continues dialog bring-up, calling
// CloudPrintFlowHandler::RegisterMessages.  This is where the
// additional object model capabilities are registered for the dialog
// contents to use.  It is also at this time that capabilities for the
// dialog contents are adjusted to allow the dialog contents to close
// the window.  In addition, the pending URL is redirected to the
// actual cloud print service URL.  The flow controller also registers
// for notification of when the dialog contents finish loading, which
// is currently used to send the data to the dialog contents.

// In order to send the data to the dialog contents, the flow
// handler uses a CloudPrintDataSender.  It creates one, letting it
// know the name of the temporary file containing the data, and
// posts the task of reading the file
// (CloudPrintDataSender::ReadPrintDataFile) to the file thread.  That
// routine reads in the file, and then hops over to the IO thread to
// send that data to the dialog contents.

// When the dialog contents are finished (by either being cancelled or
// hitting the print button), the delegate is notified, and responds
// that the dialog should be closed, at which point things are torn
// down and released.

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::RenderViewHost;
using content::WebContents;
using content::WebPreferences;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;

namespace {

const int kDefaultWidth = 912;
const int kDefaultHeight = 633;

bool IsSimilarUrl(const GURL& url, const GURL& cloud_print_url) {
  return url.host() == cloud_print_url.host() &&
         StartsWithASCII(url.path(), cloud_print_url.path(), false) &&
         url.scheme() == cloud_print_url.scheme();
}

class SignInObserver : public content::WebContentsObserver {
 public:
  SignInObserver(content::WebContents* web_contents,
                 GURL cloud_print_url,
                 const base::Closure& callback)
      : WebContentsObserver(web_contents),
        cloud_print_url_(cloud_print_url),
        callback_(callback),
        weak_ptr_factory_(this) {
  }

 private:
  // Overridden from content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override {
    if (IsSimilarUrl(params.url, cloud_print_url_)) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&SignInObserver::OnSignIn,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void WebContentsDestroyed() override { delete this; }

  void OnSignIn() {
    callback_.Run();
    if (web_contents())
      web_contents()->Close();
  }

  GURL cloud_print_url_;
  base::Closure callback_;
  base::WeakPtrFactory<SignInObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SignInObserver);
};

}  // namespace

namespace internal_cloud_print_helpers {

// From the JSON parsed value, get the entries for the page setup
// parameters.
bool GetPageSetupParameters(const std::string& json,
                            PrintMsg_Print_Params& parameters) {
  scoped_ptr<base::Value> parsed_value(base::JSONReader::Read(json));
  DLOG_IF(ERROR, (!parsed_value.get() ||
                  !parsed_value->IsType(base::Value::TYPE_DICTIONARY)))
      << "PageSetup call didn't have expected contents";
  if (!parsed_value.get() ||
      !parsed_value->IsType(base::Value::TYPE_DICTIONARY)) {
    return false;
  }

  bool result = true;
  base::DictionaryValue* params =
      static_cast<base::DictionaryValue*>(parsed_value.get());
  result &= params->GetDouble("dpi", &parameters.dpi);
  result &= params->GetDouble("min_shrink", &parameters.min_shrink);
  result &= params->GetDouble("max_shrink", &parameters.max_shrink);
  result &= params->GetBoolean("selection_only", &parameters.selection_only);
  return result;
}

base::string16 GetSwitchValueString16(const base::CommandLine& command_line,
                                      const char* switchName) {
#if defined(OS_WIN)
  return command_line.GetSwitchValueNative(switchName);
#elif defined(OS_POSIX)
  // POSIX Command line string types are different.
  base::CommandLine::StringType native_switch_val;
  native_switch_val = command_line.GetSwitchValueASCII(switchName);
  // Convert the ASCII string to UTF16 to prepare to pass.
  return base::ASCIIToUTF16(native_switch_val);
#endif
}

void CloudPrintDataSenderHelper::CallJavascriptFunction(
    const std::string& function_name,
    const base::Value& arg1,
    const base::Value& arg2) {
  web_ui_->CallJavascriptFunction(function_name, arg1, arg2);
}

// Clears out the pointer we're using to communicate.  Either routine is
// potentially expensive enough that stopping whatever is in progress
// is worth it.
void CloudPrintDataSender::CancelPrintDataFile() {
  base::AutoLock lock(lock_);
  // We don't own helper, it was passed in to us, so no need to
  // delete, just let it go.
  helper_ = NULL;
}

CloudPrintDataSender::CloudPrintDataSender(
    CloudPrintDataSenderHelper* helper,
    const base::string16& print_job_title,
    const base::string16& print_ticket,
    const std::string& file_type,
    const base::RefCountedMemory* data)
    : helper_(helper),
      print_job_title_(print_job_title),
      print_ticket_(print_ticket),
      file_type_(file_type),
      data_(data) {
}

CloudPrintDataSender::~CloudPrintDataSender() {}

// We have the data in hand that needs to be pushed into the dialog
// contents; do so from the IO thread.

// TODO(scottbyer): If the print data ends up being larger than the
// upload limit (currently 10MB), what we need to do is upload that
// large data to google docs and set the URL in the printing
// JavaScript to that location, and make sure it gets deleted when not
// needed. - 4/1/2010
void CloudPrintDataSender::SendPrintData() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!data_.get() || !data_->size())
    return;

  std::string base64_data;
  base::Base64Encode(
      base::StringPiece(data_->front_as<char>(), data_->size()),
      &base64_data);
  std::string header("data:");
  header.append(file_type_);
  header.append(";base64,");
  base64_data.insert(0, header);

  base::AutoLock lock(lock_);
  if (helper_) {
    base::StringValue title(print_job_title_);
    base::StringValue ticket(print_ticket_);
    // TODO(abodenha): Change Javascript call to pass in print ticket
    // after server side support is added. Add test for it.

    // Send the print data to the dialog contents.  The JavaScript
    // function is a preliminary API for prototyping purposes and is
    // subject to change.
    helper_->CallJavascriptFunction(
        "printApp._printDataUrl", base::StringValue(base64_data), title);
  }
}


CloudPrintFlowHandler::CloudPrintFlowHandler(
    const base::RefCountedMemory* data,
    const base::string16& print_job_title,
    const base::string16& print_ticket,
    const std::string& file_type)
    : dialog_delegate_(NULL),
      data_(data),
      print_job_title_(print_job_title),
      print_ticket_(print_ticket),
      file_type_(file_type) {
}

CloudPrintFlowHandler::~CloudPrintFlowHandler() {
  // This will also cancel any task in flight.
  CancelAnyRunningTask();
}


void CloudPrintFlowHandler::SetDialogDelegate(
    CloudPrintWebDialogDelegate* delegate) {
  // Even if setting a new WebUI, it means any previous task needs
  // to be canceled, its now invalid.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelAnyRunningTask();
  dialog_delegate_ = delegate;
}

// Cancels any print data sender we have in flight and removes our
// reference to it, so when the task that is calling it finishes and
// removes its reference, it goes away.
void CloudPrintFlowHandler::CancelAnyRunningTask() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (print_data_sender_.get()) {
    print_data_sender_->CancelPrintDataFile();
    print_data_sender_ = NULL;
  }
}

void CloudPrintFlowHandler::RegisterMessages() {
  // TODO(scottbyer) - This is where we will register messages for the
  // UI JS to use.  Needed: Call to update page setup parameters.
  web_ui()->RegisterMessageCallback("ShowDebugger",
      base::Bind(&CloudPrintFlowHandler::HandleShowDebugger,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SendPrintData",
      base::Bind(&CloudPrintFlowHandler::HandleSendPrintData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("SetPageParameters",
      base::Bind(&CloudPrintFlowHandler::HandleSetPageParameters,
                 base::Unretained(this)));

  // Register for appropriate notifications, and re-direct the URL
  // to the real server URL, now that we've gotten an HTML dialog
  // going.
  NavigationController* controller =
      &web_ui()->GetWebContents()->GetController();
  NavigationEntry* pending_entry = controller->GetPendingEntry();
  if (pending_entry) {
    pending_entry->SetURL(google_util::AppendGoogleLocaleParam(
        cloud_devices::GetCloudPrintRelativeURL("client/dialog.html"),
        g_browser_process->GetApplicationLocale()));
  }
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::Source<NavigationController>(controller));
}

void CloudPrintFlowHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_STOP: {
      GURL url = web_ui()->GetWebContents()->GetURL();
      if (IsCloudPrintDialogUrl(url)) {
        // Take the opportunity to set some (minimal) additional
        // script permissions required for the web UI.
        RenderViewHost* rvh = web_ui()->GetWebContents()->GetRenderViewHost();
        if (rvh) {
          // TODO(chrishtr): this is wrong. allow_scripts_to_close_windows will
          // be reset the next time a preference changes.
          WebPreferences webkit_prefs = rvh->GetWebkitPreferences();
          webkit_prefs.allow_scripts_to_close_windows = true;
          rvh->UpdateWebkitPreferences(webkit_prefs);
        } else {
          NOTREACHED();
        }
        // Choose one or the other.  If you need to debug, bring up the
        // debugger.  You can then use the various chrome.send()
        // registrations above to kick of the various function calls,
        // including chrome.send("SendPrintData") in the javaScript
        // console and watch things happen with:
        // HandleShowDebugger(NULL);
        HandleSendPrintData(NULL);
      }
      break;
    }
  }
}

void CloudPrintFlowHandler::HandleShowDebugger(const base::ListValue* args) {
  ShowDebugger();
}

void CloudPrintFlowHandler::ShowDebugger() {
  if (web_ui()) {
    WebContents* web_contents = web_ui()->GetWebContents();
    if (web_contents)
      DevToolsWindow::OpenDevToolsWindow(web_contents);
  }
}

scoped_refptr<CloudPrintDataSender>
CloudPrintFlowHandler::CreateCloudPrintDataSender() {
  DCHECK(web_ui());
  print_data_helper_.reset(new CloudPrintDataSenderHelper(web_ui()));
  scoped_refptr<CloudPrintDataSender> sender(
      new CloudPrintDataSender(print_data_helper_.get(),
                               print_job_title_,
                               print_ticket_,
                               file_type_,
                               data_.get()));
  return sender;
}

void CloudPrintFlowHandler::HandleSendPrintData(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This will cancel any ReadPrintDataFile() or SendPrintDataFile()
  // requests in flight (this is anticipation of when setting page
  // setup parameters becomes asynchronous and may be set while some
  // data is in flight).  Then we can clear out the print data.
  CancelAnyRunningTask();
  if (web_ui()) {
    print_data_sender_ = CreateCloudPrintDataSender();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&CloudPrintDataSender::SendPrintData, print_data_sender_));
  }
}

void CloudPrintFlowHandler::HandleSetPageParameters(
    const base::ListValue* args) {
  std::string json;
  bool ret = args->GetString(0, &json);
  if (!ret || json.empty()) {
    NOTREACHED() << "Empty json string";
    return;
  }

  // These are backstop default values - 72 dpi to match the screen,
  // 8.5x11 inch paper with margins subtracted (1/4 inch top, left,
  // right and 0.56 bottom), and the min page shrink and max page
  // shrink values appear all over the place with no explanation.

  // TODO(scottbyer): Get a Linux/ChromeOS edge for PrintSettings
  // working so that we can get the default values from there.  Fix up
  // PrintWebViewHelper to do the same.
  const int kDPI = 72;
  const int kWidth = static_cast<int>((8.5-0.25-0.25)*kDPI);
  const int kHeight = static_cast<int>((11-0.25-0.56)*kDPI);
  const double kMinPageShrink = 1.25;
  const double kMaxPageShrink = 2.0;

  PrintMsg_Print_Params default_settings;
  default_settings.content_size = gfx::Size(kWidth, kHeight);
  default_settings.printable_area = gfx::Rect(0, 0, kWidth, kHeight);
  default_settings.dpi = kDPI;
  default_settings.min_shrink = kMinPageShrink;
  default_settings.max_shrink = kMaxPageShrink;
  default_settings.desired_dpi = kDPI;
  default_settings.document_cookie = 0;
  default_settings.selection_only = false;
  default_settings.preview_request_id = 0;
  default_settings.is_first_request = true;
  default_settings.print_to_pdf = false;

  if (!GetPageSetupParameters(json, default_settings)) {
    NOTREACHED();
    return;
  }

  // TODO(scottbyer) - Here is where we would kick the originating
  // renderer thread with these new parameters in order to get it to
  // re-generate the PDF data and hand it back to us.  window.print() is
  // currently synchronous, so there's a lot of work to do to get to
  // that point.
}

void CloudPrintFlowHandler::StoreDialogClientSize() const {
  if (web_ui() && web_ui()->GetWebContents()) {
    gfx::Size size = web_ui()->GetWebContents()->GetContainerBounds().size();
    Profile* profile = Profile::FromWebUI(web_ui());
    profile->GetPrefs()->SetInteger(prefs::kCloudPrintDialogWidth,
                                    size.width());
    profile->GetPrefs()->SetInteger(prefs::kCloudPrintDialogHeight,
                                    size.height());
  }
}

bool CloudPrintFlowHandler::IsCloudPrintDialogUrl(const GURL& url) {
  GURL cloud_print_url = cloud_devices::GetCloudPrintURL();
  return IsSimilarUrl(url, cloud_print_url);
}

CloudPrintWebDialogDelegate::CloudPrintWebDialogDelegate(
    content::BrowserContext* browser_context,
    gfx::NativeView modal_parent,
    const base::RefCountedMemory* data,
    const std::string& json_arguments,
    const base::string16& print_job_title,
    const base::string16& print_ticket,
    const std::string& file_type)
    : flow_handler_(
          new CloudPrintFlowHandler(data, print_job_title, print_ticket,
                                    file_type)),
      modal_parent_(modal_parent),
      owns_flow_handler_(true),
      keep_alive_when_non_modal_(true) {
  Init(browser_context, json_arguments);
}

// For unit testing.
CloudPrintWebDialogDelegate::CloudPrintWebDialogDelegate(
    CloudPrintFlowHandler* flow_handler,
    const std::string& json_arguments)
    : flow_handler_(flow_handler),
      modal_parent_(NULL),
      owns_flow_handler_(true),
      keep_alive_when_non_modal_(false) {
  Init(NULL, json_arguments);
}

// Returns the persisted width/height for the print dialog.
void GetDialogWidthAndHeightFromPrefs(content::BrowserContext* browser_context,
                                      int* width,
                                      int* height) {
  if (!browser_context) {
    *width = kDefaultWidth;
    *height = kDefaultHeight;
    return;
  }

  PrefService* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();
  *width = prefs->GetInteger(prefs::kCloudPrintDialogWidth);
  *height = prefs->GetInteger(prefs::kCloudPrintDialogHeight);
}

void CloudPrintWebDialogDelegate::Init(content::BrowserContext* browser_context,
                                       const std::string& json_arguments) {
  // This information is needed to show the dialog HTML content.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  params_.url = GURL(chrome::kChromeUICloudPrintResourcesURL);
  GetDialogWidthAndHeightFromPrefs(browser_context,
                                   &params_.width,
                                   &params_.height);
  params_.json_input = json_arguments;

  flow_handler_->SetDialogDelegate(this);
  // If we're not modal we can show the dialog with no browser.
  // We need this to keep Chrome alive while our dialog is up.
  if (!modal_parent_ && keep_alive_when_non_modal_)
    chrome::IncrementKeepAliveCount();
}

CloudPrintWebDialogDelegate::~CloudPrintWebDialogDelegate() {
  // If the flow_handler_ is about to outlive us because we don't own
  // it anymore, we need to have it remove its reference to us.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  flow_handler_->SetDialogDelegate(NULL);
  if (owns_flow_handler_) {
    delete flow_handler_;
  }
}

ui::ModalType CloudPrintWebDialogDelegate::GetDialogModalType() const {
    return modal_parent_ ? ui::MODAL_TYPE_WINDOW : ui::MODAL_TYPE_NONE;
}

base::string16 CloudPrintWebDialogDelegate::GetDialogTitle() const {
  return base::string16();
}

GURL CloudPrintWebDialogDelegate::GetDialogContentURL() const {
  return params_.url;
}

void CloudPrintWebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(flow_handler_);
  // We don't own flow_handler_ anymore, but it sticks around until at
  // least right after OnDialogClosed() is called (and this object is
  // destroyed).
  owns_flow_handler_ = false;
}

void CloudPrintWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->set_width(params_.width);
  size->set_height(params_.height);
}

std::string CloudPrintWebDialogDelegate::GetDialogArgs() const {
  return params_.json_input;
}

void CloudPrintWebDialogDelegate::OnDialogClosed(
    const std::string& json_retval) {
  // Get the final dialog size and store it.
  flow_handler_->StoreDialogClientSize();

  // If we're modal we can show the dialog with no browser.
  // End the keep-alive so that Chrome can exit.
  if (!modal_parent_ && keep_alive_when_non_modal_) {
    // Post to prevent recursive call tho this function.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&chrome::DecrementKeepAliveCount));
  }
  delete this;
}

void CloudPrintWebDialogDelegate::OnCloseContents(WebContents* source,
                                                  bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool CloudPrintWebDialogDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool CloudPrintWebDialogDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}

// Called from the UI thread, starts up the dialog.
void CreateDialogImpl(content::BrowserContext* browser_context,
                      gfx::NativeView modal_parent,
                      const base::RefCountedMemory* data,
                      const base::string16& print_job_title,
                      const base::string16& print_ticket,
                      const std::string& file_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebDialogDelegate* dialog_delegate =
      new internal_cloud_print_helpers::CloudPrintWebDialogDelegate(
          browser_context, modal_parent, data, std::string(), print_job_title,
          print_ticket, file_type);
#if defined(OS_WIN)
  gfx::NativeWindow window =
#endif
      chrome::ShowWebDialog(modal_parent,
                            Profile::FromBrowserContext(browser_context),
                            dialog_delegate);
#if defined(OS_WIN)
  if (window) {
    HWND dialog_handle;
#if defined(USE_AURA)
    dialog_handle = window->GetHost()->GetAcceleratedWidget();
#else
    dialog_handle = window;
#endif
    if (::GetForegroundWindow() != dialog_handle) {
      ui::ForegroundHelper::SetForeground(dialog_handle);
    }
  }
#endif
}

void CreateDialogForFileImpl(content::BrowserContext* browser_context,
                             gfx::NativeView modal_parent,
                             const base::FilePath& path_to_file,
                             const base::string16& print_job_title,
                             const base::string16& print_ticket,
                             const std::string& file_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  scoped_refptr<base::RefCountedMemory> data;
  int64 file_size = 0;
  if (base::GetFileSize(path_to_file, &file_size) && file_size != 0) {
    std::string file_data;
    if (file_size < kuint32max) {
      file_data.reserve(static_cast<unsigned int>(file_size));
    } else {
      DLOG(WARNING) << " print data file too large to reserve space";
    }
    if (base::ReadFileToString(path_to_file, &file_data)) {
      data = base::RefCountedString::TakeString(&file_data);
    }
  }
  // Proceed even for empty data to simplify testing.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&print_dialog_cloud::CreatePrintDialogForBytes,
                 browser_context, modal_parent, data, print_job_title,
                 print_ticket, file_type));
  base::DeleteFile(path_to_file, false);
}

}  // namespace internal_cloud_print_helpers

namespace print_dialog_cloud {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kCloudPrintDialogWidth, kDefaultWidth);
  registry->RegisterIntegerPref(prefs::kCloudPrintDialogHeight, kDefaultHeight);
}

// Called on the FILE or UI thread.  This is the main entry point into creating
// the dialog.

void CreatePrintDialogForFile(content::BrowserContext* browser_context,
                              gfx::NativeView modal_parent,
                              const base::FilePath& path_to_file,
                              const base::string16& print_job_title,
                              const base::string16& print_ticket,
                              const std::string& file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&internal_cloud_print_helpers::CreateDialogForFileImpl,
                 browser_context, modal_parent, path_to_file, print_job_title,
                 print_ticket, file_type));
}

void CreateCloudPrintSigninTab(Browser* browser,
                               bool add_account,
                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (switches::IsEnableAccountConsistency() &&
      !browser->profile()->IsOffTheRecord()) {
    browser->window()->ShowAvatarBubbleFromAvatarButton(
        add_account ? BrowserWindow::AVATAR_BUBBLE_MODE_ADD_ACCOUNT
                    : BrowserWindow::AVATAR_BUBBLE_MODE_SIGNIN,
        signin::ManageAccountsParams());
  } else {
    GURL url = add_account ? cloud_devices::GetCloudPrintAddAccountURL()
                           : cloud_devices::GetCloudPrintSigninURL();
    content::WebContents* web_contents =
        browser->OpenURL(content::OpenURLParams(
            google_util::AppendGoogleLocaleParam(
                url, g_browser_process->GetApplicationLocale()),
            content::Referrer(),
            NEW_FOREGROUND_TAB,
            ui::PAGE_TRANSITION_AUTO_BOOKMARK,
            false));
    new SignInObserver(web_contents, cloud_devices::GetCloudPrintURL(),
                        callback);
  }
}

void CreatePrintDialogForBytes(content::BrowserContext* browser_context,
                               gfx::NativeView modal_parent,
                               const base::RefCountedMemory* data,
                               const base::string16& print_job_title,
                               const base::string16& print_ticket,
                               const std::string& file_type) {
  internal_cloud_print_helpers::CreateDialogImpl(browser_context, modal_parent,
                                                 data, print_job_title,
                                                 print_ticket, file_type);
}

bool CreatePrintDialogFromCommandLine(Profile* profile,
                                      const base::CommandLine& command_line) {
  DCHECK(command_line.HasSwitch(switches::kCloudPrintFile));
  if (!command_line.GetSwitchValuePath(switches::kCloudPrintFile).empty()) {
    base::FilePath cloud_print_file;
    cloud_print_file =
        command_line.GetSwitchValuePath(switches::kCloudPrintFile);
    if (!cloud_print_file.empty()) {
      base::string16 print_job_title;
      base::string16 print_job_print_ticket;
      if (command_line.HasSwitch(switches::kCloudPrintJobTitle)) {
        print_job_title =
          internal_cloud_print_helpers::GetSwitchValueString16(
              command_line, switches::kCloudPrintJobTitle);
      }
      if (command_line.HasSwitch(switches::kCloudPrintPrintTicket)) {
        print_job_print_ticket =
          internal_cloud_print_helpers::GetSwitchValueString16(
              command_line, switches::kCloudPrintPrintTicket);
      }
      std::string file_type = "application/pdf";
      if (command_line.HasSwitch(switches::kCloudPrintFileType)) {
        file_type = command_line.GetSwitchValueASCII(
            switches::kCloudPrintFileType);
      }

      print_dialog_cloud::CreatePrintDialogForFile(profile, NULL,
          cloud_print_file, print_job_title, print_job_print_ticket, file_type);
      return true;
    }
  }
  return false;
}

}  // namespace print_dialog_cloud
