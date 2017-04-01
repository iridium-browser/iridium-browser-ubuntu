// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/mash/mash_runner.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "mash/package/mash_packaged_service.h"
#include "mash/session/public/interfaces/constants.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_CHROMEOS)
#include "base/debug/leak_annotations.h"
#include "chrome/app/mash/mash_crash_reporter_client.h"
#include "components/crash/content/app/breakpad_linux.h"
#endif

using service_manager::mojom::ServiceFactory;

namespace {

// kProcessType used to identify child processes.
const char* kMashChild = "mash-child";

const char kChromeMashServiceName[] = "chrome_mash";

const char kChromeContentBrowserPackageName[] = "chrome_content_browser";
const char kChromeContentGpuPackageName[] = "chrome_content_gpu";
const char kChromeContentRendererPackageName[] = "chrome_content_renderer";
const char kChromeContentUtilityPackageName[] = "chrome_content_utility";

const char kPackagesPath[] = "Packages";
const char kManifestFilename[] = "manifest.json";

base::FilePath GetPackageManifestPath(const std::string& package_name) {
  base::FilePath exe = base::CommandLine::ForCurrentProcess()->GetProgram();
  return exe.DirName().AppendASCII(kPackagesPath).AppendASCII(package_name)
      .AppendASCII(kManifestFilename);
}

bool IsChild() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kProcessType) &&
         base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kProcessType) == kMashChild;
}

void InitializeResources() {
  ui::RegisterPathProvider();
  const std::string locale =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kLang);
  // This loads the Chrome's resources (chrome_100_percent.pak etc.)
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
}

