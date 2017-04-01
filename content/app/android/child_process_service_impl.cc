// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/android/child_process_service_impl.h"

#include <android/native_window_jni.h>
#include <cpu-features.h>

#include "base/android/jni_array.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/android/unguessable_token_android.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/global_descriptors.h"
#include "base/unguessable_token.h"
#include "content/child/child_thread_impl.h"
#include "content/public/common/content_descriptors.h"
#include "gpu/ipc/common/android/scoped_surface_request_conduit.h"
#include "gpu/ipc/common/android/surface_texture_peer.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "ipc/ipc_descriptors.h"
#include "jni/ChildProcessServiceImpl_jni.h"
#include "ui/gl/android/scoped_java_surface.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;

namespace content {

namespace {

// TODO(sievers): Use two different implementations of this depending on if
// we're in a renderer or gpu process.
class ChildProcessSurfaceManager : public gpu::SurfaceTexturePeer,
                                   public gpu::ScopedSurfaceRequestConduit,
                                   public gpu::GpuSurfaceLookup {
 public:
  ChildProcessSurfaceManager() {}
  ~ChildProcessSurfaceManager() override {}

  // |service impl| is the instance of
  // org.chromium.content.app.ChildProcessServiceImpl.
  void SetServiceImpl(const base::android::JavaRef<jobject>& service_impl) {
    service_impl_.Reset(service_impl);
  }

  // Overridden from SurfaceTexturePeer:
  void EstablishSurfaceTexturePeer(
      base::ProcessHandle pid,
      scoped_refptr<gl::SurfaceTexture> surface_texture,
      int primary_id,
      int secondary_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::Java_ChildProcessServiceImpl_establishSurfaceTexturePeer(
        env, service_impl_, pid, surface_texture->j_surface_texture(),
        primary_id, secondary_id);
  }

  // Overriden from ScopedSurfaceRequestConduit:
  void ForwardSurfaceTextureForSurfaceRequest(
      const base::UnguessableToken& request_token,
      const gl::SurfaceTexture* surface_texture) override {
    JNIEnv* env = base::android::AttachCurrentThread();

    content::
        Java_ChildProcessServiceImpl_forwardSurfaceTextureForSurfaceRequest(
            env, service_impl_,
            base::android::UnguessableTokenAndroid::Create(env, request_token),
            surface_texture->j_surface_texture());
  }

  // Overridden from GpuSurfaceLookup:
  gfx::AcceleratedWidget AcquireNativeWidget(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    gl::ScopedJavaSurface surface(
        content::Java_ChildProcessServiceImpl_getViewSurface(env, service_impl_,
                                                             surface_id));

    if (surface.j_surface().is_null())
      return NULL;

    // Note: This ensures that any local references used by
    // ANativeWindow_fromSurface are released immediately. This is needed as a
    // workaround for https://code.google.com/p/android/issues/detail?id=68174
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    ANativeWindow* native_window =
        ANativeWindow_fromSurface(env, surface.j_surface().obj());

    return native_window;
  }

  // Overridden from GpuSurfaceLookup:
  gl::ScopedJavaSurface AcquireJavaSurface(int surface_id) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    return gl::ScopedJavaSurface(
        content::Java_ChildProcessServiceImpl_getViewSurface(env, service_impl_,
                                                             surface_id));
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<ChildProcessSurfaceManager>;
  // The instance of org.chromium.content.app.ChildProcessServiceImpl.
  base::android::ScopedJavaGlobalRef<jobject> service_impl_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessSurfaceManager);
};

static base::LazyInstance<ChildProcessSurfaceManager>::Leaky
    g_child_process_surface_manager = LAZY_INSTANCE_INITIALIZER;

// Chrome actually uses the renderer code path for all of its child
// processes such as renderers, plugins, etc.
void InternalInitChildProcessImpl(JNIEnv* env,
                                  const JavaParamRef<jobject>& service_impl,
                                  jint cpu_count,
                                  jlong cpu_features) {
  // Set the CPU properties.
  android_setCpu(cpu_count, cpu_features);

  g_child_process_surface_manager.Get().SetServiceImpl(service_impl);

  gpu::SurfaceTexturePeer::InitInstance(
      g_child_process_surface_manager.Pointer());
  gpu::GpuSurfaceLookup::InitInstance(
      g_child_process_surface_manager.Pointer());
  gpu::ScopedSurfaceRequestConduit::SetInstance(
      g_child_process_surface_manager.Pointer());

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(env);
}

}  // namespace <anonymous>

void RegisterGlobalFileDescriptor(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  jint id,
                                  jint fd,
                                  jlong offset,
                                  jlong size) {
  base::MemoryMappedFile::Region region = {offset, size};
  base::GlobalDescriptors::GetInstance()->Set(id, fd, region);
}

void InitChildProcessImpl(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jobject>& service_impl,
                          jint cpu_count,
                          jlong cpu_features) {
  InternalInitChildProcessImpl(env, service_impl, cpu_count, cpu_features);
}

void ExitChildProcess(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  VLOG(0) << "ChildProcessServiceImpl: Exiting child process.";
  base::android::LibraryLoaderExitHook();
  _exit(0);
}

bool RegisterChildProcessServiceImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ShutdownMainThread(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  ChildThreadImpl::ShutdownThread();
}

}  // namespace content
