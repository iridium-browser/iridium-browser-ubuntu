// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/child_process_launcher_android.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/unguessable_token_android.h"
#include "base/logging.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "gpu/ipc/common/gpu_surface_tracker.h"
#include "jni/ChildProcessLauncher_jni.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/surface_texture.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::StartChildProcessCallback;

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render frame ID and player ID.
static void SetSurfacePeer(
    const base::android::JavaRef<jobject>& surface,
    base::ProcessHandle render_process_handle,
    int render_frame_id,
    int player_id) {
  int render_process_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      render_process_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }
  if (!render_process_id) {
    DVLOG(1) << "Cannot find render process for render_process_handle "
             << render_process_handle;
    return;
  }

  RenderFrameHostImpl* frame =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame) {
    DVLOG(1) << "Cannot find frame for render_frame_id " << render_frame_id;
    return;
  }

  BrowserMediaPlayerManager* player_manager =
      MediaWebContentsObserverAndroid::FromWebContents(
          WebContents::FromRenderFrameHost(frame))
          ->GetMediaPlayerManager(frame);
  if (!player_manager) {
    DVLOG(1) << "Cannot find the media player manager for frame " << frame;
    return;
  }

  media::MediaPlayerAndroid* player = player_manager->GetPlayer(player_id);
  if (!player) {
    DVLOG(1) << "Cannot find media player for player_id " << player_id;
    return;
  }

  if (player != player_manager->GetFullscreenPlayer()) {
    gl::ScopedJavaSurface scoped_surface(surface);
    player->SetVideoSurface(std::move(scoped_surface));
  }
}

}  // anonymous namespace

// Called from ChildProcessLauncher.java when the ChildProcess was
// started.
// |client_context| is the pointer to StartChildProcessCallback which was
// passed in from StartChildProcess.
// |handle| is the processID of the child process as originated in Java, 0 if
// the ChildProcess could not be created.
static void OnChildProcessStarted(JNIEnv*,
                                  const JavaParamRef<jclass>&,
                                  jlong client_context,
                                  jint handle) {
  StartChildProcessCallback* callback =
      reinterpret_cast<StartChildProcessCallback*>(client_context);
  int launch_result = (handle == base::kNullProcessHandle)
                      ? LAUNCH_RESULT_FAILURE
                      : LAUNCH_RESULT_SUCCESS;
  callback->Run(static_cast<base::ProcessHandle>(handle), launch_result);
  delete callback;
}

void StartChildProcess(
    const base::CommandLine::StringVector& argv,
    int child_process_id,
    content::FileDescriptorInfo* files_to_register,
    const StartChildProcessCallback& callback) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv = ToJavaArrayOfStrings(env, argv);

  size_t file_count = files_to_register->GetMappingSize();
  DCHECK(file_count > 0);

  ScopedJavaLocalRef<jclass> j_file_info_class = base::android::GetClass(
      env, "org/chromium/content/common/FileDescriptorInfo");
  ScopedJavaLocalRef<jobjectArray> j_file_infos(
      env, env->NewObjectArray(file_count, j_file_info_class.obj(), NULL));
  base::android::CheckException(env);

  for (size_t i = 0; i < file_count; ++i) {
    int fd = files_to_register->GetFDAt(i);
    PCHECK(0 <= fd);
    int id = files_to_register->GetIDAt(i);
    const auto& region = files_to_register->GetRegionAt(i);
    bool auto_close = files_to_register->OwnsFD(fd);
    ScopedJavaLocalRef<jobject> j_file_info =
        Java_ChildProcessLauncher_makeFdInfo(env, id, fd, auto_close,
            region.offset, region.size);
    PCHECK(j_file_info.obj());
    env->SetObjectArrayElement(j_file_infos.obj(), i, j_file_info.obj());
    if (auto_close) {
      ignore_result(files_to_register->ReleaseFD(fd).release());
    }
  }

  Java_ChildProcessLauncher_start(
      env, base::android::GetApplicationContext(), j_argv, child_process_id,
      j_file_infos,
      reinterpret_cast<intptr_t>(new StartChildProcessCallback(callback)));
}

void StopChildProcess(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncher_stop(env, static_cast<jint>(handle));
}

bool IsChildProcessOomProtected(base::ProcessHandle handle) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_isOomProtected(env,
      static_cast<jint>(handle));
}

void SetChildProcessInForeground(base::ProcessHandle handle,
    bool in_foreground) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncher_setInForeground(env,
      static_cast<jint>(handle), static_cast<jboolean>(in_foreground));
}

void EstablishSurfacePeer(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          jint pid,
                          const JavaParamRef<jobject>& surface,
                          jint primary_id,
                          jint secondary_id) {
  ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  if (jsurface.is_null())
    return;

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, jsurface, pid, primary_id, secondary_id));
}

void CompleteScopedSurfaceRequest(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jobject>& token,
                                  const JavaParamRef<jobject>& surface) {
  base::UnguessableToken requestToken =
      base::android::UnguessableTokenAndroid::FromJavaUnguessableToken(env,
                                                                       token);
  if (!requestToken) {
    DLOG(ERROR) << "Received invalid surface request token.";
    return;
  }

  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  ScopedJavaGlobalRef<jobject> jsurface;
  jsurface.Reset(env, surface);
  ScopedSurfaceRequestManager::GetInstance()->FulfillScopedSurfaceRequest(
      requestToken, gl::ScopedJavaSurface(jsurface));
}

jboolean IsSingleProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

base::android::ScopedJavaLocalRef<jobject> GetViewSurface(JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    jint surface_id) {
  gl::ScopedJavaSurface surface_view =
      gpu::GpuSurfaceTracker::GetInstance()->AcquireJavaSurface(surface_id);
  return base::android::ScopedJavaLocalRef<jobject>(surface_view.j_surface());
}

bool RegisterChildProcessLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
