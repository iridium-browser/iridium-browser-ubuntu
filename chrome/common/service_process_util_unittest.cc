// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if !defined(OS_MACOSX)
#include "base/at_exit.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_POSIX)
#include "chrome/common/auto_start_linux.h"
#endif

#if defined(USE_AURA)
// This test fails http://crbug.com/84854, and is very flaky on CrOS and
// somewhat flaky on other Linux.
#define MAYBE_ForceShutdown DISABLED_ForceShutdown
#else
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_ForceShutdown DISABLED_ForceShutdown
#else
#define MAYBE_ForceShutdown ForceShutdown
#endif
#endif

namespace {

bool g_good_shutdown = false;

void ShutdownTask(base::RunLoop* loop) {
  // Quit the main message loop.
  ASSERT_FALSE(g_good_shutdown);
  g_good_shutdown = true;
  loop->QuitWhenIdle();
}

}  // namespace

TEST(ServiceProcessUtilTest, ScopedVersionedName) {
  std::string test_str = "test";
  std::string scoped_name = GetServiceProcessScopedVersionedName(test_str);
  EXPECT_TRUE(base::EndsWith(scoped_name, test_str,
                             base::CompareCase::SENSITIVE));
  EXPECT_NE(std::string::npos,
            scoped_name.find(version_info::GetVersionNumber()));
}

class ServiceProcessStateTest : public base::MultiProcessTest {
 public:
  ServiceProcessStateTest();
  ~ServiceProcessStateTest() override;
  void SetUp() override;
  base::SingleThreadTaskRunner* IOTaskRunner() {
    return io_thread_.task_runner().get();
  }
  void LaunchAndWait(const std::string& name);

 private:
  // This is used to release the ServiceProcessState singleton after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::Thread io_thread_;
};

ServiceProcessStateTest::ServiceProcessStateTest()
    : io_thread_("ServiceProcessStateTestThread") {
}

ServiceProcessStateTest::~ServiceProcessStateTest() {
}

void ServiceProcessStateTest::SetUp() {
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread_.StartWithOptions(options));
}

void ServiceProcessStateTest::LaunchAndWait(const std::string& name) {
  base::Process process = SpawnChild(name);
  ASSERT_TRUE(process.IsValid());
  int exit_code = 0;
  ASSERT_TRUE(process.WaitForExit(&exit_code));
  ASSERT_EQ(exit_code, 0);
}

TEST_F(ServiceProcessStateTest, Singleton) {
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  LaunchAndWait("ServiceProcessStateTestSingleton");
}

// http://crbug.com/396390
TEST_F(ServiceProcessStateTest, DISABLED_ReadyState) {
  ASSERT_FALSE(CheckServiceProcessReady());
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  ASSERT_TRUE(state.SignalReady(IOTaskRunner(), base::Closure()));
  LaunchAndWait("ServiceProcessStateTestReadyTrue");
  state.SignalStopped();
  LaunchAndWait("ServiceProcessStateTestReadyFalse");
}

TEST_F(ServiceProcessStateTest, AutoRun) {
  ServiceProcessState state;
  ASSERT_TRUE(state.AddToAutoRun());
  std::unique_ptr<base::CommandLine> autorun_command_line;
#if defined(OS_WIN)
  std::string value_name = GetServiceProcessScopedName("_service_run");
  base::string16 value;
  EXPECT_TRUE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                base::UTF8ToWide(value_name),
                                                &value));
  autorun_command_line.reset(
      new base::CommandLine(base::CommandLine::FromString(value)));
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
  std::string base_desktop_name = "google-chrome-service.desktop";
#else  // CHROMIUM_BUILD
  std::string base_desktop_name = "chromium-service.desktop";
#endif
  std::string exec_value;
  EXPECT_TRUE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));

  // Make sure |exec_value| doesn't contain strings a shell would
  // treat specially.
  ASSERT_EQ(std::string::npos, exec_value.find('#'));
  ASSERT_EQ(std::string::npos, exec_value.find('\n'));
  ASSERT_EQ(std::string::npos, exec_value.find('"'));
  ASSERT_EQ(std::string::npos, exec_value.find('\''));

  base::CommandLine::StringVector argv = base::SplitString(
      exec_value, base::CommandLine::StringType(1, ' '),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_GE(argv.size(), 2U)
      << "Expected at least one command-line option in: " << exec_value;
  autorun_command_line.reset(new base::CommandLine(argv));
#endif  // defined(OS_WIN)
  if (autorun_command_line.get()) {
    EXPECT_EQ(autorun_command_line->GetSwitchValueASCII(switches::kProcessType),
              std::string(switches::kServiceProcess));
  }
  ASSERT_TRUE(state.RemoveFromAutoRun());
#if defined(OS_WIN)
  EXPECT_FALSE(base::win::ReadCommandFromAutoRun(HKEY_CURRENT_USER,
                                                 base::UTF8ToWide(value_name),
                                                 &value));
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  EXPECT_FALSE(AutoStart::GetAutostartFileValue(
      GetServiceProcessScopedName(base_desktop_name), "Exec", &exec_value));
#endif  // defined(OS_WIN)
}

