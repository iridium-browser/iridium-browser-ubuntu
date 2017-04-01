// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

#include <vector>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

// The GL Api being used. This could be g_real_gl or gl_trace_gl
static GLApi* g_gl = NULL;
// A GL Api that calls directly into the driver.
static RealGLApi* g_real_gl = NULL;
// A GL Api that does nothing but warn about illegal GL calls without a context
// current.
static NoContextGLApi* g_no_context_gl = NULL;
// A GL Api that calls TRACE and then calls another GL api.
static TraceGLApi* g_trace_gl = NULL;
// The GL Api being used for stub contexts. If null, g_gl is used instead.
static GLApi* g_stub_gl = NULL;
// GL version used when initializing dynamic bindings.
static GLVersionInfo* g_version_info = NULL;

namespace {

static inline GLenum GetInternalFormat(GLenum internal_format) {
  if (!g_version_info->is_es) {
    if (internal_format == GL_BGRA_EXT || internal_format == GL_BGRA8_EXT)
      return GL_RGBA8;
  }
  if (g_version_info->is_es3 && g_version_info->is_mesa) {
    // Mesa bug workaround: Mipmapping does not work when using GL_BGRA_EXT
    if (internal_format == GL_BGRA_EXT)
      return GL_RGBA;
  }
  return internal_format;
}

// TODO(epenner): Could the above function be merged into this and removed?
static inline GLenum GetTexInternalFormat(GLenum internal_format,
                                          GLenum format,
                                          GLenum type) {
  DCHECK(g_version_info);
  GLenum gl_internal_format = GetInternalFormat(internal_format);

  // g_version_info must be initialized when this function is bound.
  if (g_version_info->is_es3) {
    if (internal_format == GL_RED_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_R8_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_R16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_R32F_EXT;
          break;
        default:
          NOTREACHED();
          break;
      }
      return gl_internal_format;
    } else if (internal_format == GL_RG_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_RG8_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_RG16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_RG32F_EXT;
          break;
        default:
          NOTREACHED();
          break;
      }
      return gl_internal_format;
    }
  }

  if (type == GL_FLOAT && g_version_info->is_angle && g_version_info->is_es &&
      g_version_info->major_version == 2) {
    // It's possible that the texture is using a sized internal format, and
    // ANGLE exposing GLES2 API doesn't support those.
    // TODO(oetuaho@nvidia.com): Remove these conversions once ANGLE has the
    // support.
    // http://code.google.com/p/angleproject/issues/detail?id=556
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB;
        break;
      default:
        break;
    }
  }

  if (g_version_info->IsAtLeastGL(2, 1) ||
      g_version_info->IsAtLeastGLES(3, 0)) {
    switch (internal_format) {
      case GL_SRGB_EXT:
        gl_internal_format = GL_SRGB8;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_internal_format = GL_SRGB8_ALPHA8;
        break;
      default:
        break;
    }
  }

  if (g_version_info->is_es)
    return gl_internal_format;

  if (type == GL_FLOAT) {
    switch (internal_format) {
      // We need to map all the unsized internal formats from ES2 clients.
      case GL_RGBA:
        gl_internal_format = GL_RGBA32F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB32F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA32F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE32F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA32F_ARB;
        break;
      // RED and RG are reached here because on Desktop GL core profile,
      // LUMINANCE/ALPHA formats are emulated through RED and RG in Chrome.
      case GL_RED:
        gl_internal_format = GL_R32F;
        break;
      case GL_RG:
        gl_internal_format = GL_RG32F;
        break;
      default:
        // We can't assert here because if the client context is ES3,
        // all sized internal_format will reach here.
        break;
    }
  } else if (type == GL_HALF_FLOAT_OES) {
    switch (internal_format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA16F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB16F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA16F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE16F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA16F_ARB;
        break;
      // RED and RG are reached here because on Desktop GL core profile,
      // LUMINANCE/ALPHA formats are emulated through RED and RG in Chrome.
      case GL_RED:
        gl_internal_format = GL_R16F;
        break;
      case GL_RG:
        gl_internal_format = GL_RG16F;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  return gl_internal_format;
}

static inline GLenum GetTexFormat(GLenum format) {
  GLenum gl_format = format;

  DCHECK(g_version_info);
  if (g_version_info->IsAtLeastGL(2, 1) ||
      g_version_info->IsAtLeastGLES(3, 0)) {
    switch (format) {
      case GL_SRGB_EXT:
        gl_format = GL_RGB;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_format = GL_RGBA;
        break;
      default:
        break;
    }
  }

  return gl_format;
}

static inline GLenum GetTexType(GLenum type) {
  if (!g_version_info->is_es) {
    if (type == GL_HALF_FLOAT_OES)
      return GL_HALF_FLOAT_ARB;
  }
  return type;
}

static void GL_BINDING_CALL CustomTexImage2D(
    GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  GLenum gl_internal_format = GetTexInternalFormat(
      internalformat, format, type);
  GLenum gl_format = GetTexFormat(format);
  GLenum gl_type = GetTexType(type);
  g_driver_gl.orig_fn.glTexImage2DFn(
      target, level, gl_internal_format, width, height, border, gl_format,
      gl_type, pixels);
}

static void GL_BINDING_CALL CustomTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, const void* pixels) {
  GLenum gl_format = GetTexFormat(format);
  GLenum gl_type = GetTexType(type);
  g_driver_gl.orig_fn.glTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, gl_format, gl_type,
      pixels);
}

