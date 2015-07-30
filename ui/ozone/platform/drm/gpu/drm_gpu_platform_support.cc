// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

void MessageProcessedOnMain(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    const base::Closure& io_thread_task) {
  io_thread_task_runner->PostTask(FROM_HERE, io_thread_task);
}

class DrmGpuPlatformSupportMessageFilter : public IPC::MessageFilter {
 public:
  typedef base::Callback<void(
      const scoped_refptr<base::SingleThreadTaskRunner>&)>
      OnFilterAddedCallback;

  DrmGpuPlatformSupportMessageFilter(
      ScreenManager* screen_manager,
      const OnFilterAddedCallback& on_filter_added_callback,
      IPC::Listener* main_thread_listener)
      : screen_manager_(screen_manager),
        on_filter_added_callback_(on_filter_added_callback),
        main_thread_listener_(main_thread_listener),
        main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        pending_main_thread_operations_(0),
        cursor_animating_(false) {}

  void OnFilterAdded(IPC::Sender* sender) override {
    io_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(on_filter_added_callback_, io_thread_task_runner_));
  }

  // This code is meant to be very temporary and only as a special case to fix
  // cursor movement jank resulting from slowdowns on the gpu main thread.
  // It handles cursor movement on IO thread when display config is stable
  // and returns it to main thread during transitions.
  bool OnMessageReceived(const IPC::Message& message) override {
    // If this message affects the state needed to set cursor, handle it on
    // the main thread. If a cursor move message arrives but we haven't
    // processed the previous main thread message, keep processing on main
    // until nothing is pending.
    bool cursor_position_message = MessageAffectsCursorPosition(message.type());
    bool cursor_state_message = MessageAffectsCursorState(message.type());

    // Only handle cursor related messages here.
    if (!cursor_position_message && !cursor_state_message)
      return false;

    bool cursor_was_animating = cursor_animating_;
    UpdateAnimationState(message);
    if (cursor_state_message || pending_main_thread_operations_ ||
        cursor_animating_ || cursor_was_animating) {
      pending_main_thread_operations_++;

      base::Closure main_thread_message_handler =
          base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                     base::Unretained(main_thread_listener_), message);
      main_thread_task_runner_->PostTask(FROM_HERE,
                                         main_thread_message_handler);

      // This is an echo from the main thread to decrement pending ops.
      // When the main thread is done with the task, it posts back to IO to
      // signal completion.
      base::Closure io_thread_task = base::Bind(
          &DrmGpuPlatformSupportMessageFilter::DecrementPendingOperationsOnIO,
          this);

      base::Closure message_processed_callback = base::Bind(
          &MessageProcessedOnMain, io_thread_task_runner_, io_thread_task);
      main_thread_task_runner_->PostTask(FROM_HERE, message_processed_callback);

      return true;
    }

    // Otherwise, we are in a steady state and it's safe to move cursor on IO.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportMessageFilter, message)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
    IPC_MESSAGE_UNHANDLED(handled = false);
    IPC_END_MESSAGE_MAP()

    return handled;
  }

 protected:
  ~DrmGpuPlatformSupportMessageFilter() override {}

  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location) {
    screen_manager_->GetWindow(widget)->MoveCursor(location);
  }

  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const std::vector<SkBitmap>& bitmaps,
                   const gfx::Point& location,
                   int frame_delay_ms) {
    screen_manager_->GetWindow(widget)
        ->SetCursorWithoutAnimations(bitmaps, location);
  }

  void DecrementPendingOperationsOnIO() { pending_main_thread_operations_--; }

  bool MessageAffectsCursorState(uint32 message_type) {
    switch (message_type) {
      case OzoneGpuMsg_CreateWindowDelegate::ID:
      case OzoneGpuMsg_DestroyWindowDelegate::ID:
      case OzoneGpuMsg_WindowBoundsChanged::ID:
      case OzoneGpuMsg_ConfigureNativeDisplay::ID:
      case OzoneGpuMsg_DisableNativeDisplay::ID:
        return true;
      default:
        return false;
    }
  }

  bool MessageAffectsCursorPosition(uint32 message_type) {
    switch (message_type) {
      case OzoneGpuMsg_CursorMove::ID:
      case OzoneGpuMsg_CursorSet::ID:
        return true;
      default:
        return false;
    }
  }

  void UpdateAnimationState(const IPC::Message& message) {
    if (message.type() != OzoneGpuMsg_CursorSet::ID)
      return;

    OzoneGpuMsg_CursorSet::Param param;
    if (!OzoneGpuMsg_CursorSet::Read(&message, &param))
      return;

    int frame_delay_ms = get<3>(param);
    cursor_animating_ = frame_delay_ms != 0;
  }

  ScreenManager* screen_manager_;
  OnFilterAddedCallback on_filter_added_callback_;
  IPC::Listener* main_thread_listener_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  int32 pending_main_thread_operations_;
  bool cursor_animating_;
};
}

