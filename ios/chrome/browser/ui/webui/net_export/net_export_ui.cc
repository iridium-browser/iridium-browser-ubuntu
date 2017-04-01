// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/net_export/net_export_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/net_log/chrome_net_log.h"
#include "components/net_log/net_export_ui_constants.h"
#include "components/net_log/net_log_file_writer.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ui/show_mail_composer_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

web::WebUIIOSDataSource* CreateNetExportHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUINetExportHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread. All static
// functions except SendEmail run on FILE_USER_BLOCKING thread.
class NetExportMessageHandler
    : public web::WebUIIOSMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler> {
 public:
  NetExportMessageHandler();
  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages.
  void OnGetExportNetLogInfo(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);

 private:
  // Calls NetLogFileWriter's ProcessCommand with DO_START and DO_STOP commands.
  static void ProcessNetLogCommand(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      net_log::NetLogFileWriter* net_log_file_writer,
      net_log::NetLogFileWriter::Command command);

  // Returns the path to the file which has NetLog data.
  static base::FilePath GetNetLogFileName(
      net_log::NetLogFileWriter* net_log_file_writer);

  // Send state/file information from NetLogFileWriter.
  static void SendExportNetLogInfo(
      base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
      net_log::NetLogFileWriter* net_log_file_writer);

  // Send NetLog data via email. This runs on UI thread.
  static void SendEmail(const base::FilePath& file_to_send);

  // Call NetExportView.onExportNetLogInfoChanged JavsScript function in the
  // renderer, passing in |arg|. Takes ownership of |arg|.
  void OnExportNetLogInfoChanged(base::Value* arg);

  // Cache of GetApplicationContex()->GetNetLog()->net_log_file_writer(). This
  // is owned by ChromeNetLog which is owned by BrowserProcessImpl. There are
  // four instances in this class where a pointer to net_log_file_writer_ is
  // posted to the FILE_USER_BLOCKING thread. Base::Unretained is used here
  // because BrowserProcessImpl is destroyed on the UI thread after joining the
  // FILE_USER_BLOCKING thread making it impossible for there to be an invalid
  // pointer this object when going back to the UI thread. Furthermore this
  // pointer is never dereferenced prematurely on the UI thread. Thus the
  // lifetime of this object is assured and can be safely used with
  // base::Unretained.
  net_log::NetLogFileWriter* net_log_file_writer_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : net_log_file_writer_(
          GetApplicationContext()->GetNetLog()->net_log_file_writer()),
      weak_ptr_factory_(this) {}

NetExportMessageHandler::~NetExportMessageHandler() {
  // Cancel any in-progress requests to collect net_log into temporary file.
  web::WebThread::PostTask(
      web::WebThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&net_log::NetLogFileWriter::ProcessCommand,
                 base::Unretained(net_log_file_writer_),
                 net_log::NetLogFileWriter::DO_STOP));
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  web_ui()->RegisterMessageCallback(
      net_log::kGetExportNetLogInfoHandler,
      base::Bind(&NetExportMessageHandler::OnGetExportNetLogInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStartNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStartNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStopNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStopNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kSendNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnSendNetLog,
                 base::Unretained(this)));
}

void NetExportMessageHandler::OnGetExportNetLogInfo(
    const base::ListValue* list) {
  web::WebThread::PostTask(
      web::WebThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&NetExportMessageHandler::SendExportNetLogInfo,
                 weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_));
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  std::string log_mode;
  bool result = list->GetString(0, &log_mode);
  DCHECK(result);

  net_log::NetLogFileWriter::Command command;
  if (log_mode == "LOG_BYTES") {
    command = net_log::NetLogFileWriter::DO_START_LOG_BYTES;
  } else if (log_mode == "NORMAL") {
    command = net_log::NetLogFileWriter::DO_START;
  } else {
    DCHECK_EQ("STRIP_PRIVATE_DATA", log_mode);
    command = net_log::NetLogFileWriter::DO_START_STRIP_PRIVATE_DATA;
  }

  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_,
                       command);
}

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  ProcessNetLogCommand(weak_ptr_factory_.GetWeakPtr(), net_log_file_writer_,
                       net_log::NetLogFileWriter::DO_STOP);
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  web::WebThread::PostTaskAndReplyWithResult(
      web::WebThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&NetExportMessageHandler::GetNetLogFileName,
                 base::Unretained(net_log_file_writer_)),
      base::Bind(&NetExportMessageHandler::SendEmail));
}

// static
void NetExportMessageHandler::ProcessNetLogCommand(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    net_log::NetLogFileWriter* net_log_file_writer,
    net_log::NetLogFileWriter::Command command) {
  if (!web::WebThread::CurrentlyOn(web::WebThread::FILE_USER_BLOCKING)) {
    web::WebThread::PostTask(
        web::WebThread::FILE_USER_BLOCKING, FROM_HERE,
        base::Bind(&NetExportMessageHandler::ProcessNetLogCommand,
                   net_export_message_handler, net_log_file_writer, command));
    return;
  }

  DCHECK_CURRENTLY_ON(web::WebThread::FILE_USER_BLOCKING);
  net_log_file_writer->ProcessCommand(command);
  SendExportNetLogInfo(net_export_message_handler, net_log_file_writer);
}

// static
base::FilePath NetExportMessageHandler::GetNetLogFileName(
    net_log::NetLogFileWriter* net_log_file_writer) {
  DCHECK_CURRENTLY_ON(web::WebThread::FILE_USER_BLOCKING);
  base::FilePath net_export_file_path;
  net_log_file_writer->GetFilePath(&net_export_file_path);
  return net_export_file_path;
}

// static
void NetExportMessageHandler::SendExportNetLogInfo(
    base::WeakPtr<NetExportMessageHandler> net_export_message_handler,
    net_log::NetLogFileWriter* net_log_file_writer) {
  DCHECK_CURRENTLY_ON(web::WebThread::FILE_USER_BLOCKING);
  base::Value* value = net_log_file_writer->GetState();
  if (!web::WebThread::PostTask(
          web::WebThread::UI, FROM_HERE,
          base::Bind(&NetExportMessageHandler::OnExportNetLogInfoChanged,
                     net_export_message_handler, value))) {
    // Failed posting the task, avoid leaking.
    delete value;
  }
}

// static
void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  if (file_to_send.empty())
    return;
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  std::string email;
  std::string subject = "net_internals_log";
  std::string title = "Issue number: ";
  std::string body =
      "Please add some informative text about the network issues.";
  ShowMailComposer(base::UTF8ToUTF16(email), base::UTF8ToUTF16(subject),
                   base::UTF8ToUTF16(body), base::UTF8ToUTF16(title),
                   file_to_send,
                   IDS_IOS_NET_EXPORT_NO_EMAIL_ACCOUNTS_ALERT_TITLE,
                   IDS_IOS_NET_EXPORT_NO_EMAIL_ACCOUNTS_ALERT_MESSAGE);
}

void NetExportMessageHandler::OnExportNetLogInfoChanged(base::Value* arg) {
  std::unique_ptr<base::Value> value(arg);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web_ui()->CallJavascriptFunction(net_log::kOnExportNetLogInfoChanged, *arg);
}

}  // namespace

NetExportUI::NetExportUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<NetExportMessageHandler>());
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateNetExportHTMLSource());
}