static void GL_BINDING_CALL CustomTexStorage2DEXT(
    GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(internalformat);
  g_driver_gl.orig_fn.glTexStorage2DEXTFn(
      target, levels, gl_internal_format, width, height);
}

static void GL_BINDING_CALL CustomRenderbufferStorageEXT(
    GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(internalformat);
  g_driver_gl.orig_fn.glRenderbufferStorageEXTFn(
      target, gl_internal_format, width, height);
}

// The ANGLE and IMG variants of glRenderbufferStorageMultisample currently do
// not support BGRA render buffers so only the EXT one is customized. If
// GL_CHROMIUM_renderbuffer_format_BGRA8888 support is added to ANGLE then the
// ANGLE version should also be customized.
static void GL_BINDING_CALL CustomRenderbufferStorageMultisampleEXT(
    GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(internalformat);
  g_driver_gl.orig_fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, gl_internal_format, width, height);
}

static void GL_BINDING_CALL
CustomRenderbufferStorageMultisample(GLenum target,
                                     GLsizei samples,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(internalformat);
  g_driver_gl.orig_fn.glRenderbufferStorageMultisampleFn(
      target, samples, gl_internal_format, width, height);
}

}  // anonymous namespace

void DriverGL::InitializeCustomDynamicBindings(GLContext* context) {
  InitializeDynamicBindings(context);

  DCHECK(orig_fn.glTexImage2DFn == NULL);
  orig_fn.glTexImage2DFn = fn.glTexImage2DFn;
  fn.glTexImage2DFn =
      reinterpret_cast<glTexImage2DProc>(CustomTexImage2D);

  DCHECK(orig_fn.glTexSubImage2DFn == NULL);
  orig_fn.glTexSubImage2DFn = fn.glTexSubImage2DFn;
  fn.glTexSubImage2DFn =
      reinterpret_cast<glTexSubImage2DProc>(CustomTexSubImage2D);

  DCHECK(orig_fn.glTexStorage2DEXTFn == NULL);
  orig_fn.glTexStorage2DEXTFn = fn.glTexStorage2DEXTFn;
  fn.glTexStorage2DEXTFn =
      reinterpret_cast<glTexStorage2DEXTProc>(CustomTexStorage2DEXT);

  DCHECK(orig_fn.glRenderbufferStorageEXTFn == NULL);
  orig_fn.glRenderbufferStorageEXTFn = fn.glRenderbufferStorageEXTFn;
  fn.glRenderbufferStorageEXTFn =
      reinterpret_cast<glRenderbufferStorageEXTProc>(
      CustomRenderbufferStorageEXT);

  DCHECK(orig_fn.glRenderbufferStorageMultisampleEXTFn == NULL);
  orig_fn.glRenderbufferStorageMultisampleEXTFn =
      fn.glRenderbufferStorageMultisampleEXTFn;
  fn.glRenderbufferStorageMultisampleEXTFn =
      reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
      CustomRenderbufferStorageMultisampleEXT);

  DCHECK(orig_fn.glRenderbufferStorageMultisampleFn == NULL);
  orig_fn.glRenderbufferStorageMultisampleFn =
      fn.glRenderbufferStorageMultisampleFn;
  fn.glRenderbufferStorageMultisampleFn =
      reinterpret_cast<glRenderbufferStorageMultisampleProc>(
          CustomRenderbufferStorageMultisample);
}

