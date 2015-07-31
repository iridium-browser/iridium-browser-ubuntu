// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_output_protection_message_filter.h"

#include "build/build_config.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_output_protection_private.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ui/aura/window.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/gfx/screen.h"
#endif

namespace chrome {

namespace {

#if defined(OS_CHROMEOS)
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE) ==
                   static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_NONE),
              "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NONE value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN) ==
        static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_UNKNOWN),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_UNKNOWN value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL) ==
        static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_INTERNAL),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_INTERNAL value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA) ==
                   static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_VGA),
               "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_VGA value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI) ==
                   static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_HDMI),
               "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_HDMI value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI) ==
                   static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_DVI),
               "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DVI value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT) ==
        static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_DISPLAYPORT),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_DISPLAYPORT value mismatch");
static_assert(
    static_cast<int>(PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK) ==
        static_cast<int>(ui::DISPLAY_CONNECTION_TYPE_NETWORK),
    "PP_OUTPUT_PROTECTION_LINK_TYPE_PRIVATE_NETWORK value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE) ==
                   static_cast<int>(ui::CONTENT_PROTECTION_METHOD_NONE),
               "PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE value mismatch");
static_assert(static_cast<int>(PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP) ==
                   static_cast<int>(ui::CONTENT_PROTECTION_METHOD_HDCP),
               "PP_OUTPUT_PROTECTION_METHOD_PRIVATE_HDCP value mismatch");

bool GetCurrentDisplayId(content::RenderFrameHost* rfh, int64* display_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  gfx::NativeView native_view = rfh->GetNativeView();
  gfx::Screen* screen = gfx::Screen::GetScreenFor(native_view);
  if (!screen)
    return false;
  gfx::Display display = screen->GetDisplayNearestWindow(native_view);
  *display_id = display.id();
  return true;
}

void DoNothing(bool status) {
}

#endif

}  // namespace

#if defined(OS_CHROMEOS)
// Output protection delegate. All methods except constructor should be
// invoked in UI thread.
class PepperOutputProtectionMessageFilter::Delegate
    : public aura::WindowObserver {
 public:
  typedef base::Callback<void(int32_t /* result */,
                              uint32_t /* link_mask */,
                              uint32_t /* protection_mask*/)>
      QueryStatusCallback;
  typedef base::Callback<void(int32_t /* result */)> EnableProtectionCallback;

  Delegate(int render_process_id, int render_frame_id);
  ~Delegate() override;

  // aura::WindowObserver overrides.
  void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) override;
  void OnWindowDestroying(aura::Window* window) override;

  void QueryStatus(const QueryStatusCallback& callback);
  void EnableProtection(uint32_t desired_method_mask,
                        const EnableProtectionCallback& callback);

 private:
  ui::DisplayConfigurator::ContentProtectionClientId GetClientId();

  void QueryStatusComplete(
      const QueryStatusCallback& callback,
      const ui::DisplayConfigurator::QueryProtectionResponse& response);
  void EnableProtectionComplete(const EnableProtectionCallback& callback,
                                bool success);

  // Used to lookup the WebContents associated with this PP_Instance.
  int render_process_id_;
  int render_frame_id_;

  // Native window being observed.
  aura::Window* window_;

  ui::DisplayConfigurator::ContentProtectionClientId client_id_;

  // The display id which the renderer currently uses.
  int64 display_id_;

  // The last desired method mask. Will enable this mask on new display if
  // renderer changes display.
  uint32_t desired_method_mask_;

  base::WeakPtrFactory<PepperOutputProtectionMessageFilter::Delegate>
      weak_ptr_factory_;
};

PepperOutputProtectionMessageFilter::Delegate::Delegate(int render_process_id,
                                                        int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      window_(NULL),
      client_id_(ui::DisplayConfigurator::kInvalidClientId),
      display_id_(0),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

PepperOutputProtectionMessageFilter::Delegate::~Delegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->UnregisterContentProtectionClient(client_id_);

  if (window_)
    window_->RemoveObserver(this);
}

ui::DisplayConfigurator::ContentProtectionClientId
PepperOutputProtectionMessageFilter::Delegate::GetClientId() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (client_id_ == ui::DisplayConfigurator::kInvalidClientId) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!GetCurrentDisplayId(rfh, &display_id_))
      return ui::DisplayConfigurator::kInvalidClientId;

    window_ = rfh->GetNativeView();
    if (!window_)
      return ui::DisplayConfigurator::kInvalidClientId;

    ui::DisplayConfigurator* configurator =
        ash::Shell::GetInstance()->display_configurator();
    client_id_ = configurator->RegisterContentProtectionClient();

    if (client_id_ != ui::DisplayConfigurator::kInvalidClientId)
      window_->AddObserver(this);
  }
  return client_id_;
}

void PepperOutputProtectionMessageFilter::Delegate::QueryStatus(
    const QueryStatusCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    callback.Run(PP_ERROR_FAILED, 0, 0);
    return;
  }

  ui::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->QueryContentProtectionStatus(
      GetClientId(), display_id_,
      base::Bind(
          &PepperOutputProtectionMessageFilter::Delegate::QueryStatusComplete,
          weak_ptr_factory_.GetWeakPtr(), callback));
}

