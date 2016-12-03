// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_posix.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// See comment in |PreEarlyInitialization()|, where sigaction is called.
void SIGCHLDHandler(int signal) {
}

// The OSX fork() implementation can crash in the child process before
// fork() returns.  In that case, the shutdown pipe will still be
// shared with the parent process.  To prevent child crashes from
// causing parent shutdowns, |g_pipe_pid| is the pid for the process
// which registered |g_shutdown_pipe_write_fd|.
// See <http://crbug.com/175341>.
pid_t g_pipe_pid = -1;
int g_shutdown_pipe_write_fd = -1;
int g_shutdown_pipe_read_fd = -1;

// Common code between SIG{HUP, INT, TERM}Handler.
void GracefulShutdownHandler(int signal) {
  // Reinstall the default handler.  We had one shot at graceful shutdown.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  RAW_CHECK(sigaction(signal, &action, NULL) == 0);

  RAW_CHECK(g_pipe_pid != -1);
  RAW_CHECK(g_shutdown_pipe_write_fd != -1);
  RAW_CHECK(g_shutdown_pipe_read_fd != -1);
  RAW_CHECK(g_pipe_pid == getpid());
  size_t bytes_written = 0;
  do {
    int rv = HANDLE_EINTR(
        write(g_shutdown_pipe_write_fd,
              reinterpret_cast<const char*>(&signal) + bytes_written,
              sizeof(signal) - bytes_written));
    RAW_CHECK(rv >= 0);
    bytes_written += rv;
  } while (bytes_written < sizeof(signal));
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGHUPHandler(int signal) {
  RAW_CHECK(signal == SIGHUP);
  GracefulShutdownHandler(signal);
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGINTHandler(int signal) {
  RAW_CHECK(signal == SIGINT);
  GracefulShutdownHandler(signal);
}

// See comment in |PostMainMessageLoopStart()|, where sigaction is called.
void SIGTERMHandler(int signal) {
  RAW_CHECK(signal == SIGTERM);
  GracefulShutdownHandler(signal);
}

// ExitHandler takes care of servicing an exit (from a signal) at the
// appropriate time. Specifically if we get an exit and have not finished
// session restore we delay the exit. To do otherwise means we're exiting part
// way through startup which causes all sorts of problems.
class ExitHandler {
 public:
  // Invokes exit when appropriate.
  static void ExitWhenPossibleOnUIThread();

 private:
  ExitHandler();
  ~ExitHandler();

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  // Does the appropriate call to Exit.
  static void Exit();

  // Points to the on-session-restored callback that was registered with
  // SessionRestore's callback list. When objects of this class are destroyed,
  // the subscription object's destructor will automatically unregister the
  // callback in SessionRestore, so that the callback list does not contain any
  // obsolete callbacks.
  SessionRestore::CallbackSubscription
      on_session_restored_callback_subscription_;

  DISALLOW_COPY_AND_ASSIGN(ExitHandler);
};

// static
void ExitHandler::ExitWhenPossibleOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (SessionRestore::IsRestoringSynchronously()) {
    // ExitHandler takes care of deleting itself.
    new ExitHandler();
  } else {
    Exit();
  }
}

ExitHandler::ExitHandler() {
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(
          base::Bind(&ExitHandler::OnSessionRestoreDone,
                     base::Unretained(this)));
}

ExitHandler::~ExitHandler() {
}

void ExitHandler::OnSessionRestoreDone(int /* num_tabs */) {
  if (!SessionRestore::IsRestoringSynchronously()) {
    // At this point the message loop may not be running (meaning we haven't
    // gotten through browser startup, but are close). Post the task to at which
    // point the message loop is running.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&ExitHandler::Exit));
    delete this;
  }
}

// static
void ExitHandler::Exit() {
#if defined(OS_CHROMEOS)
  // On ChromeOS, exiting on signal should be always clean.
  chrome::ExitCleanly();
#else
  chrome::AttemptExit();
#endif
}

class ShutdownDetector : public base::PlatformThread::Delegate {
 public:
  explicit ShutdownDetector(int shutdown_fd);

  void ThreadMain() override;

 private:
  const int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownDetector);
};

ShutdownDetector::ShutdownDetector(int shutdown_fd)
    : shutdown_fd_(shutdown_fd) {
  CHECK_NE(shutdown_fd_, -1);
}

// These functions are used to help us diagnose crash dumps that happen
// during the shutdown process.
NOINLINE void ShutdownFDReadError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

NOINLINE void ShutdownFDClosedError() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

NOINLINE void ExitPosted() {
  // Ensure function isn't optimized away.
  asm("");
  sleep(UINT_MAX);
}