// http://crbug.com/396390
TEST_F(ServiceProcessStateTest, DISABLED_SharedMem) {
  std::string version;
  base::ProcessId pid;
#if defined(OS_WIN)
  // On Posix, named shared memory uses a file on disk. This file
  // could be lying around from previous crashes which could cause
  // GetServiceProcessPid to lie. On Windows, we use a named event so we
  // don't have this issue. Until we have a more stable shared memory
  // implementation on Posix, this check will only execute on Windows.
  ASSERT_FALSE(GetServiceProcessData(&version, &pid));
#endif  // defined(OS_WIN)
  ServiceProcessState state;
  ASSERT_TRUE(state.Initialize());
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_EQ(base::GetCurrentProcId(), pid);
}

TEST_F(ServiceProcessStateTest, MAYBE_ForceShutdown) {
  base::Process process = SpawnChild("ServiceProcessStateTestShutdown");
  ASSERT_TRUE(process.IsValid());
  for (int i = 0; !CheckServiceProcessReady() && i < 10; ++i) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
  ASSERT_TRUE(CheckServiceProcessReady());
  std::string version;
  base::ProcessId pid;
  ASSERT_TRUE(GetServiceProcessData(&version, &pid));
  ASSERT_TRUE(ForceServiceProcessShutdown(version, pid));
  int exit_code = 0;
  ASSERT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  ASSERT_EQ(exit_code, 0);
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestSingleton) {
  ServiceProcessState state;
  EXPECT_FALSE(state.Initialize());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyTrue) {
  EXPECT_TRUE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestReadyFalse) {
  EXPECT_FALSE(CheckServiceProcessReady());
  return 0;
}

MULTIPROCESS_TEST_MAIN(ServiceProcessStateTestShutdown) {
  base::PlatformThread::SetName("ServiceProcessStateTestShutdownMainThread");
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  base::Thread io_thread_("ServiceProcessStateTestShutdownIOThread");
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  EXPECT_TRUE(io_thread_.StartWithOptions(options));
  ServiceProcessState state;
  EXPECT_TRUE(state.Initialize());
  EXPECT_TRUE(state.SignalReady(io_thread_.task_runner().get(),
                                base::Bind(&ShutdownTask, &run_loop)));
  message_loop.task_runner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(),
      TestTimeouts::action_max_timeout());
  EXPECT_FALSE(g_good_shutdown);
  run_loop.Run();
  EXPECT_TRUE(g_good_shutdown);
  return 0;
}

#else  // !OS_MACOSX

#include <CoreFoundation/CoreFoundation.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/mac/mac_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/mac/mock_launchd.h"
#include "testing/gtest/include/gtest/gtest.h"

class ServiceProcessStateFileManipulationTest : public ::testing::Test {
 protected:
  ServiceProcessStateFileManipulationTest()
      : io_thread_("ServiceProcessStateFileManipulationTest_IO") {
  }

