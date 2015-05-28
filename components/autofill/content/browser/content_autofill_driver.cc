// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_message_macros.h"

namespace autofill {

ContentAutofillDriver::ContentAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    AutofillManager::AutofillDownloadManagerState enable_download_manager)
    : render_frame_host_(render_frame_host),
      client_(client),
      autofill_manager_(new AutofillManager(this,
                                            client,
                                            app_locale,
                                            enable_download_manager)),
      autofill_external_delegate_(autofill_manager_.get(), this),
      request_autocomplete_manager_(this) {
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

ContentAutofillDriver::~ContentAutofillDriver() {}

bool ContentAutofillDriver::IsOffTheRecord() const {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->IsOffTheRecord();
}

net::URLRequestContextGetter* ContentAutofillDriver::GetURLRequestContext() {
  return render_frame_host_->GetSiteInstance()
      ->GetBrowserContext()
      ->GetRequestContext();
}

base::SequencedWorkerPool* ContentAutofillDriver::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

bool ContentAutofillDriver::RendererIsAvailable() {
  return render_frame_host_->GetRenderViewHost() != NULL;
}

void ContentAutofillDriver::SendFormDataToRenderer(
    int query_id,
    RendererFormDataAction action,
    const FormData& data) {
  if (!RendererIsAvailable())
    return;
  switch (action) {
    case FORM_DATA_ACTION_FILL:
      render_frame_host_->Send(new AutofillMsg_FillForm(
          render_frame_host_->GetRoutingID(), query_id, data));
      break;
    case FORM_DATA_ACTION_PREVIEW:
      render_frame_host_->Send(new AutofillMsg_PreviewForm(
          render_frame_host_->GetRoutingID(), query_id, data));
      break;
  }
}

void ContentAutofillDriver::PingRenderer() {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(
      new AutofillMsg_Ping(render_frame_host_->GetRoutingID()));
}

void ContentAutofillDriver::PropagateAutofillPredictions(
    const std::vector<FormStructure*>& forms) {
  autofill_manager_->client()->PropagateAutofillPredictions(render_frame_host_,
                                                            forms);
}

void ContentAutofillDriver::SendAutofillTypePredictionsToRenderer(
    const std::vector<FormStructure*>& forms) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAutofillTypePredictions))
    return;

  if (!RendererIsAvailable())
    return;

  std::vector<FormDataPredictions> type_predictions =
      FormStructure::GetFieldTypePredictions(forms);
  render_frame_host_->Send(new AutofillMsg_FieldTypePredictionsAvailable(
      render_frame_host_->GetRoutingID(), type_predictions));
}

void ContentAutofillDriver::RendererShouldAcceptDataListSuggestion(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(new AutofillMsg_AcceptDataListSuggestion(
      render_frame_host_->GetRoutingID(), value));
}

void ContentAutofillDriver::RendererShouldClearFilledForm() {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(
      new AutofillMsg_ClearForm(render_frame_host_->GetRoutingID()));
}

void ContentAutofillDriver::RendererShouldClearPreviewedForm() {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(
      new AutofillMsg_ClearPreviewedForm(render_frame_host_->GetRoutingID()));
}

void ContentAutofillDriver::RendererShouldFillFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(new AutofillMsg_FillFieldWithValue(
      render_frame_host_->GetRoutingID(), value));
}

void ContentAutofillDriver::RendererShouldPreviewFieldWithValue(
    const base::string16& value) {
  if (!RendererIsAvailable())
    return;
  render_frame_host_->Send(new AutofillMsg_PreviewFieldWithValue(
      render_frame_host_->GetRoutingID(), value));
}

void ContentAutofillDriver::PopupHidden() {
  // If the unmask prompt is showing, keep showing the preview. The preview
  // will be cleared when the prompt closes.
  if (!autofill_manager_->IsShowingUnmaskPrompt())
    RendererShouldClearPreviewedForm();
}

bool ContentAutofillDriver::HandleMessage(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ContentAutofillDriver, message)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_FirstUserGestureObserved, client_,
                      AutofillClient::OnFirstUserGestureObserved)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_FormsSeen,
                      autofill_manager_.get(),
                      AutofillManager::OnFormsSeen)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_WillSubmitForm, autofill_manager_.get(),
                      AutofillManager::OnWillSubmitForm)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_FormSubmitted,
                      autofill_manager_.get(),
                      AutofillManager::OnFormSubmitted)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_TextFieldDidChange,
                      autofill_manager_.get(),
                      AutofillManager::OnTextFieldDidChange)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_QueryFormFieldAutofill,
                      autofill_manager_.get(),
                      AutofillManager::OnQueryFormFieldAutofill)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidPreviewAutofillFormData,
                      autofill_manager_.get(),
                      AutofillManager::OnDidPreviewAutofillFormData)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PingAck,
                      &autofill_external_delegate_,
                      AutofillExternalDelegate::OnPingAck)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidFillAutofillFormData,
                      autofill_manager_.get(),
                      AutofillManager::OnDidFillAutofillFormData)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_DidEndTextFieldEditing,
                      autofill_manager_.get(),
                      AutofillManager::OnDidEndTextFieldEditing)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_HidePopup,
                      autofill_manager_.get(),
                      AutofillManager::OnHidePopup)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_SetDataList,
                      autofill_manager_.get(),
                      AutofillManager::OnSetDataList)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_RequestAutocomplete,
                      &request_autocomplete_manager_,
                      RequestAutocompleteManager::OnRequestAutocomplete)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ContentAutofillDriver::DidNavigateFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_navigation_to_different_page())
    autofill_manager_->Reset();
}

void ContentAutofillDriver::SetAutofillManager(
    scoped_ptr<AutofillManager> manager) {
  autofill_manager_ = manager.Pass();
  autofill_manager_->SetExternalDelegate(&autofill_external_delegate_);
}

}  // namespace autofill
