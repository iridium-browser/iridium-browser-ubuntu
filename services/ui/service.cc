// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/service.h"

#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "services/catalog/public/cpp/resource_loader.h"
#include "services/shell/public/c/main.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/tracing/public/cpp/provider.h"
#include "services/ui/clipboard/clipboard_impl.h"
#include "services/ui/common/switches.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/ime/ime_registrar_impl.h"
#include "services/ui/ime/ime_server_impl.h"
#include "services/ui/ws/accessibility_manager.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/gpu_service_proxy.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_test_impl.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "services/ui/ws/window_tree_factory.h"
#include "services/ui/ws/window_tree_host_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/events/event_switches.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/platform_window/x11/x11_window.h"
#elif defined(USE_OZONE)
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

using shell::Connection;
using mojo::InterfaceRequest;
using ui::mojom::WindowServerTest;
using ui::mojom::WindowTreeHostFactory;

namespace ui {

namespace {

const char kResourceFileStrings[] = "mus_app_resources_strings.pak";
const char kResourceFile100[] = "mus_app_resources_100.pak";
const char kResourceFile200[] = "mus_app_resources_200.pak";

}  // namespace

// TODO(sky): this is a pretty typical pattern, make it easier to do.
struct Service::PendingRequest {
  shell::Identity remote_identity;
  std::unique_ptr<mojom::WindowTreeFactoryRequest> wtf_request;
  std::unique_ptr<mojom::DisplayManagerRequest> dm_request;
};

struct Service::UserState {
  std::unique_ptr<ws::AccessibilityManager> accessibility;
  std::unique_ptr<ws::WindowTreeHostFactory> window_tree_host_factory;
};

Service::Service()
    : test_config_(false),
      platform_screen_(display::PlatformScreen::Create()),
      ime_registrar_(&ime_server_),
      weak_ptr_factory_(this) {}

Service::~Service() {
  // Destroy |window_server_| first, since it depends on |event_source_|.
  // WindowServer (or more correctly its Displays) may have state that needs to
  // be destroyed before GpuState as well.
  window_server_.reset();
}

void Service::InitializeResources(shell::Connector* connector) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;

  std::set<std::string> resource_paths;
  resource_paths.insert(kResourceFileStrings);
  resource_paths.insert(kResourceFile100);
  resource_paths.insert(kResourceFile200);

  catalog::ResourceLoader loader;
  filesystem::mojom::DirectoryPtr directory;
  connector->ConnectToInterface("mojo:catalog", &directory);
  CHECK(loader.OpenFiles(std::move(directory), resource_paths));

  ui::RegisterPathProvider();

  // Initialize resource bundle with 1x and 2x cursor bitmaps.
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      loader.TakeFile(kResourceFileStrings),
      base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile100),
                         ui::SCALE_FACTOR_100P);
  rb.AddDataPackFromFile(loader.TakeFile(kResourceFile200),
                         ui::SCALE_FACTOR_200P);
}

Service::UserState* Service::GetUserState(
    const shell::Identity& remote_identity) {
  const ws::UserId& user_id = remote_identity.user_id();
  auto it = user_id_to_user_state_.find(user_id);
  if (it != user_id_to_user_state_.end())
    return it->second.get();
  user_id_to_user_state_[user_id] = base::WrapUnique(new UserState);
  return user_id_to_user_state_[user_id].get();
}

void Service::AddUserIfNecessary(const shell::Identity& remote_identity) {
  window_server_->user_id_tracker()->AddUserId(remote_identity.user_id());
}

void Service::OnStart(const shell::Identity& identity) {
  platform_display_init_params_.surfaces_state = new SurfacesState;

  base::PlatformThread::SetName("mus");
  tracing_.Initialize(connector(), identity.name());
  TRACE_EVENT0("mus", "Service::Initialize started");

  test_config_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseTestConfig);
#if defined(USE_X11)
  XInitThreads();
  if (test_config_)
    ui::test::SetUseOverrideRedirectWindowByDefault(true);
#endif

  InitializeResources(connector());

#if defined(USE_OZONE)
  // The ozone platform can provide its own event source. So initialize the
  // platform before creating the default event source.
  // Because GL libraries need to be initialized before entering the sandbox,
  // in MUS, |InitializeForUI| will load the GL libraries.
  ui::OzonePlatform::InitParams params;
  params.connector = connector();
  params.single_process = false;
  ui::OzonePlatform::InitializeForUI(params);

  // TODO(kylechar): We might not always want a US keyboard layout.
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");
  client_native_pixmap_factory_ = ui::ClientNativePixmapFactory::Create();
  ui::ClientNativePixmapFactory::SetInstance(
      client_native_pixmap_factory_.get());

  DCHECK(ui::ClientNativePixmapFactory::GetInstance());
#endif

// TODO(rjkroege): Enter sandbox here before we start threads in GpuState
// http://crbug.com/584532

#if !defined(OS_ANDROID)
  event_source_ = ui::PlatformEventSource::CreateDefault();
#endif

  // This needs to happen after DeviceDataManager has been constructed. That
  // happens either during OzonePlatform or PlatformEventSource initialization,
  // so keep this line below both of those.
  input_device_server_.RegisterAsObserver();

  gpu_proxy_.reset(new GpuServiceProxy());

  // Gpu must be running before the PlatformScreen can be initialized.
  platform_screen_->Init();
  window_server_.reset(
      new ws::WindowServer(this, platform_display_init_params_.surfaces_state));

  // DeviceDataManager must be initialized before TouchController. On non-Linux
  // platforms there is no DeviceDataManager so don't create touch controller.
  if (ui::DeviceDataManager::HasInstance())
    touch_controller_.reset(
        new ws::TouchController(window_server_->display_manager()));
}