DrmGpuPlatformSupport::DrmGpuPlatformSupport(
    DrmDeviceManager* drm_device_manager,
    ScreenManager* screen_manager,
    scoped_ptr<DrmGpuDisplayManager> ndd)
    : sender_(NULL),
      drm_device_manager_(drm_device_manager),
      screen_manager_(screen_manager),
      ndd_(ndd.Pass()) {
  filter_ = new DrmGpuPlatformSupportMessageFilter(
      screen_manager, base::Bind(&DrmGpuPlatformSupport::SetIOTaskRunner,
                                 base::Unretained(this)),
      this);
}

DrmGpuPlatformSupport::~DrmGpuPlatformSupport() {
}

void DrmGpuPlatformSupport::AddHandler(scoped_ptr<GpuPlatformSupport> handler) {
  handlers_.push_back(handler.release());
}

void DrmGpuPlatformSupport::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;

  for (size_t i = 0; i < handlers_.size(); ++i)
    handlers_[i]->OnChannelEstablished(sender);
}

bool DrmGpuPlatformSupport::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupport, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CreateWindowDelegate, OnCreateWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DestroyWindowDelegate,
                      OnDestroyWindowDelegate)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_WindowBoundsChanged, OnWindowBoundsChanged)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RefreshNativeDisplays,
                      OnRefreshNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ConfigureNativeDisplay,
                      OnConfigureNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DisableNativeDisplay, OnDisableNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_TakeDisplayControl, OnTakeDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RelinquishDisplayControl,
                      OnRelinquishDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_AddGraphicsDevice, OnAddGraphicsDevice)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RemoveGraphicsDevice, OnRemoveGraphicsDevice)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_GetHDCPState, OnGetHDCPState)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetHDCPState, OnSetHDCPState)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetGammaRamp, OnSetGammaRamp);
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  if (!handled)
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i]->OnMessageReceived(message))
        return true;

  return false;
}

void DrmGpuPlatformSupport::OnCreateWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> delegate(
      new DrmWindow(widget, drm_device_manager_, screen_manager_));
  delegate->Initialize();
  screen_manager_->AddWindow(widget, delegate.Pass());
}

void DrmGpuPlatformSupport::OnDestroyWindowDelegate(
    gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> delegate = screen_manager_->RemoveWindow(widget);
  delegate->Shutdown();
}

void DrmGpuPlatformSupport::OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                                  const gfx::Rect& bounds) {
  screen_manager_->GetWindow(widget)->OnBoundsChanged(bounds);
}

void DrmGpuPlatformSupport::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location,
                                        int frame_delay_ms) {
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmGpuPlatformSupport::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmGpuPlatformSupport::OnRefreshNativeDisplays() {
  sender_->Send(new OzoneHostMsg_UpdateNativeDisplays(ndd_->GetDisplays()));
}

void DrmGpuPlatformSupport::OnConfigureNativeDisplay(
    int64_t id,
    const DisplayMode_Params& mode_param,
    const gfx::Point& origin) {
  sender_->Send(new OzoneHostMsg_DisplayConfigured(
      id, ndd_->ConfigureDisplay(id, mode_param, origin)));
}

void DrmGpuPlatformSupport::OnDisableNativeDisplay(int64_t id) {
  sender_->Send(
      new OzoneHostMsg_DisplayConfigured(id, ndd_->DisableDisplay(id)));
}

void DrmGpuPlatformSupport::OnTakeDisplayControl() {
  ndd_->TakeDisplayControl();
}

void DrmGpuPlatformSupport::OnRelinquishDisplayControl() {
  ndd_->RelinquishDisplayControl();
}

void DrmGpuPlatformSupport::OnAddGraphicsDevice(
    const base::FilePath& path,
    const base::FileDescriptor& fd) {
  drm_device_manager_->AddDrmDevice(path, fd);
}

void DrmGpuPlatformSupport::OnRemoveGraphicsDevice(const base::FilePath& path) {
  drm_device_manager_->RemoveDrmDevice(path);
}

void DrmGpuPlatformSupport::OnSetGammaRamp(
    int64_t id,
    const std::vector<GammaRampRGBEntry>& lut) {
  ndd_->SetGammaRamp(id, lut);
}

void DrmGpuPlatformSupport::RelinquishGpuResources(
    const base::Closure& callback) {
  callback.Run();
}

void DrmGpuPlatformSupport::OnGetHDCPState(int64_t display_id) {
  HDCPState state = HDCP_STATE_UNDESIRED;
  bool success = ndd_->GetHDCPState(display_id, &state);
  sender_->Send(new OzoneHostMsg_HDCPStateReceived(display_id, success, state));
}

void DrmGpuPlatformSupport::OnSetHDCPState(int64_t display_id,
                                           HDCPState state) {
  sender_->Send(new OzoneHostMsg_HDCPStateUpdated(
      display_id, ndd_->SetHDCPState(display_id, state)));
}

void DrmGpuPlatformSupport::SetIOTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  drm_device_manager_->InitializeIOTaskRunner(io_task_runner);
}

IPC::MessageFilter* DrmGpuPlatformSupport::GetMessageFilter() {
  return filter_.get();
}

}  // namespace ui