static void GL_BINDING_CALL NullDrawClearFn(GLbitfield mask) {
  if (!g_driver_gl.null_draw_bindings_enabled)
    g_driver_gl.orig_fn.glClearFn(mask);
}

static void GL_BINDING_CALL
NullDrawDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  if (!g_driver_gl.null_draw_bindings_enabled)
    g_driver_gl.orig_fn.glDrawArraysFn(mode, first, count);
}

static void GL_BINDING_CALL NullDrawDrawElementsFn(GLenum mode,
                                                   GLsizei count,
                                                   GLenum type,
                                                   const void* indices) {
  if (!g_driver_gl.null_draw_bindings_enabled)
    g_driver_gl.orig_fn.glDrawElementsFn(mode, count, type, indices);
}

void DriverGL::InitializeNullDrawBindings() {
  DCHECK(orig_fn.glClearFn == NULL);
  orig_fn.glClearFn = fn.glClearFn;
  fn.glClearFn = NullDrawClearFn;

  DCHECK(orig_fn.glDrawArraysFn == NULL);
  orig_fn.glDrawArraysFn = fn.glDrawArraysFn;
  fn.glDrawArraysFn = NullDrawDrawArraysFn;

  DCHECK(orig_fn.glDrawElementsFn == NULL);
  orig_fn.glDrawElementsFn = fn.glDrawElementsFn;
  fn.glDrawElementsFn = NullDrawDrawElementsFn;

  null_draw_bindings_enabled = true;
}

bool DriverGL::HasInitializedNullDrawBindings() {
  return orig_fn.glClearFn != NULL && orig_fn.glDrawArraysFn != NULL &&
         orig_fn.glDrawElementsFn != NULL;
}

bool DriverGL::SetNullDrawBindingsEnabled(bool enabled) {
  DCHECK(orig_fn.glClearFn != NULL);
  DCHECK(orig_fn.glDrawArraysFn != NULL);
  DCHECK(orig_fn.glDrawElementsFn != NULL);

  bool before = null_draw_bindings_enabled;
  null_draw_bindings_enabled = enabled;
  return before;
}

void InitializeStaticGLBindingsGL() {
  g_current_gl_context_tls = new base::ThreadLocalPointer<GLApi>;
  g_driver_gl.InitializeStaticBindings();
  if (!g_real_gl) {
    g_real_gl = new RealGLApi();
    g_trace_gl = new TraceGLApi(g_real_gl);
    g_no_context_gl = new NoContextGLApi();
  }
  g_real_gl->Initialize(&g_driver_gl);
  g_gl = g_real_gl;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGPUServiceTracing)) {
    g_gl = g_trace_gl;
  }
  SetGLToRealGLApi();
}

GLApi* GetCurrentGLApi() {
  return g_current_gl_context_tls ? g_current_gl_context_tls->Get() : nullptr;
}

void SetGLApi(GLApi* api) {
  g_current_gl_context_tls->Set(api);
}

void SetGLToRealGLApi() {
  SetGLApi(g_gl);
}

void SetGLToStubGLApi() {
  SetGLApi(g_stub_gl ? g_stub_gl : g_gl);
}

void SetGLApiToNoContext() {
  SetGLApi(g_no_context_gl);
}

void SetStubGLApi(GLApi* api) {
  g_stub_gl = api;
}

const GLVersionInfo* GetGLVersionInfo() {
  return g_version_info;
}

void InitializeDynamicGLBindingsGL(GLContext* context) {
  if (g_version_info)
    return;
  g_real_gl->InitializeFilteredExtensions();
  g_driver_gl.InitializeCustomDynamicBindings(context);
  DCHECK(context && context->IsCurrent(NULL) && !g_version_info);
  g_version_info = new GLVersionInfo(
      context->GetGLVersion().c_str(),
      context->GetGLRenderer().c_str(),
      context->GetExtensions().c_str());
}

void InitializeDebugGLBindingsGL() {
  g_driver_gl.InitializeDebugBindings();
}