bool Service::OnConnect(const shell::Identity& remote_identity,
                        shell::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::AccessibilityManager>(this);
  registry->AddInterface<mojom::Clipboard>(this);
  registry->AddInterface<mojom::DisplayManager>(this);
  registry->AddInterface<mojom::GpuService>(this);
  registry->AddInterface<mojom::IMERegistrar>(this);
  registry->AddInterface<mojom::IMEServer>(this);
  registry->AddInterface<mojom::UserAccessManager>(this);
  registry->AddInterface<mojom::UserActivityMonitor>(this);
  registry->AddInterface<WindowTreeHostFactory>(this);
  registry->AddInterface<mojom::WindowManagerWindowTreeFactory>(this);
  registry->AddInterface<mojom::WindowTreeFactory>(this);
  if (test_config_)
    registry->AddInterface<WindowServerTest>(this);

  // On non-Linux platforms there will be no DeviceDataManager instance and no
  // purpose in adding the Mojo interface to connect to.
  if (input_device_server_.IsRegisteredAsObserver())
    input_device_server_.AddInterface(registry);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()->AddInterfaces(registry);
#endif

  return true;
}

void Service::OnFirstDisplayReady() {
  PendingRequests requests;
  requests.swap(pending_requests_);
  for (auto& request : requests) {
    if (request->wtf_request)
      Create(request->remote_identity, std::move(*request->wtf_request));
    else
      Create(request->remote_identity, std::move(*request->dm_request));
  }
}

void Service::OnNoMoreDisplays() {
  // We may get here from the destructor, in which case there is no messageloop.
  if (base::MessageLoop::current())
    base::MessageLoop::current()->QuitWhenIdle();
}

bool Service::IsTestConfig() const {
  return test_config_;
}

void Service::CreateDefaultDisplays() {
  // An asynchronous callback will create the Displays once the physical
  // displays are ready.
  platform_screen_->ConfigurePhysicalDisplay(base::Bind(
      &Service::OnCreatedPhysicalDisplay, weak_ptr_factory_.GetWeakPtr()));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::AccessibilityManagerRequest request) {
  UserState* user_state = GetUserState(remote_identity);
  if (!user_state->accessibility) {
    const ws::UserId& user_id = remote_identity.user_id();
    user_state->accessibility.reset(
        new ws::AccessibilityManager(window_server_.get(), user_id));
  }
  user_state->accessibility->Bind(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::ClipboardRequest request) {
  const ws::UserId& user_id = remote_identity.user_id();
  window_server_->GetClipboardForUser(user_id)->AddBinding(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::DisplayManagerRequest request) {
  // DisplayManagerObservers generally expect there to be at least one display.
  if (!window_server_->display_manager()->has_displays()) {
    std::unique_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->remote_identity = remote_identity;
    pending_request->dm_request.reset(
        new mojom::DisplayManagerRequest(std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  window_server_->display_manager()
      ->GetUserDisplayManager(remote_identity.user_id())
      ->AddDisplayManagerBinding(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::GpuServiceRequest request) {
  gpu_proxy_->Add(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::IMERegistrarRequest request) {
  ime_registrar_.AddBinding(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::IMEServerRequest request) {
  ime_server_.AddBinding(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::UserAccessManagerRequest request) {
  window_server_->user_id_tracker()->Bind(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::UserActivityMonitorRequest request) {
  AddUserIfNecessary(remote_identity);
  const ws::UserId& user_id = remote_identity.user_id();
  window_server_->GetUserActivityMonitorForUser(user_id)->Add(
      std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::WindowManagerWindowTreeFactoryRequest request) {
  AddUserIfNecessary(remote_identity);
  window_server_->window_manager_window_tree_factory_set()->Add(
    remote_identity.user_id(), std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::WindowTreeFactoryRequest request) {
  AddUserIfNecessary(remote_identity);
  if (!window_server_->display_manager()->has_displays()) {
    std::unique_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->remote_identity = remote_identity;
    pending_request->wtf_request.reset(
        new mojom::WindowTreeFactoryRequest(std::move(request)));
    pending_requests_.push_back(std::move(pending_request));
    return;
  }
  AddUserIfNecessary(remote_identity);
  new ws::WindowTreeFactory(window_server_.get(), remote_identity.user_id(),
                            remote_identity.name(), std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::WindowTreeHostFactoryRequest request) {
  UserState* user_state = GetUserState(remote_identity);
  if (!user_state->window_tree_host_factory) {
    user_state->window_tree_host_factory.reset(new ws::WindowTreeHostFactory(
        window_server_.get(), remote_identity.user_id(),
        platform_display_init_params_));
  }
  user_state->window_tree_host_factory->AddBinding(std::move(request));
}

void Service::Create(const shell::Identity& remote_identity,
                     mojom::WindowServerTestRequest request) {
  if (!test_config_)
    return;
  new ws::WindowServerTestImpl(window_server_.get(), std::move(request));
}

void Service::OnCreatedPhysicalDisplay(int64_t id, const gfx::Rect& bounds) {
  platform_display_init_params_.display_bounds = bounds;
  platform_display_init_params_.display_id = id;
  platform_display_init_params_.platform_screen = platform_screen_.get();

  // Display manages its own lifetime.
  ws::Display* host_impl =
      new ws::Display(window_server_.get(), platform_display_init_params_);
  host_impl->Init(nullptr);

  if (touch_controller_)
    touch_controller_->UpdateTouchTransforms();
}

}  // namespace ui