  void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    ASSERT_TRUE(io_thread_.StartWithOptions(options));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(MockLaunchd::MakeABundle(GetTempDirPath(),
                                         "Test",
                                         &bundle_path_,
                                         &executable_path_));
    mock_launchd_.reset(new MockLaunchd(executable_path_, &loop_,
                                        false, false));
    scoped_launchd_instance_.reset(
        new Launchd::ScopedInstance(mock_launchd_.get()));
    ASSERT_TRUE(service_process_state_.Initialize());
    ASSERT_TRUE(service_process_state_.SignalReady(
        io_thread_.task_runner().get(), base::Closure()));
    loop_.task_runner()->PostDelayedTask(FROM_HERE,
                                         run_loop_.QuitWhenIdleClosure(),
                                         TestTimeouts::action_max_timeout());
  }

  const MockLaunchd* mock_launchd() const { return mock_launchd_.get(); }
  const base::FilePath& executable_path() const { return executable_path_; }
  const base::FilePath& bundle_path() const { return bundle_path_; }
  const base::FilePath& GetTempDirPath() const { return temp_dir_.path(); }

  base::SingleThreadTaskRunner* GetIOTaskRunner() {
    return io_thread_.task_runner().get();
  }
  void Run() { run_loop_.Run(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::MessageLoopForUI loop_;
  base::RunLoop run_loop_;
  base::Thread io_thread_;
  base::FilePath executable_path_, bundle_path_;
  std::unique_ptr<MockLaunchd> mock_launchd_;
  std::unique_ptr<Launchd::ScopedInstance> scoped_launchd_instance_;
  ServiceProcessState service_process_state_;
};

void DeleteFunc(const base::FilePath& file) {
  EXPECT_TRUE(base::DeleteFile(file, true));
}

void MoveFunc(const base::FilePath& from, const base::FilePath& to) {
  EXPECT_TRUE(base::Move(from, to));
}

void ChangeAttr(const base::FilePath& from, int mode) {
  EXPECT_EQ(chmod(from.value().c_str(), mode), 0);
}

class ScopedAttributesRestorer {
 public:
  ScopedAttributesRestorer(const base::FilePath& path, int mode)
      : path_(path), mode_(mode) {
  }
  ~ScopedAttributesRestorer() {
    ChangeAttr(path_, mode_);
  }
 private:
  base::FilePath path_;
  int mode_;
};

void TrashFunc(const base::FilePath& src) {
  FSRef path_ref;
  FSRef new_path_ref;
  EXPECT_TRUE(base::mac::FSRefFromPath(src.value(), &path_ref));
  OSStatus status = FSMoveObjectToTrashSync(&path_ref,
                                            &new_path_ref,
                                            kFSFileOperationDefaultOptions);
  EXPECT_EQ(status, noErr) << "FSMoveObjectToTrashSync " << status;
}

TEST_F(ServiceProcessStateFileManipulationTest, VerifyLaunchD) {
  // There have been problems where launchd has gotten into a bad state, usually
  // because something had deleted all the files in /tmp. launchd depends on
  // a Unix Domain Socket that it creates at /tmp/launchd*/sock.
  // The symptom of this problem is that the service process connect fails
  // on Mac and "launch_msg(): Socket is not connected" appears.
  // This test is designed to make sure that launchd is working.
  // http://crbug/75518
  // Note: This particular problem no longer affects launchd in 10.10+, since
  // there is no user owned launchd process and sockets are no longer made at
  // /tmp/launchd*/sock. This test is still useful as a sanity check to make
  // sure that launchd appears to be working.

  base::CommandLine cl(base::FilePath("/bin/launchctl"));
  cl.AppendArg("limit");

  std::string output;
  int exit_code = -1;
  ASSERT_TRUE(base::GetAppOutputWithExitCode(cl, &output, &exit_code)
              && exit_code == 0)
      << " exit_code:" << exit_code << " " << output;
}

TEST_F(ServiceProcessStateFileManipulationTest, DeleteFile) {
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::Bind(&DeleteFunc, executable_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, DeleteBundle) {
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::Bind(&DeleteFunc, bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveBundle) {
  base::FilePath new_loc = GetTempDirPath().AppendASCII("MoveBundle");
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::Bind(&MoveFunc, bundle_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->restart_called());
  ASSERT_TRUE(mock_launchd()->write_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, MoveFile) {
  base::FilePath new_loc = GetTempDirPath().AppendASCII("MoveFile");
  GetIOTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&MoveFunc, executable_path(), new_loc));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

TEST_F(ServiceProcessStateFileManipulationTest, TrashBundle) {
  FSRef bundle_ref;
  ASSERT_TRUE(base::mac::FSRefFromPath(bundle_path().value(), &bundle_ref));
  GetIOTaskRunner()->PostTask(FROM_HERE, base::Bind(&TrashFunc, bundle_path()));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
  std::string path(base::mac::PathFromFSRef(bundle_ref));
  base::FilePath file_path(path);
  ASSERT_TRUE(base::DeleteFile(file_path, true));
}

TEST_F(ServiceProcessStateFileManipulationTest, ChangeAttr) {
  ScopedAttributesRestorer restorer(bundle_path(), 0777);
  GetIOTaskRunner()->PostTask(FROM_HERE,
                              base::Bind(&ChangeAttr, bundle_path(), 0222));
  Run();
  ASSERT_TRUE(mock_launchd()->remove_called());
  ASSERT_TRUE(mock_launchd()->delete_called());
}

#endif  // !OS_MACOSX