class ServiceProcessLauncherDelegateImpl
    : public service_manager::ServiceProcessLauncher::Delegate {
 public:
  ServiceProcessLauncherDelegateImpl() {}
  ~ServiceProcessLauncherDelegateImpl() override {}

 private:
  // service_manager::ServiceProcessLauncher::Delegate:
  void AdjustCommandLineArgumentsForTarget(
        const service_manager::Identity& target,
        base::CommandLine* command_line) override {
    if (target.name() == kChromeMashServiceName ||
        target.name() == content::mojom::kBrowserServiceName) {
      base::FilePath exe_path;
      base::PathService::Get(base::FILE_EXE, &exe_path);
      command_line->SetProgram(exe_path);
    }
    if (target.name() != content::mojom::kBrowserServiceName) {
      // If running anything other than the browser process, launch a mash
      // child process. The new process will execute MashRunner::RunChild().
      command_line->AppendSwitchASCII(switches::kProcessType, kMashChild);
#if defined(OS_WIN)
      command_line->AppendArg(switches::kPrefetchArgumentOther);
#endif
      return;
    }

    // When launching the browser process, ensure that we don't inherit the
    // --mash flag so it proceeds with the normal content/browser startup path.
    // Eliminate all copies in case the developer passed more than one.
    base::CommandLine::StringVector new_argv;
    for (const base::CommandLine::StringType& arg : command_line->argv()) {
      if (arg != FILE_PATH_LITERAL("--mash"))
        new_argv.push_back(arg);
    }
    *command_line = base::CommandLine(new_argv);
  }

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

#if defined(OS_CHROMEOS)
// Initializes breakpad crash reporting. MashCrashReporterClient handles
// registering crash keys.
void InitializeCrashReporting() {
  DCHECK(!breakpad::IsCrashReporterEnabled());

  // Intentionally leaked. The crash client needs to outlive all other code.
  MashCrashReporterClient* client = new MashCrashReporterClient;
  ANNOTATE_LEAKING_OBJECT_PTR(client);
  crash_reporter::SetCrashReporterClient(client);

  // For now all standalone services act like the browser process and write
  // their own in-process crash dumps. When ash and the window server are
  // sandboxed we will need to hook up the crash signal file descriptor, make
  // the root process handle dumping, and pass a process type here.
  const std::string process_type_unused;
  breakpad::InitCrashReporter(process_type_unused);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

MashRunner::MashRunner() {}

MashRunner::~MashRunner() {}

int MashRunner::Run() {
  base::TaskScheduler::CreateAndSetSimpleTaskScheduler(
      base::SysInfo::NumberOfProcessors());

  if (IsChild())
    return RunChild();
  RunMain();
  return 0;
}

void MashRunner::RunMain() {
  base::SequencedWorkerPool::EnableWithRedirectionToTaskSchedulerForProcess();

  // TODO(sky): refactor BackgroundServiceManager so can supply own context, we
  // shouldn't we using context as it has a lot of stuff we don't really want
  // in chrome.
  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate;
  service_manager::BackgroundServiceManager background_service_manager;
  std::unique_ptr<service_manager::BackgroundServiceManager::InitParams>
      init_params(new service_manager::BackgroundServiceManager::InitParams);
  init_params->service_process_launcher_delegate =
      &service_process_launcher_delegate;
  background_service_manager.Init(std::move(init_params));
  context_.reset(new service_manager::ServiceContext(
      base::MakeUnique<mash::MashPackagedService>(),
      background_service_manager.CreateServiceRequest(kChromeMashServiceName)));

  // We need to send a sync messages to the Catalog, so we wait for a completed
  // connection first.
  std::unique_ptr<service_manager::Connection> catalog_connection =
      context_->connector()->Connect(catalog::mojom::kServiceName);
  {
    base::RunLoop run_loop;
    catalog_connection->AddConnectionCompletedClosure(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Synchronously override manifests needed for content process services.
  catalog::mojom::CatalogControlPtr catalog_control;
  catalog_connection->GetInterface(&catalog_control);
  CHECK(catalog_control->OverrideManifestPath(
      content::mojom::kBrowserServiceName,
      GetPackageManifestPath(kChromeContentBrowserPackageName)));
  CHECK(catalog_control->OverrideManifestPath(
      content::mojom::kGpuServiceName,
      GetPackageManifestPath(kChromeContentGpuPackageName)));
  CHECK(catalog_control->OverrideManifestPath(
      content::mojom::kRendererServiceName,
      GetPackageManifestPath(kChromeContentRendererPackageName)));
  CHECK(catalog_control->OverrideManifestPath(
      content::mojom::kUtilityServiceName,
      GetPackageManifestPath(kChromeContentUtilityPackageName)));

  // Ping mash_session to ensure an instance is brought up
  context_->connector()->Connect(mash::session::mojom::kServiceName);
  base::RunLoop().Run();

  base::TaskScheduler::GetInstance()->Shutdown();
}

int MashRunner::RunChild() {
  service_manager::WaitForDebuggerIfNecessary();

  base::i18n::InitializeICU();
  InitializeResources();
  service_manager::RunStandaloneService(
      base::Bind(&MashRunner::StartChildApp, base::Unretained(this)));
  return 0;
}

void MashRunner::StartChildApp(
    service_manager::mojom::ServiceRequest service_request) {
  // TODO(sad): Normally, this would be a TYPE_DEFAULT message loop. However,
  // TYPE_UI is needed for mojo:ui. But it is not known whether the child app is
  // going to be mojo:ui at this point. So always create a TYPE_UI message loop
  // for now.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  context_.reset(new service_manager::ServiceContext(
      base::MakeUnique<mash::MashPackagedService>(),
      std::move(service_request)));
  base::RunLoop().Run();
}

int MashMain() {
#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
  base::RouteStdioToConsole(false);
#endif
  // TODO(sky): wire this up correctly.
  service_manager::InitializeLogging();

#if defined(OS_LINUX)
  base::AtExitManager exit_manager;
#endif

#if !defined(OFFICIAL_BUILD)
  // Initialize stack dumping before initializing sandbox to make sure symbol
  // names in all loaded libraries will be cached.
  base::debug::EnableInProcessStackDumping();
#endif

#if defined(OS_CHROMEOS)
  // Breakpad installs signal handlers, so crash reporting must be set up after
  // EnableInProcessStackDumping() resets the signal handlers.
  InitializeCrashReporting();
#endif

  std::unique_ptr<base::MessageLoop> message_loop;
  if (!IsChild())
    message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTraceToConsole)) {
    base::trace_event::TraceConfig trace_config =
        tracing::GetConfigForTraceToConsole();
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        trace_config,
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  MashRunner mash_runner;
  return mash_runner.Run();
}

void WaitForMashDebuggerIfNecessary() {
  if (!service_manager::ServiceManagerIsRemote())
    return;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const std::string service_name =
      command_line->GetSwitchValueASCII(switches::kProcessServiceName);
  if (service_name !=
      command_line->GetSwitchValueASCII(switches::kWaitForDebugger)) {
    return;
  }

  // Include the pid as logging may not have been initialized yet (the pid
  // printed out by logging is wrong).
  LOG(WARNING) << "waiting for debugger to attach for service " << service_name
               << " pid=" << base::Process::Current().Pid();
  base::debug::WaitForDebugger(120, true);
}