void ShutdownDetector::ThreadMain() {
  base::PlatformThread::SetName("CrShutdownDetector");

  int signal;
  size_t bytes_read = 0;
  ssize_t ret;
  do {
    ret = HANDLE_EINTR(
        read(shutdown_fd_,
             reinterpret_cast<char*>(&signal) + bytes_read,
             sizeof(signal) - bytes_read));
    if (ret < 0) {
      NOTREACHED() << "Unexpected error: " << strerror(errno);
      ShutdownFDReadError();
      break;
    } else if (ret == 0) {
      NOTREACHED() << "Unexpected closure of shutdown pipe.";
      ShutdownFDClosedError();
      break;
    }
    bytes_read += ret;
  } while (bytes_read < sizeof(signal));
  VLOG(1) << "Handling shutdown for signal " << signal << ".";
  base::Closure task = base::Bind(&ExitHandler::ExitWhenPossibleOnUIThread);

  if (!BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, task)) {
    // Without a UI thread to post the exit task to, there aren't many
    // options.  Raise the signal again.  The default handler will pick it up
    // and cause an ungraceful exit.
    RAW_LOG(WARNING, "No UI thread, exiting ungracefully.");
    kill(getpid(), signal);

    // The signal may be handled on another thread.  Give that a chance to
    // happen.
    sleep(3);

    // We really should be dead by now.  For whatever reason, we're not. Exit
    // immediately, with the exit status set to the signal number with bit 8
    // set.  On the systems that we care about, this exit status is what is
    // normally used to indicate an exit by this signal's default handler.
    // This mechanism isn't a de jure standard, but even in the worst case, it
    // should at least result in an immediate exit.
    RAW_LOG(WARNING, "Still here, exiting really ungracefully.");
    _exit(signal | (1 << 7));
  }
  ExitPosted();
}

}  // namespace

// ChromeBrowserMainPartsPosix -------------------------------------------------

ChromeBrowserMainPartsPosix::ChromeBrowserMainPartsPosix(
    const content::MainFunctionParams& parameters)
    : ChromeBrowserMainParts(parameters) {
}

void ChromeBrowserMainPartsPosix::PreEarlyInitialization() {
  ChromeBrowserMainParts::PreEarlyInitialization();

  // We need to accept SIGCHLD, even though our handler is a no-op because
  // otherwise we cannot wait on children. (According to POSIX 2001.)
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIGCHLDHandler;
  CHECK(sigaction(SIGCHLD, &action, NULL) == 0);
}

void ChromeBrowserMainPartsPosix::PostMainMessageLoopStart() {
  ChromeBrowserMainParts::PostMainMessageLoopStart();

  int pipefd[2];
  int ret = pipe(pipefd);
  if (ret < 0) {
    PLOG(DFATAL) << "Failed to create pipe";
  } else {
    g_pipe_pid = getpid();
    g_shutdown_pipe_read_fd = pipefd[0];
    g_shutdown_pipe_write_fd = pipefd[1];
#if !defined(ADDRESS_SANITIZER) && !defined(KEEP_SHADOW_STACKS)
    const size_t kShutdownDetectorThreadStackSize = PTHREAD_STACK_MIN * 2;
#else
    // ASan instrumentation and -finstrument-functions (used for keeping the
    // shadow stacks) bloat the stack frames, so we need to increase the stack
    // size to avoid hitting the guard page.
    const size_t kShutdownDetectorThreadStackSize = PTHREAD_STACK_MIN * 4;
#endif
    // TODO(viettrungluu,willchan): crbug.com/29675 - This currently leaks, so
    // if you change this, you'll probably need to change the suppression.
    if (!base::PlatformThread::CreateNonJoinable(
            kShutdownDetectorThreadStackSize,
            new ShutdownDetector(g_shutdown_pipe_read_fd))) {
      LOG(DFATAL) << "Failed to create shutdown detector task.";
    }

    // Setup signal handlers for shutdown AFTER shutdown pipe is setup because
    // it may be called right away after handler is set.

    // If adding to this list of signal handlers, note the new signal probably
    // needs to be reset in child processes. See
    // base/process_util_posix.cc:LaunchProcess.

    // We need to handle SIGTERM, because that is how many POSIX-based distros
    // ask processes to quit gracefully at shutdown time.
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGTERMHandler;
    CHECK(sigaction(SIGTERM, &action, NULL) == 0);

    // Also handle SIGINT - when the user terminates the browser via Ctrl+C. If
    // the browser process is being debugged, GDB will catch the SIGINT first.
    action.sa_handler = SIGINTHandler;
    CHECK(sigaction(SIGINT, &action, NULL) == 0);

    // And SIGHUP, for when the terminal disappears. On shutdown, many Linux
    // distros send SIGHUP, SIGTERM, and then SIGKILL.
    action.sa_handler = SIGHUPHandler;
    CHECK(sigaction(SIGHUP, &action, NULL) == 0);
  }
}

void ChromeBrowserMainPartsPosix::ShowMissingLocaleMessageBox() {
#if defined(OS_CHROMEOS)
  NOTREACHED();  // Should not ever happen on ChromeOS.
#elif defined(OS_MACOSX)
  // Not called on Mac because we load the locale files differently.
  NOTREACHED();
#elif defined(USE_AURA)
  // TODO(port): We may want a views based message dialog here eventually, but
  // for now, crash.
  NOTREACHED();
#else
#error "Need MessageBox implementation."
#endif
}