void InitializeNullDrawGLBindingsGL() {
  g_driver_gl.InitializeNullDrawBindings();
}

bool HasInitializedNullDrawGLBindingsGL() {
  return g_driver_gl.HasInitializedNullDrawBindings();
}

bool SetNullDrawGLBindingsEnabledGL(bool enabled) {
  return g_driver_gl.SetNullDrawBindingsEnabled(enabled);
}

void ClearBindingsGL() {
  if (g_real_gl) {
    delete g_real_gl;
    g_real_gl = NULL;
  }
  if (g_trace_gl) {
    delete g_trace_gl;
    g_trace_gl = NULL;
  }
  if (g_no_context_gl) {
    delete g_no_context_gl;
    g_no_context_gl = NULL;
  }
  g_gl = NULL;
  g_stub_gl = NULL;
  g_driver_gl.ClearBindings();
  if (g_current_gl_context_tls) {
    delete g_current_gl_context_tls;
    g_current_gl_context_tls = NULL;
  }
  if (g_version_info) {
    delete g_version_info;
    g_version_info = NULL;
  }
}

GLApi::GLApi() {
}

GLApi::~GLApi() {
  if (GetCurrentGLApi() == this)
    SetGLApi(NULL);
}

GLApiBase::GLApiBase()
    : driver_(NULL) {
}

GLApiBase::~GLApiBase() {
}

void GLApiBase::InitializeBase(DriverGL* driver) {
  driver_ = driver;
}

RealGLApi::RealGLApi() {
#if DCHECK_IS_ON()
  filtered_exts_initialized_ = false;
#endif
}

RealGLApi::~RealGLApi() {
}

void RealGLApi::Initialize(DriverGL* driver) {
  InitializeWithCommandLine(driver, base::CommandLine::ForCurrentProcess());
}

void RealGLApi::InitializeWithCommandLine(DriverGL* driver,
                                          base::CommandLine* command_line) {
  DCHECK(command_line);
  InitializeBase(driver);

  const std::string disabled_extensions = command_line->GetSwitchValueASCII(
      switches::kDisableGLExtensions);
  if (!disabled_extensions.empty()) {
    disabled_exts_ = base::SplitString(
        disabled_extensions, ", ;",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
}

void RealGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  if (pname == GL_NUM_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    *params = static_cast<GLint>(filtered_exts_.size());
  } else {
    GLApiBase::glGetIntegervFn(pname, params);
  }
}

const GLubyte* RealGLApi::glGetStringFn(GLenum name) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    return reinterpret_cast<const GLubyte*>(filtered_exts_str_.c_str());
  }
  return GLApiBase::glGetStringFn(name);
}

const GLubyte* RealGLApi::glGetStringiFn(GLenum name, GLuint index) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    if (index >= filtered_exts_.size()) {
      return NULL;
    }
    return reinterpret_cast<const GLubyte*>(filtered_exts_[index].c_str());
  }
  return GLApiBase::glGetStringiFn(name, index);
}

void RealGLApi::InitializeFilteredExtensions() {
  if (disabled_exts_.size()) {
    filtered_exts_.clear();
    if (WillUseGLGetStringForExtensions()) {
      filtered_exts_str_ =
          FilterGLExtensionList(reinterpret_cast<const char*>(
                                    GLApiBase::glGetStringFn(GL_EXTENSIONS)),
                                disabled_exts_);
      filtered_exts_ = base::SplitString(
          filtered_exts_str_, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    } else {
      GLint num_extensions = 0;
      GLApiBase::glGetIntegervFn(GL_NUM_EXTENSIONS, &num_extensions);
      for (GLint i = 0; i < num_extensions; ++i) {
        const char* gl_extension = reinterpret_cast<const char*>(
            GLApiBase::glGetStringiFn(GL_EXTENSIONS, i));
        DCHECK(gl_extension != NULL);
        if (!base::ContainsValue(disabled_exts_, gl_extension))
          filtered_exts_.push_back(gl_extension);
      }
      filtered_exts_str_ = base::JoinString(filtered_exts_, " ");
    }
#if DCHECK_IS_ON()
    filtered_exts_initialized_ = true;
#endif
  }
}

TraceGLApi::~TraceGLApi() {
}

NoContextGLApi::NoContextGLApi() {
}

NoContextGLApi::~NoContextGLApi() {
}

}  // namespace gl