void PepperOutputProtectionMessageFilter::Delegate::EnableProtection(
    uint32_t desired_method_mask,
    const EnableProtectionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ui::DisplayConfigurator* configurator =
      ash::Shell::GetInstance()->display_configurator();
  configurator->EnableContentProtection(
      GetClientId(), display_id_, desired_method_mask,
      base::Bind(&PepperOutputProtectionMessageFilter::Delegate::
                     EnableProtectionComplete,
                 weak_ptr_factory_.GetWeakPtr(), callback));
  desired_method_mask_ = desired_method_mask;
}

void PepperOutputProtectionMessageFilter::Delegate::QueryStatusComplete(
    const QueryStatusCallback& callback,
    const ui::DisplayConfigurator::QueryProtectionResponse& response) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    callback.Run(PP_ERROR_FAILED, 0, 0);
    return;
  }

  uint32_t link_mask = response.link_mask;
  // If we successfully retrieved the device level status, check for capturers.
  if (response.success) {
    const bool capture_detected =
        // Check for tab capture on the current tab.
        content::WebContents::FromRenderFrameHost(rfh)->GetCapturerCount() >
            0 ||
        // Check for desktop capture.
        MediaCaptureDevicesDispatcher::GetInstance()
            ->IsDesktopCaptureInProgress();
    if (capture_detected)
      link_mask |= ui::DISPLAY_CONNECTION_TYPE_NETWORK;
  }

  callback.Run(response.success ? PP_OK : PP_ERROR_FAILED, link_mask,
               response.protection_mask);
}

void PepperOutputProtectionMessageFilter::Delegate::EnableProtectionComplete(
    const EnableProtectionCallback& callback,
    bool result) {
  callback.Run(result ? PP_OK : PP_ERROR_FAILED);
}

void PepperOutputProtectionMessageFilter::Delegate::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!rfh) {
    LOG(WARNING) << "RenderFrameHost is not alive.";
    return;
  }

  int64 new_display_id = 0;
  if (!GetCurrentDisplayId(rfh, &new_display_id))
    return;
  if (display_id_ == new_display_id)
    return;

  if (desired_method_mask_ != ui::CONTENT_PROTECTION_METHOD_NONE) {
    // Display changed and should enable output protections on new display.
    ui::DisplayConfigurator* configurator =
        ash::Shell::GetInstance()->display_configurator();
    configurator->EnableContentProtection(GetClientId(), new_display_id,
                                          desired_method_mask_,
                                          base::Bind(&DoNothing));
    configurator->EnableContentProtection(GetClientId(), display_id_,
                                          ui::CONTENT_PROTECTION_METHOD_NONE,
                                          base::Bind(&DoNothing));
  }
  display_id_ = new_display_id;
}

void PepperOutputProtectionMessageFilter::Delegate::OnWindowDestroying(
    aura::Window* window) {
  DCHECK_EQ(window, window_);
  window_->RemoveObserver(this);
  window_ = NULL;
}
#endif  // defined(OS_CHROMEOS)

PepperOutputProtectionMessageFilter::PepperOutputProtectionMessageFilter(
    content::BrowserPpapiHost* host,
    PP_Instance instance)
    : weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  int render_process_id = 0;
  int render_frame_id = 0;
  host->GetRenderFrameIDsForInstance(
      instance, &render_process_id, &render_frame_id);
  delegate_ = new Delegate(render_process_id, render_frame_id);
#else
  NOTIMPLEMENTED();
#endif
}

PepperOutputProtectionMessageFilter::~PepperOutputProtectionMessageFilter() {
#if defined(OS_CHROMEOS)
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, delegate_);
  delegate_ = NULL;
#endif
}

scoped_refptr<base::TaskRunner>
PepperOutputProtectionMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperOutputProtectionMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperOutputProtectionMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_OutputProtection_QueryStatus, OnQueryStatus);
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(
        PpapiHostMsg_OutputProtection_EnableProtection, OnEnableProtection);
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

int32_t PepperOutputProtectionMessageFilter::OnQueryStatus(
    ppapi::host::HostMessageContext* context) {
#if defined(OS_CHROMEOS)
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  delegate_->QueryStatus(
      base::Bind(&PepperOutputProtectionMessageFilter::OnQueryStatusComplete,
                 weak_ptr_factory_.GetWeakPtr(), reply_context));
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

int32_t PepperOutputProtectionMessageFilter::OnEnableProtection(
    ppapi::host::HostMessageContext* context,
    uint32_t desired_method_mask) {
#if defined(OS_CHROMEOS)
  ppapi::host::ReplyMessageContext reply_context =
      context->MakeReplyMessageContext();
  delegate_->EnableProtection(
      desired_method_mask,
      base::Bind(
          &PepperOutputProtectionMessageFilter::OnEnableProtectionComplete,
          weak_ptr_factory_.GetWeakPtr(), reply_context));
  return PP_OK_COMPLETIONPENDING;
#else
  NOTIMPLEMENTED();
  return PP_ERROR_NOTSUPPORTED;
#endif
}

void PepperOutputProtectionMessageFilter::OnQueryStatusComplete(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t result,
    uint32_t link_mask,
    uint32_t protection_mask) {
  reply_context.params.set_result(result);
  SendReply(reply_context, PpapiPluginMsg_OutputProtection_QueryStatusReply(
                               link_mask, protection_mask));
}

void PepperOutputProtectionMessageFilter::OnEnableProtectionComplete(
    ppapi::host::ReplyMessageContext reply_context,
    int32_t result) {
  reply_context.params.set_result(result);
  SendReply(reply_context,
            PpapiPluginMsg_OutputProtection_EnableProtectionReply());
}

}  // namespace chrome
