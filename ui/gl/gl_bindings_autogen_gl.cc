// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include <string>

#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

static bool g_debugBindingsInitialized;
DriverGL g_driver_gl;

void DriverGL::InitializeStaticBindings() {
  fn.glActiveTextureFn = reinterpret_cast<glActiveTextureProc>(
      GetGLProcAddress("glActiveTexture"));
  fn.glApplyFramebufferAttachmentCMAAINTELFn = 0;
  fn.glAttachShaderFn =
      reinterpret_cast<glAttachShaderProc>(GetGLProcAddress("glAttachShader"));
  fn.glBeginQueryFn = 0;
  fn.glBeginTransformFeedbackFn = 0;
  fn.glBindAttribLocationFn = reinterpret_cast<glBindAttribLocationProc>(
      GetGLProcAddress("glBindAttribLocation"));
  fn.glBindBufferFn =
      reinterpret_cast<glBindBufferProc>(GetGLProcAddress("glBindBuffer"));
  fn.glBindBufferBaseFn = 0;
  fn.glBindBufferRangeFn = 0;
  fn.glBindFragDataLocationFn = 0;
  fn.glBindFragDataLocationIndexedFn = 0;
  fn.glBindFramebufferEXTFn = 0;
  fn.glBindImageTextureEXTFn = 0;
  fn.glBindRenderbufferEXTFn = 0;
  fn.glBindSamplerFn = 0;
  fn.glBindTextureFn =
      reinterpret_cast<glBindTextureProc>(GetGLProcAddress("glBindTexture"));
  fn.glBindTransformFeedbackFn = 0;
  fn.glBindUniformLocationCHROMIUMFn = 0;
  fn.glBindVertexArrayOESFn = 0;
  fn.glBlendBarrierKHRFn = 0;
  fn.glBlendColorFn =
      reinterpret_cast<glBlendColorProc>(GetGLProcAddress("glBlendColor"));
  fn.glBlendEquationFn = reinterpret_cast<glBlendEquationProc>(
      GetGLProcAddress("glBlendEquation"));
  fn.glBlendEquationSeparateFn = reinterpret_cast<glBlendEquationSeparateProc>(
      GetGLProcAddress("glBlendEquationSeparate"));
  fn.glBlendFuncFn =
      reinterpret_cast<glBlendFuncProc>(GetGLProcAddress("glBlendFunc"));
  fn.glBlendFuncSeparateFn = reinterpret_cast<glBlendFuncSeparateProc>(
      GetGLProcAddress("glBlendFuncSeparate"));
  fn.glBlitFramebufferFn = 0;
  fn.glBlitFramebufferANGLEFn = 0;
  fn.glBlitFramebufferEXTFn = 0;
  fn.glBufferDataFn =
      reinterpret_cast<glBufferDataProc>(GetGLProcAddress("glBufferData"));
  fn.glBufferSubDataFn = reinterpret_cast<glBufferSubDataProc>(
      GetGLProcAddress("glBufferSubData"));
  fn.glCheckFramebufferStatusEXTFn = 0;
  fn.glClearFn = reinterpret_cast<glClearProc>(GetGLProcAddress("glClear"));
  fn.glClearBufferfiFn = 0;
  fn.glClearBufferfvFn = 0;
  fn.glClearBufferivFn = 0;
  fn.glClearBufferuivFn = 0;
  fn.glClearColorFn =
      reinterpret_cast<glClearColorProc>(GetGLProcAddress("glClearColor"));
  fn.glClearDepthFn =
      reinterpret_cast<glClearDepthProc>(GetGLProcAddress("glClearDepth"));
  fn.glClearDepthfFn = 0;
  fn.glClearStencilFn =
      reinterpret_cast<glClearStencilProc>(GetGLProcAddress("glClearStencil"));
  fn.glClientWaitSyncFn = 0;
  fn.glColorMaskFn =
      reinterpret_cast<glColorMaskProc>(GetGLProcAddress("glColorMask"));
  fn.glCompileShaderFn = reinterpret_cast<glCompileShaderProc>(
      GetGLProcAddress("glCompileShader"));
  fn.glCompressedCopyTextureCHROMIUMFn = 0;
  fn.glCompressedTexImage2DFn = reinterpret_cast<glCompressedTexImage2DProc>(
      GetGLProcAddress("glCompressedTexImage2D"));
  fn.glCompressedTexImage3DFn = 0;
  fn.glCompressedTexSubImage2DFn =
      reinterpret_cast<glCompressedTexSubImage2DProc>(
          GetGLProcAddress("glCompressedTexSubImage2D"));
  fn.glCompressedTexSubImage3DFn = 0;
  fn.glCopyBufferSubDataFn = 0;
  fn.glCopySubTextureCHROMIUMFn = 0;
  fn.glCopyTexImage2DFn = reinterpret_cast<glCopyTexImage2DProc>(
      GetGLProcAddress("glCopyTexImage2D"));
  fn.glCopyTexSubImage2DFn = reinterpret_cast<glCopyTexSubImage2DProc>(
      GetGLProcAddress("glCopyTexSubImage2D"));
  fn.glCopyTexSubImage3DFn = 0;
  fn.glCopyTextureCHROMIUMFn = 0;
  fn.glCoverageModulationNVFn = 0;
  fn.glCoverFillPathInstancedNVFn = 0;
  fn.glCoverFillPathNVFn = 0;
  fn.glCoverStrokePathInstancedNVFn = 0;
  fn.glCoverStrokePathNVFn = 0;
  fn.glCreateProgramFn = reinterpret_cast<glCreateProgramProc>(
      GetGLProcAddress("glCreateProgram"));
  fn.glCreateShaderFn =
      reinterpret_cast<glCreateShaderProc>(GetGLProcAddress("glCreateShader"));
  fn.glCullFaceFn =
      reinterpret_cast<glCullFaceProc>(GetGLProcAddress("glCullFace"));
  fn.glDebugMessageCallbackFn = 0;
  fn.glDebugMessageControlFn = 0;
  fn.glDebugMessageInsertFn = 0;
  fn.glDeleteBuffersARBFn = reinterpret_cast<glDeleteBuffersARBProc>(
      GetGLProcAddress("glDeleteBuffers"));
  fn.glDeleteFencesAPPLEFn = 0;
  fn.glDeleteFencesNVFn = 0;
  fn.glDeleteFramebuffersEXTFn = 0;
  fn.glDeletePathsNVFn = 0;
  fn.glDeleteProgramFn = reinterpret_cast<glDeleteProgramProc>(
      GetGLProcAddress("glDeleteProgram"));
  fn.glDeleteQueriesFn = 0;
  fn.glDeleteRenderbuffersEXTFn = 0;
  fn.glDeleteSamplersFn = 0;
  fn.glDeleteShaderFn =
      reinterpret_cast<glDeleteShaderProc>(GetGLProcAddress("glDeleteShader"));
  fn.glDeleteSyncFn = 0;
  fn.glDeleteTexturesFn = reinterpret_cast<glDeleteTexturesProc>(
      GetGLProcAddress("glDeleteTextures"));
  fn.glDeleteTransformFeedbacksFn = 0;
  fn.glDeleteVertexArraysOESFn = 0;
  fn.glDepthFuncFn =
      reinterpret_cast<glDepthFuncProc>(GetGLProcAddress("glDepthFunc"));
  fn.glDepthMaskFn =
      reinterpret_cast<glDepthMaskProc>(GetGLProcAddress("glDepthMask"));
  fn.glDepthRangeFn =
      reinterpret_cast<glDepthRangeProc>(GetGLProcAddress("glDepthRange"));
  fn.glDepthRangefFn = 0;
  fn.glDetachShaderFn =
      reinterpret_cast<glDetachShaderProc>(GetGLProcAddress("glDetachShader"));
  fn.glDisableFn =
      reinterpret_cast<glDisableProc>(GetGLProcAddress("glDisable"));
  fn.glDisableVertexAttribArrayFn =
      reinterpret_cast<glDisableVertexAttribArrayProc>(
          GetGLProcAddress("glDisableVertexAttribArray"));
  fn.glDiscardFramebufferEXTFn = 0;
  fn.glDrawArraysFn =
      reinterpret_cast<glDrawArraysProc>(GetGLProcAddress("glDrawArrays"));
  fn.glDrawArraysInstancedANGLEFn = 0;
  fn.glDrawBufferFn = 0;
  fn.glDrawBuffersARBFn = 0;
  fn.glDrawElementsFn =
      reinterpret_cast<glDrawElementsProc>(GetGLProcAddress("glDrawElements"));
  fn.glDrawElementsInstancedANGLEFn = 0;
  fn.glDrawRangeElementsFn = 0;
  fn.glEGLImageTargetRenderbufferStorageOESFn = 0;
  fn.glEGLImageTargetTexture2DOESFn = 0;
  fn.glEnableFn = reinterpret_cast<glEnableProc>(GetGLProcAddress("glEnable"));
  fn.glEnableVertexAttribArrayFn =
      reinterpret_cast<glEnableVertexAttribArrayProc>(
          GetGLProcAddress("glEnableVertexAttribArray"));
  fn.glEndQueryFn = 0;
  fn.glEndTransformFeedbackFn = 0;
  fn.glFenceSyncFn = 0;
  fn.glFinishFn = reinterpret_cast<glFinishProc>(GetGLProcAddress("glFinish"));
  fn.glFinishFenceAPPLEFn = 0;
  fn.glFinishFenceNVFn = 0;
  fn.glFlushFn = reinterpret_cast<glFlushProc>(GetGLProcAddress("glFlush"));
  fn.glFlushMappedBufferRangeFn = 0;
  fn.glFramebufferRenderbufferEXTFn = 0;
  fn.glFramebufferTexture2DEXTFn = 0;
  fn.glFramebufferTexture2DMultisampleEXTFn = 0;
  fn.glFramebufferTexture2DMultisampleIMGFn = 0;
  fn.glFramebufferTextureLayerFn = 0;
  fn.glFrontFaceFn =
      reinterpret_cast<glFrontFaceProc>(GetGLProcAddress("glFrontFace"));
  fn.glGenBuffersARBFn =
      reinterpret_cast<glGenBuffersARBProc>(GetGLProcAddress("glGenBuffers"));
  fn.glGenerateMipmapEXTFn = 0;
  fn.glGenFencesAPPLEFn = 0;
  fn.glGenFencesNVFn = 0;
  fn.glGenFramebuffersEXTFn = 0;
  fn.glGenPathsNVFn = 0;
  fn.glGenQueriesFn = 0;
  fn.glGenRenderbuffersEXTFn = 0;
  fn.glGenSamplersFn = 0;
  fn.glGenTexturesFn =
      reinterpret_cast<glGenTexturesProc>(GetGLProcAddress("glGenTextures"));
  fn.glGenTransformFeedbacksFn = 0;
  fn.glGenVertexArraysOESFn = 0;
  fn.glGetActiveAttribFn = reinterpret_cast<glGetActiveAttribProc>(
      GetGLProcAddress("glGetActiveAttrib"));
  fn.glGetActiveUniformFn = reinterpret_cast<glGetActiveUniformProc>(
      GetGLProcAddress("glGetActiveUniform"));
  fn.glGetActiveUniformBlockivFn = 0;
  fn.glGetActiveUniformBlockivRobustANGLEFn = 0;
  fn.glGetActiveUniformBlockNameFn = 0;
  fn.glGetActiveUniformsivFn = 0;
  fn.glGetAttachedShadersFn = reinterpret_cast<glGetAttachedShadersProc>(
      GetGLProcAddress("glGetAttachedShaders"));
  fn.glGetAttribLocationFn = reinterpret_cast<glGetAttribLocationProc>(
      GetGLProcAddress("glGetAttribLocation"));
  fn.glGetBooleani_vRobustANGLEFn = 0;
  fn.glGetBooleanvFn =
      reinterpret_cast<glGetBooleanvProc>(GetGLProcAddress("glGetBooleanv"));
  fn.glGetBooleanvRobustANGLEFn = 0;
  fn.glGetBufferParameteri64vRobustANGLEFn = 0;
  fn.glGetBufferParameterivFn = reinterpret_cast<glGetBufferParameterivProc>(
      GetGLProcAddress("glGetBufferParameteriv"));
  fn.glGetBufferParameterivRobustANGLEFn = 0;
  fn.glGetBufferPointervRobustANGLEFn = 0;
  fn.glGetDebugMessageLogFn = 0;
  fn.glGetErrorFn =
      reinterpret_cast<glGetErrorProc>(GetGLProcAddress("glGetError"));
  fn.glGetFenceivNVFn = 0;
  fn.glGetFloatvFn =
      reinterpret_cast<glGetFloatvProc>(GetGLProcAddress("glGetFloatv"));
  fn.glGetFloatvRobustANGLEFn = 0;
  fn.glGetFragDataIndexFn = 0;
  fn.glGetFragDataLocationFn = 0;
  fn.glGetFramebufferAttachmentParameterivEXTFn = 0;
  fn.glGetFramebufferAttachmentParameterivRobustANGLEFn = 0;
  fn.glGetFramebufferParameterivRobustANGLEFn = 0;
  fn.glGetGraphicsResetStatusARBFn = 0;
  fn.glGetInteger64i_vFn = 0;
  fn.glGetInteger64i_vRobustANGLEFn = 0;
  fn.glGetInteger64vFn = 0;
  fn.glGetInteger64vRobustANGLEFn = 0;
  fn.glGetIntegeri_vFn = 0;
  fn.glGetIntegeri_vRobustANGLEFn = 0;
  fn.glGetIntegervFn =
      reinterpret_cast<glGetIntegervProc>(GetGLProcAddress("glGetIntegerv"));
  fn.glGetIntegervRobustANGLEFn = 0;
  fn.glGetInternalformativFn = 0;
  fn.glGetInternalformativRobustANGLEFn = 0;
  fn.glGetMultisamplefvRobustANGLEFn = 0;
  fn.glGetnUniformfvRobustANGLEFn = 0;
  fn.glGetnUniformivRobustANGLEFn = 0;
  fn.glGetnUniformuivRobustANGLEFn = 0;
  fn.glGetObjectLabelFn = 0;
  fn.glGetObjectPtrLabelFn = 0;
  fn.glGetPointervFn = 0;
  fn.glGetPointervRobustANGLERobustANGLEFn = 0;
  fn.glGetProgramBinaryFn = 0;
  fn.glGetProgramInfoLogFn = reinterpret_cast<glGetProgramInfoLogProc>(
      GetGLProcAddress("glGetProgramInfoLog"));
  fn.glGetProgramInterfaceivFn = 0;
  fn.glGetProgramInterfaceivRobustANGLEFn = 0;
  fn.glGetProgramivFn =
      reinterpret_cast<glGetProgramivProc>(GetGLProcAddress("glGetProgramiv"));
  fn.glGetProgramivRobustANGLEFn = 0;
  fn.glGetProgramResourceivFn = 0;
  fn.glGetProgramResourceLocationFn = 0;
  fn.glGetProgramResourceNameFn = 0;
  fn.glGetQueryivFn = 0;
  fn.glGetQueryivRobustANGLEFn = 0;
  fn.glGetQueryObjecti64vFn = 0;
  fn.glGetQueryObjecti64vRobustANGLEFn = 0;
  fn.glGetQueryObjectivFn = 0;
  fn.glGetQueryObjectivRobustANGLEFn = 0;
  fn.glGetQueryObjectui64vFn = 0;
  fn.glGetQueryObjectui64vRobustANGLEFn = 0;
  fn.glGetQueryObjectuivFn = 0;
  fn.glGetQueryObjectuivRobustANGLEFn = 0;
  fn.glGetRenderbufferParameterivEXTFn = 0;
  fn.glGetRenderbufferParameterivRobustANGLEFn = 0;
  fn.glGetSamplerParameterfvFn = 0;
  fn.glGetSamplerParameterfvRobustANGLEFn = 0;
  fn.glGetSamplerParameterIivRobustANGLEFn = 0;
  fn.glGetSamplerParameterIuivRobustANGLEFn = 0;
  fn.glGetSamplerParameterivFn = 0;
  fn.glGetSamplerParameterivRobustANGLEFn = 0;
  fn.glGetShaderInfoLogFn = reinterpret_cast<glGetShaderInfoLogProc>(
      GetGLProcAddress("glGetShaderInfoLog"));
  fn.glGetShaderivFn =
      reinterpret_cast<glGetShaderivProc>(GetGLProcAddress("glGetShaderiv"));
  fn.glGetShaderivRobustANGLEFn = 0;
  fn.glGetShaderPrecisionFormatFn = 0;
  fn.glGetShaderSourceFn = reinterpret_cast<glGetShaderSourceProc>(
      GetGLProcAddress("glGetShaderSource"));
  fn.glGetStringFn =
      reinterpret_cast<glGetStringProc>(GetGLProcAddress("glGetString"));
  fn.glGetStringiFn =
      reinterpret_cast<glGetStringiProc>(GetGLProcAddress("glGetStringi"));
  fn.glGetSyncivFn = 0;
  fn.glGetTexLevelParameterfvFn = 0;
  fn.glGetTexLevelParameterfvRobustANGLEFn = 0;
  fn.glGetTexLevelParameterivFn = 0;
  fn.glGetTexLevelParameterivRobustANGLEFn = 0;
  fn.glGetTexParameterfvFn = reinterpret_cast<glGetTexParameterfvProc>(
      GetGLProcAddress("glGetTexParameterfv"));
  fn.glGetTexParameterfvRobustANGLEFn = 0;
  fn.glGetTexParameterIivRobustANGLEFn = 0;
  fn.glGetTexParameterIuivRobustANGLEFn = 0;
  fn.glGetTexParameterivFn = reinterpret_cast<glGetTexParameterivProc>(
      GetGLProcAddress("glGetTexParameteriv"));
  fn.glGetTexParameterivRobustANGLEFn = 0;
  fn.glGetTransformFeedbackVaryingFn = 0;
  fn.glGetTranslatedShaderSourceANGLEFn = 0;
  fn.glGetUniformBlockIndexFn = 0;
  fn.glGetUniformfvFn =
      reinterpret_cast<glGetUniformfvProc>(GetGLProcAddress("glGetUniformfv"));
  fn.glGetUniformfvRobustANGLEFn = 0;
  fn.glGetUniformIndicesFn = 0;
  fn.glGetUniformivFn =
      reinterpret_cast<glGetUniformivProc>(GetGLProcAddress("glGetUniformiv"));
  fn.glGetUniformivRobustANGLEFn = 0;
  fn.glGetUniformLocationFn = reinterpret_cast<glGetUniformLocationProc>(
      GetGLProcAddress("glGetUniformLocation"));
  fn.glGetUniformuivFn = 0;
  fn.glGetUniformuivRobustANGLEFn = 0;
  fn.glGetVertexAttribfvFn = reinterpret_cast<glGetVertexAttribfvProc>(
      GetGLProcAddress("glGetVertexAttribfv"));
  fn.glGetVertexAttribfvRobustANGLEFn = 0;
  fn.glGetVertexAttribIivRobustANGLEFn = 0;
  fn.glGetVertexAttribIuivRobustANGLEFn = 0;
  fn.glGetVertexAttribivFn = reinterpret_cast<glGetVertexAttribivProc>(
      GetGLProcAddress("glGetVertexAttribiv"));
  fn.glGetVertexAttribivRobustANGLEFn = 0;
  fn.glGetVertexAttribPointervFn =
      reinterpret_cast<glGetVertexAttribPointervProc>(
          GetGLProcAddress("glGetVertexAttribPointerv"));
  fn.glGetVertexAttribPointervRobustANGLEFn = 0;
  fn.glHintFn = reinterpret_cast<glHintProc>(GetGLProcAddress("glHint"));
  fn.glInsertEventMarkerEXTFn = 0;
  fn.glInvalidateFramebufferFn = 0;
  fn.glInvalidateSubFramebufferFn = 0;
  fn.glIsBufferFn =
      reinterpret_cast<glIsBufferProc>(GetGLProcAddress("glIsBuffer"));
  fn.glIsEnabledFn =
      reinterpret_cast<glIsEnabledProc>(GetGLProcAddress("glIsEnabled"));
  fn.glIsFenceAPPLEFn = 0;
  fn.glIsFenceNVFn = 0;
  fn.glIsFramebufferEXTFn = 0;
  fn.glIsPathNVFn = 0;
  fn.glIsProgramFn =
      reinterpret_cast<glIsProgramProc>(GetGLProcAddress("glIsProgram"));
  fn.glIsQueryFn = 0;
  fn.glIsRenderbufferEXTFn = 0;
  fn.glIsSamplerFn = 0;
  fn.glIsShaderFn =
      reinterpret_cast<glIsShaderProc>(GetGLProcAddress("glIsShader"));
  fn.glIsSyncFn = 0;
  fn.glIsTextureFn =
      reinterpret_cast<glIsTextureProc>(GetGLProcAddress("glIsTexture"));
  fn.glIsTransformFeedbackFn = 0;
  fn.glIsVertexArrayOESFn = 0;
  fn.glLineWidthFn =
      reinterpret_cast<glLineWidthProc>(GetGLProcAddress("glLineWidth"));
  fn.glLinkProgramFn =
      reinterpret_cast<glLinkProgramProc>(GetGLProcAddress("glLinkProgram"));
  fn.glMapBufferFn = 0;
  fn.glMapBufferRangeFn = 0;
  fn.glMatrixLoadfEXTFn = 0;
  fn.glMatrixLoadIdentityEXTFn = 0;
  fn.glMemoryBarrierEXTFn = 0;
  fn.glObjectLabelFn = 0;
  fn.glObjectPtrLabelFn = 0;
  fn.glPathCommandsNVFn = 0;
  fn.glPathParameterfNVFn = 0;
  fn.glPathParameteriNVFn = 0;
  fn.glPathStencilFuncNVFn = 0;
  fn.glPauseTransformFeedbackFn = 0;
  fn.glPixelStoreiFn =
      reinterpret_cast<glPixelStoreiProc>(GetGLProcAddress("glPixelStorei"));
  fn.glPointParameteriFn = 0;
  fn.glPolygonOffsetFn = reinterpret_cast<glPolygonOffsetProc>(
      GetGLProcAddress("glPolygonOffset"));
  fn.glPopDebugGroupFn = 0;
  fn.glPopGroupMarkerEXTFn = 0;
  fn.glPrimitiveRestartIndexFn = 0;
  fn.glProgramBinaryFn = 0;
  fn.glProgramParameteriFn = 0;
  fn.glProgramPathFragmentInputGenNVFn = 0;
  fn.glPushDebugGroupFn = 0;
  fn.glPushGroupMarkerEXTFn = 0;
  fn.glQueryCounterFn = 0;
  fn.glReadBufferFn = 0;
  fn.glReadnPixelsRobustANGLEFn = 0;
  fn.glReadPixelsFn =
      reinterpret_cast<glReadPixelsProc>(GetGLProcAddress("glReadPixels"));
  fn.glReadPixelsRobustANGLEFn = 0;
  fn.glReleaseShaderCompilerFn = 0;
  fn.glRenderbufferStorageEXTFn = 0;
  fn.glRenderbufferStorageMultisampleFn = 0;
  fn.glRenderbufferStorageMultisampleANGLEFn = 0;
  fn.glRenderbufferStorageMultisampleEXTFn = 0;
  fn.glRenderbufferStorageMultisampleIMGFn = 0;
  fn.glResumeTransformFeedbackFn = 0;
  fn.glSampleCoverageFn = reinterpret_cast<glSampleCoverageProc>(
      GetGLProcAddress("glSampleCoverage"));
  fn.glSamplerParameterfFn = 0;
  fn.glSamplerParameterfvFn = 0;
  fn.glSamplerParameterfvRobustANGLEFn = 0;
  fn.glSamplerParameteriFn = 0;
  fn.glSamplerParameterIivRobustANGLEFn = 0;
  fn.glSamplerParameterIuivRobustANGLEFn = 0;
  fn.glSamplerParameterivFn = 0;
  fn.glSamplerParameterivRobustANGLEFn = 0;
  fn.glScissorFn =
      reinterpret_cast<glScissorProc>(GetGLProcAddress("glScissor"));
  fn.glSetFenceAPPLEFn = 0;
  fn.glSetFenceNVFn = 0;
  fn.glShaderBinaryFn = 0;
  fn.glShaderSourceFn =
      reinterpret_cast<glShaderSourceProc>(GetGLProcAddress("glShaderSource"));
  fn.glStencilFillPathInstancedNVFn = 0;
  fn.glStencilFillPathNVFn = 0;
  fn.glStencilFuncFn =
      reinterpret_cast<glStencilFuncProc>(GetGLProcAddress("glStencilFunc"));
  fn.glStencilFuncSeparateFn = reinterpret_cast<glStencilFuncSeparateProc>(
      GetGLProcAddress("glStencilFuncSeparate"));
  fn.glStencilMaskFn =
      reinterpret_cast<glStencilMaskProc>(GetGLProcAddress("glStencilMask"));
  fn.glStencilMaskSeparateFn = reinterpret_cast<glStencilMaskSeparateProc>(
      GetGLProcAddress("glStencilMaskSeparate"));
  fn.glStencilOpFn =
      reinterpret_cast<glStencilOpProc>(GetGLProcAddress("glStencilOp"));
  fn.glStencilOpSeparateFn = reinterpret_cast<glStencilOpSeparateProc>(
      GetGLProcAddress("glStencilOpSeparate"));
  fn.glStencilStrokePathInstancedNVFn = 0;
  fn.glStencilStrokePathNVFn = 0;
  fn.glStencilThenCoverFillPathInstancedNVFn = 0;
  fn.glStencilThenCoverFillPathNVFn = 0;
  fn.glStencilThenCoverStrokePathInstancedNVFn = 0;
  fn.glStencilThenCoverStrokePathNVFn = 0;
  fn.glTestFenceAPPLEFn = 0;
  fn.glTestFenceNVFn = 0;
  fn.glTexImage2DFn =
      reinterpret_cast<glTexImage2DProc>(GetGLProcAddress("glTexImage2D"));
  fn.glTexImage2DRobustANGLEFn = 0;
  fn.glTexImage3DFn = 0;
  fn.glTexImage3DRobustANGLEFn = 0;
  fn.glTexParameterfFn = reinterpret_cast<glTexParameterfProc>(
      GetGLProcAddress("glTexParameterf"));
  fn.glTexParameterfvFn = reinterpret_cast<glTexParameterfvProc>(
      GetGLProcAddress("glTexParameterfv"));
  fn.glTexParameterfvRobustANGLEFn = 0;
  fn.glTexParameteriFn = reinterpret_cast<glTexParameteriProc>(
      GetGLProcAddress("glTexParameteri"));
  fn.glTexParameterIivRobustANGLEFn = 0;
  fn.glTexParameterIuivRobustANGLEFn = 0;
  fn.glTexParameterivFn = reinterpret_cast<glTexParameterivProc>(
      GetGLProcAddress("glTexParameteriv"));
  fn.glTexParameterivRobustANGLEFn = 0;
  fn.glTexStorage2DEXTFn = 0;
  fn.glTexStorage3DFn = 0;
  fn.glTexSubImage2DFn = reinterpret_cast<glTexSubImage2DProc>(
      GetGLProcAddress("glTexSubImage2D"));
  fn.glTexSubImage2DRobustANGLEFn = 0;
  fn.glTexSubImage3DFn = 0;
  fn.glTexSubImage3DRobustANGLEFn = 0;
  fn.glTransformFeedbackVaryingsFn = 0;
  fn.glUniform1fFn =
      reinterpret_cast<glUniform1fProc>(GetGLProcAddress("glUniform1f"));
  fn.glUniform1fvFn =
      reinterpret_cast<glUniform1fvProc>(GetGLProcAddress("glUniform1fv"));
  fn.glUniform1iFn =
      reinterpret_cast<glUniform1iProc>(GetGLProcAddress("glUniform1i"));
  fn.glUniform1ivFn =
      reinterpret_cast<glUniform1ivProc>(GetGLProcAddress("glUniform1iv"));
  fn.glUniform1uiFn = 0;
  fn.glUniform1uivFn = 0;
  fn.glUniform2fFn =
      reinterpret_cast<glUniform2fProc>(GetGLProcAddress("glUniform2f"));
  fn.glUniform2fvFn =
      reinterpret_cast<glUniform2fvProc>(GetGLProcAddress("glUniform2fv"));
  fn.glUniform2iFn =
      reinterpret_cast<glUniform2iProc>(GetGLProcAddress("glUniform2i"));
  fn.glUniform2ivFn =
      reinterpret_cast<glUniform2ivProc>(GetGLProcAddress("glUniform2iv"));
  fn.glUniform2uiFn = 0;
  fn.glUniform2uivFn = 0;
  fn.glUniform3fFn =
      reinterpret_cast<glUniform3fProc>(GetGLProcAddress("glUniform3f"));
  fn.glUniform3fvFn =
      reinterpret_cast<glUniform3fvProc>(GetGLProcAddress("glUniform3fv"));
  fn.glUniform3iFn =
      reinterpret_cast<glUniform3iProc>(GetGLProcAddress("glUniform3i"));
  fn.glUniform3ivFn =
      reinterpret_cast<glUniform3ivProc>(GetGLProcAddress("glUniform3iv"));
  fn.glUniform3uiFn = 0;
  fn.glUniform3uivFn = 0;
  fn.glUniform4fFn =
      reinterpret_cast<glUniform4fProc>(GetGLProcAddress("glUniform4f"));
  fn.glUniform4fvFn =
      reinterpret_cast<glUniform4fvProc>(GetGLProcAddress("glUniform4fv"));
  fn.glUniform4iFn =
      reinterpret_cast<glUniform4iProc>(GetGLProcAddress("glUniform4i"));
  fn.glUniform4ivFn =
      reinterpret_cast<glUniform4ivProc>(GetGLProcAddress("glUniform4iv"));
  fn.glUniform4uiFn = 0;
  fn.glUniform4uivFn = 0;
  fn.glUniformBlockBindingFn = 0;
  fn.glUniformMatrix2fvFn = reinterpret_cast<glUniformMatrix2fvProc>(
      GetGLProcAddress("glUniformMatrix2fv"));
  fn.glUniformMatrix2x3fvFn = 0;
  fn.glUniformMatrix2x4fvFn = 0;
  fn.glUniformMatrix3fvFn = reinterpret_cast<glUniformMatrix3fvProc>(
      GetGLProcAddress("glUniformMatrix3fv"));
  fn.glUniformMatrix3x2fvFn = 0;
  fn.glUniformMatrix3x4fvFn = 0;
  fn.glUniformMatrix4fvFn = reinterpret_cast<glUniformMatrix4fvProc>(
      GetGLProcAddress("glUniformMatrix4fv"));
  fn.glUniformMatrix4x2fvFn = 0;
  fn.glUniformMatrix4x3fvFn = 0;
  fn.glUnmapBufferFn = 0;
  fn.glUseProgramFn =
      reinterpret_cast<glUseProgramProc>(GetGLProcAddress("glUseProgram"));
  fn.glValidateProgramFn = reinterpret_cast<glValidateProgramProc>(
      GetGLProcAddress("glValidateProgram"));
  fn.glVertexAttrib1fFn = reinterpret_cast<glVertexAttrib1fProc>(
      GetGLProcAddress("glVertexAttrib1f"));
  fn.glVertexAttrib1fvFn = reinterpret_cast<glVertexAttrib1fvProc>(
      GetGLProcAddress("glVertexAttrib1fv"));
  fn.glVertexAttrib2fFn = reinterpret_cast<glVertexAttrib2fProc>(
      GetGLProcAddress("glVertexAttrib2f"));
  fn.glVertexAttrib2fvFn = reinterpret_cast<glVertexAttrib2fvProc>(
      GetGLProcAddress("glVertexAttrib2fv"));
  fn.glVertexAttrib3fFn = reinterpret_cast<glVertexAttrib3fProc>(
      GetGLProcAddress("glVertexAttrib3f"));
  fn.glVertexAttrib3fvFn = reinterpret_cast<glVertexAttrib3fvProc>(
      GetGLProcAddress("glVertexAttrib3fv"));
  fn.glVertexAttrib4fFn = reinterpret_cast<glVertexAttrib4fProc>(
      GetGLProcAddress("glVertexAttrib4f"));
  fn.glVertexAttrib4fvFn = reinterpret_cast<glVertexAttrib4fvProc>(
      GetGLProcAddress("glVertexAttrib4fv"));
  fn.glVertexAttribDivisorANGLEFn = 0;
  fn.glVertexAttribI4iFn = 0;
  fn.glVertexAttribI4ivFn = 0;
  fn.glVertexAttribI4uiFn = 0;
  fn.glVertexAttribI4uivFn = 0;
  fn.glVertexAttribIPointerFn = 0;
  fn.glVertexAttribPointerFn = reinterpret_cast<glVertexAttribPointerProc>(
      GetGLProcAddress("glVertexAttribPointer"));
  fn.glViewportFn =
      reinterpret_cast<glViewportProc>(GetGLProcAddress("glViewport"));
  fn.glWaitSyncFn = 0;
}

void DriverGL::InitializeDynamicBindings(GLContext* context) {
  DCHECK(context && context->IsCurrent(NULL));
  const GLVersionInfo* ver = context->GetVersionInfo();
  ALLOW_UNUSED_LOCAL(ver);
  std::string extensions = context->GetExtensions() + " ";
  ALLOW_UNUSED_LOCAL(extensions);

  ext.b_GL_ANGLE_framebuffer_blit =
      extensions.find("GL_ANGLE_framebuffer_blit ") != std::string::npos;
  ext.b_GL_ANGLE_framebuffer_multisample =
      extensions.find("GL_ANGLE_framebuffer_multisample ") != std::string::npos;
  ext.b_GL_ANGLE_instanced_arrays =
      extensions.find("GL_ANGLE_instanced_arrays ") != std::string::npos;
  ext.b_GL_ANGLE_robust_client_memory =
      extensions.find("GL_ANGLE_robust_client_memory ") != std::string::npos;
  ext.b_GL_ANGLE_translated_shader_source =
      extensions.find("GL_ANGLE_translated_shader_source ") !=
      std::string::npos;
  ext.b_GL_APPLE_fence =
      extensions.find("GL_APPLE_fence ") != std::string::npos;
  ext.b_GL_APPLE_vertex_array_object =
      extensions.find("GL_APPLE_vertex_array_object ") != std::string::npos;
  ext.b_GL_ARB_blend_func_extended =
      extensions.find("GL_ARB_blend_func_extended ") != std::string::npos;
  ext.b_GL_ARB_draw_buffers =
      extensions.find("GL_ARB_draw_buffers ") != std::string::npos;
  ext.b_GL_ARB_draw_instanced =
      extensions.find("GL_ARB_draw_instanced ") != std::string::npos;
  ext.b_GL_ARB_get_program_binary =
      extensions.find("GL_ARB_get_program_binary ") != std::string::npos;
  ext.b_GL_ARB_instanced_arrays =
      extensions.find("GL_ARB_instanced_arrays ") != std::string::npos;
  ext.b_GL_ARB_map_buffer_range =
      extensions.find("GL_ARB_map_buffer_range ") != std::string::npos;
  ext.b_GL_ARB_occlusion_query =
      extensions.find("GL_ARB_occlusion_query ") != std::string::npos;
  ext.b_GL_ARB_program_interface_query =
      extensions.find("GL_ARB_program_interface_query ") != std::string::npos;
  ext.b_GL_ARB_robustness =
      extensions.find("GL_ARB_robustness ") != std::string::npos;
  ext.b_GL_ARB_sampler_objects =
      extensions.find("GL_ARB_sampler_objects ") != std::string::npos;
  ext.b_GL_ARB_shader_image_load_store =
      extensions.find("GL_ARB_shader_image_load_store ") != std::string::npos;
  ext.b_GL_ARB_sync = extensions.find("GL_ARB_sync ") != std::string::npos;
  ext.b_GL_ARB_texture_storage =
      extensions.find("GL_ARB_texture_storage ") != std::string::npos;
  ext.b_GL_ARB_timer_query =
      extensions.find("GL_ARB_timer_query ") != std::string::npos;
  ext.b_GL_ARB_transform_feedback2 =
      extensions.find("GL_ARB_transform_feedback2 ") != std::string::npos;
  ext.b_GL_ARB_vertex_array_object =
      extensions.find("GL_ARB_vertex_array_object ") != std::string::npos;
  ext.b_GL_CHROMIUM_bind_uniform_location =
      extensions.find("GL_CHROMIUM_bind_uniform_location ") !=
      std::string::npos;
  ext.b_GL_CHROMIUM_compressed_copy_texture =
      extensions.find("GL_CHROMIUM_compressed_copy_texture ") !=
      std::string::npos;
  ext.b_GL_CHROMIUM_copy_compressed_texture =
      extensions.find("GL_CHROMIUM_copy_compressed_texture ") !=
      std::string::npos;
  ext.b_GL_CHROMIUM_copy_texture =
      extensions.find("GL_CHROMIUM_copy_texture ") != std::string::npos;
  ext.b_GL_CHROMIUM_gles_depth_binding_hack =
      extensions.find("GL_CHROMIUM_gles_depth_binding_hack ") !=
      std::string::npos;
  ext.b_GL_CHROMIUM_glgetstringi_hack =
      extensions.find("GL_CHROMIUM_glgetstringi_hack ") != std::string::npos;
  ext.b_GL_EXT_blend_func_extended =
      extensions.find("GL_EXT_blend_func_extended ") != std::string::npos;
  ext.b_GL_EXT_debug_marker =
      extensions.find("GL_EXT_debug_marker ") != std::string::npos;
  ext.b_GL_EXT_direct_state_access =
      extensions.find("GL_EXT_direct_state_access ") != std::string::npos;
  ext.b_GL_EXT_discard_framebuffer =
      extensions.find("GL_EXT_discard_framebuffer ") != std::string::npos;
  ext.b_GL_EXT_disjoint_timer_query =
      extensions.find("GL_EXT_disjoint_timer_query ") != std::string::npos;
  ext.b_GL_EXT_draw_buffers =
      extensions.find("GL_EXT_draw_buffers ") != std::string::npos;
  ext.b_GL_EXT_framebuffer_blit =
      extensions.find("GL_EXT_framebuffer_blit ") != std::string::npos;
  ext.b_GL_EXT_framebuffer_multisample =
      extensions.find("GL_EXT_framebuffer_multisample ") != std::string::npos;
  ext.b_GL_EXT_framebuffer_object =
      extensions.find("GL_EXT_framebuffer_object ") != std::string::npos;
  ext.b_GL_EXT_gpu_shader4 =
      extensions.find("GL_EXT_gpu_shader4 ") != std::string::npos;
  ext.b_GL_EXT_map_buffer_range =
      extensions.find("GL_EXT_map_buffer_range ") != std::string::npos;
  ext.b_GL_EXT_multisampled_render_to_texture =
      extensions.find("GL_EXT_multisampled_render_to_texture ") !=
      std::string::npos;
  ext.b_GL_EXT_occlusion_query_boolean =
      extensions.find("GL_EXT_occlusion_query_boolean ") != std::string::npos;
  ext.b_GL_EXT_robustness =
      extensions.find("GL_EXT_robustness ") != std::string::npos;
  ext.b_GL_EXT_shader_image_load_store =
      extensions.find("GL_EXT_shader_image_load_store ") != std::string::npos;
  ext.b_GL_EXT_texture_storage =
      extensions.find("GL_EXT_texture_storage ") != std::string::npos;
  ext.b_GL_EXT_timer_query =
      extensions.find("GL_EXT_timer_query ") != std::string::npos;
  ext.b_GL_EXT_transform_feedback =
      extensions.find("GL_EXT_transform_feedback ") != std::string::npos;
  ext.b_GL_EXT_unpack_subimage =
      extensions.find("GL_EXT_unpack_subimage ") != std::string::npos;
  ext.b_GL_IMG_multisampled_render_to_texture =
      extensions.find("GL_IMG_multisampled_render_to_texture ") !=
      std::string::npos;
  ext.b_GL_INTEL_framebuffer_CMAA =
      extensions.find("GL_INTEL_framebuffer_CMAA ") != std::string::npos;
  ext.b_GL_KHR_blend_equation_advanced =
      extensions.find("GL_KHR_blend_equation_advanced ") != std::string::npos;
  ext.b_GL_KHR_debug = extensions.find("GL_KHR_debug ") != std::string::npos;
  ext.b_GL_KHR_robustness =
      extensions.find("GL_KHR_robustness ") != std::string::npos;
  ext.b_GL_NV_blend_equation_advanced =
      extensions.find("GL_NV_blend_equation_advanced ") != std::string::npos;
  ext.b_GL_NV_fence = extensions.find("GL_NV_fence ") != std::string::npos;
  ext.b_GL_NV_framebuffer_mixed_samples =
      extensions.find("GL_NV_framebuffer_mixed_samples ") != std::string::npos;
  ext.b_GL_NV_path_rendering =
      extensions.find("GL_NV_path_rendering ") != std::string::npos;
  ext.b_GL_OES_EGL_image =
      extensions.find("GL_OES_EGL_image ") != std::string::npos;
  ext.b_GL_OES_get_program_binary =
      extensions.find("GL_OES_get_program_binary ") != std::string::npos;
  ext.b_GL_OES_mapbuffer =
      extensions.find("GL_OES_mapbuffer ") != std::string::npos;
  ext.b_GL_OES_vertex_array_object =
      extensions.find("GL_OES_vertex_array_object ") != std::string::npos;

  debug_fn.glApplyFramebufferAttachmentCMAAINTELFn = 0;
  if (ext.b_GL_INTEL_framebuffer_CMAA) {
    fn.glApplyFramebufferAttachmentCMAAINTELFn =
        reinterpret_cast<glApplyFramebufferAttachmentCMAAINTELProc>(
            GetGLProcAddress("glApplyFramebufferAttachmentCMAAINTEL"));
  }

  debug_fn.glBeginQueryFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glBeginQueryFn =
        reinterpret_cast<glBeginQueryProc>(GetGLProcAddress("glBeginQueryEXT"));
  }

  debug_fn.glBeginTransformFeedbackFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBeginTransformFeedbackFn =
        reinterpret_cast<glBeginTransformFeedbackProc>(
            GetGLProcAddress("glBeginTransformFeedbackEXT"));
  }

  debug_fn.glBindBufferBaseFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBase"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferBaseFn = reinterpret_cast<glBindBufferBaseProc>(
        GetGLProcAddress("glBindBufferBaseEXT"));
  }

  debug_fn.glBindBufferRangeFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRange"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glBindBufferRangeFn = reinterpret_cast<glBindBufferRangeProc>(
        GetGLProcAddress("glBindBufferRangeEXT"));
  }

  debug_fn.glBindFragDataLocationFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocation"));
  } else if (ext.b_GL_EXT_gpu_shader4 || ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationFn = reinterpret_cast<glBindFragDataLocationProc>(
        GetGLProcAddress("glBindFragDataLocationEXT"));
  }

  debug_fn.glBindFragDataLocationIndexedFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexed"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glBindFragDataLocationIndexedFn =
        reinterpret_cast<glBindFragDataLocationIndexedProc>(
            GetGLProcAddress("glBindFragDataLocationIndexedEXT"));
  }

  debug_fn.glBindFramebufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindFramebufferEXTFn = reinterpret_cast<glBindFramebufferEXTProc>(
        GetGLProcAddress("glBindFramebufferEXT"));
  }

  debug_fn.glBindImageTextureEXTFn = 0;
  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTexture"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glBindImageTextureEXTFn = reinterpret_cast<glBindImageTextureEXTProc>(
        GetGLProcAddress("glBindImageTextureEXT"));
  }

  debug_fn.glBindRenderbufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glBindRenderbufferEXTFn = reinterpret_cast<glBindRenderbufferEXTProc>(
        GetGLProcAddress("glBindRenderbufferEXT"));
  }

  debug_fn.glBindSamplerFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glBindSamplerFn =
        reinterpret_cast<glBindSamplerProc>(GetGLProcAddress("glBindSampler"));
  }

  debug_fn.glBindTransformFeedbackFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glBindTransformFeedbackFn =
        reinterpret_cast<glBindTransformFeedbackProc>(
            GetGLProcAddress("glBindTransformFeedback"));
  }

  debug_fn.glBindUniformLocationCHROMIUMFn = 0;
  if (ext.b_GL_CHROMIUM_bind_uniform_location) {
    fn.glBindUniformLocationCHROMIUMFn =
        reinterpret_cast<glBindUniformLocationCHROMIUMProc>(
            GetGLProcAddress("glBindUniformLocationCHROMIUM"));
  }

  debug_fn.glBindVertexArrayOESFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glBindVertexArrayOESFn = reinterpret_cast<glBindVertexArrayOESProc>(
        GetGLProcAddress("glBindVertexArrayAPPLE"));
  }

  debug_fn.glBlendBarrierKHRFn = 0;
  if (ext.b_GL_NV_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierNV"));
  } else if (ext.b_GL_KHR_blend_equation_advanced) {
    fn.glBlendBarrierKHRFn = reinterpret_cast<glBlendBarrierKHRProc>(
        GetGLProcAddress("glBlendBarrierKHR"));
  }

  debug_fn.glBlitFramebufferFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBlitFramebufferFn = reinterpret_cast<glBlitFramebufferProc>(
        GetGLProcAddress("glBlitFramebuffer"));
  }

  debug_fn.glBlitFramebufferANGLEFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBlitFramebufferANGLEFn = reinterpret_cast<glBlitFramebufferANGLEProc>(
        GetGLProcAddress("glBlitFramebuffer"));
  } else if (ext.b_GL_ANGLE_framebuffer_blit) {
    fn.glBlitFramebufferANGLEFn = reinterpret_cast<glBlitFramebufferANGLEProc>(
        GetGLProcAddress("glBlitFramebufferANGLE"));
  }

  debug_fn.glBlitFramebufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glBlitFramebufferEXTFn = reinterpret_cast<glBlitFramebufferEXTProc>(
        GetGLProcAddress("glBlitFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_blit) {
    fn.glBlitFramebufferEXTFn = reinterpret_cast<glBlitFramebufferEXTProc>(
        GetGLProcAddress("glBlitFramebufferEXT"));
  }

  debug_fn.glCheckFramebufferStatusEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatus"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glCheckFramebufferStatusEXTFn =
        reinterpret_cast<glCheckFramebufferStatusEXTProc>(
            GetGLProcAddress("glCheckFramebufferStatusEXT"));
  }

  debug_fn.glClearBufferfiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfiFn = reinterpret_cast<glClearBufferfiProc>(
        GetGLProcAddress("glClearBufferfi"));
  }

  debug_fn.glClearBufferfvFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferfvFn = reinterpret_cast<glClearBufferfvProc>(
        GetGLProcAddress("glClearBufferfv"));
  }

  debug_fn.glClearBufferivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferivFn = reinterpret_cast<glClearBufferivProc>(
        GetGLProcAddress("glClearBufferiv"));
  }

  debug_fn.glClearBufferuivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glClearBufferuivFn = reinterpret_cast<glClearBufferuivProc>(
        GetGLProcAddress("glClearBufferuiv"));
  }

  debug_fn.glClearDepthfFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glClearDepthfFn =
        reinterpret_cast<glClearDepthfProc>(GetGLProcAddress("glClearDepthf"));
  }

  debug_fn.glClientWaitSyncFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glClientWaitSyncFn = reinterpret_cast<glClientWaitSyncProc>(
        GetGLProcAddress("glClientWaitSync"));
  }

  debug_fn.glCompressedCopyTextureCHROMIUMFn = 0;
  if (ext.b_GL_CHROMIUM_copy_compressed_texture ||
      ext.b_GL_CHROMIUM_compressed_copy_texture) {
    fn.glCompressedCopyTextureCHROMIUMFn =
        reinterpret_cast<glCompressedCopyTextureCHROMIUMProc>(
            GetGLProcAddress("glCompressedCopyTextureCHROMIUM"));
  }

  debug_fn.glCompressedTexImage3DFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexImage3DFn = reinterpret_cast<glCompressedTexImage3DProc>(
        GetGLProcAddress("glCompressedTexImage3D"));
  }

  debug_fn.glCompressedTexSubImage3DFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCompressedTexSubImage3DFn =
        reinterpret_cast<glCompressedTexSubImage3DProc>(
            GetGLProcAddress("glCompressedTexSubImage3D"));
  }

  debug_fn.glCopyBufferSubDataFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glCopyBufferSubDataFn = reinterpret_cast<glCopyBufferSubDataProc>(
        GetGLProcAddress("glCopyBufferSubData"));
  }

  debug_fn.glCopySubTextureCHROMIUMFn = 0;
  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopySubTextureCHROMIUMFn =
        reinterpret_cast<glCopySubTextureCHROMIUMProc>(
            GetGLProcAddress("glCopySubTextureCHROMIUM"));
  }

  debug_fn.glCopyTexSubImage3DFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glCopyTexSubImage3DFn = reinterpret_cast<glCopyTexSubImage3DProc>(
        GetGLProcAddress("glCopyTexSubImage3D"));
  }

  debug_fn.glCopyTextureCHROMIUMFn = 0;
  if (ext.b_GL_CHROMIUM_copy_texture) {
    fn.glCopyTextureCHROMIUMFn = reinterpret_cast<glCopyTextureCHROMIUMProc>(
        GetGLProcAddress("glCopyTextureCHROMIUM"));
  }

  debug_fn.glCoverageModulationNVFn = 0;
  if (ext.b_GL_NV_framebuffer_mixed_samples) {
    fn.glCoverageModulationNVFn = reinterpret_cast<glCoverageModulationNVProc>(
        GetGLProcAddress("glCoverageModulationNV"));
  }

  debug_fn.glCoverFillPathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathInstancedNVFn =
        reinterpret_cast<glCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glCoverFillPathInstancedNV"));
  }

  debug_fn.glCoverFillPathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverFillPathNVFn = reinterpret_cast<glCoverFillPathNVProc>(
        GetGLProcAddress("glCoverFillPathNV"));
  }

  debug_fn.glCoverStrokePathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathInstancedNVFn =
        reinterpret_cast<glCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glCoverStrokePathInstancedNV"));
  }

  debug_fn.glCoverStrokePathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glCoverStrokePathNVFn = reinterpret_cast<glCoverStrokePathNVProc>(
        GetGLProcAddress("glCoverStrokePathNV"));
  }

  debug_fn.glDebugMessageCallbackFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallback"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageCallbackFn = reinterpret_cast<glDebugMessageCallbackProc>(
        GetGLProcAddress("glDebugMessageCallbackKHR"));
  }

  debug_fn.glDebugMessageControlFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControl"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageControlFn = reinterpret_cast<glDebugMessageControlProc>(
        GetGLProcAddress("glDebugMessageControlKHR"));
  }

  debug_fn.glDebugMessageInsertFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsert"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glDebugMessageInsertFn = reinterpret_cast<glDebugMessageInsertProc>(
        GetGLProcAddress("glDebugMessageInsertKHR"));
  }

  debug_fn.glDeleteFencesAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glDeleteFencesAPPLEFn = reinterpret_cast<glDeleteFencesAPPLEProc>(
        GetGLProcAddress("glDeleteFencesAPPLE"));
  }

  debug_fn.glDeleteFencesNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glDeleteFencesNVFn = reinterpret_cast<glDeleteFencesNVProc>(
        GetGLProcAddress("glDeleteFencesNV"));
  }

  debug_fn.glDeleteFramebuffersEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteFramebuffersEXTFn =
        reinterpret_cast<glDeleteFramebuffersEXTProc>(
            GetGLProcAddress("glDeleteFramebuffersEXT"));
  }

  debug_fn.glDeletePathsNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glDeletePathsNVFn = reinterpret_cast<glDeletePathsNVProc>(
        GetGLProcAddress("glDeletePathsNV"));
  }

  debug_fn.glDeleteQueriesFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glDeleteQueriesFn = reinterpret_cast<glDeleteQueriesProc>(
        GetGLProcAddress("glDeleteQueriesEXT"));
  }

  debug_fn.glDeleteRenderbuffersEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glDeleteRenderbuffersEXTFn =
        reinterpret_cast<glDeleteRenderbuffersEXTProc>(
            GetGLProcAddress("glDeleteRenderbuffersEXT"));
  }

  debug_fn.glDeleteSamplersFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glDeleteSamplersFn = reinterpret_cast<glDeleteSamplersProc>(
        GetGLProcAddress("glDeleteSamplers"));
  }

  debug_fn.glDeleteSyncFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glDeleteSyncFn =
        reinterpret_cast<glDeleteSyncProc>(GetGLProcAddress("glDeleteSync"));
  }

  debug_fn.glDeleteTransformFeedbacksFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glDeleteTransformFeedbacksFn =
        reinterpret_cast<glDeleteTransformFeedbacksProc>(
            GetGLProcAddress("glDeleteTransformFeedbacks"));
  }

  debug_fn.glDeleteVertexArraysOESFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glDeleteVertexArraysOESFn =
        reinterpret_cast<glDeleteVertexArraysOESProc>(
            GetGLProcAddress("glDeleteVertexArraysAPPLE"));
  }

  debug_fn.glDepthRangefFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glDepthRangefFn =
        reinterpret_cast<glDepthRangefProc>(GetGLProcAddress("glDepthRangef"));
  }

  debug_fn.glDiscardFramebufferEXTFn = 0;
  if (ext.b_GL_EXT_discard_framebuffer) {
    fn.glDiscardFramebufferEXTFn =
        reinterpret_cast<glDiscardFramebufferEXTProc>(
            GetGLProcAddress("glDiscardFramebufferEXT"));
  }

  debug_fn.glDrawArraysInstancedANGLEFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawArraysInstancedANGLEFn =
        reinterpret_cast<glDrawArraysInstancedANGLEProc>(
            GetGLProcAddress("glDrawArraysInstancedANGLE"));
  }

  debug_fn.glDrawBufferFn = 0;
  if (!ver->is_es) {
    fn.glDrawBufferFn =
        reinterpret_cast<glDrawBufferProc>(GetGLProcAddress("glDrawBuffer"));
  }

  debug_fn.glDrawBuffersARBFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffers"));
  } else if (ext.b_GL_ARB_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersARB"));
  } else if (ext.b_GL_EXT_draw_buffers) {
    fn.glDrawBuffersARBFn = reinterpret_cast<glDrawBuffersARBProc>(
        GetGLProcAddress("glDrawBuffersEXT"));
  }

  debug_fn.glDrawElementsInstancedANGLEFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstanced"));
  } else if (ext.b_GL_ARB_draw_instanced) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glDrawElementsInstancedANGLEFn =
        reinterpret_cast<glDrawElementsInstancedANGLEProc>(
            GetGLProcAddress("glDrawElementsInstancedANGLE"));
  }

  debug_fn.glDrawRangeElementsFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glDrawRangeElementsFn = reinterpret_cast<glDrawRangeElementsProc>(
        GetGLProcAddress("glDrawRangeElements"));
  }

  debug_fn.glEGLImageTargetRenderbufferStorageOESFn = 0;
  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetRenderbufferStorageOESFn =
        reinterpret_cast<glEGLImageTargetRenderbufferStorageOESProc>(
            GetGLProcAddress("glEGLImageTargetRenderbufferStorageOES"));
  }

  debug_fn.glEGLImageTargetTexture2DOESFn = 0;
  if (ext.b_GL_OES_EGL_image) {
    fn.glEGLImageTargetTexture2DOESFn =
        reinterpret_cast<glEGLImageTargetTexture2DOESProc>(
            GetGLProcAddress("glEGLImageTargetTexture2DOES"));
  }

  debug_fn.glEndQueryFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glEndQueryFn =
        reinterpret_cast<glEndQueryProc>(GetGLProcAddress("glEndQueryEXT"));
  }

  debug_fn.glEndTransformFeedbackFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedback"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glEndTransformFeedbackFn = reinterpret_cast<glEndTransformFeedbackProc>(
        GetGLProcAddress("glEndTransformFeedbackEXT"));
  }

  debug_fn.glFenceSyncFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glFenceSyncFn =
        reinterpret_cast<glFenceSyncProc>(GetGLProcAddress("glFenceSync"));
  }

  debug_fn.glFinishFenceAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glFinishFenceAPPLEFn = reinterpret_cast<glFinishFenceAPPLEProc>(
        GetGLProcAddress("glFinishFenceAPPLE"));
  }

  debug_fn.glFinishFenceNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glFinishFenceNVFn = reinterpret_cast<glFinishFenceNVProc>(
        GetGLProcAddress("glFinishFenceNV"));
  }

  debug_fn.glFlushMappedBufferRangeFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glFlushMappedBufferRangeFn =
        reinterpret_cast<glFlushMappedBufferRangeProc>(
            GetGLProcAddress("glFlushMappedBufferRange"));
  }

  debug_fn.glFramebufferRenderbufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferRenderbufferEXTFn =
        reinterpret_cast<glFramebufferRenderbufferEXTProc>(
            GetGLProcAddress("glFramebufferRenderbufferEXT"));
  }

  debug_fn.glFramebufferTexture2DEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2D"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glFramebufferTexture2DEXTFn =
        reinterpret_cast<glFramebufferTexture2DEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DEXT"));
  }

  debug_fn.glFramebufferTexture2DMultisampleEXTFn = 0;
  if (ext.b_GL_EXT_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleEXTFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleEXTProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleEXT"));
  }

  debug_fn.glFramebufferTexture2DMultisampleIMGFn = 0;
  if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glFramebufferTexture2DMultisampleIMGFn =
        reinterpret_cast<glFramebufferTexture2DMultisampleIMGProc>(
            GetGLProcAddress("glFramebufferTexture2DMultisampleIMG"));
  }

  debug_fn.glFramebufferTextureLayerFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glFramebufferTextureLayerFn =
        reinterpret_cast<glFramebufferTextureLayerProc>(
            GetGLProcAddress("glFramebufferTextureLayer"));
  }

  debug_fn.glGenerateMipmapEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmap"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenerateMipmapEXTFn = reinterpret_cast<glGenerateMipmapEXTProc>(
        GetGLProcAddress("glGenerateMipmapEXT"));
  }

  debug_fn.glGenFencesAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glGenFencesAPPLEFn = reinterpret_cast<glGenFencesAPPLEProc>(
        GetGLProcAddress("glGenFencesAPPLE"));
  }

  debug_fn.glGenFencesNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glGenFencesNVFn =
        reinterpret_cast<glGenFencesNVProc>(GetGLProcAddress("glGenFencesNV"));
  }

  debug_fn.glGenFramebuffersEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenFramebuffersEXTFn = reinterpret_cast<glGenFramebuffersEXTProc>(
        GetGLProcAddress("glGenFramebuffersEXT"));
  }

  debug_fn.glGenPathsNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glGenPathsNVFn =
        reinterpret_cast<glGenPathsNVProc>(GetGLProcAddress("glGenPathsNV"));
  }

  debug_fn.glGenQueriesFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueries"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGenQueriesFn =
        reinterpret_cast<glGenQueriesProc>(GetGLProcAddress("glGenQueriesEXT"));
  }

  debug_fn.glGenRenderbuffersEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffers"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGenRenderbuffersEXTFn = reinterpret_cast<glGenRenderbuffersEXTProc>(
        GetGLProcAddress("glGenRenderbuffersEXT"));
  }

  debug_fn.glGenSamplersFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGenSamplersFn =
        reinterpret_cast<glGenSamplersProc>(GetGLProcAddress("glGenSamplers"));
  }

  debug_fn.glGenTransformFeedbacksFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glGenTransformFeedbacksFn =
        reinterpret_cast<glGenTransformFeedbacksProc>(
            GetGLProcAddress("glGenTransformFeedbacks"));
  }

  debug_fn.glGenVertexArraysOESFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArrays"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glGenVertexArraysOESFn = reinterpret_cast<glGenVertexArraysOESProc>(
        GetGLProcAddress("glGenVertexArraysAPPLE"));
  }

  debug_fn.glGetActiveUniformBlockivFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockivFn =
        reinterpret_cast<glGetActiveUniformBlockivProc>(
            GetGLProcAddress("glGetActiveUniformBlockiv"));
  }

  debug_fn.glGetActiveUniformBlockivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetActiveUniformBlockivRobustANGLEFn =
        reinterpret_cast<glGetActiveUniformBlockivRobustANGLEProc>(
            GetGLProcAddress("glGetActiveUniformBlockivRobustANGLE"));
  }

  debug_fn.glGetActiveUniformBlockNameFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformBlockNameFn =
        reinterpret_cast<glGetActiveUniformBlockNameProc>(
            GetGLProcAddress("glGetActiveUniformBlockName"));
  }

  debug_fn.glGetActiveUniformsivFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetActiveUniformsivFn = reinterpret_cast<glGetActiveUniformsivProc>(
        GetGLProcAddress("glGetActiveUniformsiv"));
  }

  debug_fn.glGetBooleani_vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleani_vRobustANGLEFn =
        reinterpret_cast<glGetBooleani_vRobustANGLEProc>(
            GetGLProcAddress("glGetBooleani_vRobustANGLE"));
  }

  debug_fn.glGetBooleanvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBooleanvRobustANGLEFn =
        reinterpret_cast<glGetBooleanvRobustANGLEProc>(
            GetGLProcAddress("glGetBooleanvRobustANGLE"));
  }

  debug_fn.glGetBufferParameteri64vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameteri64vRobustANGLEFn =
        reinterpret_cast<glGetBufferParameteri64vRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameteri64vRobustANGLE"));
  }

  debug_fn.glGetBufferParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetBufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetBufferParameterivRobustANGLE"));
  }

  debug_fn.glGetBufferPointervRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetBufferPointervRobustANGLEFn =
        reinterpret_cast<glGetBufferPointervRobustANGLEProc>(
            GetGLProcAddress("glGetBufferPointervRobustANGLE"));
  }

  debug_fn.glGetDebugMessageLogFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLog"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetDebugMessageLogFn = reinterpret_cast<glGetDebugMessageLogProc>(
        GetGLProcAddress("glGetDebugMessageLogKHR"));
  }

  debug_fn.glGetFenceivNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glGetFenceivNVFn = reinterpret_cast<glGetFenceivNVProc>(
        GetGLProcAddress("glGetFenceivNV"));
  }

  debug_fn.glGetFloatvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFloatvRobustANGLEFn = reinterpret_cast<glGetFloatvRobustANGLEProc>(
        GetGLProcAddress("glGetFloatvRobustANGLE"));
  }

  debug_fn.glGetFragDataIndexFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndex"));
  } else if (ext.b_GL_EXT_blend_func_extended) {
    fn.glGetFragDataIndexFn = reinterpret_cast<glGetFragDataIndexProc>(
        GetGLProcAddress("glGetFragDataIndexEXT"));
  }

  debug_fn.glGetFragDataLocationFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetFragDataLocationFn = reinterpret_cast<glGetFragDataLocationProc>(
        GetGLProcAddress("glGetFragDataLocation"));
  }

  debug_fn.glGetFramebufferAttachmentParameterivEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivEXTProc>(
            GetGLProcAddress("glGetFramebufferAttachmentParameterivEXT"));
  }

  debug_fn.glGetFramebufferAttachmentParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferAttachmentParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferAttachmentParameterivRobustANGLEProc>(
            GetGLProcAddress(
                "glGetFramebufferAttachmentParameterivRobustANGLE"));
  }

  debug_fn.glGetFramebufferParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetFramebufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetFramebufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetFramebufferParameterivRobustANGLE"));
  }

  debug_fn.glGetGraphicsResetStatusARBFn = 0;
  if (ver->IsAtLeastGL(4u, 5u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatus"));
  } else if (ext.b_GL_ARB_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusARB"));
  } else if (ext.b_GL_KHR_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusKHR"));
  } else if (ext.b_GL_EXT_robustness) {
    fn.glGetGraphicsResetStatusARBFn =
        reinterpret_cast<glGetGraphicsResetStatusARBProc>(
            GetGLProcAddress("glGetGraphicsResetStatusEXT"));
  }

  debug_fn.glGetInteger64i_vFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64i_vFn = reinterpret_cast<glGetInteger64i_vProc>(
        GetGLProcAddress("glGetInteger64i_v"));
  }

  debug_fn.glGetInteger64i_vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64i_vRobustANGLEFn =
        reinterpret_cast<glGetInteger64i_vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64i_vRobustANGLE"));
  }

  debug_fn.glGetInteger64vFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInteger64vFn = reinterpret_cast<glGetInteger64vProc>(
        GetGLProcAddress("glGetInteger64v"));
  }

  debug_fn.glGetInteger64vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInteger64vRobustANGLEFn =
        reinterpret_cast<glGetInteger64vRobustANGLEProc>(
            GetGLProcAddress("glGetInteger64vRobustANGLE"));
  }

  debug_fn.glGetIntegeri_vFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetIntegeri_vFn = reinterpret_cast<glGetIntegeri_vProc>(
        GetGLProcAddress("glGetIntegeri_v"));
  }

  debug_fn.glGetIntegeri_vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegeri_vRobustANGLEFn =
        reinterpret_cast<glGetIntegeri_vRobustANGLEProc>(
            GetGLProcAddress("glGetIntegeri_vRobustANGLE"));
  }

  debug_fn.glGetIntegervRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetIntegervRobustANGLEFn =
        reinterpret_cast<glGetIntegervRobustANGLEProc>(
            GetGLProcAddress("glGetIntegervRobustANGLE"));
  }

  debug_fn.glGetInternalformativFn = 0;
  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetInternalformativFn = reinterpret_cast<glGetInternalformativProc>(
        GetGLProcAddress("glGetInternalformativ"));
  }

  debug_fn.glGetInternalformativRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetInternalformativRobustANGLEFn =
        reinterpret_cast<glGetInternalformativRobustANGLEProc>(
            GetGLProcAddress("glGetInternalformativRobustANGLE"));
  }

  debug_fn.glGetMultisamplefvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetMultisamplefvRobustANGLEFn =
        reinterpret_cast<glGetMultisamplefvRobustANGLEProc>(
            GetGLProcAddress("glGetMultisamplefvRobustANGLE"));
  }

  debug_fn.glGetnUniformfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformfvRobustANGLEFn =
        reinterpret_cast<glGetnUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformfvRobustANGLE"));
  }

  debug_fn.glGetnUniformivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformivRobustANGLEFn =
        reinterpret_cast<glGetnUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformivRobustANGLE"));
  }

  debug_fn.glGetnUniformuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetnUniformuivRobustANGLEFn =
        reinterpret_cast<glGetnUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetnUniformuivRobustANGLE"));
  }

  debug_fn.glGetObjectLabelFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectLabelFn = reinterpret_cast<glGetObjectLabelProc>(
        GetGLProcAddress("glGetObjectLabelKHR"));
  }

  debug_fn.glGetObjectPtrLabelFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetObjectPtrLabelFn = reinterpret_cast<glGetObjectPtrLabelProc>(
        GetGLProcAddress("glGetObjectPtrLabelKHR"));
  }

  debug_fn.glGetPointervFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glGetPointervFn =
        reinterpret_cast<glGetPointervProc>(GetGLProcAddress("glGetPointerv"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glGetPointervFn = reinterpret_cast<glGetPointervProc>(
        GetGLProcAddress("glGetPointervKHR"));
  }

  debug_fn.glGetPointervRobustANGLERobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetPointervRobustANGLERobustANGLEFn =
        reinterpret_cast<glGetPointervRobustANGLERobustANGLEProc>(
            GetGLProcAddress("glGetPointervRobustANGLERobustANGLE"));
  }

  debug_fn.glGetProgramBinaryFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glGetProgramBinaryFn = reinterpret_cast<glGetProgramBinaryProc>(
        GetGLProcAddress("glGetProgramBinaryOES"));
  }

  debug_fn.glGetProgramInterfaceivFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramInterfaceivFn =
        reinterpret_cast<glGetProgramInterfaceivProc>(
            GetGLProcAddress("glGetProgramInterfaceiv"));
  }

  debug_fn.glGetProgramInterfaceivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramInterfaceivRobustANGLEFn =
        reinterpret_cast<glGetProgramInterfaceivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramInterfaceivRobustANGLE"));
  }

  debug_fn.glGetProgramivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetProgramivRobustANGLEFn =
        reinterpret_cast<glGetProgramivRobustANGLEProc>(
            GetGLProcAddress("glGetProgramivRobustANGLE"));
  }

  debug_fn.glGetProgramResourceivFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceivFn = reinterpret_cast<glGetProgramResourceivProc>(
        GetGLProcAddress("glGetProgramResourceiv"));
  }

  debug_fn.glGetProgramResourceLocationFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetProgramResourceLocationFn =
        reinterpret_cast<glGetProgramResourceLocationProc>(
            GetGLProcAddress("glGetProgramResourceLocation"));
  }

  debug_fn.glGetProgramResourceNameFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_program_interface_query) {
    fn.glGetProgramResourceNameFn =
        reinterpret_cast<glGetProgramResourceNameProc>(
            GetGLProcAddress("glGetProgramResourceName"));
  }

  debug_fn.glGetQueryivFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryivFn =
        reinterpret_cast<glGetQueryivProc>(GetGLProcAddress("glGetQueryivEXT"));
  }

  debug_fn.glGetQueryivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryivRobustANGLEFn =
        reinterpret_cast<glGetQueryivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryivRobustANGLE"));
  }

  debug_fn.glGetQueryObjecti64vFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjecti64vFn = reinterpret_cast<glGetQueryObjecti64vProc>(
        GetGLProcAddress("glGetQueryObjecti64vEXT"));
  }

  debug_fn.glGetQueryObjecti64vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjecti64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjecti64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjecti64vRobustANGLE"));
  }

  debug_fn.glGetQueryObjectivFn = 0;
  if (!ver->is_es) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectivFn = reinterpret_cast<glGetQueryObjectivProc>(
        GetGLProcAddress("glGetQueryObjectivEXT"));
  }

  debug_fn.glGetQueryObjectivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectivRobustANGLE"));
  }

  debug_fn.glGetQueryObjectui64vFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64v"));
  } else if (ext.b_GL_EXT_timer_query || ext.b_GL_EXT_disjoint_timer_query) {
    fn.glGetQueryObjectui64vFn = reinterpret_cast<glGetQueryObjectui64vProc>(
        GetGLProcAddress("glGetQueryObjectui64vEXT"));
  }

  debug_fn.glGetQueryObjectui64vRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectui64vRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectui64vRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectui64vRobustANGLE"));
  }

  debug_fn.glGetQueryObjectuivFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuiv"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glGetQueryObjectuivFn = reinterpret_cast<glGetQueryObjectuivProc>(
        GetGLProcAddress("glGetQueryObjectuivEXT"));
  }

  debug_fn.glGetQueryObjectuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetQueryObjectuivRobustANGLEFn =
        reinterpret_cast<glGetQueryObjectuivRobustANGLEProc>(
            GetGLProcAddress("glGetQueryObjectuivRobustANGLE"));
  }

  debug_fn.glGetRenderbufferParameterivEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameteriv"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glGetRenderbufferParameterivEXTFn =
        reinterpret_cast<glGetRenderbufferParameterivEXTProc>(
            GetGLProcAddress("glGetRenderbufferParameterivEXT"));
  }

  debug_fn.glGetRenderbufferParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetRenderbufferParameterivRobustANGLEFn =
        reinterpret_cast<glGetRenderbufferParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetRenderbufferParameterivRobustANGLE"));
  }

  debug_fn.glGetSamplerParameterfvFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterfvFn =
        reinterpret_cast<glGetSamplerParameterfvProc>(
            GetGLProcAddress("glGetSamplerParameterfv"));
  }

  debug_fn.glGetSamplerParameterfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterfvRobustANGLE"));
  }

  debug_fn.glGetSamplerParameterIivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIivRobustANGLE"));
  }

  debug_fn.glGetSamplerParameterIuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterIuivRobustANGLE"));
  }

  debug_fn.glGetSamplerParameterivFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glGetSamplerParameterivFn =
        reinterpret_cast<glGetSamplerParameterivProc>(
            GetGLProcAddress("glGetSamplerParameteriv"));
  }

  debug_fn.glGetSamplerParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glGetSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetSamplerParameterivRobustANGLE"));
  }

  debug_fn.glGetShaderivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetShaderivRobustANGLEFn =
        reinterpret_cast<glGetShaderivRobustANGLEProc>(
            GetGLProcAddress("glGetShaderivRobustANGLE"));
  }

  debug_fn.glGetShaderPrecisionFormatFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glGetShaderPrecisionFormatFn =
        reinterpret_cast<glGetShaderPrecisionFormatProc>(
            GetGLProcAddress("glGetShaderPrecisionFormat"));
  }

  debug_fn.glGetSyncivFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glGetSyncivFn =
        reinterpret_cast<glGetSyncivProc>(GetGLProcAddress("glGetSynciv"));
  }

  debug_fn.glGetTexLevelParameterfvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterfvFn =
        reinterpret_cast<glGetTexLevelParameterfvProc>(
            GetGLProcAddress("glGetTexLevelParameterfv"));
  }

  debug_fn.glGetTexLevelParameterfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterfvRobustANGLE"));
  }

  debug_fn.glGetTexLevelParameterivFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 1u)) {
    fn.glGetTexLevelParameterivFn =
        reinterpret_cast<glGetTexLevelParameterivProc>(
            GetGLProcAddress("glGetTexLevelParameteriv"));
  }

  debug_fn.glGetTexLevelParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexLevelParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexLevelParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexLevelParameterivRobustANGLE"));
  }

  debug_fn.glGetTexParameterfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterfvRobustANGLEFn =
        reinterpret_cast<glGetTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterfvRobustANGLE"));
  }

  debug_fn.glGetTexParameterIivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIivRobustANGLE"));
  }

  debug_fn.glGetTexParameterIuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterIuivRobustANGLE"));
  }

  debug_fn.glGetTexParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetTexParameterivRobustANGLEFn =
        reinterpret_cast<glGetTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glGetTexParameterivRobustANGLE"));
  }

  debug_fn.glGetTransformFeedbackVaryingFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVarying"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glGetTransformFeedbackVaryingFn =
        reinterpret_cast<glGetTransformFeedbackVaryingProc>(
            GetGLProcAddress("glGetTransformFeedbackVaryingEXT"));
  }

  debug_fn.glGetTranslatedShaderSourceANGLEFn = 0;
  if (ext.b_GL_ANGLE_translated_shader_source) {
    fn.glGetTranslatedShaderSourceANGLEFn =
        reinterpret_cast<glGetTranslatedShaderSourceANGLEProc>(
            GetGLProcAddress("glGetTranslatedShaderSourceANGLE"));
  }

  debug_fn.glGetUniformBlockIndexFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformBlockIndexFn = reinterpret_cast<glGetUniformBlockIndexProc>(
        GetGLProcAddress("glGetUniformBlockIndex"));
  }

  debug_fn.glGetUniformfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformfvRobustANGLEFn =
        reinterpret_cast<glGetUniformfvRobustANGLEProc>(
            GetGLProcAddress("glGetUniformfvRobustANGLE"));
  }

  debug_fn.glGetUniformIndicesFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glGetUniformIndicesFn = reinterpret_cast<glGetUniformIndicesProc>(
        GetGLProcAddress("glGetUniformIndices"));
  }

  debug_fn.glGetUniformivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformivRobustANGLEFn =
        reinterpret_cast<glGetUniformivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformivRobustANGLE"));
  }

  debug_fn.glGetUniformuivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glGetUniformuivFn = reinterpret_cast<glGetUniformuivProc>(
        GetGLProcAddress("glGetUniformuiv"));
  }

  debug_fn.glGetUniformuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetUniformuivRobustANGLEFn =
        reinterpret_cast<glGetUniformuivRobustANGLEProc>(
            GetGLProcAddress("glGetUniformuivRobustANGLE"));
  }

  debug_fn.glGetVertexAttribfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribfvRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribfvRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribfvRobustANGLE"));
  }

  debug_fn.glGetVertexAttribIivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIivRobustANGLE"));
  }

  debug_fn.glGetVertexAttribIuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribIuivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribIuivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribIuivRobustANGLE"));
  }

  debug_fn.glGetVertexAttribivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribivRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribivRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribivRobustANGLE"));
  }

  debug_fn.glGetVertexAttribPointervRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glGetVertexAttribPointervRobustANGLEFn =
        reinterpret_cast<glGetVertexAttribPointervRobustANGLEProc>(
            GetGLProcAddress("glGetVertexAttribPointervRobustANGLE"));
  }

  debug_fn.glInsertEventMarkerEXTFn = 0;
  if (ext.b_GL_EXT_debug_marker) {
    fn.glInsertEventMarkerEXTFn = reinterpret_cast<glInsertEventMarkerEXTProc>(
        GetGLProcAddress("glInsertEventMarkerEXT"));
  }

  debug_fn.glInvalidateFramebufferFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateFramebufferFn =
        reinterpret_cast<glInvalidateFramebufferProc>(
            GetGLProcAddress("glInvalidateFramebuffer"));
  }

  debug_fn.glInvalidateSubFramebufferFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glInvalidateSubFramebufferFn =
        reinterpret_cast<glInvalidateSubFramebufferProc>(
            GetGLProcAddress("glInvalidateSubFramebuffer"));
  }

  debug_fn.glIsFenceAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glIsFenceAPPLEFn = reinterpret_cast<glIsFenceAPPLEProc>(
        GetGLProcAddress("glIsFenceAPPLE"));
  }

  debug_fn.glIsFenceNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glIsFenceNVFn =
        reinterpret_cast<glIsFenceNVProc>(GetGLProcAddress("glIsFenceNV"));
  }

  debug_fn.glIsFramebufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsFramebufferEXTFn = reinterpret_cast<glIsFramebufferEXTProc>(
        GetGLProcAddress("glIsFramebufferEXT"));
  }

  debug_fn.glIsPathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glIsPathNVFn =
        reinterpret_cast<glIsPathNVProc>(GetGLProcAddress("glIsPathNV"));
  }

  debug_fn.glIsQueryFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQuery"));
  } else if (ext.b_GL_ARB_occlusion_query) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryARB"));
  } else if (ext.b_GL_EXT_disjoint_timer_query ||
             ext.b_GL_EXT_occlusion_query_boolean) {
    fn.glIsQueryFn =
        reinterpret_cast<glIsQueryProc>(GetGLProcAddress("glIsQueryEXT"));
  }

  debug_fn.glIsRenderbufferEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbuffer"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glIsRenderbufferEXTFn = reinterpret_cast<glIsRenderbufferEXTProc>(
        GetGLProcAddress("glIsRenderbufferEXT"));
  }

  debug_fn.glIsSamplerFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glIsSamplerFn =
        reinterpret_cast<glIsSamplerProc>(GetGLProcAddress("glIsSampler"));
  }

  debug_fn.glIsSyncFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glIsSyncFn =
        reinterpret_cast<glIsSyncProc>(GetGLProcAddress("glIsSync"));
  }

  debug_fn.glIsTransformFeedbackFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glIsTransformFeedbackFn = reinterpret_cast<glIsTransformFeedbackProc>(
        GetGLProcAddress("glIsTransformFeedback"));
  }

  debug_fn.glIsVertexArrayOESFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArray"));
  } else if (ext.b_GL_OES_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayOES"));
  } else if (ext.b_GL_APPLE_vertex_array_object) {
    fn.glIsVertexArrayOESFn = reinterpret_cast<glIsVertexArrayOESProc>(
        GetGLProcAddress("glIsVertexArrayAPPLE"));
  }

  debug_fn.glMapBufferFn = 0;
  if (!ver->is_es) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glMapBufferFn =
        reinterpret_cast<glMapBufferProc>(GetGLProcAddress("glMapBufferOES"));
  }

  debug_fn.glMapBufferRangeFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRange"));
  } else if (ext.b_GL_EXT_map_buffer_range) {
    fn.glMapBufferRangeFn = reinterpret_cast<glMapBufferRangeProc>(
        GetGLProcAddress("glMapBufferRangeEXT"));
  }

  debug_fn.glMatrixLoadfEXTFn = 0;
  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadfEXTFn = reinterpret_cast<glMatrixLoadfEXTProc>(
        GetGLProcAddress("glMatrixLoadfEXT"));
  }

  debug_fn.glMatrixLoadIdentityEXTFn = 0;
  if (ext.b_GL_EXT_direct_state_access || ext.b_GL_NV_path_rendering) {
    fn.glMatrixLoadIdentityEXTFn =
        reinterpret_cast<glMatrixLoadIdentityEXTProc>(
            GetGLProcAddress("glMatrixLoadIdentityEXT"));
  }

  debug_fn.glMemoryBarrierEXTFn = 0;
  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 1u) ||
      ext.b_GL_ARB_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrier"));
  } else if (ext.b_GL_EXT_shader_image_load_store) {
    fn.glMemoryBarrierEXTFn = reinterpret_cast<glMemoryBarrierEXTProc>(
        GetGLProcAddress("glMemoryBarrierEXT"));
  }

  debug_fn.glObjectLabelFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectLabelFn =
        reinterpret_cast<glObjectLabelProc>(GetGLProcAddress("glObjectLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectLabelFn = reinterpret_cast<glObjectLabelProc>(
        GetGLProcAddress("glObjectLabelKHR"));
  }

  debug_fn.glObjectPtrLabelFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabel"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glObjectPtrLabelFn = reinterpret_cast<glObjectPtrLabelProc>(
        GetGLProcAddress("glObjectPtrLabelKHR"));
  }

  debug_fn.glPathCommandsNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glPathCommandsNVFn = reinterpret_cast<glPathCommandsNVProc>(
        GetGLProcAddress("glPathCommandsNV"));
  }

  debug_fn.glPathParameterfNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameterfNVFn = reinterpret_cast<glPathParameterfNVProc>(
        GetGLProcAddress("glPathParameterfNV"));
  }

  debug_fn.glPathParameteriNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glPathParameteriNVFn = reinterpret_cast<glPathParameteriNVProc>(
        GetGLProcAddress("glPathParameteriNV"));
  }

  debug_fn.glPathStencilFuncNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glPathStencilFuncNVFn = reinterpret_cast<glPathStencilFuncNVProc>(
        GetGLProcAddress("glPathStencilFuncNV"));
  }

  debug_fn.glPauseTransformFeedbackFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glPauseTransformFeedbackFn =
        reinterpret_cast<glPauseTransformFeedbackProc>(
            GetGLProcAddress("glPauseTransformFeedback"));
  }

  debug_fn.glPointParameteriFn = 0;
  if (!ver->is_es) {
    fn.glPointParameteriFn = reinterpret_cast<glPointParameteriProc>(
        GetGLProcAddress("glPointParameteri"));
  }

  debug_fn.glPopDebugGroupFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPopDebugGroupFn = reinterpret_cast<glPopDebugGroupProc>(
        GetGLProcAddress("glPopDebugGroupKHR"));
  }

  debug_fn.glPopGroupMarkerEXTFn = 0;
  if (ext.b_GL_EXT_debug_marker) {
    fn.glPopGroupMarkerEXTFn = reinterpret_cast<glPopGroupMarkerEXTProc>(
        GetGLProcAddress("glPopGroupMarkerEXT"));
  }

  debug_fn.glPrimitiveRestartIndexFn = 0;
  if (ver->IsAtLeastGL(3u, 1u)) {
    fn.glPrimitiveRestartIndexFn =
        reinterpret_cast<glPrimitiveRestartIndexProc>(
            GetGLProcAddress("glPrimitiveRestartIndex"));
  }

  debug_fn.glProgramBinaryFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinary"));
  } else if (ext.b_GL_OES_get_program_binary) {
    fn.glProgramBinaryFn = reinterpret_cast<glProgramBinaryProc>(
        GetGLProcAddress("glProgramBinaryOES"));
  }

  debug_fn.glProgramParameteriFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_get_program_binary) {
    fn.glProgramParameteriFn = reinterpret_cast<glProgramParameteriProc>(
        GetGLProcAddress("glProgramParameteri"));
  }

  debug_fn.glProgramPathFragmentInputGenNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glProgramPathFragmentInputGenNVFn =
        reinterpret_cast<glProgramPathFragmentInputGenNVProc>(
            GetGLProcAddress("glProgramPathFragmentInputGenNV"));
  }

  debug_fn.glPushDebugGroupFn = 0;
  if (ver->IsAtLeastGL(4u, 3u) || ver->IsAtLeastGLES(3u, 2u)) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroup"));
  } else if (ext.b_GL_KHR_debug) {
    fn.glPushDebugGroupFn = reinterpret_cast<glPushDebugGroupProc>(
        GetGLProcAddress("glPushDebugGroupKHR"));
  }

  debug_fn.glPushGroupMarkerEXTFn = 0;
  if (ext.b_GL_EXT_debug_marker) {
    fn.glPushGroupMarkerEXTFn = reinterpret_cast<glPushGroupMarkerEXTProc>(
        GetGLProcAddress("glPushGroupMarkerEXT"));
  }

  debug_fn.glQueryCounterFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ext.b_GL_ARB_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounter"));
  } else if (ext.b_GL_EXT_disjoint_timer_query) {
    fn.glQueryCounterFn = reinterpret_cast<glQueryCounterProc>(
        GetGLProcAddress("glQueryCounterEXT"));
  }

  debug_fn.glReadBufferFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glReadBufferFn =
        reinterpret_cast<glReadBufferProc>(GetGLProcAddress("glReadBuffer"));
  }

  debug_fn.glReadnPixelsRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadnPixelsRobustANGLEFn =
        reinterpret_cast<glReadnPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadnPixelsRobustANGLE"));
  }

  debug_fn.glReadPixelsRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glReadPixelsRobustANGLEFn =
        reinterpret_cast<glReadPixelsRobustANGLEProc>(
            GetGLProcAddress("glReadPixelsRobustANGLE"));
  }

  debug_fn.glReleaseShaderCompilerFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glReleaseShaderCompilerFn =
        reinterpret_cast<glReleaseShaderCompilerProc>(
            GetGLProcAddress("glReleaseShaderCompiler"));
  }

  debug_fn.glRenderbufferStorageEXTFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->is_es) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorage"));
  } else if (ext.b_GL_EXT_framebuffer_object) {
    fn.glRenderbufferStorageEXTFn =
        reinterpret_cast<glRenderbufferStorageEXTProc>(
            GetGLProcAddress("glRenderbufferStorageEXT"));
  }

  debug_fn.glRenderbufferStorageMultisampleFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glRenderbufferStorageMultisampleFn =
        reinterpret_cast<glRenderbufferStorageMultisampleProc>(
            GetGLProcAddress("glRenderbufferStorageMultisample"));
  }

  debug_fn.glRenderbufferStorageMultisampleANGLEFn = 0;
  if (ext.b_GL_ANGLE_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleANGLEFn =
        reinterpret_cast<glRenderbufferStorageMultisampleANGLEProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleANGLE"));
  }

  debug_fn.glRenderbufferStorageMultisampleEXTFn = 0;
  if (ext.b_GL_EXT_multisampled_render_to_texture ||
      ext.b_GL_EXT_framebuffer_multisample) {
    fn.glRenderbufferStorageMultisampleEXTFn =
        reinterpret_cast<glRenderbufferStorageMultisampleEXTProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleEXT"));
  }

  debug_fn.glRenderbufferStorageMultisampleIMGFn = 0;
  if (ext.b_GL_IMG_multisampled_render_to_texture) {
    fn.glRenderbufferStorageMultisampleIMGFn =
        reinterpret_cast<glRenderbufferStorageMultisampleIMGProc>(
            GetGLProcAddress("glRenderbufferStorageMultisampleIMG"));
  }

  debug_fn.glResumeTransformFeedbackFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(4u, 0u) ||
      ext.b_GL_ARB_transform_feedback2) {
    fn.glResumeTransformFeedbackFn =
        reinterpret_cast<glResumeTransformFeedbackProc>(
            GetGLProcAddress("glResumeTransformFeedback"));
  }

  debug_fn.glSamplerParameterfFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfFn = reinterpret_cast<glSamplerParameterfProc>(
        GetGLProcAddress("glSamplerParameterf"));
  }

  debug_fn.glSamplerParameterfvFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterfvFn = reinterpret_cast<glSamplerParameterfvProc>(
        GetGLProcAddress("glSamplerParameterfv"));
  }

  debug_fn.glSamplerParameterfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterfvRobustANGLEFn =
        reinterpret_cast<glSamplerParameterfvRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterfvRobustANGLE"));
  }

  debug_fn.glSamplerParameteriFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameteriFn = reinterpret_cast<glSamplerParameteriProc>(
        GetGLProcAddress("glSamplerParameteri"));
  }

  debug_fn.glSamplerParameterIivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIivRobustANGLE"));
  }

  debug_fn.glSamplerParameterIuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterIuivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterIuivRobustANGLE"));
  }

  debug_fn.glSamplerParameterivFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sampler_objects) {
    fn.glSamplerParameterivFn = reinterpret_cast<glSamplerParameterivProc>(
        GetGLProcAddress("glSamplerParameteriv"));
  }

  debug_fn.glSamplerParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glSamplerParameterivRobustANGLEFn =
        reinterpret_cast<glSamplerParameterivRobustANGLEProc>(
            GetGLProcAddress("glSamplerParameterivRobustANGLE"));
  }

  debug_fn.glSetFenceAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glSetFenceAPPLEFn = reinterpret_cast<glSetFenceAPPLEProc>(
        GetGLProcAddress("glSetFenceAPPLE"));
  }

  debug_fn.glSetFenceNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glSetFenceNVFn =
        reinterpret_cast<glSetFenceNVProc>(GetGLProcAddress("glSetFenceNV"));
  }

  debug_fn.glShaderBinaryFn = 0;
  if (ver->IsAtLeastGL(4u, 1u) || ver->is_es) {
    fn.glShaderBinaryFn = reinterpret_cast<glShaderBinaryProc>(
        GetGLProcAddress("glShaderBinary"));
  }

  debug_fn.glStencilFillPathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathInstancedNVFn =
        reinterpret_cast<glStencilFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilFillPathInstancedNV"));
  }

  debug_fn.glStencilFillPathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilFillPathNVFn = reinterpret_cast<glStencilFillPathNVProc>(
        GetGLProcAddress("glStencilFillPathNV"));
  }

  debug_fn.glStencilStrokePathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathInstancedNVFn =
        reinterpret_cast<glStencilStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilStrokePathInstancedNV"));
  }

  debug_fn.glStencilStrokePathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilStrokePathNVFn = reinterpret_cast<glStencilStrokePathNVProc>(
        GetGLProcAddress("glStencilStrokePathNV"));
  }

  debug_fn.glStencilThenCoverFillPathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverFillPathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathInstancedNV"));
  }

  debug_fn.glStencilThenCoverFillPathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverFillPathNVFn =
        reinterpret_cast<glStencilThenCoverFillPathNVProc>(
            GetGLProcAddress("glStencilThenCoverFillPathNV"));
  }

  debug_fn.glStencilThenCoverStrokePathInstancedNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathInstancedNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathInstancedNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathInstancedNV"));
  }

  debug_fn.glStencilThenCoverStrokePathNVFn = 0;
  if (ext.b_GL_NV_path_rendering) {
    fn.glStencilThenCoverStrokePathNVFn =
        reinterpret_cast<glStencilThenCoverStrokePathNVProc>(
            GetGLProcAddress("glStencilThenCoverStrokePathNV"));
  }

  debug_fn.glTestFenceAPPLEFn = 0;
  if (ext.b_GL_APPLE_fence) {
    fn.glTestFenceAPPLEFn = reinterpret_cast<glTestFenceAPPLEProc>(
        GetGLProcAddress("glTestFenceAPPLE"));
  }

  debug_fn.glTestFenceNVFn = 0;
  if (ext.b_GL_NV_fence) {
    fn.glTestFenceNVFn =
        reinterpret_cast<glTestFenceNVProc>(GetGLProcAddress("glTestFenceNV"));
  }

  debug_fn.glTexImage2DRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage2DRobustANGLEFn =
        reinterpret_cast<glTexImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexImage2DRobustANGLE"));
  }

  debug_fn.glTexImage3DFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexImage3DFn =
        reinterpret_cast<glTexImage3DProc>(GetGLProcAddress("glTexImage3D"));
  }

  debug_fn.glTexImage3DRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexImage3DRobustANGLEFn =
        reinterpret_cast<glTexImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexImage3DRobustANGLE"));
  }

  debug_fn.glTexParameterfvRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterfvRobustANGLEFn =
        reinterpret_cast<glTexParameterfvRobustANGLEProc>(
            GetGLProcAddress("glTexParameterfvRobustANGLE"));
  }

  debug_fn.glTexParameterIivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIivRobustANGLEFn =
        reinterpret_cast<glTexParameterIivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIivRobustANGLE"));
  }

  debug_fn.glTexParameterIuivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterIuivRobustANGLEFn =
        reinterpret_cast<glTexParameterIuivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterIuivRobustANGLE"));
  }

  debug_fn.glTexParameterivRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexParameterivRobustANGLEFn =
        reinterpret_cast<glTexParameterivRobustANGLEProc>(
            GetGLProcAddress("glTexParameterivRobustANGLE"));
  }

  debug_fn.glTexStorage2DEXTFn = 0;
  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2D"));
  } else if (ext.b_GL_EXT_texture_storage) {
    fn.glTexStorage2DEXTFn = reinterpret_cast<glTexStorage2DEXTProc>(
        GetGLProcAddress("glTexStorage2DEXT"));
  }

  debug_fn.glTexStorage3DFn = 0;
  if (ver->IsAtLeastGL(4u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_texture_storage) {
    fn.glTexStorage3DFn = reinterpret_cast<glTexStorage3DProc>(
        GetGLProcAddress("glTexStorage3D"));
  }

  debug_fn.glTexSubImage2DRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage2DRobustANGLEFn =
        reinterpret_cast<glTexSubImage2DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage2DRobustANGLE"));
  }

  debug_fn.glTexSubImage3DFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTexSubImage3DFn = reinterpret_cast<glTexSubImage3DProc>(
        GetGLProcAddress("glTexSubImage3D"));
  }

  debug_fn.glTexSubImage3DRobustANGLEFn = 0;
  if (ext.b_GL_ANGLE_robust_client_memory) {
    fn.glTexSubImage3DRobustANGLEFn =
        reinterpret_cast<glTexSubImage3DRobustANGLEProc>(
            GetGLProcAddress("glTexSubImage3DRobustANGLE"));
  }

  debug_fn.glTransformFeedbackVaryingsFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryings"));
  } else if (ext.b_GL_EXT_transform_feedback) {
    fn.glTransformFeedbackVaryingsFn =
        reinterpret_cast<glTransformFeedbackVaryingsProc>(
            GetGLProcAddress("glTransformFeedbackVaryingsEXT"));
  }

  debug_fn.glUniform1uiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uiFn =
        reinterpret_cast<glUniform1uiProc>(GetGLProcAddress("glUniform1ui"));
  }

  debug_fn.glUniform1uivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform1uivFn =
        reinterpret_cast<glUniform1uivProc>(GetGLProcAddress("glUniform1uiv"));
  }

  debug_fn.glUniform2uiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uiFn =
        reinterpret_cast<glUniform2uiProc>(GetGLProcAddress("glUniform2ui"));
  }

  debug_fn.glUniform2uivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform2uivFn =
        reinterpret_cast<glUniform2uivProc>(GetGLProcAddress("glUniform2uiv"));
  }

  debug_fn.glUniform3uiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uiFn =
        reinterpret_cast<glUniform3uiProc>(GetGLProcAddress("glUniform3ui"));
  }

  debug_fn.glUniform3uivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform3uivFn =
        reinterpret_cast<glUniform3uivProc>(GetGLProcAddress("glUniform3uiv"));
  }

  debug_fn.glUniform4uiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uiFn =
        reinterpret_cast<glUniform4uiProc>(GetGLProcAddress("glUniform4ui"));
  }

  debug_fn.glUniform4uivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniform4uivFn =
        reinterpret_cast<glUniform4uivProc>(GetGLProcAddress("glUniform4uiv"));
  }

  debug_fn.glUniformBlockBindingFn = 0;
  if (ver->IsAtLeastGLES(3u, 0u) || ver->IsAtLeastGL(3u, 1u)) {
    fn.glUniformBlockBindingFn = reinterpret_cast<glUniformBlockBindingProc>(
        GetGLProcAddress("glUniformBlockBinding"));
  }

  debug_fn.glUniformMatrix2x3fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x3fvFn = reinterpret_cast<glUniformMatrix2x3fvProc>(
        GetGLProcAddress("glUniformMatrix2x3fv"));
  }

  debug_fn.glUniformMatrix2x4fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix2x4fvFn = reinterpret_cast<glUniformMatrix2x4fvProc>(
        GetGLProcAddress("glUniformMatrix2x4fv"));
  }

  debug_fn.glUniformMatrix3x2fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x2fvFn = reinterpret_cast<glUniformMatrix3x2fvProc>(
        GetGLProcAddress("glUniformMatrix3x2fv"));
  }

  debug_fn.glUniformMatrix3x4fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix3x4fvFn = reinterpret_cast<glUniformMatrix3x4fvProc>(
        GetGLProcAddress("glUniformMatrix3x4fv"));
  }

  debug_fn.glUniformMatrix4x2fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x2fvFn = reinterpret_cast<glUniformMatrix4x2fvProc>(
        GetGLProcAddress("glUniformMatrix4x2fv"));
  }

  debug_fn.glUniformMatrix4x3fvFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUniformMatrix4x3fvFn = reinterpret_cast<glUniformMatrix4x3fvProc>(
        GetGLProcAddress("glUniformMatrix4x3fv"));
  }

  debug_fn.glUnmapBufferFn = 0;
  if (!ver->is_es || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glUnmapBufferFn =
        reinterpret_cast<glUnmapBufferProc>(GetGLProcAddress("glUnmapBuffer"));
  } else if (ext.b_GL_OES_mapbuffer) {
    fn.glUnmapBufferFn = reinterpret_cast<glUnmapBufferProc>(
        GetGLProcAddress("glUnmapBufferOES"));
  }

  debug_fn.glVertexAttribDivisorANGLEFn = 0;
  if (ver->IsAtLeastGL(3u, 3u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisor"));
  } else if (ext.b_GL_ARB_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorARB"));
  } else if (ext.b_GL_ANGLE_instanced_arrays) {
    fn.glVertexAttribDivisorANGLEFn =
        reinterpret_cast<glVertexAttribDivisorANGLEProc>(
            GetGLProcAddress("glVertexAttribDivisorANGLE"));
  }

  debug_fn.glVertexAttribI4iFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4iFn = reinterpret_cast<glVertexAttribI4iProc>(
        GetGLProcAddress("glVertexAttribI4i"));
  }

  debug_fn.glVertexAttribI4ivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4ivFn = reinterpret_cast<glVertexAttribI4ivProc>(
        GetGLProcAddress("glVertexAttribI4iv"));
  }

  debug_fn.glVertexAttribI4uiFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uiFn = reinterpret_cast<glVertexAttribI4uiProc>(
        GetGLProcAddress("glVertexAttribI4ui"));
  }

  debug_fn.glVertexAttribI4uivFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribI4uivFn = reinterpret_cast<glVertexAttribI4uivProc>(
        GetGLProcAddress("glVertexAttribI4uiv"));
  }

  debug_fn.glVertexAttribIPointerFn = 0;
  if (ver->IsAtLeastGL(3u, 0u) || ver->IsAtLeastGLES(3u, 0u)) {
    fn.glVertexAttribIPointerFn = reinterpret_cast<glVertexAttribIPointerProc>(
        GetGLProcAddress("glVertexAttribIPointer"));
  }

  debug_fn.glWaitSyncFn = 0;
  if (ver->IsAtLeastGL(3u, 2u) || ver->IsAtLeastGLES(3u, 0u) ||
      ext.b_GL_ARB_sync) {
    fn.glWaitSyncFn =
        reinterpret_cast<glWaitSyncProc>(GetGLProcAddress("glWaitSync"));
  }

  if (g_debugBindingsInitialized)
    InitializeDebugBindings();
}

extern "C" {

static void GL_BINDING_CALL Debug_glActiveTexture(GLenum texture) {
  GL_SERVICE_LOG("glActiveTexture"
                 << "(" << GLEnums::GetStringEnum(texture) << ")");
  DCHECK(g_driver_gl.debug_fn.glActiveTextureFn != nullptr);
  g_driver_gl.debug_fn.glActiveTextureFn(texture);
}

static void GL_BINDING_CALL Debug_glApplyFramebufferAttachmentCMAAINTEL(void) {
  GL_SERVICE_LOG("glApplyFramebufferAttachmentCMAAINTEL"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glApplyFramebufferAttachmentCMAAINTELFn !=
         nullptr);
  g_driver_gl.debug_fn.glApplyFramebufferAttachmentCMAAINTELFn();
}

static void GL_BINDING_CALL Debug_glAttachShader(GLuint program,
                                                 GLuint shader) {
  GL_SERVICE_LOG("glAttachShader"
                 << "(" << program << ", " << shader << ")");
  DCHECK(g_driver_gl.debug_fn.glAttachShaderFn != nullptr);
  g_driver_gl.debug_fn.glAttachShaderFn(program, shader);
}

static void GL_BINDING_CALL Debug_glBeginQuery(GLenum target, GLuint id) {
  GL_SERVICE_LOG("glBeginQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  DCHECK(g_driver_gl.debug_fn.glBeginQueryFn != nullptr);
  g_driver_gl.debug_fn.glBeginQueryFn(target, id);
}

static void GL_BINDING_CALL
Debug_glBeginTransformFeedback(GLenum primitiveMode) {
  GL_SERVICE_LOG("glBeginTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(primitiveMode) << ")");
  DCHECK(g_driver_gl.debug_fn.glBeginTransformFeedbackFn != nullptr);
  g_driver_gl.debug_fn.glBeginTransformFeedbackFn(primitiveMode);
}

static void GL_BINDING_CALL Debug_glBindAttribLocation(GLuint program,
                                                       GLuint index,
                                                       const char* name) {
  GL_SERVICE_LOG("glBindAttribLocation"
                 << "(" << program << ", " << index << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glBindAttribLocationFn != nullptr);
  g_driver_gl.debug_fn.glBindAttribLocationFn(program, index, name);
}

static void GL_BINDING_CALL Debug_glBindBuffer(GLenum target, GLuint buffer) {
  GL_SERVICE_LOG("glBindBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << buffer
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBindBufferFn != nullptr);
  g_driver_gl.debug_fn.glBindBufferFn(target, buffer);
}

static void GL_BINDING_CALL Debug_glBindBufferBase(GLenum target,
                                                   GLuint index,
                                                   GLuint buffer) {
  GL_SERVICE_LOG("glBindBufferBase"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ")");
  DCHECK(g_driver_gl.debug_fn.glBindBufferBaseFn != nullptr);
  g_driver_gl.debug_fn.glBindBufferBaseFn(target, index, buffer);
}

static void GL_BINDING_CALL Debug_glBindBufferRange(GLenum target,
                                                    GLuint index,
                                                    GLuint buffer,
                                                    GLintptr offset,
                                                    GLsizeiptr size) {
  GL_SERVICE_LOG("glBindBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << buffer << ", " << offset << ", " << size << ")");
  DCHECK(g_driver_gl.debug_fn.glBindBufferRangeFn != nullptr);
  g_driver_gl.debug_fn.glBindBufferRangeFn(target, index, buffer, offset, size);
}

static void GL_BINDING_CALL Debug_glBindFragDataLocation(GLuint program,
                                                         GLuint colorNumber,
                                                         const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocation"
                 << "(" << program << ", " << colorNumber << ", " << name
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBindFragDataLocationFn != nullptr);
  g_driver_gl.debug_fn.glBindFragDataLocationFn(program, colorNumber, name);
}

static void GL_BINDING_CALL
Debug_glBindFragDataLocationIndexed(GLuint program,
                                    GLuint colorNumber,
                                    GLuint index,
                                    const char* name) {
  GL_SERVICE_LOG("glBindFragDataLocationIndexed"
                 << "(" << program << ", " << colorNumber << ", " << index
                 << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glBindFragDataLocationIndexedFn != nullptr);
  g_driver_gl.debug_fn.glBindFragDataLocationIndexedFn(program, colorNumber,
                                                       index, name);
}

static void GL_BINDING_CALL Debug_glBindFramebufferEXT(GLenum target,
                                                       GLuint framebuffer) {
  GL_SERVICE_LOG("glBindFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << framebuffer
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBindFramebufferEXTFn != nullptr);
  g_driver_gl.debug_fn.glBindFramebufferEXTFn(target, framebuffer);
}

static void GL_BINDING_CALL Debug_glBindImageTextureEXT(GLuint index,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLboolean layered,
                                                        GLint layer,
                                                        GLenum access,
                                                        GLint format) {
  GL_SERVICE_LOG("glBindImageTextureEXT"
                 << "(" << index << ", " << texture << ", " << level << ", "
                 << GLEnums::GetStringBool(layered) << ", " << layer << ", "
                 << GLEnums::GetStringEnum(access) << ", " << format << ")");
  DCHECK(g_driver_gl.debug_fn.glBindImageTextureEXTFn != nullptr);
  g_driver_gl.debug_fn.glBindImageTextureEXTFn(index, texture, level, layered,
                                               layer, access, format);
}

static void GL_BINDING_CALL Debug_glBindRenderbufferEXT(GLenum target,
                                                        GLuint renderbuffer) {
  GL_SERVICE_LOG("glBindRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << renderbuffer << ")");
  DCHECK(g_driver_gl.debug_fn.glBindRenderbufferEXTFn != nullptr);
  g_driver_gl.debug_fn.glBindRenderbufferEXTFn(target, renderbuffer);
}

static void GL_BINDING_CALL Debug_glBindSampler(GLuint unit, GLuint sampler) {
  GL_SERVICE_LOG("glBindSampler"
                 << "(" << unit << ", " << sampler << ")");
  DCHECK(g_driver_gl.debug_fn.glBindSamplerFn != nullptr);
  g_driver_gl.debug_fn.glBindSamplerFn(unit, sampler);
}

static void GL_BINDING_CALL Debug_glBindTexture(GLenum target, GLuint texture) {
  GL_SERVICE_LOG("glBindTexture"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << texture
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBindTextureFn != nullptr);
  g_driver_gl.debug_fn.glBindTextureFn(target, texture);
}

static void GL_BINDING_CALL Debug_glBindTransformFeedback(GLenum target,
                                                          GLuint id) {
  GL_SERVICE_LOG("glBindTransformFeedback"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << id << ")");
  DCHECK(g_driver_gl.debug_fn.glBindTransformFeedbackFn != nullptr);
  g_driver_gl.debug_fn.glBindTransformFeedbackFn(target, id);
}

static void GL_BINDING_CALL
Debug_glBindUniformLocationCHROMIUM(GLuint program,
                                    GLint location,
                                    const char* name) {
  GL_SERVICE_LOG("glBindUniformLocationCHROMIUM"
                 << "(" << program << ", " << location << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glBindUniformLocationCHROMIUMFn != nullptr);
  g_driver_gl.debug_fn.glBindUniformLocationCHROMIUMFn(program, location, name);
}

static void GL_BINDING_CALL Debug_glBindVertexArrayOES(GLuint array) {
  GL_SERVICE_LOG("glBindVertexArrayOES"
                 << "(" << array << ")");
  DCHECK(g_driver_gl.debug_fn.glBindVertexArrayOESFn != nullptr);
  g_driver_gl.debug_fn.glBindVertexArrayOESFn(array);
}

static void GL_BINDING_CALL Debug_glBlendBarrierKHR(void) {
  GL_SERVICE_LOG("glBlendBarrierKHR"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendBarrierKHRFn != nullptr);
  g_driver_gl.debug_fn.glBlendBarrierKHRFn();
}

static void GL_BINDING_CALL Debug_glBlendColor(GLclampf red,
                                               GLclampf green,
                                               GLclampf blue,
                                               GLclampf alpha) {
  GL_SERVICE_LOG("glBlendColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendColorFn != nullptr);
  g_driver_gl.debug_fn.glBlendColorFn(red, green, blue, alpha);
}

static void GL_BINDING_CALL Debug_glBlendEquation(GLenum mode) {
  GL_SERVICE_LOG("glBlendEquation"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendEquationFn != nullptr);
  g_driver_gl.debug_fn.glBlendEquationFn(mode);
}

static void GL_BINDING_CALL Debug_glBlendEquationSeparate(GLenum modeRGB,
                                                          GLenum modeAlpha) {
  GL_SERVICE_LOG("glBlendEquationSeparate"
                 << "(" << GLEnums::GetStringEnum(modeRGB) << ", "
                 << GLEnums::GetStringEnum(modeAlpha) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendEquationSeparateFn != nullptr);
  g_driver_gl.debug_fn.glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

static void GL_BINDING_CALL Debug_glBlendFunc(GLenum sfactor, GLenum dfactor) {
  GL_SERVICE_LOG("glBlendFunc"
                 << "(" << GLEnums::GetStringEnum(sfactor) << ", "
                 << GLEnums::GetStringEnum(dfactor) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendFuncFn != nullptr);
  g_driver_gl.debug_fn.glBlendFuncFn(sfactor, dfactor);
}

static void GL_BINDING_CALL Debug_glBlendFuncSeparate(GLenum srcRGB,
                                                      GLenum dstRGB,
                                                      GLenum srcAlpha,
                                                      GLenum dstAlpha) {
  GL_SERVICE_LOG("glBlendFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(srcRGB) << ", "
                 << GLEnums::GetStringEnum(dstRGB) << ", "
                 << GLEnums::GetStringEnum(srcAlpha) << ", "
                 << GLEnums::GetStringEnum(dstAlpha) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlendFuncSeparateFn != nullptr);
  g_driver_gl.debug_fn.glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha,
                                             dstAlpha);
}

static void GL_BINDING_CALL Debug_glBlitFramebuffer(GLint srcX0,
                                                    GLint srcY0,
                                                    GLint srcX1,
                                                    GLint srcY1,
                                                    GLint dstX0,
                                                    GLint dstY0,
                                                    GLint dstX1,
                                                    GLint dstY1,
                                                    GLbitfield mask,
                                                    GLenum filter) {
  GL_SERVICE_LOG("glBlitFramebuffer"
                 << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", "
                 << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1
                 << ", " << dstY1 << ", " << mask << ", "
                 << GLEnums::GetStringEnum(filter) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlitFramebufferFn != nullptr);
  g_driver_gl.debug_fn.glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0,
                                           dstY0, dstX1, dstY1, mask, filter);
}

static void GL_BINDING_CALL Debug_glBlitFramebufferANGLE(GLint srcX0,
                                                         GLint srcY0,
                                                         GLint srcX1,
                                                         GLint srcY1,
                                                         GLint dstX0,
                                                         GLint dstY0,
                                                         GLint dstX1,
                                                         GLint dstY1,
                                                         GLbitfield mask,
                                                         GLenum filter) {
  GL_SERVICE_LOG("glBlitFramebufferANGLE"
                 << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", "
                 << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1
                 << ", " << dstY1 << ", " << mask << ", "
                 << GLEnums::GetStringEnum(filter) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlitFramebufferANGLEFn != nullptr);
  g_driver_gl.debug_fn.glBlitFramebufferANGLEFn(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

static void GL_BINDING_CALL Debug_glBlitFramebufferEXT(GLint srcX0,
                                                       GLint srcY0,
                                                       GLint srcX1,
                                                       GLint srcY1,
                                                       GLint dstX0,
                                                       GLint dstY0,
                                                       GLint dstX1,
                                                       GLint dstY1,
                                                       GLbitfield mask,
                                                       GLenum filter) {
  GL_SERVICE_LOG("glBlitFramebufferEXT"
                 << "(" << srcX0 << ", " << srcY0 << ", " << srcX1 << ", "
                 << srcY1 << ", " << dstX0 << ", " << dstY0 << ", " << dstX1
                 << ", " << dstY1 << ", " << mask << ", "
                 << GLEnums::GetStringEnum(filter) << ")");
  DCHECK(g_driver_gl.debug_fn.glBlitFramebufferEXTFn != nullptr);
  g_driver_gl.debug_fn.glBlitFramebufferEXTFn(
      srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

static void GL_BINDING_CALL Debug_glBufferData(GLenum target,
                                               GLsizeiptr size,
                                               const void* data,
                                               GLenum usage) {
  GL_SERVICE_LOG("glBufferData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << size
                 << ", " << static_cast<const void*>(data) << ", "
                 << GLEnums::GetStringEnum(usage) << ")");
  DCHECK(g_driver_gl.debug_fn.glBufferDataFn != nullptr);
  g_driver_gl.debug_fn.glBufferDataFn(target, size, data, usage);
}

static void GL_BINDING_CALL Debug_glBufferSubData(GLenum target,
                                                  GLintptr offset,
                                                  GLsizeiptr size,
                                                  const void* data) {
  GL_SERVICE_LOG("glBufferSubData"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << size << ", " << static_cast<const void*>(data)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glBufferSubDataFn != nullptr);
  g_driver_gl.debug_fn.glBufferSubDataFn(target, offset, size, data);
}

static GLenum GL_BINDING_CALL Debug_glCheckFramebufferStatusEXT(GLenum target) {
  GL_SERVICE_LOG("glCheckFramebufferStatusEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  DCHECK(g_driver_gl.debug_fn.glCheckFramebufferStatusEXTFn != nullptr);
  GLenum result = g_driver_gl.debug_fn.glCheckFramebufferStatusEXTFn(target);

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringEnum(result));

  return result;
}

static void GL_BINDING_CALL Debug_glClear(GLbitfield mask) {
  GL_SERVICE_LOG("glClear"
                 << "(" << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glClearFn != nullptr);
  g_driver_gl.debug_fn.glClearFn(mask);
}

static void GL_BINDING_CALL Debug_glClearBufferfi(GLenum buffer,
                                                  GLint drawbuffer,
                                                  const GLfloat depth,
                                                  GLint stencil) {
  GL_SERVICE_LOG("glClearBufferfi"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << depth << ", " << stencil << ")");
  DCHECK(g_driver_gl.debug_fn.glClearBufferfiFn != nullptr);
  g_driver_gl.debug_fn.glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

static void GL_BINDING_CALL Debug_glClearBufferfv(GLenum buffer,
                                                  GLint drawbuffer,
                                                  const GLfloat* value) {
  GL_SERVICE_LOG("glClearBufferfv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glClearBufferfvFn != nullptr);
  g_driver_gl.debug_fn.glClearBufferfvFn(buffer, drawbuffer, value);
}

static void GL_BINDING_CALL Debug_glClearBufferiv(GLenum buffer,
                                                  GLint drawbuffer,
                                                  const GLint* value) {
  GL_SERVICE_LOG("glClearBufferiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glClearBufferivFn != nullptr);
  g_driver_gl.debug_fn.glClearBufferivFn(buffer, drawbuffer, value);
}

static void GL_BINDING_CALL Debug_glClearBufferuiv(GLenum buffer,
                                                   GLint drawbuffer,
                                                   const GLuint* value) {
  GL_SERVICE_LOG("glClearBufferuiv"
                 << "(" << GLEnums::GetStringEnum(buffer) << ", " << drawbuffer
                 << ", " << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glClearBufferuivFn != nullptr);
  g_driver_gl.debug_fn.glClearBufferuivFn(buffer, drawbuffer, value);
}

static void GL_BINDING_CALL Debug_glClearColor(GLclampf red,
                                               GLclampf green,
                                               GLclampf blue,
                                               GLclampf alpha) {
  GL_SERVICE_LOG("glClearColor"
                 << "(" << red << ", " << green << ", " << blue << ", " << alpha
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glClearColorFn != nullptr);
  g_driver_gl.debug_fn.glClearColorFn(red, green, blue, alpha);
}

static void GL_BINDING_CALL Debug_glClearDepth(GLclampd depth) {
  GL_SERVICE_LOG("glClearDepth"
                 << "(" << depth << ")");
  DCHECK(g_driver_gl.debug_fn.glClearDepthFn != nullptr);
  g_driver_gl.debug_fn.glClearDepthFn(depth);
}

static void GL_BINDING_CALL Debug_glClearDepthf(GLclampf depth) {
  GL_SERVICE_LOG("glClearDepthf"
                 << "(" << depth << ")");
  DCHECK(g_driver_gl.debug_fn.glClearDepthfFn != nullptr);
  g_driver_gl.debug_fn.glClearDepthfFn(depth);
}

static void GL_BINDING_CALL Debug_glClearStencil(GLint s) {
  GL_SERVICE_LOG("glClearStencil"
                 << "(" << s << ")");
  DCHECK(g_driver_gl.debug_fn.glClearStencilFn != nullptr);
  g_driver_gl.debug_fn.glClearStencilFn(s);
}

static GLenum GL_BINDING_CALL Debug_glClientWaitSync(GLsync sync,
                                                     GLbitfield flags,
                                                     GLuint64 timeout) {
  GL_SERVICE_LOG("glClientWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  DCHECK(g_driver_gl.debug_fn.glClientWaitSyncFn != nullptr);
  GLenum result = g_driver_gl.debug_fn.glClientWaitSyncFn(sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glColorMask(GLboolean red,
                                              GLboolean green,
                                              GLboolean blue,
                                              GLboolean alpha) {
  GL_SERVICE_LOG("glColorMask"
                 << "(" << GLEnums::GetStringBool(red) << ", "
                 << GLEnums::GetStringBool(green) << ", "
                 << GLEnums::GetStringBool(blue) << ", "
                 << GLEnums::GetStringBool(alpha) << ")");
  DCHECK(g_driver_gl.debug_fn.glColorMaskFn != nullptr);
  g_driver_gl.debug_fn.glColorMaskFn(red, green, blue, alpha);
}

static void GL_BINDING_CALL Debug_glCompileShader(GLuint shader) {
  GL_SERVICE_LOG("glCompileShader"
                 << "(" << shader << ")");
  DCHECK(g_driver_gl.debug_fn.glCompileShaderFn != nullptr);
  g_driver_gl.debug_fn.glCompileShaderFn(shader);
}

static void GL_BINDING_CALL
Debug_glCompressedCopyTextureCHROMIUM(GLuint sourceId, GLuint destId) {
  GL_SERVICE_LOG("glCompressedCopyTextureCHROMIUM"
                 << "(" << sourceId << ", " << destId << ")");
  DCHECK(g_driver_gl.debug_fn.glCompressedCopyTextureCHROMIUMFn != nullptr);
  g_driver_gl.debug_fn.glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

static void GL_BINDING_CALL Debug_glCompressedTexImage2D(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << border << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glCompressedTexImage2DFn != nullptr);
  g_driver_gl.debug_fn.glCompressedTexImage2DFn(
      target, level, internalformat, width, height, border, imageSize, data);
}

static void GL_BINDING_CALL Debug_glCompressedTexImage3D(GLenum target,
                                                         GLint level,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth,
                                                         GLint border,
                                                         GLsizei imageSize,
                                                         const void* data) {
  GL_SERVICE_LOG("glCompressedTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ", " << border
                 << ", " << imageSize << ", " << static_cast<const void*>(data)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCompressedTexImage3DFn != nullptr);
  g_driver_gl.debug_fn.glCompressedTexImage3DFn(target, level, internalformat,
                                                width, height, depth, border,
                                                imageSize, data);
}

static void GL_BINDING_CALL Debug_glCompressedTexSubImage2D(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLenum format,
                                                            GLsizei imageSize,
                                                            const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << imageSize << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glCompressedTexSubImage2DFn != nullptr);
  g_driver_gl.debug_fn.glCompressedTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

static void GL_BINDING_CALL Debug_glCompressedTexSubImage3D(GLenum target,
                                                            GLint level,
                                                            GLint xoffset,
                                                            GLint yoffset,
                                                            GLint zoffset,
                                                            GLsizei width,
                                                            GLsizei height,
                                                            GLsizei depth,
                                                            GLenum format,
                                                            GLsizei imageSize,
                                                            const void* data) {
  GL_SERVICE_LOG("glCompressedTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", " << imageSize << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glCompressedTexSubImage3DFn != nullptr);
  g_driver_gl.debug_fn.glCompressedTexSubImage3DFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      imageSize, data);
}

static void GL_BINDING_CALL Debug_glCopyBufferSubData(GLenum readTarget,
                                                      GLenum writeTarget,
                                                      GLintptr readOffset,
                                                      GLintptr writeOffset,
                                                      GLsizeiptr size) {
  GL_SERVICE_LOG("glCopyBufferSubData"
                 << "(" << GLEnums::GetStringEnum(readTarget) << ", "
                 << GLEnums::GetStringEnum(writeTarget) << ", " << readOffset
                 << ", " << writeOffset << ", " << size << ")");
  DCHECK(g_driver_gl.debug_fn.glCopyBufferSubDataFn != nullptr);
  g_driver_gl.debug_fn.glCopyBufferSubDataFn(readTarget, writeTarget,
                                             readOffset, writeOffset, size);
}

static void GL_BINDING_CALL
Debug_glCopySubTextureCHROMIUM(GLuint sourceId,
                               GLuint destId,
                               GLint xoffset,
                               GLint yoffset,
                               GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLboolean unpackFlipY,
                               GLboolean unpackPremultiplyAlpha,
                               GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopySubTextureCHROMIUM"
                 << "(" << sourceId << ", " << destId << ", " << xoffset << ", "
                 << yoffset << ", " << x << ", " << y << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringBool(unpackFlipY)
                 << ", " << GLEnums::GetStringBool(unpackPremultiplyAlpha)
                 << ", " << GLEnums::GetStringBool(unpackUnmultiplyAlpha)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCopySubTextureCHROMIUMFn != nullptr);
  g_driver_gl.debug_fn.glCopySubTextureCHROMIUMFn(
      sourceId, destId, xoffset, yoffset, x, y, width, height, unpackFlipY,
      unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

static void GL_BINDING_CALL Debug_glCopyTexImage2D(GLenum target,
                                                   GLint level,
                                                   GLenum internalformat,
                                                   GLint x,
                                                   GLint y,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLint border) {
  GL_SERVICE_LOG("glCopyTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", " << x
                 << ", " << y << ", " << width << ", " << height << ", "
                 << border << ")");
  DCHECK(g_driver_gl.debug_fn.glCopyTexImage2DFn != nullptr);
  g_driver_gl.debug_fn.glCopyTexImage2DFn(target, level, internalformat, x, y,
                                          width, height, border);
}

static void GL_BINDING_CALL Debug_glCopyTexSubImage2D(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLint x,
                                                      GLint y,
                                                      GLsizei width,
                                                      GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << x << ", " << y
                 << ", " << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glCopyTexSubImage2DFn != nullptr);
  g_driver_gl.debug_fn.glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x,
                                             y, width, height);
}

static void GL_BINDING_CALL Debug_glCopyTexSubImage3D(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLint zoffset,
                                                      GLint x,
                                                      GLint y,
                                                      GLsizei width,
                                                      GLsizei height) {
  GL_SERVICE_LOG("glCopyTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << x << ", " << y << ", " << width << ", " << height
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCopyTexSubImage3DFn != nullptr);
  g_driver_gl.debug_fn.glCopyTexSubImage3DFn(target, level, xoffset, yoffset,
                                             zoffset, x, y, width, height);
}

static void GL_BINDING_CALL
Debug_glCopyTextureCHROMIUM(GLuint sourceId,
                            GLuint destId,
                            GLint internalFormat,
                            GLenum destType,
                            GLboolean unpackFlipY,
                            GLboolean unpackPremultiplyAlpha,
                            GLboolean unpackUnmultiplyAlpha) {
  GL_SERVICE_LOG("glCopyTextureCHROMIUM"
                 << "(" << sourceId << ", " << destId << ", " << internalFormat
                 << ", " << GLEnums::GetStringEnum(destType) << ", "
                 << GLEnums::GetStringBool(unpackFlipY) << ", "
                 << GLEnums::GetStringBool(unpackPremultiplyAlpha) << ", "
                 << GLEnums::GetStringBool(unpackUnmultiplyAlpha) << ")");
  DCHECK(g_driver_gl.debug_fn.glCopyTextureCHROMIUMFn != nullptr);
  g_driver_gl.debug_fn.glCopyTextureCHROMIUMFn(
      sourceId, destId, internalFormat, destType, unpackFlipY,
      unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

static void GL_BINDING_CALL Debug_glCoverageModulationNV(GLenum components) {
  GL_SERVICE_LOG("glCoverageModulationNV"
                 << "(" << GLEnums::GetStringEnum(components) << ")");
  DCHECK(g_driver_gl.debug_fn.glCoverageModulationNVFn != nullptr);
  g_driver_gl.debug_fn.glCoverageModulationNVFn(components);
}

static void GL_BINDING_CALL
Debug_glCoverFillPathInstancedNV(GLsizei numPaths,
                                 GLenum pathNameType,
                                 const void* paths,
                                 GLuint pathBase,
                                 GLenum coverMode,
                                 GLenum transformType,
                                 const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glCoverFillPathInstancedNVFn != nullptr);
  g_driver_gl.debug_fn.glCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, coverMode, transformType,
      transformValues);
}

static void GL_BINDING_CALL Debug_glCoverFillPathNV(GLuint path,
                                                    GLenum coverMode) {
  GL_SERVICE_LOG("glCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCoverFillPathNVFn != nullptr);
  g_driver_gl.debug_fn.glCoverFillPathNVFn(path, coverMode);
}

static void GL_BINDING_CALL
Debug_glCoverStrokePathInstancedNV(GLsizei numPaths,
                                   GLenum pathNameType,
                                   const void* paths,
                                   GLuint pathBase,
                                   GLenum coverMode,
                                   GLenum transformType,
                                   const GLfloat* transformValues) {
  GL_SERVICE_LOG("glCoverStrokePathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glCoverStrokePathInstancedNVFn != nullptr);
  g_driver_gl.debug_fn.glCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, coverMode, transformType,
      transformValues);
}

static void GL_BINDING_CALL Debug_glCoverStrokePathNV(GLuint name,
                                                      GLenum coverMode) {
  GL_SERVICE_LOG("glCoverStrokePathNV"
                 << "(" << name << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCoverStrokePathNVFn != nullptr);
  g_driver_gl.debug_fn.glCoverStrokePathNVFn(name, coverMode);
}

static GLuint GL_BINDING_CALL Debug_glCreateProgram(void) {
  GL_SERVICE_LOG("glCreateProgram"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glCreateProgramFn != nullptr);
  GLuint result = g_driver_gl.debug_fn.glCreateProgramFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLuint GL_BINDING_CALL Debug_glCreateShader(GLenum type) {
  GL_SERVICE_LOG("glCreateShader"
                 << "(" << GLEnums::GetStringEnum(type) << ")");
  DCHECK(g_driver_gl.debug_fn.glCreateShaderFn != nullptr);
  GLuint result = g_driver_gl.debug_fn.glCreateShaderFn(type);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glCullFace(GLenum mode) {
  GL_SERVICE_LOG("glCullFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  DCHECK(g_driver_gl.debug_fn.glCullFaceFn != nullptr);
  g_driver_gl.debug_fn.glCullFaceFn(mode);
}

static void GL_BINDING_CALL
Debug_glDebugMessageCallback(GLDEBUGPROC callback, const void* userParam) {
  GL_SERVICE_LOG("glDebugMessageCallback"
                 << "(" << callback << ", "
                 << static_cast<const void*>(userParam) << ")");
  DCHECK(g_driver_gl.debug_fn.glDebugMessageCallbackFn != nullptr);
  g_driver_gl.debug_fn.glDebugMessageCallbackFn(callback, userParam);
}

static void GL_BINDING_CALL Debug_glDebugMessageControl(GLenum source,
                                                        GLenum type,
                                                        GLenum severity,
                                                        GLsizei count,
                                                        const GLuint* ids,
                                                        GLboolean enabled) {
  GL_SERVICE_LOG("glDebugMessageControl"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << count << ", "
                 << static_cast<const void*>(ids) << ", "
                 << GLEnums::GetStringBool(enabled) << ")");
  DCHECK(g_driver_gl.debug_fn.glDebugMessageControlFn != nullptr);
  g_driver_gl.debug_fn.glDebugMessageControlFn(source, type, severity, count,
                                               ids, enabled);
}

static void GL_BINDING_CALL Debug_glDebugMessageInsert(GLenum source,
                                                       GLenum type,
                                                       GLuint id,
                                                       GLenum severity,
                                                       GLsizei length,
                                                       const char* buf) {
  GL_SERVICE_LOG("glDebugMessageInsert"
                 << "(" << GLEnums::GetStringEnum(source) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << id << ", "
                 << GLEnums::GetStringEnum(severity) << ", " << length << ", "
                 << buf << ")");
  DCHECK(g_driver_gl.debug_fn.glDebugMessageInsertFn != nullptr);
  g_driver_gl.debug_fn.glDebugMessageInsertFn(source, type, id, severity,
                                              length, buf);
}

static void GL_BINDING_CALL Debug_glDeleteBuffersARB(GLsizei n,
                                                     const GLuint* buffers) {
  GL_SERVICE_LOG("glDeleteBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteBuffersARBFn != nullptr);
  g_driver_gl.debug_fn.glDeleteBuffersARBFn(n, buffers);
}

static void GL_BINDING_CALL Debug_glDeleteFencesAPPLE(GLsizei n,
                                                      const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteFencesAPPLEFn != nullptr);
  g_driver_gl.debug_fn.glDeleteFencesAPPLEFn(n, fences);
}

static void GL_BINDING_CALL Debug_glDeleteFencesNV(GLsizei n,
                                                   const GLuint* fences) {
  GL_SERVICE_LOG("glDeleteFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteFencesNVFn != nullptr);
  g_driver_gl.debug_fn.glDeleteFencesNVFn(n, fences);
}

static void GL_BINDING_CALL
Debug_glDeleteFramebuffersEXT(GLsizei n, const GLuint* framebuffers) {
  GL_SERVICE_LOG("glDeleteFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteFramebuffersEXTFn != nullptr);
  g_driver_gl.debug_fn.glDeleteFramebuffersEXTFn(n, framebuffers);
}

static void GL_BINDING_CALL Debug_glDeletePathsNV(GLuint path, GLsizei range) {
  GL_SERVICE_LOG("glDeletePathsNV"
                 << "(" << path << ", " << range << ")");
  DCHECK(g_driver_gl.debug_fn.glDeletePathsNVFn != nullptr);
  g_driver_gl.debug_fn.glDeletePathsNVFn(path, range);
}

static void GL_BINDING_CALL Debug_glDeleteProgram(GLuint program) {
  GL_SERVICE_LOG("glDeleteProgram"
                 << "(" << program << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteProgramFn != nullptr);
  g_driver_gl.debug_fn.glDeleteProgramFn(program);
}

static void GL_BINDING_CALL Debug_glDeleteQueries(GLsizei n,
                                                  const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteQueriesFn != nullptr);
  g_driver_gl.debug_fn.glDeleteQueriesFn(n, ids);
}

static void GL_BINDING_CALL
Debug_glDeleteRenderbuffersEXT(GLsizei n, const GLuint* renderbuffers) {
  GL_SERVICE_LOG("glDeleteRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteRenderbuffersEXTFn != nullptr);
  g_driver_gl.debug_fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

static void GL_BINDING_CALL Debug_glDeleteSamplers(GLsizei n,
                                                   const GLuint* samplers) {
  GL_SERVICE_LOG("glDeleteSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteSamplersFn != nullptr);
  g_driver_gl.debug_fn.glDeleteSamplersFn(n, samplers);
}

static void GL_BINDING_CALL Debug_glDeleteShader(GLuint shader) {
  GL_SERVICE_LOG("glDeleteShader"
                 << "(" << shader << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteShaderFn != nullptr);
  g_driver_gl.debug_fn.glDeleteShaderFn(shader);
}

static void GL_BINDING_CALL Debug_glDeleteSync(GLsync sync) {
  GL_SERVICE_LOG("glDeleteSync"
                 << "(" << sync << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteSyncFn != nullptr);
  g_driver_gl.debug_fn.glDeleteSyncFn(sync);
}

static void GL_BINDING_CALL Debug_glDeleteTextures(GLsizei n,
                                                   const GLuint* textures) {
  GL_SERVICE_LOG("glDeleteTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteTexturesFn != nullptr);
  g_driver_gl.debug_fn.glDeleteTexturesFn(n, textures);
}

static void GL_BINDING_CALL
Debug_glDeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
  GL_SERVICE_LOG("glDeleteTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteTransformFeedbacksFn != nullptr);
  g_driver_gl.debug_fn.glDeleteTransformFeedbacksFn(n, ids);
}

static void GL_BINDING_CALL
Debug_glDeleteVertexArraysOES(GLsizei n, const GLuint* arrays) {
  GL_SERVICE_LOG("glDeleteVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDeleteVertexArraysOESFn != nullptr);
  g_driver_gl.debug_fn.glDeleteVertexArraysOESFn(n, arrays);
}

static void GL_BINDING_CALL Debug_glDepthFunc(GLenum func) {
  GL_SERVICE_LOG("glDepthFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ")");
  DCHECK(g_driver_gl.debug_fn.glDepthFuncFn != nullptr);
  g_driver_gl.debug_fn.glDepthFuncFn(func);
}

static void GL_BINDING_CALL Debug_glDepthMask(GLboolean flag) {
  GL_SERVICE_LOG("glDepthMask"
                 << "(" << GLEnums::GetStringBool(flag) << ")");
  DCHECK(g_driver_gl.debug_fn.glDepthMaskFn != nullptr);
  g_driver_gl.debug_fn.glDepthMaskFn(flag);
}

static void GL_BINDING_CALL Debug_glDepthRange(GLclampd zNear, GLclampd zFar) {
  GL_SERVICE_LOG("glDepthRange"
                 << "(" << zNear << ", " << zFar << ")");
  DCHECK(g_driver_gl.debug_fn.glDepthRangeFn != nullptr);
  g_driver_gl.debug_fn.glDepthRangeFn(zNear, zFar);
}

static void GL_BINDING_CALL Debug_glDepthRangef(GLclampf zNear, GLclampf zFar) {
  GL_SERVICE_LOG("glDepthRangef"
                 << "(" << zNear << ", " << zFar << ")");
  DCHECK(g_driver_gl.debug_fn.glDepthRangefFn != nullptr);
  g_driver_gl.debug_fn.glDepthRangefFn(zNear, zFar);
}

static void GL_BINDING_CALL Debug_glDetachShader(GLuint program,
                                                 GLuint shader) {
  GL_SERVICE_LOG("glDetachShader"
                 << "(" << program << ", " << shader << ")");
  DCHECK(g_driver_gl.debug_fn.glDetachShaderFn != nullptr);
  g_driver_gl.debug_fn.glDetachShaderFn(program, shader);
}

static void GL_BINDING_CALL Debug_glDisable(GLenum cap) {
  GL_SERVICE_LOG("glDisable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  DCHECK(g_driver_gl.debug_fn.glDisableFn != nullptr);
  g_driver_gl.debug_fn.glDisableFn(cap);
}

static void GL_BINDING_CALL Debug_glDisableVertexAttribArray(GLuint index) {
  GL_SERVICE_LOG("glDisableVertexAttribArray"
                 << "(" << index << ")");
  DCHECK(g_driver_gl.debug_fn.glDisableVertexAttribArrayFn != nullptr);
  g_driver_gl.debug_fn.glDisableVertexAttribArrayFn(index);
}

static void GL_BINDING_CALL
Debug_glDiscardFramebufferEXT(GLenum target,
                              GLsizei numAttachments,
                              const GLenum* attachments) {
  GL_SERVICE_LOG("glDiscardFramebufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  DCHECK(g_driver_gl.debug_fn.glDiscardFramebufferEXTFn != nullptr);
  g_driver_gl.debug_fn.glDiscardFramebufferEXTFn(target, numAttachments,
                                                 attachments);
}

static void GL_BINDING_CALL Debug_glDrawArrays(GLenum mode,
                                               GLint first,
                                               GLsizei count) {
  GL_SERVICE_LOG("glDrawArrays"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawArraysFn != nullptr);
  g_driver_gl.debug_fn.glDrawArraysFn(mode, first, count);
}

static void GL_BINDING_CALL
Debug_glDrawArraysInstancedANGLE(GLenum mode,
                                 GLint first,
                                 GLsizei count,
                                 GLsizei primcount) {
  GL_SERVICE_LOG("glDrawArraysInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << first << ", "
                 << count << ", " << primcount << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawArraysInstancedANGLEFn != nullptr);
  g_driver_gl.debug_fn.glDrawArraysInstancedANGLEFn(mode, first, count,
                                                    primcount);
}

static void GL_BINDING_CALL Debug_glDrawBuffer(GLenum mode) {
  GL_SERVICE_LOG("glDrawBuffer"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawBufferFn != nullptr);
  g_driver_gl.debug_fn.glDrawBufferFn(mode);
}

static void GL_BINDING_CALL Debug_glDrawBuffersARB(GLsizei n,
                                                   const GLenum* bufs) {
  GL_SERVICE_LOG("glDrawBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(bufs) << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawBuffersARBFn != nullptr);
  g_driver_gl.debug_fn.glDrawBuffersARBFn(n, bufs);
}

static void GL_BINDING_CALL Debug_glDrawElements(GLenum mode,
                                                 GLsizei count,
                                                 GLenum type,
                                                 const void* indices) {
  GL_SERVICE_LOG("glDrawElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawElementsFn != nullptr);
  g_driver_gl.debug_fn.glDrawElementsFn(mode, count, type, indices);
}

static void GL_BINDING_CALL
Debug_glDrawElementsInstancedANGLE(GLenum mode,
                                   GLsizei count,
                                   GLenum type,
                                   const void* indices,
                                   GLsizei primcount) {
  GL_SERVICE_LOG("glDrawElementsInstancedANGLE"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << count << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(indices) << ", " << primcount
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawElementsInstancedANGLEFn != nullptr);
  g_driver_gl.debug_fn.glDrawElementsInstancedANGLEFn(mode, count, type,
                                                      indices, primcount);
}

static void GL_BINDING_CALL Debug_glDrawRangeElements(GLenum mode,
                                                      GLuint start,
                                                      GLuint end,
                                                      GLsizei count,
                                                      GLenum type,
                                                      const void* indices) {
  GL_SERVICE_LOG("glDrawRangeElements"
                 << "(" << GLEnums::GetStringEnum(mode) << ", " << start << ", "
                 << end << ", " << count << ", " << GLEnums::GetStringEnum(type)
                 << ", " << static_cast<const void*>(indices) << ")");
  DCHECK(g_driver_gl.debug_fn.glDrawRangeElementsFn != nullptr);
  g_driver_gl.debug_fn.glDrawRangeElementsFn(mode, start, end, count, type,
                                             indices);
}

static void GL_BINDING_CALL
Debug_glEGLImageTargetRenderbufferStorageOES(GLenum target,
                                             GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetRenderbufferStorageOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glEGLImageTargetRenderbufferStorageOESFn !=
         nullptr);
  g_driver_gl.debug_fn.glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

static void GL_BINDING_CALL
Debug_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
  GL_SERVICE_LOG("glEGLImageTargetTexture2DOES"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << image
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glEGLImageTargetTexture2DOESFn != nullptr);
  g_driver_gl.debug_fn.glEGLImageTargetTexture2DOESFn(target, image);
}

static void GL_BINDING_CALL Debug_glEnable(GLenum cap) {
  GL_SERVICE_LOG("glEnable"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  DCHECK(g_driver_gl.debug_fn.glEnableFn != nullptr);
  g_driver_gl.debug_fn.glEnableFn(cap);
}

static void GL_BINDING_CALL Debug_glEnableVertexAttribArray(GLuint index) {
  GL_SERVICE_LOG("glEnableVertexAttribArray"
                 << "(" << index << ")");
  DCHECK(g_driver_gl.debug_fn.glEnableVertexAttribArrayFn != nullptr);
  g_driver_gl.debug_fn.glEnableVertexAttribArrayFn(index);
}

static void GL_BINDING_CALL Debug_glEndQuery(GLenum target) {
  GL_SERVICE_LOG("glEndQuery"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  DCHECK(g_driver_gl.debug_fn.glEndQueryFn != nullptr);
  g_driver_gl.debug_fn.glEndQueryFn(target);
}

static void GL_BINDING_CALL Debug_glEndTransformFeedback(void) {
  GL_SERVICE_LOG("glEndTransformFeedback"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glEndTransformFeedbackFn != nullptr);
  g_driver_gl.debug_fn.glEndTransformFeedbackFn();
}

static GLsync GL_BINDING_CALL Debug_glFenceSync(GLenum condition,
                                                GLbitfield flags) {
  GL_SERVICE_LOG("glFenceSync"
                 << "(" << GLEnums::GetStringEnum(condition) << ", " << flags
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glFenceSyncFn != nullptr);
  GLsync result = g_driver_gl.debug_fn.glFenceSyncFn(condition, flags);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glFinish(void) {
  GL_SERVICE_LOG("glFinish"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glFinishFn != nullptr);
  g_driver_gl.debug_fn.glFinishFn();
}

static void GL_BINDING_CALL Debug_glFinishFenceAPPLE(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceAPPLE"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glFinishFenceAPPLEFn != nullptr);
  g_driver_gl.debug_fn.glFinishFenceAPPLEFn(fence);
}

static void GL_BINDING_CALL Debug_glFinishFenceNV(GLuint fence) {
  GL_SERVICE_LOG("glFinishFenceNV"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glFinishFenceNVFn != nullptr);
  g_driver_gl.debug_fn.glFinishFenceNVFn(fence);
}

static void GL_BINDING_CALL Debug_glFlush(void) {
  GL_SERVICE_LOG("glFlush"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glFlushFn != nullptr);
  g_driver_gl.debug_fn.glFlushFn();
}

static void GL_BINDING_CALL Debug_glFlushMappedBufferRange(GLenum target,
                                                           GLintptr offset,
                                                           GLsizeiptr length) {
  GL_SERVICE_LOG("glFlushMappedBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ")");
  DCHECK(g_driver_gl.debug_fn.glFlushMappedBufferRangeFn != nullptr);
  g_driver_gl.debug_fn.glFlushMappedBufferRangeFn(target, offset, length);
}

static void GL_BINDING_CALL
Debug_glFramebufferRenderbufferEXT(GLenum target,
                                   GLenum attachment,
                                   GLenum renderbuffertarget,
                                   GLuint renderbuffer) {
  GL_SERVICE_LOG("glFramebufferRenderbufferEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(renderbuffertarget) << ", "
                 << renderbuffer << ")");
  DCHECK(g_driver_gl.debug_fn.glFramebufferRenderbufferEXTFn != nullptr);
  g_driver_gl.debug_fn.glFramebufferRenderbufferEXTFn(
      target, attachment, renderbuffertarget, renderbuffer);
}

static void GL_BINDING_CALL Debug_glFramebufferTexture2DEXT(GLenum target,
                                                            GLenum attachment,
                                                            GLenum textarget,
                                                            GLuint texture,
                                                            GLint level) {
  GL_SERVICE_LOG("glFramebufferTexture2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ")");
  DCHECK(g_driver_gl.debug_fn.glFramebufferTexture2DEXTFn != nullptr);
  g_driver_gl.debug_fn.glFramebufferTexture2DEXTFn(target, attachment,
                                                   textarget, texture, level);
}

static void GL_BINDING_CALL
Debug_glFramebufferTexture2DMultisampleEXT(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples) {
  GL_SERVICE_LOG("glFramebufferTexture2DMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ", " << samples << ")");
  DCHECK(g_driver_gl.debug_fn.glFramebufferTexture2DMultisampleEXTFn !=
         nullptr);
  g_driver_gl.debug_fn.glFramebufferTexture2DMultisampleEXTFn(
      target, attachment, textarget, texture, level, samples);
}

static void GL_BINDING_CALL
Debug_glFramebufferTexture2DMultisampleIMG(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples) {
  GL_SERVICE_LOG("glFramebufferTexture2DMultisampleIMG"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(textarget) << ", " << texture << ", "
                 << level << ", " << samples << ")");
  DCHECK(g_driver_gl.debug_fn.glFramebufferTexture2DMultisampleIMGFn !=
         nullptr);
  g_driver_gl.debug_fn.glFramebufferTexture2DMultisampleIMGFn(
      target, attachment, textarget, texture, level, samples);
}

static void GL_BINDING_CALL Debug_glFramebufferTextureLayer(GLenum target,
                                                            GLenum attachment,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLint layer) {
  GL_SERVICE_LOG("glFramebufferTextureLayer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", " << texture
                 << ", " << level << ", " << layer << ")");
  DCHECK(g_driver_gl.debug_fn.glFramebufferTextureLayerFn != nullptr);
  g_driver_gl.debug_fn.glFramebufferTextureLayerFn(target, attachment, texture,
                                                   level, layer);
}

static void GL_BINDING_CALL Debug_glFrontFace(GLenum mode) {
  GL_SERVICE_LOG("glFrontFace"
                 << "(" << GLEnums::GetStringEnum(mode) << ")");
  DCHECK(g_driver_gl.debug_fn.glFrontFaceFn != nullptr);
  g_driver_gl.debug_fn.glFrontFaceFn(mode);
}

static void GL_BINDING_CALL Debug_glGenBuffersARB(GLsizei n, GLuint* buffers) {
  GL_SERVICE_LOG("glGenBuffersARB"
                 << "(" << n << ", " << static_cast<const void*>(buffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenBuffersARBFn != nullptr);
  g_driver_gl.debug_fn.glGenBuffersARBFn(n, buffers);
}

static void GL_BINDING_CALL Debug_glGenerateMipmapEXT(GLenum target) {
  GL_SERVICE_LOG("glGenerateMipmapEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  DCHECK(g_driver_gl.debug_fn.glGenerateMipmapEXTFn != nullptr);
  g_driver_gl.debug_fn.glGenerateMipmapEXTFn(target);
}

static void GL_BINDING_CALL Debug_glGenFencesAPPLE(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesAPPLE"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenFencesAPPLEFn != nullptr);
  g_driver_gl.debug_fn.glGenFencesAPPLEFn(n, fences);
}

static void GL_BINDING_CALL Debug_glGenFencesNV(GLsizei n, GLuint* fences) {
  GL_SERVICE_LOG("glGenFencesNV"
                 << "(" << n << ", " << static_cast<const void*>(fences)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenFencesNVFn != nullptr);
  g_driver_gl.debug_fn.glGenFencesNVFn(n, fences);
}

static void GL_BINDING_CALL Debug_glGenFramebuffersEXT(GLsizei n,
                                                       GLuint* framebuffers) {
  GL_SERVICE_LOG("glGenFramebuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(framebuffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenFramebuffersEXTFn != nullptr);
  g_driver_gl.debug_fn.glGenFramebuffersEXTFn(n, framebuffers);
}

static GLuint GL_BINDING_CALL Debug_glGenPathsNV(GLsizei range) {
  GL_SERVICE_LOG("glGenPathsNV"
                 << "(" << range << ")");
  DCHECK(g_driver_gl.debug_fn.glGenPathsNVFn != nullptr);
  GLuint result = g_driver_gl.debug_fn.glGenPathsNVFn(range);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGenQueries(GLsizei n, GLuint* ids) {
  GL_SERVICE_LOG("glGenQueries"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  DCHECK(g_driver_gl.debug_fn.glGenQueriesFn != nullptr);
  g_driver_gl.debug_fn.glGenQueriesFn(n, ids);
}

static void GL_BINDING_CALL Debug_glGenRenderbuffersEXT(GLsizei n,
                                                        GLuint* renderbuffers) {
  GL_SERVICE_LOG("glGenRenderbuffersEXT"
                 << "(" << n << ", " << static_cast<const void*>(renderbuffers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenRenderbuffersEXTFn != nullptr);
  g_driver_gl.debug_fn.glGenRenderbuffersEXTFn(n, renderbuffers);
}

static void GL_BINDING_CALL Debug_glGenSamplers(GLsizei n, GLuint* samplers) {
  GL_SERVICE_LOG("glGenSamplers"
                 << "(" << n << ", " << static_cast<const void*>(samplers)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenSamplersFn != nullptr);
  g_driver_gl.debug_fn.glGenSamplersFn(n, samplers);
}

static void GL_BINDING_CALL Debug_glGenTextures(GLsizei n, GLuint* textures) {
  GL_SERVICE_LOG("glGenTextures"
                 << "(" << n << ", " << static_cast<const void*>(textures)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenTexturesFn != nullptr);
  g_driver_gl.debug_fn.glGenTexturesFn(n, textures);
}

static void GL_BINDING_CALL Debug_glGenTransformFeedbacks(GLsizei n,
                                                          GLuint* ids) {
  GL_SERVICE_LOG("glGenTransformFeedbacks"
                 << "(" << n << ", " << static_cast<const void*>(ids) << ")");
  DCHECK(g_driver_gl.debug_fn.glGenTransformFeedbacksFn != nullptr);
  g_driver_gl.debug_fn.glGenTransformFeedbacksFn(n, ids);
}

static void GL_BINDING_CALL Debug_glGenVertexArraysOES(GLsizei n,
                                                       GLuint* arrays) {
  GL_SERVICE_LOG("glGenVertexArraysOES"
                 << "(" << n << ", " << static_cast<const void*>(arrays)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGenVertexArraysOESFn != nullptr);
  g_driver_gl.debug_fn.glGenVertexArraysOESFn(n, arrays);
}

static void GL_BINDING_CALL Debug_glGetActiveAttrib(GLuint program,
                                                    GLuint index,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    GLint* size,
                                                    GLenum* type,
                                                    char* name) {
  GL_SERVICE_LOG("glGetActiveAttrib"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveAttribFn != nullptr);
  g_driver_gl.debug_fn.glGetActiveAttribFn(program, index, bufsize, length,
                                           size, type, name);
}

static void GL_BINDING_CALL Debug_glGetActiveUniform(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     GLint* size,
                                                     GLenum* type,
                                                     char* name) {
  GL_SERVICE_LOG("glGetActiveUniform"
                 << "(" << program << ", " << index << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveUniformFn != nullptr);
  g_driver_gl.debug_fn.glGetActiveUniformFn(program, index, bufsize, length,
                                            size, type, name);
}

static void GL_BINDING_CALL
Debug_glGetActiveUniformBlockiv(GLuint program,
                                GLuint uniformBlockIndex,
                                GLenum pname,
                                GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockiv"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveUniformBlockivFn != nullptr);
  g_driver_gl.debug_fn.glGetActiveUniformBlockivFn(program, uniformBlockIndex,
                                                   pname, params);
}

static void GL_BINDING_CALL
Debug_glGetActiveUniformBlockivRobustANGLE(GLuint program,
                                           GLuint uniformBlockIndex,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformBlockivRobustANGLE"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveUniformBlockivRobustANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

static void GL_BINDING_CALL
Debug_glGetActiveUniformBlockName(GLuint program,
                                  GLuint uniformBlockIndex,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  char* uniformBlockName) {
  GL_SERVICE_LOG("glGetActiveUniformBlockName"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(uniformBlockName) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveUniformBlockNameFn != nullptr);
  g_driver_gl.debug_fn.glGetActiveUniformBlockNameFn(
      program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

static void GL_BINDING_CALL
Debug_glGetActiveUniformsiv(GLuint program,
                            GLsizei uniformCount,
                            const GLuint* uniformIndices,
                            GLenum pname,
                            GLint* params) {
  GL_SERVICE_LOG("glGetActiveUniformsiv"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformIndices) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetActiveUniformsivFn != nullptr);
  g_driver_gl.debug_fn.glGetActiveUniformsivFn(program, uniformCount,
                                               uniformIndices, pname, params);
}

static void GL_BINDING_CALL Debug_glGetAttachedShaders(GLuint program,
                                                       GLsizei maxcount,
                                                       GLsizei* count,
                                                       GLuint* shaders) {
  GL_SERVICE_LOG("glGetAttachedShaders"
                 << "(" << program << ", " << maxcount << ", "
                 << static_cast<const void*>(count) << ", "
                 << static_cast<const void*>(shaders) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetAttachedShadersFn != nullptr);
  g_driver_gl.debug_fn.glGetAttachedShadersFn(program, maxcount, count,
                                              shaders);
}

static GLint GL_BINDING_CALL Debug_glGetAttribLocation(GLuint program,
                                                       const char* name) {
  GL_SERVICE_LOG("glGetAttribLocation"
                 << "(" << program << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glGetAttribLocationFn != nullptr);
  GLint result = g_driver_gl.debug_fn.glGetAttribLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGetBooleani_vRobustANGLE(GLenum target,
                                                             GLuint index,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleani_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBooleani_vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetBooleani_vRobustANGLEFn(target, index, bufSize,
                                                    length, data);
}

static void GL_BINDING_CALL Debug_glGetBooleanv(GLenum pname,
                                                GLboolean* params) {
  GL_SERVICE_LOG("glGetBooleanv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBooleanvFn != nullptr);
  g_driver_gl.debug_fn.glGetBooleanvFn(pname, params);
}

static void GL_BINDING_CALL Debug_glGetBooleanvRobustANGLE(GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLboolean* data) {
  GL_SERVICE_LOG("glGetBooleanvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBooleanvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

static void GL_BINDING_CALL
Debug_glGetBufferParameteri64vRobustANGLE(GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint64* params) {
  GL_SERVICE_LOG("glGetBufferParameteri64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBufferParameteri64vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetBufferParameteri64vRobustANGLEFn(
      target, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetBufferParameteriv(GLenum target,
                                                         GLenum pname,
                                                         GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBufferParameterivFn != nullptr);
  g_driver_gl.debug_fn.glGetBufferParameterivFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetBufferParameterivRobustANGLE(GLenum target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        GLint* params) {
  GL_SERVICE_LOG("glGetBufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBufferParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetBufferParameterivRobustANGLEFn(
      target, pname, bufSize, length, params);
}

static void GL_BINDING_CALL
Debug_glGetBufferPointervRobustANGLE(GLenum target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     void** params) {
  GL_SERVICE_LOG("glGetBufferPointervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", " << params << ")");
  DCHECK(g_driver_gl.debug_fn.glGetBufferPointervRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetBufferPointervRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL Debug_glGetDebugMessageLog(GLuint count,
                                                       GLsizei bufSize,
                                                       GLenum* sources,
                                                       GLenum* types,
                                                       GLuint* ids,
                                                       GLenum* severities,
                                                       GLsizei* lengths,
                                                       char* messageLog) {
  GL_SERVICE_LOG("glGetDebugMessageLog"
                 << "(" << count << ", " << bufSize << ", "
                 << static_cast<const void*>(sources) << ", "
                 << static_cast<const void*>(types) << ", "
                 << static_cast<const void*>(ids) << ", "
                 << static_cast<const void*>(severities) << ", "
                 << static_cast<const void*>(lengths) << ", "
                 << static_cast<const void*>(messageLog) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetDebugMessageLogFn != nullptr);
  g_driver_gl.debug_fn.glGetDebugMessageLogFn(
      count, bufSize, sources, types, ids, severities, lengths, messageLog);
}

static GLenum GL_BINDING_CALL Debug_glGetError(void) {
  GL_SERVICE_LOG("glGetError"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetErrorFn != nullptr);
  GLenum result = g_driver_gl.debug_fn.glGetErrorFn();

  GL_SERVICE_LOG("GL_RESULT: " << GLEnums::GetStringError(result));

  return result;
}

static void GL_BINDING_CALL Debug_glGetFenceivNV(GLuint fence,
                                                 GLenum pname,
                                                 GLint* params) {
  GL_SERVICE_LOG("glGetFenceivNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFenceivNVFn != nullptr);
  g_driver_gl.debug_fn.glGetFenceivNVFn(fence, pname, params);
}

static void GL_BINDING_CALL Debug_glGetFloatv(GLenum pname, GLfloat* params) {
  GL_SERVICE_LOG("glGetFloatv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFloatvFn != nullptr);
  g_driver_gl.debug_fn.glGetFloatvFn(pname, params);
}

static void GL_BINDING_CALL Debug_glGetFloatvRobustANGLE(GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLfloat* data) {
  GL_SERVICE_LOG("glGetFloatvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFloatvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

static GLint GL_BINDING_CALL Debug_glGetFragDataIndex(GLuint program,
                                                      const char* name) {
  GL_SERVICE_LOG("glGetFragDataIndex"
                 << "(" << program << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFragDataIndexFn != nullptr);
  GLint result = g_driver_gl.debug_fn.glGetFragDataIndexFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLint GL_BINDING_CALL Debug_glGetFragDataLocation(GLuint program,
                                                         const char* name) {
  GL_SERVICE_LOG("glGetFragDataLocation"
                 << "(" << program << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFragDataLocationFn != nullptr);
  GLint result = g_driver_gl.debug_fn.glGetFragDataLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL
Debug_glGetFramebufferAttachmentParameterivEXT(GLenum target,
                                               GLenum attachment,
                                               GLenum pname,
                                               GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFramebufferAttachmentParameterivEXTFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetFramebufferAttachmentParameterivEXTFn(
      target, attachment, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetFramebufferAttachmentParameterivRobustANGLE(GLenum target,
                                                       GLenum attachment,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferAttachmentParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(attachment) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(
      g_driver_gl.debug_fn.glGetFramebufferAttachmentParameterivRobustANGLEFn !=
      nullptr);
  g_driver_gl.debug_fn.glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

static void GL_BINDING_CALL
Debug_glGetFramebufferParameterivRobustANGLE(GLenum target,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  GL_SERVICE_LOG("glGetFramebufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetFramebufferParameterivRobustANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetFramebufferParameterivRobustANGLEFn(
      target, pname, bufSize, length, params);
}

static GLenum GL_BINDING_CALL Debug_glGetGraphicsResetStatusARB(void) {
  GL_SERVICE_LOG("glGetGraphicsResetStatusARB"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetGraphicsResetStatusARBFn != nullptr);
  GLenum result = g_driver_gl.debug_fn.glGetGraphicsResetStatusARBFn();
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGetInteger64i_v(GLenum target,
                                                    GLuint index,
                                                    GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInteger64i_vFn != nullptr);
  g_driver_gl.debug_fn.glGetInteger64i_vFn(target, index, data);
}

static void GL_BINDING_CALL Debug_glGetInteger64i_vRobustANGLE(GLenum target,
                                                               GLuint index,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64i_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInteger64i_vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetInteger64i_vRobustANGLEFn(target, index, bufSize,
                                                      length, data);
}

static void GL_BINDING_CALL Debug_glGetInteger64v(GLenum pname,
                                                  GLint64* params) {
  GL_SERVICE_LOG("glGetInteger64v"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInteger64vFn != nullptr);
  g_driver_gl.debug_fn.glGetInteger64vFn(pname, params);
}

static void GL_BINDING_CALL Debug_glGetInteger64vRobustANGLE(GLenum pname,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint64* data) {
  GL_SERVICE_LOG("glGetInteger64vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInteger64vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetInteger64vRobustANGLEFn(pname, bufSize, length,
                                                    data);
}

static void GL_BINDING_CALL Debug_glGetIntegeri_v(GLenum target,
                                                  GLuint index,
                                                  GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_v"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetIntegeri_vFn != nullptr);
  g_driver_gl.debug_fn.glGetIntegeri_vFn(target, index, data);
}

static void GL_BINDING_CALL Debug_glGetIntegeri_vRobustANGLE(GLenum target,
                                                             GLuint index,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* data) {
  GL_SERVICE_LOG("glGetIntegeri_vRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetIntegeri_vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetIntegeri_vRobustANGLEFn(target, index, bufSize,
                                                    length, data);
}

static void GL_BINDING_CALL Debug_glGetIntegerv(GLenum pname, GLint* params) {
  GL_SERVICE_LOG("glGetIntegerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetIntegervFn != nullptr);
  g_driver_gl.debug_fn.glGetIntegervFn(pname, params);
}

static void GL_BINDING_CALL Debug_glGetIntegervRobustANGLE(GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* data) {
  GL_SERVICE_LOG("glGetIntegervRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetIntegervRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

static void GL_BINDING_CALL Debug_glGetInternalformativ(GLenum target,
                                                        GLenum internalformat,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativ"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInternalformativFn != nullptr);
  g_driver_gl.debug_fn.glGetInternalformativFn(target, internalformat, pname,
                                               bufSize, params);
}

static void GL_BINDING_CALL
Debug_glGetInternalformativRobustANGLE(GLenum target,
                                       GLenum internalformat,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLint* params) {
  GL_SERVICE_LOG("glGetInternalformativRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetInternalformativRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetInternalformativRobustANGLEFn(
      target, internalformat, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetMultisamplefvRobustANGLE(GLenum pname,
                                                                GLuint index,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLfloat* val) {
  GL_SERVICE_LOG("glGetMultisamplefvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(val) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetMultisamplefvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetMultisamplefvRobustANGLEFn(pname, index, bufSize,
                                                       length, val);
}

static void GL_BINDING_CALL Debug_glGetnUniformfvRobustANGLE(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLfloat* params) {
  GL_SERVICE_LOG("glGetnUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetnUniformfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetnUniformfvRobustANGLEFn(program, location, bufSize,
                                                    length, params);
}

static void GL_BINDING_CALL Debug_glGetnUniformivRobustANGLE(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLint* params) {
  GL_SERVICE_LOG("glGetnUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetnUniformivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetnUniformivRobustANGLEFn(program, location, bufSize,
                                                    length, params);
}

static void GL_BINDING_CALL Debug_glGetnUniformuivRobustANGLE(GLuint program,
                                                              GLint location,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLuint* params) {
  GL_SERVICE_LOG("glGetnUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetnUniformuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetnUniformuivRobustANGLEFn(program, location, bufSize,
                                                     length, params);
}

static void GL_BINDING_CALL Debug_glGetObjectLabel(GLenum identifier,
                                                   GLuint name,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   char* label) {
  GL_SERVICE_LOG("glGetObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(label) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetObjectLabelFn != nullptr);
  g_driver_gl.debug_fn.glGetObjectLabelFn(identifier, name, bufSize, length,
                                          label);
}

static void GL_BINDING_CALL Debug_glGetObjectPtrLabel(void* ptr,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      char* label) {
  GL_SERVICE_LOG("glGetObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(label) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetObjectPtrLabelFn != nullptr);
  g_driver_gl.debug_fn.glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

static void GL_BINDING_CALL Debug_glGetPointerv(GLenum pname, void** params) {
  GL_SERVICE_LOG("glGetPointerv"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << params
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetPointervFn != nullptr);
  g_driver_gl.debug_fn.glGetPointervFn(pname, params);
}

static void GL_BINDING_CALL
Debug_glGetPointervRobustANGLERobustANGLE(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          void** params) {
  GL_SERVICE_LOG("glGetPointervRobustANGLERobustANGLE"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", " << params
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetPointervRobustANGLERobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetPointervRobustANGLERobustANGLEFn(pname, bufSize,
                                                             length, params);
}

static void GL_BINDING_CALL Debug_glGetProgramBinary(GLuint program,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLenum* binaryFormat,
                                                     GLvoid* binary) {
  GL_SERVICE_LOG("glGetProgramBinary"
                 << "(" << program << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramBinaryFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramBinaryFn(program, bufSize, length,
                                            binaryFormat, binary);
}

static void GL_BINDING_CALL Debug_glGetProgramInfoLog(GLuint program,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      char* infolog) {
  GL_SERVICE_LOG("glGetProgramInfoLog"
                 << "(" << program << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramInfoLogFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

static void GL_BINDING_CALL
Debug_glGetProgramInterfaceiv(GLuint program,
                              GLenum programInterface,
                              GLenum pname,
                              GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramInterfaceivFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramInterfaceivFn(program, programInterface,
                                                 pname, params);
}

static void GL_BINDING_CALL
Debug_glGetProgramInterfaceivRobustANGLE(GLuint program,
                                         GLenum programInterface,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetProgramInterfaceivRobustANGLE"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramInterfaceivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramInterfaceivRobustANGLEFn(
      program, programInterface, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetProgramiv(GLuint program,
                                                 GLenum pname,
                                                 GLint* params) {
  GL_SERVICE_LOG("glGetProgramiv"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramivFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramivFn(program, pname, params);
}

static void GL_BINDING_CALL Debug_glGetProgramivRobustANGLE(GLuint program,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* params) {
  GL_SERVICE_LOG("glGetProgramivRobustANGLE"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramivRobustANGLEFn(program, pname, bufSize,
                                                   length, params);
}

static void GL_BINDING_CALL
Debug_glGetProgramResourceiv(GLuint program,
                             GLenum programInterface,
                             GLuint index,
                             GLsizei propCount,
                             const GLenum* props,
                             GLsizei bufSize,
                             GLsizei* length,
                             GLint* params) {
  GL_SERVICE_LOG("glGetProgramResourceiv"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << propCount << ", " << static_cast<const void*>(props)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramResourceivFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramResourceivFn(program, programInterface,
                                                index, propCount, props,
                                                bufSize, length, params);
}

static GLint GL_BINDING_CALL
Debug_glGetProgramResourceLocation(GLuint program,
                                   GLenum programInterface,
                                   const char* name) {
  GL_SERVICE_LOG("glGetProgramResourceLocation"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << name
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramResourceLocationFn != nullptr);
  GLint result = g_driver_gl.debug_fn.glGetProgramResourceLocationFn(
      program, programInterface, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL
Debug_glGetProgramResourceName(GLuint program,
                               GLenum programInterface,
                               GLuint index,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLchar* name) {
  GL_SERVICE_LOG("glGetProgramResourceName"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(programInterface) << ", " << index
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(name) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetProgramResourceNameFn != nullptr);
  g_driver_gl.debug_fn.glGetProgramResourceNameFn(program, programInterface,
                                                  index, bufSize, length, name);
}

static void GL_BINDING_CALL Debug_glGetQueryiv(GLenum target,
                                               GLenum pname,
                                               GLint* params) {
  GL_SERVICE_LOG("glGetQueryiv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryivFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryivFn(target, pname, params);
}

static void GL_BINDING_CALL Debug_glGetQueryivRobustANGLE(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  GL_SERVICE_LOG("glGetQueryivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryivRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

static void GL_BINDING_CALL Debug_glGetQueryObjecti64v(GLuint id,
                                                       GLenum pname,
                                                       GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjecti64vFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjecti64vFn(id, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetQueryObjecti64vRobustANGLE(GLuint id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint64* params) {
  GL_SERVICE_LOG("glGetQueryObjecti64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjecti64vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize,
                                                         length, params);
}

static void GL_BINDING_CALL Debug_glGetQueryObjectiv(GLuint id,
                                                     GLenum pname,
                                                     GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectivFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectivFn(id, pname, params);
}

static void GL_BINDING_CALL Debug_glGetQueryObjectivRobustANGLE(GLuint id,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params) {
  GL_SERVICE_LOG("glGetQueryObjectivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectivRobustANGLEFn(id, pname, bufSize,
                                                       length, params);
}

static void GL_BINDING_CALL Debug_glGetQueryObjectui64v(GLuint id,
                                                        GLenum pname,
                                                        GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64v"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectui64vFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectui64vFn(id, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetQueryObjectui64vRobustANGLE(GLuint id,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLuint64* params) {
  GL_SERVICE_LOG("glGetQueryObjectui64vRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectui64vRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize,
                                                          length, params);
}

static void GL_BINDING_CALL Debug_glGetQueryObjectuiv(GLuint id,
                                                      GLenum pname,
                                                      GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuiv"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectuivFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectuivFn(id, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetQueryObjectuivRobustANGLE(GLuint id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLuint* params) {
  GL_SERVICE_LOG("glGetQueryObjectuivRobustANGLE"
                 << "(" << id << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetQueryObjectuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL
Debug_glGetRenderbufferParameterivEXT(GLenum target,
                                      GLenum pname,
                                      GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetRenderbufferParameterivEXTFn != nullptr);
  g_driver_gl.debug_fn.glGetRenderbufferParameterivEXTFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetRenderbufferParameterivRobustANGLE(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  GL_SERVICE_LOG("glGetRenderbufferParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetRenderbufferParameterivRobustANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetRenderbufferParameterivRobustANGLEFn(
      target, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetSamplerParameterfv(GLuint sampler,
                                                          GLenum pname,
                                                          GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterfvFn != nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterfvFn(sampler, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetSamplerParameterfvRobustANGLE(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLfloat* params) {
  GL_SERVICE_LOG("glGetSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterfvRobustANGLEFn(
      sampler, pname, bufSize, length, params);
}

static void GL_BINDING_CALL
Debug_glGetSamplerParameterIivRobustANGLE(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterIivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterIivRobustANGLEFn(
      sampler, pname, bufSize, length, params);
}

static void GL_BINDING_CALL
Debug_glGetSamplerParameterIuivRobustANGLE(GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLuint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterIuivRobustANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterIuivRobustANGLEFn(
      sampler, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetSamplerParameteriv(GLuint sampler,
                                                          GLenum pname,
                                                          GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterivFn != nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterivFn(sampler, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetSamplerParameterivRobustANGLE(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) {
  GL_SERVICE_LOG("glGetSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSamplerParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetSamplerParameterivRobustANGLEFn(
      sampler, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetShaderInfoLog(GLuint shader,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     char* infolog) {
  GL_SERVICE_LOG("glGetShaderInfoLog"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(infolog) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetShaderInfoLogFn != nullptr);
  g_driver_gl.debug_fn.glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

static void GL_BINDING_CALL Debug_glGetShaderiv(GLuint shader,
                                                GLenum pname,
                                                GLint* params) {
  GL_SERVICE_LOG("glGetShaderiv"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetShaderivFn != nullptr);
  g_driver_gl.debug_fn.glGetShaderivFn(shader, pname, params);
}

static void GL_BINDING_CALL Debug_glGetShaderivRobustANGLE(GLuint shader,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  GL_SERVICE_LOG("glGetShaderivRobustANGLE"
                 << "(" << shader << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetShaderivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetShaderivRobustANGLEFn(shader, pname, bufSize,
                                                  length, params);
}

static void GL_BINDING_CALL
Debug_glGetShaderPrecisionFormat(GLenum shadertype,
                                 GLenum precisiontype,
                                 GLint* range,
                                 GLint* precision) {
  GL_SERVICE_LOG("glGetShaderPrecisionFormat"
                 << "(" << GLEnums::GetStringEnum(shadertype) << ", "
                 << GLEnums::GetStringEnum(precisiontype) << ", "
                 << static_cast<const void*>(range) << ", "
                 << static_cast<const void*>(precision) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetShaderPrecisionFormatFn != nullptr);
  g_driver_gl.debug_fn.glGetShaderPrecisionFormatFn(shadertype, precisiontype,
                                                    range, precision);
}

static void GL_BINDING_CALL Debug_glGetShaderSource(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  GL_SERVICE_LOG("glGetShaderSource"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetShaderSourceFn != nullptr);
  g_driver_gl.debug_fn.glGetShaderSourceFn(shader, bufsize, length, source);
}

static const GLubyte* GL_BINDING_CALL Debug_glGetString(GLenum name) {
  GL_SERVICE_LOG("glGetString"
                 << "(" << GLEnums::GetStringEnum(name) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetStringFn != nullptr);
  const GLubyte* result = g_driver_gl.debug_fn.glGetStringFn(name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static const GLubyte* GL_BINDING_CALL Debug_glGetStringi(GLenum name,
                                                         GLuint index) {
  GL_SERVICE_LOG("glGetStringi"
                 << "(" << GLEnums::GetStringEnum(name) << ", " << index
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glGetStringiFn != nullptr);
  const GLubyte* result = g_driver_gl.debug_fn.glGetStringiFn(name, index);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGetSynciv(GLsync sync,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* values) {
  GL_SERVICE_LOG("glGetSynciv"
                 << "(" << sync << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << bufSize << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(values) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetSyncivFn != nullptr);
  g_driver_gl.debug_fn.glGetSyncivFn(sync, pname, bufSize, length, values);
}

static void GL_BINDING_CALL Debug_glGetTexLevelParameterfv(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexLevelParameterfvFn != nullptr);
  g_driver_gl.debug_fn.glGetTexLevelParameterfvFn(target, level, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetTexLevelParameterfvRobustANGLE(GLenum target,
                                          GLint level,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexLevelParameterfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexLevelParameterfvRobustANGLEFn(
      target, level, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetTexLevelParameteriv(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexLevelParameterivFn != nullptr);
  g_driver_gl.debug_fn.glGetTexLevelParameterivFn(target, level, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetTexLevelParameterivRobustANGLE(GLenum target,
                                          GLint level,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  GL_SERVICE_LOG("glGetTexLevelParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << GLEnums::GetStringEnum(pname) << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexLevelParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexLevelParameterivRobustANGLEFn(
      target, level, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetTexParameterfv(GLenum target,
                                                      GLenum pname,
                                                      GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterfvFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterfvFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetTexParameterfvRobustANGLE(GLenum target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLfloat* params) {
  GL_SERVICE_LOG("glGetTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterfvRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL
Debug_glGetTexParameterIivRobustANGLE(GLenum target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterIivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterIivRobustANGLEFn(target, pname, bufSize,
                                                         length, params);
}

static void GL_BINDING_CALL
Debug_glGetTexParameterIuivRobustANGLE(GLenum target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLuint* params) {
  GL_SERVICE_LOG("glGetTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterIuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterIuivRobustANGLEFn(
      target, pname, bufSize, length, params);
}

static void GL_BINDING_CALL Debug_glGetTexParameteriv(GLenum target,
                                                      GLenum pname,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterivFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterivFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetTexParameterivRobustANGLE(GLenum target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint* params) {
  GL_SERVICE_LOG("glGetTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTexParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTexParameterivRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL Debug_glGetTransformFeedbackVarying(GLuint program,
                                                                GLuint index,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLsizei* size,
                                                                GLenum* type,
                                                                char* name) {
  GL_SERVICE_LOG("glGetTransformFeedbackVarying"
                 << "(" << program << ", " << index << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(size) << ", "
                 << static_cast<const void*>(type) << ", "
                 << static_cast<const void*>(name) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTransformFeedbackVaryingFn != nullptr);
  g_driver_gl.debug_fn.glGetTransformFeedbackVaryingFn(
      program, index, bufSize, length, size, type, name);
}

static void GL_BINDING_CALL
Debug_glGetTranslatedShaderSourceANGLE(GLuint shader,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* source) {
  GL_SERVICE_LOG("glGetTranslatedShaderSourceANGLE"
                 << "(" << shader << ", " << bufsize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(source) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetTranslatedShaderSourceANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetTranslatedShaderSourceANGLEFn(shader, bufsize,
                                                          length, source);
}

static GLuint GL_BINDING_CALL
Debug_glGetUniformBlockIndex(GLuint program, const char* uniformBlockName) {
  GL_SERVICE_LOG("glGetUniformBlockIndex"
                 << "(" << program << ", " << uniformBlockName << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformBlockIndexFn != nullptr);
  GLuint result =
      g_driver_gl.debug_fn.glGetUniformBlockIndexFn(program, uniformBlockName);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGetUniformfv(GLuint program,
                                                 GLint location,
                                                 GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformfvFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformfvFn(program, location, params);
}

static void GL_BINDING_CALL Debug_glGetUniformfvRobustANGLE(GLuint program,
                                                            GLint location,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLfloat* params) {
  GL_SERVICE_LOG("glGetUniformfvRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformfvRobustANGLEFn(program, location, bufSize,
                                                   length, params);
}

static void GL_BINDING_CALL
Debug_glGetUniformIndices(GLuint program,
                          GLsizei uniformCount,
                          const char* const* uniformNames,
                          GLuint* uniformIndices) {
  GL_SERVICE_LOG("glGetUniformIndices"
                 << "(" << program << ", " << uniformCount << ", "
                 << static_cast<const void*>(uniformNames) << ", "
                 << static_cast<const void*>(uniformIndices) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformIndicesFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformIndicesFn(program, uniformCount,
                                             uniformNames, uniformIndices);
}

static void GL_BINDING_CALL Debug_glGetUniformiv(GLuint program,
                                                 GLint location,
                                                 GLint* params) {
  GL_SERVICE_LOG("glGetUniformiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformivFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformivFn(program, location, params);
}

static void GL_BINDING_CALL Debug_glGetUniformivRobustANGLE(GLuint program,
                                                            GLint location,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLint* params) {
  GL_SERVICE_LOG("glGetUniformivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformivRobustANGLEFn(program, location, bufSize,
                                                   length, params);
}

static GLint GL_BINDING_CALL Debug_glGetUniformLocation(GLuint program,
                                                        const char* name) {
  GL_SERVICE_LOG("glGetUniformLocation"
                 << "(" << program << ", " << name << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformLocationFn != nullptr);
  GLint result = g_driver_gl.debug_fn.glGetUniformLocationFn(program, name);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glGetUniformuiv(GLuint program,
                                                  GLint location,
                                                  GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuiv"
                 << "(" << program << ", " << location << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformuivFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformuivFn(program, location, params);
}

static void GL_BINDING_CALL Debug_glGetUniformuivRobustANGLE(GLuint program,
                                                             GLint location,
                                                             GLsizei bufSize,
                                                             GLsizei* length,
                                                             GLuint* params) {
  GL_SERVICE_LOG("glGetUniformuivRobustANGLE"
                 << "(" << program << ", " << location << ", " << bufSize
                 << ", " << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetUniformuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetUniformuivRobustANGLEFn(program, location, bufSize,
                                                    length, params);
}

static void GL_BINDING_CALL Debug_glGetVertexAttribfv(GLuint index,
                                                      GLenum pname,
                                                      GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribfvFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribfvFn(index, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetVertexAttribfvRobustANGLE(GLuint index,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLfloat* params) {
  GL_SERVICE_LOG("glGetVertexAttribfvRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL
Debug_glGetVertexAttribIivRobustANGLE(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribIivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize,
                                                         length, params);
}

static void GL_BINDING_CALL
Debug_glGetVertexAttribIuivRobustANGLE(GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       GLuint* params) {
  GL_SERVICE_LOG("glGetVertexAttribIuivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribIuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize,
                                                          length, params);
}

static void GL_BINDING_CALL Debug_glGetVertexAttribiv(GLuint index,
                                                      GLenum pname,
                                                      GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribiv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribivFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribivFn(index, pname, params);
}

static void GL_BINDING_CALL
Debug_glGetVertexAttribivRobustANGLE(GLuint index,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLint* params) {
  GL_SERVICE_LOG("glGetVertexAttribivRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribivRobustANGLEFn(index, pname, bufSize,
                                                        length, params);
}

static void GL_BINDING_CALL Debug_glGetVertexAttribPointerv(GLuint index,
                                                            GLenum pname,
                                                            void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointerv"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << pointer << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribPointervFn != nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribPointervFn(index, pname, pointer);
}

static void GL_BINDING_CALL
Debug_glGetVertexAttribPointervRobustANGLE(GLuint index,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           void** pointer) {
  GL_SERVICE_LOG("glGetVertexAttribPointervRobustANGLE"
                 << "(" << index << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(length)
                 << ", " << pointer << ")");
  DCHECK(g_driver_gl.debug_fn.glGetVertexAttribPointervRobustANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glGetVertexAttribPointervRobustANGLEFn(
      index, pname, bufSize, length, pointer);
}

static void GL_BINDING_CALL Debug_glHint(GLenum target, GLenum mode) {
  GL_SERVICE_LOG("glHint"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(mode) << ")");
  DCHECK(g_driver_gl.debug_fn.glHintFn != nullptr);
  g_driver_gl.debug_fn.glHintFn(target, mode);
}

static void GL_BINDING_CALL Debug_glInsertEventMarkerEXT(GLsizei length,
                                                         const char* marker) {
  GL_SERVICE_LOG("glInsertEventMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  DCHECK(g_driver_gl.debug_fn.glInsertEventMarkerEXTFn != nullptr);
  g_driver_gl.debug_fn.glInsertEventMarkerEXTFn(length, marker);
}

static void GL_BINDING_CALL
Debug_glInvalidateFramebuffer(GLenum target,
                              GLsizei numAttachments,
                              const GLenum* attachments) {
  GL_SERVICE_LOG("glInvalidateFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ")");
  DCHECK(g_driver_gl.debug_fn.glInvalidateFramebufferFn != nullptr);
  g_driver_gl.debug_fn.glInvalidateFramebufferFn(target, numAttachments,
                                                 attachments);
}

static void GL_BINDING_CALL
Debug_glInvalidateSubFramebuffer(GLenum target,
                                 GLsizei numAttachments,
                                 const GLenum* attachments,
                                 GLint x,
                                 GLint y,
                                 GLint width,
                                 GLint height) {
  GL_SERVICE_LOG("glInvalidateSubFramebuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << numAttachments << ", "
                 << static_cast<const void*>(attachments) << ", " << x << ", "
                 << y << ", " << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glInvalidateSubFramebufferFn != nullptr);
  g_driver_gl.debug_fn.glInvalidateSubFramebufferFn(
      target, numAttachments, attachments, x, y, width, height);
}

static GLboolean GL_BINDING_CALL Debug_glIsBuffer(GLuint buffer) {
  GL_SERVICE_LOG("glIsBuffer"
                 << "(" << buffer << ")");
  DCHECK(g_driver_gl.debug_fn.glIsBufferFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsBufferFn(buffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsEnabled(GLenum cap) {
  GL_SERVICE_LOG("glIsEnabled"
                 << "(" << GLEnums::GetStringEnum(cap) << ")");
  DCHECK(g_driver_gl.debug_fn.glIsEnabledFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsEnabledFn(cap);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsFenceAPPLE(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceAPPLE"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glIsFenceAPPLEFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsFenceNV(GLuint fence) {
  GL_SERVICE_LOG("glIsFenceNV"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glIsFenceNVFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsFramebufferEXT(GLuint framebuffer) {
  GL_SERVICE_LOG("glIsFramebufferEXT"
                 << "(" << framebuffer << ")");
  DCHECK(g_driver_gl.debug_fn.glIsFramebufferEXTFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsFramebufferEXTFn(framebuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsPathNV(GLuint path) {
  GL_SERVICE_LOG("glIsPathNV"
                 << "(" << path << ")");
  DCHECK(g_driver_gl.debug_fn.glIsPathNVFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsPathNVFn(path);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsProgram(GLuint program) {
  GL_SERVICE_LOG("glIsProgram"
                 << "(" << program << ")");
  DCHECK(g_driver_gl.debug_fn.glIsProgramFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsProgramFn(program);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsQuery(GLuint query) {
  GL_SERVICE_LOG("glIsQuery"
                 << "(" << query << ")");
  DCHECK(g_driver_gl.debug_fn.glIsQueryFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsQueryFn(query);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL
Debug_glIsRenderbufferEXT(GLuint renderbuffer) {
  GL_SERVICE_LOG("glIsRenderbufferEXT"
                 << "(" << renderbuffer << ")");
  DCHECK(g_driver_gl.debug_fn.glIsRenderbufferEXTFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsRenderbufferEXTFn(renderbuffer);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsSampler(GLuint sampler) {
  GL_SERVICE_LOG("glIsSampler"
                 << "(" << sampler << ")");
  DCHECK(g_driver_gl.debug_fn.glIsSamplerFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsSamplerFn(sampler);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsShader(GLuint shader) {
  GL_SERVICE_LOG("glIsShader"
                 << "(" << shader << ")");
  DCHECK(g_driver_gl.debug_fn.glIsShaderFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsShaderFn(shader);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsSync(GLsync sync) {
  GL_SERVICE_LOG("glIsSync"
                 << "(" << sync << ")");
  DCHECK(g_driver_gl.debug_fn.glIsSyncFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsSyncFn(sync);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsTexture(GLuint texture) {
  GL_SERVICE_LOG("glIsTexture"
                 << "(" << texture << ")");
  DCHECK(g_driver_gl.debug_fn.glIsTextureFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsTextureFn(texture);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsTransformFeedback(GLuint id) {
  GL_SERVICE_LOG("glIsTransformFeedback"
                 << "(" << id << ")");
  DCHECK(g_driver_gl.debug_fn.glIsTransformFeedbackFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsTransformFeedbackFn(id);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glIsVertexArrayOES(GLuint array) {
  GL_SERVICE_LOG("glIsVertexArrayOES"
                 << "(" << array << ")");
  DCHECK(g_driver_gl.debug_fn.glIsVertexArrayOESFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glIsVertexArrayOESFn(array);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glLineWidth(GLfloat width) {
  GL_SERVICE_LOG("glLineWidth"
                 << "(" << width << ")");
  DCHECK(g_driver_gl.debug_fn.glLineWidthFn != nullptr);
  g_driver_gl.debug_fn.glLineWidthFn(width);
}

static void GL_BINDING_CALL Debug_glLinkProgram(GLuint program) {
  GL_SERVICE_LOG("glLinkProgram"
                 << "(" << program << ")");
  DCHECK(g_driver_gl.debug_fn.glLinkProgramFn != nullptr);
  g_driver_gl.debug_fn.glLinkProgramFn(program);
}

static void* GL_BINDING_CALL Debug_glMapBuffer(GLenum target, GLenum access) {
  GL_SERVICE_LOG("glMapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(access) << ")");
  DCHECK(g_driver_gl.debug_fn.glMapBufferFn != nullptr);
  void* result = g_driver_gl.debug_fn.glMapBufferFn(target, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void* GL_BINDING_CALL Debug_glMapBufferRange(GLenum target,
                                                    GLintptr offset,
                                                    GLsizeiptr length,
                                                    GLbitfield access) {
  GL_SERVICE_LOG("glMapBufferRange"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << offset
                 << ", " << length << ", " << access << ")");
  DCHECK(g_driver_gl.debug_fn.glMapBufferRangeFn != nullptr);
  void* result =
      g_driver_gl.debug_fn.glMapBufferRangeFn(target, offset, length, access);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glMatrixLoadfEXT(GLenum matrixMode,
                                                   const GLfloat* m) {
  GL_SERVICE_LOG("glMatrixLoadfEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ", "
                 << static_cast<const void*>(m) << ")");
  DCHECK(g_driver_gl.debug_fn.glMatrixLoadfEXTFn != nullptr);
  g_driver_gl.debug_fn.glMatrixLoadfEXTFn(matrixMode, m);
}

static void GL_BINDING_CALL Debug_glMatrixLoadIdentityEXT(GLenum matrixMode) {
  GL_SERVICE_LOG("glMatrixLoadIdentityEXT"
                 << "(" << GLEnums::GetStringEnum(matrixMode) << ")");
  DCHECK(g_driver_gl.debug_fn.glMatrixLoadIdentityEXTFn != nullptr);
  g_driver_gl.debug_fn.glMatrixLoadIdentityEXTFn(matrixMode);
}

static void GL_BINDING_CALL Debug_glMemoryBarrierEXT(GLbitfield barriers) {
  GL_SERVICE_LOG("glMemoryBarrierEXT"
                 << "(" << barriers << ")");
  DCHECK(g_driver_gl.debug_fn.glMemoryBarrierEXTFn != nullptr);
  g_driver_gl.debug_fn.glMemoryBarrierEXTFn(barriers);
}

static void GL_BINDING_CALL Debug_glObjectLabel(GLenum identifier,
                                                GLuint name,
                                                GLsizei length,
                                                const char* label) {
  GL_SERVICE_LOG("glObjectLabel"
                 << "(" << GLEnums::GetStringEnum(identifier) << ", " << name
                 << ", " << length << ", " << label << ")");
  DCHECK(g_driver_gl.debug_fn.glObjectLabelFn != nullptr);
  g_driver_gl.debug_fn.glObjectLabelFn(identifier, name, length, label);
}

static void GL_BINDING_CALL Debug_glObjectPtrLabel(void* ptr,
                                                   GLsizei length,
                                                   const char* label) {
  GL_SERVICE_LOG("glObjectPtrLabel"
                 << "(" << static_cast<const void*>(ptr) << ", " << length
                 << ", " << label << ")");
  DCHECK(g_driver_gl.debug_fn.glObjectPtrLabelFn != nullptr);
  g_driver_gl.debug_fn.glObjectPtrLabelFn(ptr, length, label);
}

static void GL_BINDING_CALL Debug_glPathCommandsNV(GLuint path,
                                                   GLsizei numCommands,
                                                   const GLubyte* commands,
                                                   GLsizei numCoords,
                                                   GLenum coordType,
                                                   const GLvoid* coords) {
  GL_SERVICE_LOG("glPathCommandsNV"
                 << "(" << path << ", " << numCommands << ", "
                 << static_cast<const void*>(commands) << ", " << numCoords
                 << ", " << GLEnums::GetStringEnum(coordType) << ", "
                 << static_cast<const void*>(coords) << ")");
  DCHECK(g_driver_gl.debug_fn.glPathCommandsNVFn != nullptr);
  g_driver_gl.debug_fn.glPathCommandsNVFn(path, numCommands, commands,
                                          numCoords, coordType, coords);
}

static void GL_BINDING_CALL Debug_glPathParameterfNV(GLuint path,
                                                     GLenum pname,
                                                     GLfloat value) {
  GL_SERVICE_LOG("glPathParameterfNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  DCHECK(g_driver_gl.debug_fn.glPathParameterfNVFn != nullptr);
  g_driver_gl.debug_fn.glPathParameterfNVFn(path, pname, value);
}

static void GL_BINDING_CALL Debug_glPathParameteriNV(GLuint path,
                                                     GLenum pname,
                                                     GLint value) {
  GL_SERVICE_LOG("glPathParameteriNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(pname) << ", "
                 << value << ")");
  DCHECK(g_driver_gl.debug_fn.glPathParameteriNVFn != nullptr);
  g_driver_gl.debug_fn.glPathParameteriNVFn(path, pname, value);
}

static void GL_BINDING_CALL Debug_glPathStencilFuncNV(GLenum func,
                                                      GLint ref,
                                                      GLuint mask) {
  GL_SERVICE_LOG("glPathStencilFuncNV"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glPathStencilFuncNVFn != nullptr);
  g_driver_gl.debug_fn.glPathStencilFuncNVFn(func, ref, mask);
}

static void GL_BINDING_CALL Debug_glPauseTransformFeedback(void) {
  GL_SERVICE_LOG("glPauseTransformFeedback"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glPauseTransformFeedbackFn != nullptr);
  g_driver_gl.debug_fn.glPauseTransformFeedbackFn();
}

static void GL_BINDING_CALL Debug_glPixelStorei(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPixelStorei"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glPixelStoreiFn != nullptr);
  g_driver_gl.debug_fn.glPixelStoreiFn(pname, param);
}

static void GL_BINDING_CALL Debug_glPointParameteri(GLenum pname, GLint param) {
  GL_SERVICE_LOG("glPointParameteri"
                 << "(" << GLEnums::GetStringEnum(pname) << ", " << param
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glPointParameteriFn != nullptr);
  g_driver_gl.debug_fn.glPointParameteriFn(pname, param);
}

static void GL_BINDING_CALL Debug_glPolygonOffset(GLfloat factor,
                                                  GLfloat units) {
  GL_SERVICE_LOG("glPolygonOffset"
                 << "(" << factor << ", " << units << ")");
  DCHECK(g_driver_gl.debug_fn.glPolygonOffsetFn != nullptr);
  g_driver_gl.debug_fn.glPolygonOffsetFn(factor, units);
}

static void GL_BINDING_CALL Debug_glPopDebugGroup() {
  GL_SERVICE_LOG("glPopDebugGroup"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glPopDebugGroupFn != nullptr);
  g_driver_gl.debug_fn.glPopDebugGroupFn();
}

static void GL_BINDING_CALL Debug_glPopGroupMarkerEXT(void) {
  GL_SERVICE_LOG("glPopGroupMarkerEXT"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glPopGroupMarkerEXTFn != nullptr);
  g_driver_gl.debug_fn.glPopGroupMarkerEXTFn();
}

static void GL_BINDING_CALL Debug_glPrimitiveRestartIndex(GLuint index) {
  GL_SERVICE_LOG("glPrimitiveRestartIndex"
                 << "(" << index << ")");
  DCHECK(g_driver_gl.debug_fn.glPrimitiveRestartIndexFn != nullptr);
  g_driver_gl.debug_fn.glPrimitiveRestartIndexFn(index);
}

static void GL_BINDING_CALL Debug_glProgramBinary(GLuint program,
                                                  GLenum binaryFormat,
                                                  const GLvoid* binary,
                                                  GLsizei length) {
  GL_SERVICE_LOG("glProgramBinary"
                 << "(" << program << ", "
                 << GLEnums::GetStringEnum(binaryFormat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  DCHECK(g_driver_gl.debug_fn.glProgramBinaryFn != nullptr);
  g_driver_gl.debug_fn.glProgramBinaryFn(program, binaryFormat, binary, length);
}

static void GL_BINDING_CALL Debug_glProgramParameteri(GLuint program,
                                                      GLenum pname,
                                                      GLint value) {
  GL_SERVICE_LOG("glProgramParameteri"
                 << "(" << program << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << value << ")");
  DCHECK(g_driver_gl.debug_fn.glProgramParameteriFn != nullptr);
  g_driver_gl.debug_fn.glProgramParameteriFn(program, pname, value);
}

static void GL_BINDING_CALL
Debug_glProgramPathFragmentInputGenNV(GLuint program,
                                      GLint location,
                                      GLenum genMode,
                                      GLint components,
                                      const GLfloat* coeffs) {
  GL_SERVICE_LOG("glProgramPathFragmentInputGenNV"
                 << "(" << program << ", " << location << ", "
                 << GLEnums::GetStringEnum(genMode) << ", " << components
                 << ", " << static_cast<const void*>(coeffs) << ")");
  DCHECK(g_driver_gl.debug_fn.glProgramPathFragmentInputGenNVFn != nullptr);
  g_driver_gl.debug_fn.glProgramPathFragmentInputGenNVFn(
      program, location, genMode, components, coeffs);
}

static void GL_BINDING_CALL Debug_glPushDebugGroup(GLenum source,
                                                   GLuint id,
                                                   GLsizei length,
                                                   const char* message) {
  GL_SERVICE_LOG("glPushDebugGroup"
                 << "(" << GLEnums::GetStringEnum(source) << ", " << id << ", "
                 << length << ", " << message << ")");
  DCHECK(g_driver_gl.debug_fn.glPushDebugGroupFn != nullptr);
  g_driver_gl.debug_fn.glPushDebugGroupFn(source, id, length, message);
}

static void GL_BINDING_CALL Debug_glPushGroupMarkerEXT(GLsizei length,
                                                       const char* marker) {
  GL_SERVICE_LOG("glPushGroupMarkerEXT"
                 << "(" << length << ", " << marker << ")");
  DCHECK(g_driver_gl.debug_fn.glPushGroupMarkerEXTFn != nullptr);
  g_driver_gl.debug_fn.glPushGroupMarkerEXTFn(length, marker);
}

static void GL_BINDING_CALL Debug_glQueryCounter(GLuint id, GLenum target) {
  GL_SERVICE_LOG("glQueryCounter"
                 << "(" << id << ", " << GLEnums::GetStringEnum(target) << ")");
  DCHECK(g_driver_gl.debug_fn.glQueryCounterFn != nullptr);
  g_driver_gl.debug_fn.glQueryCounterFn(id, target);
}

static void GL_BINDING_CALL Debug_glReadBuffer(GLenum src) {
  GL_SERVICE_LOG("glReadBuffer"
                 << "(" << GLEnums::GetStringEnum(src) << ")");
  DCHECK(g_driver_gl.debug_fn.glReadBufferFn != nullptr);
  g_driver_gl.debug_fn.glReadBufferFn(src);
}

static void GL_BINDING_CALL Debug_glReadnPixelsRobustANGLE(GLint x,
                                                           GLint y,
                                                           GLsizei width,
                                                           GLsizei height,
                                                           GLenum format,
                                                           GLenum type,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           void* data) {
  GL_SERVICE_LOG("glReadnPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(data) << ")");
  DCHECK(g_driver_gl.debug_fn.glReadnPixelsRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glReadnPixelsRobustANGLEFn(x, y, width, height, format,
                                                  type, bufSize, length, data);
}

static void GL_BINDING_CALL Debug_glReadPixels(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               void* pixels) {
  GL_SERVICE_LOG("glReadPixels"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glReadPixelsFn != nullptr);
  g_driver_gl.debug_fn.glReadPixelsFn(x, y, width, height, format, type,
                                      pixels);
}

static void GL_BINDING_CALL Debug_glReadPixelsRobustANGLE(GLint x,
                                                          GLint y,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          void* pixels) {
  GL_SERVICE_LOG("glReadPixelsRobustANGLE"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(length) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glReadPixelsRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glReadPixelsRobustANGLEFn(x, y, width, height, format,
                                                 type, bufSize, length, pixels);
}

static void GL_BINDING_CALL Debug_glReleaseShaderCompiler(void) {
  GL_SERVICE_LOG("glReleaseShaderCompiler"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glReleaseShaderCompilerFn != nullptr);
  g_driver_gl.debug_fn.glReleaseShaderCompilerFn();
}

static void GL_BINDING_CALL
Debug_glRenderbufferStorageEXT(GLenum target,
                               GLenum internalformat,
                               GLsizei width,
                               GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(internalformat) << ", " << width
                 << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glRenderbufferStorageEXTFn != nullptr);
  g_driver_gl.debug_fn.glRenderbufferStorageEXTFn(target, internalformat, width,
                                                  height);
}

static void GL_BINDING_CALL
Debug_glRenderbufferStorageMultisample(GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisample"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glRenderbufferStorageMultisampleFn != nullptr);
  g_driver_gl.debug_fn.glRenderbufferStorageMultisampleFn(
      target, samples, internalformat, width, height);
}

static void GL_BINDING_CALL
Debug_glRenderbufferStorageMultisampleANGLE(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisampleANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glRenderbufferStorageMultisampleANGLEFn !=
         nullptr);
  g_driver_gl.debug_fn.glRenderbufferStorageMultisampleANGLEFn(
      target, samples, internalformat, width, height);
}

static void GL_BINDING_CALL
Debug_glRenderbufferStorageMultisampleEXT(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisampleEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glRenderbufferStorageMultisampleEXTFn != nullptr);
  g_driver_gl.debug_fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, internalformat, width, height);
}

static void GL_BINDING_CALL
Debug_glRenderbufferStorageMultisampleIMG(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) {
  GL_SERVICE_LOG("glRenderbufferStorageMultisampleIMG"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << samples
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glRenderbufferStorageMultisampleIMGFn != nullptr);
  g_driver_gl.debug_fn.glRenderbufferStorageMultisampleIMGFn(
      target, samples, internalformat, width, height);
}

static void GL_BINDING_CALL Debug_glResumeTransformFeedback(void) {
  GL_SERVICE_LOG("glResumeTransformFeedback"
                 << "("
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glResumeTransformFeedbackFn != nullptr);
  g_driver_gl.debug_fn.glResumeTransformFeedbackFn();
}

static void GL_BINDING_CALL Debug_glSampleCoverage(GLclampf value,
                                                   GLboolean invert) {
  GL_SERVICE_LOG("glSampleCoverage"
                 << "(" << value << ", " << GLEnums::GetStringBool(invert)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSampleCoverageFn != nullptr);
  g_driver_gl.debug_fn.glSampleCoverageFn(value, invert);
}

static void GL_BINDING_CALL Debug_glSamplerParameterf(GLuint sampler,
                                                      GLenum pname,
                                                      GLfloat param) {
  GL_SERVICE_LOG("glSamplerParameterf"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterfFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterfFn(sampler, pname, param);
}

static void GL_BINDING_CALL Debug_glSamplerParameterfv(GLuint sampler,
                                                       GLenum pname,
                                                       const GLfloat* params) {
  GL_SERVICE_LOG("glSamplerParameterfv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterfvFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterfvFn(sampler, pname, params);
}

static void GL_BINDING_CALL
Debug_glSamplerParameterfvRobustANGLE(GLuint sampler,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      const GLfloat* param) {
  GL_SERVICE_LOG("glSamplerParameterfvRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterfvRobustANGLEFn(sampler, pname,
                                                         bufSize, param);
}

static void GL_BINDING_CALL Debug_glSamplerParameteri(GLuint sampler,
                                                      GLenum pname,
                                                      GLint param) {
  GL_SERVICE_LOG("glSamplerParameteri"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << param << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameteriFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameteriFn(sampler, pname, param);
}

static void GL_BINDING_CALL
Debug_glSamplerParameterIivRobustANGLE(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterIivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterIivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterIivRobustANGLEFn(sampler, pname,
                                                          bufSize, param);
}

static void GL_BINDING_CALL
Debug_glSamplerParameterIuivRobustANGLE(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLuint* param) {
  GL_SERVICE_LOG("glSamplerParameterIuivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterIuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterIuivRobustANGLEFn(sampler, pname,
                                                           bufSize, param);
}

static void GL_BINDING_CALL Debug_glSamplerParameteriv(GLuint sampler,
                                                       GLenum pname,
                                                       const GLint* params) {
  GL_SERVICE_LOG("glSamplerParameteriv"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterivFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterivFn(sampler, pname, params);
}

static void GL_BINDING_CALL
Debug_glSamplerParameterivRobustANGLE(GLuint sampler,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      const GLint* param) {
  GL_SERVICE_LOG("glSamplerParameterivRobustANGLE"
                 << "(" << sampler << ", " << GLEnums::GetStringEnum(pname)
                 << ", " << bufSize << ", " << static_cast<const void*>(param)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSamplerParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glSamplerParameterivRobustANGLEFn(sampler, pname,
                                                         bufSize, param);
}

static void GL_BINDING_CALL Debug_glScissor(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height) {
  GL_SERVICE_LOG("glScissor"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glScissorFn != nullptr);
  g_driver_gl.debug_fn.glScissorFn(x, y, width, height);
}

static void GL_BINDING_CALL Debug_glSetFenceAPPLE(GLuint fence) {
  GL_SERVICE_LOG("glSetFenceAPPLE"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glSetFenceAPPLEFn != nullptr);
  g_driver_gl.debug_fn.glSetFenceAPPLEFn(fence);
}

static void GL_BINDING_CALL Debug_glSetFenceNV(GLuint fence, GLenum condition) {
  GL_SERVICE_LOG("glSetFenceNV"
                 << "(" << fence << ", " << GLEnums::GetStringEnum(condition)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glSetFenceNVFn != nullptr);
  g_driver_gl.debug_fn.glSetFenceNVFn(fence, condition);
}

static void GL_BINDING_CALL Debug_glShaderBinary(GLsizei n,
                                                 const GLuint* shaders,
                                                 GLenum binaryformat,
                                                 const void* binary,
                                                 GLsizei length) {
  GL_SERVICE_LOG("glShaderBinary"
                 << "(" << n << ", " << static_cast<const void*>(shaders)
                 << ", " << GLEnums::GetStringEnum(binaryformat) << ", "
                 << static_cast<const void*>(binary) << ", " << length << ")");
  DCHECK(g_driver_gl.debug_fn.glShaderBinaryFn != nullptr);
  g_driver_gl.debug_fn.glShaderBinaryFn(n, shaders, binaryformat, binary,
                                        length);
}

static void GL_BINDING_CALL Debug_glShaderSource(GLuint shader,
                                                 GLsizei count,
                                                 const char* const* str,
                                                 const GLint* length) {
  GL_SERVICE_LOG("glShaderSource"
                 << "(" << shader << ", " << count << ", "
                 << static_cast<const void*>(str) << ", "
                 << static_cast<const void*>(length) << ")");
  DCHECK(g_driver_gl.debug_fn.glShaderSourceFn != nullptr);
  g_driver_gl.debug_fn.glShaderSourceFn(shader, count, str, length);

  GL_SERVICE_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (str[ii]) {
        if (length && length[ii] >= 0) {
          std::string source(str[ii], length[ii]);
          GL_SERVICE_LOG("  " << ii << ": ---\n" << source << "\n---");
        } else {
          GL_SERVICE_LOG("  " << ii << ": ---\n" << str[ii] << "\n---");
        }
      } else {
        GL_SERVICE_LOG("  " << ii << ": NULL");
      }
    }
  });
}

static void GL_BINDING_CALL
Debug_glStencilFillPathInstancedNV(GLsizei numPaths,
                                   GLenum pathNameType,
                                   const void* paths,
                                   GLuint pathBase,
                                   GLenum fillMode,
                                   GLuint mask,
                                   GLenum transformType,
                                   const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilFillPathInstancedNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, transformType,
      transformValues);
}

static void GL_BINDING_CALL Debug_glStencilFillPathNV(GLuint path,
                                                      GLenum fillMode,
                                                      GLuint mask) {
  GL_SERVICE_LOG("glStencilFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilFillPathNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilFillPathNVFn(path, fillMode, mask);
}

static void GL_BINDING_CALL Debug_glStencilFunc(GLenum func,
                                                GLint ref,
                                                GLuint mask) {
  GL_SERVICE_LOG("glStencilFunc"
                 << "(" << GLEnums::GetStringEnum(func) << ", " << ref << ", "
                 << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilFuncFn != nullptr);
  g_driver_gl.debug_fn.glStencilFuncFn(func, ref, mask);
}

static void GL_BINDING_CALL Debug_glStencilFuncSeparate(GLenum face,
                                                        GLenum func,
                                                        GLint ref,
                                                        GLuint mask) {
  GL_SERVICE_LOG("glStencilFuncSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(func) << ", " << ref << ", " << mask
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilFuncSeparateFn != nullptr);
  g_driver_gl.debug_fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

static void GL_BINDING_CALL Debug_glStencilMask(GLuint mask) {
  GL_SERVICE_LOG("glStencilMask"
                 << "(" << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilMaskFn != nullptr);
  g_driver_gl.debug_fn.glStencilMaskFn(mask);
}

static void GL_BINDING_CALL Debug_glStencilMaskSeparate(GLenum face,
                                                        GLuint mask) {
  GL_SERVICE_LOG("glStencilMaskSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", " << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilMaskSeparateFn != nullptr);
  g_driver_gl.debug_fn.glStencilMaskSeparateFn(face, mask);
}

static void GL_BINDING_CALL Debug_glStencilOp(GLenum fail,
                                              GLenum zfail,
                                              GLenum zpass) {
  GL_SERVICE_LOG("glStencilOp"
                 << "(" << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilOpFn != nullptr);
  g_driver_gl.debug_fn.glStencilOpFn(fail, zfail, zpass);
}

static void GL_BINDING_CALL Debug_glStencilOpSeparate(GLenum face,
                                                      GLenum fail,
                                                      GLenum zfail,
                                                      GLenum zpass) {
  GL_SERVICE_LOG("glStencilOpSeparate"
                 << "(" << GLEnums::GetStringEnum(face) << ", "
                 << GLEnums::GetStringEnum(fail) << ", "
                 << GLEnums::GetStringEnum(zfail) << ", "
                 << GLEnums::GetStringEnum(zpass) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilOpSeparateFn != nullptr);
  g_driver_gl.debug_fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

static void GL_BINDING_CALL
Debug_glStencilStrokePathInstancedNV(GLsizei numPaths,
                                     GLenum pathNameType,
                                     const void* paths,
                                     GLuint pathBase,
                                     GLint ref,
                                     GLuint mask,
                                     GLenum transformType,
                                     const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilStrokePathInstancedNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, transformType,
      transformValues);
}

static void GL_BINDING_CALL Debug_glStencilStrokePathNV(GLuint path,
                                                        GLint reference,
                                                        GLuint mask) {
  GL_SERVICE_LOG("glStencilStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilStrokePathNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilStrokePathNVFn(path, reference, mask);
}

static void GL_BINDING_CALL
Debug_glStencilThenCoverFillPathInstancedNV(GLsizei numPaths,
                                            GLenum pathNameType,
                                            const void* paths,
                                            GLuint pathBase,
                                            GLenum fillMode,
                                            GLuint mask,
                                            GLenum coverMode,
                                            GLenum transformType,
                                            const GLfloat* transformValues) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathInstancedNV"
                 << "(" << numPaths << ", "
                 << GLEnums::GetStringEnum(pathNameType) << ", "
                 << static_cast<const void*>(paths) << ", " << pathBase << ", "
                 << GLEnums::GetStringEnum(fillMode) << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ", "
                 << GLEnums::GetStringEnum(transformType) << ", "
                 << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilThenCoverFillPathInstancedNVFn !=
         nullptr);
  g_driver_gl.debug_fn.glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

static void GL_BINDING_CALL
Debug_glStencilThenCoverFillPathNV(GLuint path,
                                   GLenum fillMode,
                                   GLuint mask,
                                   GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverFillPathNV"
                 << "(" << path << ", " << GLEnums::GetStringEnum(fillMode)
                 << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilThenCoverFillPathNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilThenCoverFillPathNVFn(path, fillMode, mask,
                                                      coverMode);
}

static void GL_BINDING_CALL
Debug_glStencilThenCoverStrokePathInstancedNV(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLint ref,
                                              GLuint mask,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  GL_SERVICE_LOG(
      "glStencilThenCoverStrokePathInstancedNV"
      << "(" << numPaths << ", " << GLEnums::GetStringEnum(pathNameType) << ", "
      << static_cast<const void*>(paths) << ", " << pathBase << ", " << ref
      << ", " << mask << ", " << GLEnums::GetStringEnum(coverMode) << ", "
      << GLEnums::GetStringEnum(transformType) << ", "
      << static_cast<const void*>(transformValues) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilThenCoverStrokePathInstancedNVFn !=
         nullptr);
  g_driver_gl.debug_fn.glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

static void GL_BINDING_CALL
Debug_glStencilThenCoverStrokePathNV(GLuint path,
                                     GLint reference,
                                     GLuint mask,
                                     GLenum coverMode) {
  GL_SERVICE_LOG("glStencilThenCoverStrokePathNV"
                 << "(" << path << ", " << reference << ", " << mask << ", "
                 << GLEnums::GetStringEnum(coverMode) << ")");
  DCHECK(g_driver_gl.debug_fn.glStencilThenCoverStrokePathNVFn != nullptr);
  g_driver_gl.debug_fn.glStencilThenCoverStrokePathNVFn(path, reference, mask,
                                                        coverMode);
}

static GLboolean GL_BINDING_CALL Debug_glTestFenceAPPLE(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceAPPLE"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glTestFenceAPPLEFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glTestFenceAPPLEFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static GLboolean GL_BINDING_CALL Debug_glTestFenceNV(GLuint fence) {
  GL_SERVICE_LOG("glTestFenceNV"
                 << "(" << fence << ")");
  DCHECK(g_driver_gl.debug_fn.glTestFenceNVFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glTestFenceNVFn(fence);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glTexImage2D(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  GL_SERVICE_LOG("glTexImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexImage2DFn != nullptr);
  g_driver_gl.debug_fn.glTexImage2DFn(target, level, internalformat, width,
                                      height, border, format, type, pixels);
}

static void GL_BINDING_CALL Debug_glTexImage2DRobustANGLE(GLenum target,
                                                          GLint level,
                                                          GLint internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLint border,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei bufSize,
                                                          const void* pixels) {
  GL_SERVICE_LOG("glTexImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << border << ", " << GLEnums::GetStringEnum(format)
                 << ", " << GLEnums::GetStringEnum(type) << ", " << bufSize
                 << ", " << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexImage2DRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexImage2DRobustANGLEFn(target, level, internalformat,
                                                 width, height, border, format,
                                                 type, bufSize, pixels);
}

static void GL_BINDING_CALL Debug_glTexImage3D(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               const void* pixels) {
  GL_SERVICE_LOG("glTexImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexImage3DFn != nullptr);
  g_driver_gl.debug_fn.glTexImage3DFn(target, level, internalformat, width,
                                      height, depth, border, format, type,
                                      pixels);
}

static void GL_BINDING_CALL Debug_glTexImage3DRobustANGLE(GLenum target,
                                                          GLint level,
                                                          GLint internalformat,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLint border,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei bufSize,
                                                          const void* pixels) {
  GL_SERVICE_LOG("glTexImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << internalformat << ", " << width << ", " << height
                 << ", " << depth << ", " << border << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexImage3DRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexImage3DRobustANGLEFn(target, level, internalformat,
                                                 width, height, depth, border,
                                                 format, type, bufSize, pixels);
}

static void GL_BINDING_CALL Debug_glTexParameterf(GLenum target,
                                                  GLenum pname,
                                                  GLfloat param) {
  GL_SERVICE_LOG("glTexParameterf"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterfFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterfFn(target, pname, param);
}

static void GL_BINDING_CALL Debug_glTexParameterfv(GLenum target,
                                                   GLenum pname,
                                                   const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterfvFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterfvFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glTexParameterfvRobustANGLE(GLenum target,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLfloat* params) {
  GL_SERVICE_LOG("glTexParameterfvRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterfvRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterfvRobustANGLEFn(target, pname, bufSize,
                                                     params);
}

static void GL_BINDING_CALL Debug_glTexParameteri(GLenum target,
                                                  GLenum pname,
                                                  GLint param) {
  GL_SERVICE_LOG("glTexParameteri"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << param << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameteriFn != nullptr);
  g_driver_gl.debug_fn.glTexParameteriFn(target, pname, param);
}

static void GL_BINDING_CALL
Debug_glTexParameterIivRobustANGLE(GLenum target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint* params) {
  GL_SERVICE_LOG("glTexParameterIivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterIivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterIivRobustANGLEFn(target, pname, bufSize,
                                                      params);
}

static void GL_BINDING_CALL
Debug_glTexParameterIuivRobustANGLE(GLenum target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLuint* params) {
  GL_SERVICE_LOG("glTexParameterIuivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterIuivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterIuivRobustANGLEFn(target, pname, bufSize,
                                                       params);
}

static void GL_BINDING_CALL Debug_glTexParameteriv(GLenum target,
                                                   GLenum pname,
                                                   const GLint* params) {
  GL_SERVICE_LOG("glTexParameteriv"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterivFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterivFn(target, pname, params);
}

static void GL_BINDING_CALL
Debug_glTexParameterivRobustANGLE(GLenum target,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  const GLint* params) {
  GL_SERVICE_LOG("glTexParameterivRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", "
                 << GLEnums::GetStringEnum(pname) << ", " << bufSize << ", "
                 << static_cast<const void*>(params) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexParameterivRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexParameterivRobustANGLEFn(target, pname, bufSize,
                                                     params);
}

static void GL_BINDING_CALL Debug_glTexStorage2DEXT(GLenum target,
                                                    GLsizei levels,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  GL_SERVICE_LOG("glTexStorage2DEXT"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ")");
  DCHECK(g_driver_gl.debug_fn.glTexStorage2DEXTFn != nullptr);
  g_driver_gl.debug_fn.glTexStorage2DEXTFn(target, levels, internalformat,
                                           width, height);
}

static void GL_BINDING_CALL Debug_glTexStorage3D(GLenum target,
                                                 GLsizei levels,
                                                 GLenum internalformat,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth) {
  GL_SERVICE_LOG("glTexStorage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << levels
                 << ", " << GLEnums::GetStringEnum(internalformat) << ", "
                 << width << ", " << height << ", " << depth << ")");
  DCHECK(g_driver_gl.debug_fn.glTexStorage3DFn != nullptr);
  g_driver_gl.debug_fn.glTexStorage3DFn(target, levels, internalformat, width,
                                        height, depth);
}

static void GL_BINDING_CALL Debug_glTexSubImage2D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLenum format,
                                                  GLenum type,
                                                  const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexSubImage2DFn != nullptr);
  g_driver_gl.debug_fn.glTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                         height, format, type, pixels);
}

static void GL_BINDING_CALL
Debug_glTexSubImage2DRobustANGLE(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei bufSize,
                                 const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage2DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << width << ", "
                 << height << ", " << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexSubImage2DRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexSubImage2DRobustANGLEFn(
      target, level, xoffset, yoffset, width, height, format, type, bufSize,
      pixels);
}

static void GL_BINDING_CALL Debug_glTexSubImage3D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint zoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLenum format,
                                                  GLenum type,
                                                  const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3D"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexSubImage3DFn != nullptr);
  g_driver_gl.debug_fn.glTexSubImage3DFn(target, level, xoffset, yoffset,
                                         zoffset, width, height, depth, format,
                                         type, pixels);
}

static void GL_BINDING_CALL
Debug_glTexSubImage3DRobustANGLE(GLenum target,
                                 GLint level,
                                 GLint xoffset,
                                 GLint yoffset,
                                 GLint zoffset,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type,
                                 GLsizei bufSize,
                                 const void* pixels) {
  GL_SERVICE_LOG("glTexSubImage3DRobustANGLE"
                 << "(" << GLEnums::GetStringEnum(target) << ", " << level
                 << ", " << xoffset << ", " << yoffset << ", " << zoffset
                 << ", " << width << ", " << height << ", " << depth << ", "
                 << GLEnums::GetStringEnum(format) << ", "
                 << GLEnums::GetStringEnum(type) << ", " << bufSize << ", "
                 << static_cast<const void*>(pixels) << ")");
  DCHECK(g_driver_gl.debug_fn.glTexSubImage3DRobustANGLEFn != nullptr);
  g_driver_gl.debug_fn.glTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      type, bufSize, pixels);
}

static void GL_BINDING_CALL
Debug_glTransformFeedbackVaryings(GLuint program,
                                  GLsizei count,
                                  const char* const* varyings,
                                  GLenum bufferMode) {
  GL_SERVICE_LOG("glTransformFeedbackVaryings"
                 << "(" << program << ", " << count << ", "
                 << static_cast<const void*>(varyings) << ", "
                 << GLEnums::GetStringEnum(bufferMode) << ")");
  DCHECK(g_driver_gl.debug_fn.glTransformFeedbackVaryingsFn != nullptr);
  g_driver_gl.debug_fn.glTransformFeedbackVaryingsFn(program, count, varyings,
                                                     bufferMode);
}

static void GL_BINDING_CALL Debug_glUniform1f(GLint location, GLfloat x) {
  GL_SERVICE_LOG("glUniform1f"
                 << "(" << location << ", " << x << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1fFn != nullptr);
  g_driver_gl.debug_fn.glUniform1fFn(location, x);
}

static void GL_BINDING_CALL Debug_glUniform1fv(GLint location,
                                               GLsizei count,
                                               const GLfloat* v) {
  GL_SERVICE_LOG("glUniform1fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1fvFn != nullptr);
  g_driver_gl.debug_fn.glUniform1fvFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform1i(GLint location, GLint x) {
  GL_SERVICE_LOG("glUniform1i"
                 << "(" << location << ", " << x << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1iFn != nullptr);
  g_driver_gl.debug_fn.glUniform1iFn(location, x);
}

static void GL_BINDING_CALL Debug_glUniform1iv(GLint location,
                                               GLsizei count,
                                               const GLint* v) {
  GL_SERVICE_LOG("glUniform1iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1ivFn != nullptr);
  g_driver_gl.debug_fn.glUniform1ivFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform1ui(GLint location, GLuint v0) {
  GL_SERVICE_LOG("glUniform1ui"
                 << "(" << location << ", " << v0 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1uiFn != nullptr);
  g_driver_gl.debug_fn.glUniform1uiFn(location, v0);
}

static void GL_BINDING_CALL Debug_glUniform1uiv(GLint location,
                                                GLsizei count,
                                                const GLuint* v) {
  GL_SERVICE_LOG("glUniform1uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform1uivFn != nullptr);
  g_driver_gl.debug_fn.glUniform1uivFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform2f(GLint location,
                                              GLfloat x,
                                              GLfloat y) {
  GL_SERVICE_LOG("glUniform2f"
                 << "(" << location << ", " << x << ", " << y << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2fFn != nullptr);
  g_driver_gl.debug_fn.glUniform2fFn(location, x, y);
}

static void GL_BINDING_CALL Debug_glUniform2fv(GLint location,
                                               GLsizei count,
                                               const GLfloat* v) {
  GL_SERVICE_LOG("glUniform2fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2fvFn != nullptr);
  g_driver_gl.debug_fn.glUniform2fvFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform2i(GLint location,
                                              GLint x,
                                              GLint y) {
  GL_SERVICE_LOG("glUniform2i"
                 << "(" << location << ", " << x << ", " << y << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2iFn != nullptr);
  g_driver_gl.debug_fn.glUniform2iFn(location, x, y);
}

static void GL_BINDING_CALL Debug_glUniform2iv(GLint location,
                                               GLsizei count,
                                               const GLint* v) {
  GL_SERVICE_LOG("glUniform2iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2ivFn != nullptr);
  g_driver_gl.debug_fn.glUniform2ivFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform2ui(GLint location,
                                               GLuint v0,
                                               GLuint v1) {
  GL_SERVICE_LOG("glUniform2ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2uiFn != nullptr);
  g_driver_gl.debug_fn.glUniform2uiFn(location, v0, v1);
}

static void GL_BINDING_CALL Debug_glUniform2uiv(GLint location,
                                                GLsizei count,
                                                const GLuint* v) {
  GL_SERVICE_LOG("glUniform2uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform2uivFn != nullptr);
  g_driver_gl.debug_fn.glUniform2uivFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform3f(GLint location,
                                              GLfloat x,
                                              GLfloat y,
                                              GLfloat z) {
  GL_SERVICE_LOG("glUniform3f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3fFn != nullptr);
  g_driver_gl.debug_fn.glUniform3fFn(location, x, y, z);
}

static void GL_BINDING_CALL Debug_glUniform3fv(GLint location,
                                               GLsizei count,
                                               const GLfloat* v) {
  GL_SERVICE_LOG("glUniform3fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3fvFn != nullptr);
  g_driver_gl.debug_fn.glUniform3fvFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform3i(GLint location,
                                              GLint x,
                                              GLint y,
                                              GLint z) {
  GL_SERVICE_LOG("glUniform3i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3iFn != nullptr);
  g_driver_gl.debug_fn.glUniform3iFn(location, x, y, z);
}

static void GL_BINDING_CALL Debug_glUniform3iv(GLint location,
                                               GLsizei count,
                                               const GLint* v) {
  GL_SERVICE_LOG("glUniform3iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3ivFn != nullptr);
  g_driver_gl.debug_fn.glUniform3ivFn(location, count, v);
}

static void GL_BINDING_CALL Debug_glUniform3ui(GLint location,
                                               GLuint v0,
                                               GLuint v1,
                                               GLuint v2) {
  GL_SERVICE_LOG("glUniform3ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3uiFn != nullptr);
  g_driver_gl.debug_fn.glUniform3uiFn(location, v0, v1, v2);
}

static void GL_BINDING_CALL Debug_glUniform3uiv(GLint location,
                                                GLsizei count,
                                                const GLuint* v) {
  GL_SERVICE_LOG("glUniform3uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform3uivFn != nullptr);
  g_driver_gl.debug_fn.glUniform3uivFn(location, count, v);
}

static void GL_BINDING_CALL
Debug_glUniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
  GL_SERVICE_LOG("glUniform4f"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4fFn != nullptr);
  g_driver_gl.debug_fn.glUniform4fFn(location, x, y, z, w);
}

static void GL_BINDING_CALL Debug_glUniform4fv(GLint location,
                                               GLsizei count,
                                               const GLfloat* v) {
  GL_SERVICE_LOG("glUniform4fv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4fvFn != nullptr);
  g_driver_gl.debug_fn.glUniform4fvFn(location, count, v);
}

static void GL_BINDING_CALL
Debug_glUniform4i(GLint location, GLint x, GLint y, GLint z, GLint w) {
  GL_SERVICE_LOG("glUniform4i"
                 << "(" << location << ", " << x << ", " << y << ", " << z
                 << ", " << w << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4iFn != nullptr);
  g_driver_gl.debug_fn.glUniform4iFn(location, x, y, z, w);
}

static void GL_BINDING_CALL Debug_glUniform4iv(GLint location,
                                               GLsizei count,
                                               const GLint* v) {
  GL_SERVICE_LOG("glUniform4iv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4ivFn != nullptr);
  g_driver_gl.debug_fn.glUniform4ivFn(location, count, v);
}

static void GL_BINDING_CALL
Debug_glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
  GL_SERVICE_LOG("glUniform4ui"
                 << "(" << location << ", " << v0 << ", " << v1 << ", " << v2
                 << ", " << v3 << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4uiFn != nullptr);
  g_driver_gl.debug_fn.glUniform4uiFn(location, v0, v1, v2, v3);
}

static void GL_BINDING_CALL Debug_glUniform4uiv(GLint location,
                                                GLsizei count,
                                                const GLuint* v) {
  GL_SERVICE_LOG("glUniform4uiv"
                 << "(" << location << ", " << count << ", "
                 << static_cast<const void*>(v) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniform4uivFn != nullptr);
  g_driver_gl.debug_fn.glUniform4uivFn(location, count, v);
}

static void GL_BINDING_CALL
Debug_glUniformBlockBinding(GLuint program,
                            GLuint uniformBlockIndex,
                            GLuint uniformBlockBinding) {
  GL_SERVICE_LOG("glUniformBlockBinding"
                 << "(" << program << ", " << uniformBlockIndex << ", "
                 << uniformBlockBinding << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformBlockBindingFn != nullptr);
  g_driver_gl.debug_fn.glUniformBlockBindingFn(program, uniformBlockIndex,
                                               uniformBlockBinding);
}

static void GL_BINDING_CALL Debug_glUniformMatrix2fv(GLint location,
                                                     GLsizei count,
                                                     GLboolean transpose,
                                                     const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix2fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix2fvFn(location, count, transpose, value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix2x3fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix2x3fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix2x3fvFn(location, count, transpose,
                                              value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix2x4fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix2x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix2x4fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix2x4fvFn(location, count, transpose,
                                              value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix3fv(GLint location,
                                                     GLsizei count,
                                                     GLboolean transpose,
                                                     const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix3fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix3fvFn(location, count, transpose, value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix3x2fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix3x2fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix3x2fvFn(location, count, transpose,
                                              value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix3x4fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix3x4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix3x4fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix3x4fvFn(location, count, transpose,
                                              value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix4fv(GLint location,
                                                     GLsizei count,
                                                     GLboolean transpose,
                                                     const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix4fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix4fvFn(location, count, transpose, value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix4x2fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x2fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix4x2fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix4x2fvFn(location, count, transpose,
                                              value);
}

static void GL_BINDING_CALL Debug_glUniformMatrix4x3fv(GLint location,
                                                       GLsizei count,
                                                       GLboolean transpose,
                                                       const GLfloat* value) {
  GL_SERVICE_LOG("glUniformMatrix4x3fv"
                 << "(" << location << ", " << count << ", "
                 << GLEnums::GetStringBool(transpose) << ", "
                 << static_cast<const void*>(value) << ")");
  DCHECK(g_driver_gl.debug_fn.glUniformMatrix4x3fvFn != nullptr);
  g_driver_gl.debug_fn.glUniformMatrix4x3fvFn(location, count, transpose,
                                              value);
}

static GLboolean GL_BINDING_CALL Debug_glUnmapBuffer(GLenum target) {
  GL_SERVICE_LOG("glUnmapBuffer"
                 << "(" << GLEnums::GetStringEnum(target) << ")");
  DCHECK(g_driver_gl.debug_fn.glUnmapBufferFn != nullptr);
  GLboolean result = g_driver_gl.debug_fn.glUnmapBufferFn(target);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}

static void GL_BINDING_CALL Debug_glUseProgram(GLuint program) {
  GL_SERVICE_LOG("glUseProgram"
                 << "(" << program << ")");
  DCHECK(g_driver_gl.debug_fn.glUseProgramFn != nullptr);
  g_driver_gl.debug_fn.glUseProgramFn(program);
}

static void GL_BINDING_CALL Debug_glValidateProgram(GLuint program) {
  GL_SERVICE_LOG("glValidateProgram"
                 << "(" << program << ")");
  DCHECK(g_driver_gl.debug_fn.glValidateProgramFn != nullptr);
  g_driver_gl.debug_fn.glValidateProgramFn(program);
}

static void GL_BINDING_CALL Debug_glVertexAttrib1f(GLuint indx, GLfloat x) {
  GL_SERVICE_LOG("glVertexAttrib1f"
                 << "(" << indx << ", " << x << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib1fFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib1fFn(indx, x);
}

static void GL_BINDING_CALL Debug_glVertexAttrib1fv(GLuint indx,
                                                    const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib1fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib1fvFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib1fvFn(indx, values);
}

static void GL_BINDING_CALL Debug_glVertexAttrib2f(GLuint indx,
                                                   GLfloat x,
                                                   GLfloat y) {
  GL_SERVICE_LOG("glVertexAttrib2f"
                 << "(" << indx << ", " << x << ", " << y << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib2fFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib2fFn(indx, x, y);
}

static void GL_BINDING_CALL Debug_glVertexAttrib2fv(GLuint indx,
                                                    const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib2fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib2fvFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib2fvFn(indx, values);
}

static void GL_BINDING_CALL Debug_glVertexAttrib3f(GLuint indx,
                                                   GLfloat x,
                                                   GLfloat y,
                                                   GLfloat z) {
  GL_SERVICE_LOG("glVertexAttrib3f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib3fFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib3fFn(indx, x, y, z);
}

static void GL_BINDING_CALL Debug_glVertexAttrib3fv(GLuint indx,
                                                    const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib3fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib3fvFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib3fvFn(indx, values);
}

static void GL_BINDING_CALL Debug_glVertexAttrib4f(GLuint indx,
                                                   GLfloat x,
                                                   GLfloat y,
                                                   GLfloat z,
                                                   GLfloat w) {
  GL_SERVICE_LOG("glVertexAttrib4f"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib4fFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib4fFn(indx, x, y, z, w);
}

static void GL_BINDING_CALL Debug_glVertexAttrib4fv(GLuint indx,
                                                    const GLfloat* values) {
  GL_SERVICE_LOG("glVertexAttrib4fv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttrib4fvFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttrib4fvFn(indx, values);
}

static void GL_BINDING_CALL Debug_glVertexAttribDivisorANGLE(GLuint index,
                                                             GLuint divisor) {
  GL_SERVICE_LOG("glVertexAttribDivisorANGLE"
                 << "(" << index << ", " << divisor << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribDivisorANGLEFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribDivisorANGLEFn(index, divisor);
}

static void GL_BINDING_CALL
Debug_glVertexAttribI4i(GLuint indx, GLint x, GLint y, GLint z, GLint w) {
  GL_SERVICE_LOG("glVertexAttribI4i"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribI4iFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribI4iFn(indx, x, y, z, w);
}

static void GL_BINDING_CALL Debug_glVertexAttribI4iv(GLuint indx,
                                                     const GLint* values) {
  GL_SERVICE_LOG("glVertexAttribI4iv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribI4ivFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribI4ivFn(indx, values);
}

static void GL_BINDING_CALL
Debug_glVertexAttribI4ui(GLuint indx, GLuint x, GLuint y, GLuint z, GLuint w) {
  GL_SERVICE_LOG("glVertexAttribI4ui"
                 << "(" << indx << ", " << x << ", " << y << ", " << z << ", "
                 << w << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribI4uiFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribI4uiFn(indx, x, y, z, w);
}

static void GL_BINDING_CALL Debug_glVertexAttribI4uiv(GLuint indx,
                                                      const GLuint* values) {
  GL_SERVICE_LOG("glVertexAttribI4uiv"
                 << "(" << indx << ", " << static_cast<const void*>(values)
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribI4uivFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribI4uivFn(indx, values);
}

static void GL_BINDING_CALL Debug_glVertexAttribIPointer(GLuint indx,
                                                         GLint size,
                                                         GLenum type,
                                                         GLsizei stride,
                                                         const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribIPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribIPointerFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

static void GL_BINDING_CALL Debug_glVertexAttribPointer(GLuint indx,
                                                        GLint size,
                                                        GLenum type,
                                                        GLboolean normalized,
                                                        GLsizei stride,
                                                        const void* ptr) {
  GL_SERVICE_LOG("glVertexAttribPointer"
                 << "(" << indx << ", " << size << ", "
                 << GLEnums::GetStringEnum(type) << ", "
                 << GLEnums::GetStringBool(normalized) << ", " << stride << ", "
                 << static_cast<const void*>(ptr) << ")");
  DCHECK(g_driver_gl.debug_fn.glVertexAttribPointerFn != nullptr);
  g_driver_gl.debug_fn.glVertexAttribPointerFn(indx, size, type, normalized,
                                               stride, ptr);
}

static void GL_BINDING_CALL Debug_glViewport(GLint x,
                                             GLint y,
                                             GLsizei width,
                                             GLsizei height) {
  GL_SERVICE_LOG("glViewport"
                 << "(" << x << ", " << y << ", " << width << ", " << height
                 << ")");
  DCHECK(g_driver_gl.debug_fn.glViewportFn != nullptr);
  g_driver_gl.debug_fn.glViewportFn(x, y, width, height);
}

static GLenum GL_BINDING_CALL Debug_glWaitSync(GLsync sync,
                                               GLbitfield flags,
                                               GLuint64 timeout) {
  GL_SERVICE_LOG("glWaitSync"
                 << "(" << sync << ", " << flags << ", " << timeout << ")");
  DCHECK(g_driver_gl.debug_fn.glWaitSyncFn != nullptr);
  GLenum result = g_driver_gl.debug_fn.glWaitSyncFn(sync, flags, timeout);
  GL_SERVICE_LOG("GL_RESULT: " << result);
  return result;
}
}  // extern "C"

void DriverGL::InitializeDebugBindings() {
  if (!debug_fn.glActiveTextureFn) {
    debug_fn.glActiveTextureFn = fn.glActiveTextureFn;
    fn.glActiveTextureFn = Debug_glActiveTexture;
  }
  if (!debug_fn.glApplyFramebufferAttachmentCMAAINTELFn) {
    debug_fn.glApplyFramebufferAttachmentCMAAINTELFn =
        fn.glApplyFramebufferAttachmentCMAAINTELFn;
    fn.glApplyFramebufferAttachmentCMAAINTELFn =
        Debug_glApplyFramebufferAttachmentCMAAINTEL;
  }
  if (!debug_fn.glAttachShaderFn) {
    debug_fn.glAttachShaderFn = fn.glAttachShaderFn;
    fn.glAttachShaderFn = Debug_glAttachShader;
  }
  if (!debug_fn.glBeginQueryFn) {
    debug_fn.glBeginQueryFn = fn.glBeginQueryFn;
    fn.glBeginQueryFn = Debug_glBeginQuery;
  }
  if (!debug_fn.glBeginTransformFeedbackFn) {
    debug_fn.glBeginTransformFeedbackFn = fn.glBeginTransformFeedbackFn;
    fn.glBeginTransformFeedbackFn = Debug_glBeginTransformFeedback;
  }
  if (!debug_fn.glBindAttribLocationFn) {
    debug_fn.glBindAttribLocationFn = fn.glBindAttribLocationFn;
    fn.glBindAttribLocationFn = Debug_glBindAttribLocation;
  }
  if (!debug_fn.glBindBufferFn) {
    debug_fn.glBindBufferFn = fn.glBindBufferFn;
    fn.glBindBufferFn = Debug_glBindBuffer;
  }
  if (!debug_fn.glBindBufferBaseFn) {
    debug_fn.glBindBufferBaseFn = fn.glBindBufferBaseFn;
    fn.glBindBufferBaseFn = Debug_glBindBufferBase;
  }
  if (!debug_fn.glBindBufferRangeFn) {
    debug_fn.glBindBufferRangeFn = fn.glBindBufferRangeFn;
    fn.glBindBufferRangeFn = Debug_glBindBufferRange;
  }
  if (!debug_fn.glBindFragDataLocationFn) {
    debug_fn.glBindFragDataLocationFn = fn.glBindFragDataLocationFn;
    fn.glBindFragDataLocationFn = Debug_glBindFragDataLocation;
  }
  if (!debug_fn.glBindFragDataLocationIndexedFn) {
    debug_fn.glBindFragDataLocationIndexedFn =
        fn.glBindFragDataLocationIndexedFn;
    fn.glBindFragDataLocationIndexedFn = Debug_glBindFragDataLocationIndexed;
  }
  if (!debug_fn.glBindFramebufferEXTFn) {
    debug_fn.glBindFramebufferEXTFn = fn.glBindFramebufferEXTFn;
    fn.glBindFramebufferEXTFn = Debug_glBindFramebufferEXT;
  }
  if (!debug_fn.glBindImageTextureEXTFn) {
    debug_fn.glBindImageTextureEXTFn = fn.glBindImageTextureEXTFn;
    fn.glBindImageTextureEXTFn = Debug_glBindImageTextureEXT;
  }
  if (!debug_fn.glBindRenderbufferEXTFn) {
    debug_fn.glBindRenderbufferEXTFn = fn.glBindRenderbufferEXTFn;
    fn.glBindRenderbufferEXTFn = Debug_glBindRenderbufferEXT;
  }
  if (!debug_fn.glBindSamplerFn) {
    debug_fn.glBindSamplerFn = fn.glBindSamplerFn;
    fn.glBindSamplerFn = Debug_glBindSampler;
  }
  if (!debug_fn.glBindTextureFn) {
    debug_fn.glBindTextureFn = fn.glBindTextureFn;
    fn.glBindTextureFn = Debug_glBindTexture;
  }
  if (!debug_fn.glBindTransformFeedbackFn) {
    debug_fn.glBindTransformFeedbackFn = fn.glBindTransformFeedbackFn;
    fn.glBindTransformFeedbackFn = Debug_glBindTransformFeedback;
  }
  if (!debug_fn.glBindUniformLocationCHROMIUMFn) {
    debug_fn.glBindUniformLocationCHROMIUMFn =
        fn.glBindUniformLocationCHROMIUMFn;
    fn.glBindUniformLocationCHROMIUMFn = Debug_glBindUniformLocationCHROMIUM;
  }
  if (!debug_fn.glBindVertexArrayOESFn) {
    debug_fn.glBindVertexArrayOESFn = fn.glBindVertexArrayOESFn;
    fn.glBindVertexArrayOESFn = Debug_glBindVertexArrayOES;
  }
  if (!debug_fn.glBlendBarrierKHRFn) {
    debug_fn.glBlendBarrierKHRFn = fn.glBlendBarrierKHRFn;
    fn.glBlendBarrierKHRFn = Debug_glBlendBarrierKHR;
  }
  if (!debug_fn.glBlendColorFn) {
    debug_fn.glBlendColorFn = fn.glBlendColorFn;
    fn.glBlendColorFn = Debug_glBlendColor;
  }
  if (!debug_fn.glBlendEquationFn) {
    debug_fn.glBlendEquationFn = fn.glBlendEquationFn;
    fn.glBlendEquationFn = Debug_glBlendEquation;
  }
  if (!debug_fn.glBlendEquationSeparateFn) {
    debug_fn.glBlendEquationSeparateFn = fn.glBlendEquationSeparateFn;
    fn.glBlendEquationSeparateFn = Debug_glBlendEquationSeparate;
  }
  if (!debug_fn.glBlendFuncFn) {
    debug_fn.glBlendFuncFn = fn.glBlendFuncFn;
    fn.glBlendFuncFn = Debug_glBlendFunc;
  }
  if (!debug_fn.glBlendFuncSeparateFn) {
    debug_fn.glBlendFuncSeparateFn = fn.glBlendFuncSeparateFn;
    fn.glBlendFuncSeparateFn = Debug_glBlendFuncSeparate;
  }
  if (!debug_fn.glBlitFramebufferFn) {
    debug_fn.glBlitFramebufferFn = fn.glBlitFramebufferFn;
    fn.glBlitFramebufferFn = Debug_glBlitFramebuffer;
  }
  if (!debug_fn.glBlitFramebufferANGLEFn) {
    debug_fn.glBlitFramebufferANGLEFn = fn.glBlitFramebufferANGLEFn;
    fn.glBlitFramebufferANGLEFn = Debug_glBlitFramebufferANGLE;
  }
  if (!debug_fn.glBlitFramebufferEXTFn) {
    debug_fn.glBlitFramebufferEXTFn = fn.glBlitFramebufferEXTFn;
    fn.glBlitFramebufferEXTFn = Debug_glBlitFramebufferEXT;
  }
  if (!debug_fn.glBufferDataFn) {
    debug_fn.glBufferDataFn = fn.glBufferDataFn;
    fn.glBufferDataFn = Debug_glBufferData;
  }
  if (!debug_fn.glBufferSubDataFn) {
    debug_fn.glBufferSubDataFn = fn.glBufferSubDataFn;
    fn.glBufferSubDataFn = Debug_glBufferSubData;
  }
  if (!debug_fn.glCheckFramebufferStatusEXTFn) {
    debug_fn.glCheckFramebufferStatusEXTFn = fn.glCheckFramebufferStatusEXTFn;
    fn.glCheckFramebufferStatusEXTFn = Debug_glCheckFramebufferStatusEXT;
  }
  if (!debug_fn.glClearFn) {
    debug_fn.glClearFn = fn.glClearFn;
    fn.glClearFn = Debug_glClear;
  }
  if (!debug_fn.glClearBufferfiFn) {
    debug_fn.glClearBufferfiFn = fn.glClearBufferfiFn;
    fn.glClearBufferfiFn = Debug_glClearBufferfi;
  }
  if (!debug_fn.glClearBufferfvFn) {
    debug_fn.glClearBufferfvFn = fn.glClearBufferfvFn;
    fn.glClearBufferfvFn = Debug_glClearBufferfv;
  }
  if (!debug_fn.glClearBufferivFn) {
    debug_fn.glClearBufferivFn = fn.glClearBufferivFn;
    fn.glClearBufferivFn = Debug_glClearBufferiv;
  }
  if (!debug_fn.glClearBufferuivFn) {
    debug_fn.glClearBufferuivFn = fn.glClearBufferuivFn;
    fn.glClearBufferuivFn = Debug_glClearBufferuiv;
  }
  if (!debug_fn.glClearColorFn) {
    debug_fn.glClearColorFn = fn.glClearColorFn;
    fn.glClearColorFn = Debug_glClearColor;
  }
  if (!debug_fn.glClearDepthFn) {
    debug_fn.glClearDepthFn = fn.glClearDepthFn;
    fn.glClearDepthFn = Debug_glClearDepth;
  }
  if (!debug_fn.glClearDepthfFn) {
    debug_fn.glClearDepthfFn = fn.glClearDepthfFn;
    fn.glClearDepthfFn = Debug_glClearDepthf;
  }
  if (!debug_fn.glClearStencilFn) {
    debug_fn.glClearStencilFn = fn.glClearStencilFn;
    fn.glClearStencilFn = Debug_glClearStencil;
  }
  if (!debug_fn.glClientWaitSyncFn) {
    debug_fn.glClientWaitSyncFn = fn.glClientWaitSyncFn;
    fn.glClientWaitSyncFn = Debug_glClientWaitSync;
  }
  if (!debug_fn.glColorMaskFn) {
    debug_fn.glColorMaskFn = fn.glColorMaskFn;
    fn.glColorMaskFn = Debug_glColorMask;
  }
  if (!debug_fn.glCompileShaderFn) {
    debug_fn.glCompileShaderFn = fn.glCompileShaderFn;
    fn.glCompileShaderFn = Debug_glCompileShader;
  }
  if (!debug_fn.glCompressedCopyTextureCHROMIUMFn) {
    debug_fn.glCompressedCopyTextureCHROMIUMFn =
        fn.glCompressedCopyTextureCHROMIUMFn;
    fn.glCompressedCopyTextureCHROMIUMFn =
        Debug_glCompressedCopyTextureCHROMIUM;
  }
  if (!debug_fn.glCompressedTexImage2DFn) {
    debug_fn.glCompressedTexImage2DFn = fn.glCompressedTexImage2DFn;
    fn.glCompressedTexImage2DFn = Debug_glCompressedTexImage2D;
  }
  if (!debug_fn.glCompressedTexImage3DFn) {
    debug_fn.glCompressedTexImage3DFn = fn.glCompressedTexImage3DFn;
    fn.glCompressedTexImage3DFn = Debug_glCompressedTexImage3D;
  }
  if (!debug_fn.glCompressedTexSubImage2DFn) {
    debug_fn.glCompressedTexSubImage2DFn = fn.glCompressedTexSubImage2DFn;
    fn.glCompressedTexSubImage2DFn = Debug_glCompressedTexSubImage2D;
  }
  if (!debug_fn.glCompressedTexSubImage3DFn) {
    debug_fn.glCompressedTexSubImage3DFn = fn.glCompressedTexSubImage3DFn;
    fn.glCompressedTexSubImage3DFn = Debug_glCompressedTexSubImage3D;
  }
  if (!debug_fn.glCopyBufferSubDataFn) {
    debug_fn.glCopyBufferSubDataFn = fn.glCopyBufferSubDataFn;
    fn.glCopyBufferSubDataFn = Debug_glCopyBufferSubData;
  }
  if (!debug_fn.glCopySubTextureCHROMIUMFn) {
    debug_fn.glCopySubTextureCHROMIUMFn = fn.glCopySubTextureCHROMIUMFn;
    fn.glCopySubTextureCHROMIUMFn = Debug_glCopySubTextureCHROMIUM;
  }
  if (!debug_fn.glCopyTexImage2DFn) {
    debug_fn.glCopyTexImage2DFn = fn.glCopyTexImage2DFn;
    fn.glCopyTexImage2DFn = Debug_glCopyTexImage2D;
  }
  if (!debug_fn.glCopyTexSubImage2DFn) {
    debug_fn.glCopyTexSubImage2DFn = fn.glCopyTexSubImage2DFn;
    fn.glCopyTexSubImage2DFn = Debug_glCopyTexSubImage2D;
  }
  if (!debug_fn.glCopyTexSubImage3DFn) {
    debug_fn.glCopyTexSubImage3DFn = fn.glCopyTexSubImage3DFn;
    fn.glCopyTexSubImage3DFn = Debug_glCopyTexSubImage3D;
  }
  if (!debug_fn.glCopyTextureCHROMIUMFn) {
    debug_fn.glCopyTextureCHROMIUMFn = fn.glCopyTextureCHROMIUMFn;
    fn.glCopyTextureCHROMIUMFn = Debug_glCopyTextureCHROMIUM;
  }
  if (!debug_fn.glCoverageModulationNVFn) {
    debug_fn.glCoverageModulationNVFn = fn.glCoverageModulationNVFn;
    fn.glCoverageModulationNVFn = Debug_glCoverageModulationNV;
  }
  if (!debug_fn.glCoverFillPathInstancedNVFn) {
    debug_fn.glCoverFillPathInstancedNVFn = fn.glCoverFillPathInstancedNVFn;
    fn.glCoverFillPathInstancedNVFn = Debug_glCoverFillPathInstancedNV;
  }
  if (!debug_fn.glCoverFillPathNVFn) {
    debug_fn.glCoverFillPathNVFn = fn.glCoverFillPathNVFn;
    fn.glCoverFillPathNVFn = Debug_glCoverFillPathNV;
  }
  if (!debug_fn.glCoverStrokePathInstancedNVFn) {
    debug_fn.glCoverStrokePathInstancedNVFn = fn.glCoverStrokePathInstancedNVFn;
    fn.glCoverStrokePathInstancedNVFn = Debug_glCoverStrokePathInstancedNV;
  }
  if (!debug_fn.glCoverStrokePathNVFn) {
    debug_fn.glCoverStrokePathNVFn = fn.glCoverStrokePathNVFn;
    fn.glCoverStrokePathNVFn = Debug_glCoverStrokePathNV;
  }
  if (!debug_fn.glCreateProgramFn) {
    debug_fn.glCreateProgramFn = fn.glCreateProgramFn;
    fn.glCreateProgramFn = Debug_glCreateProgram;
  }
  if (!debug_fn.glCreateShaderFn) {
    debug_fn.glCreateShaderFn = fn.glCreateShaderFn;
    fn.glCreateShaderFn = Debug_glCreateShader;
  }
  if (!debug_fn.glCullFaceFn) {
    debug_fn.glCullFaceFn = fn.glCullFaceFn;
    fn.glCullFaceFn = Debug_glCullFace;
  }
  if (!debug_fn.glDebugMessageCallbackFn) {
    debug_fn.glDebugMessageCallbackFn = fn.glDebugMessageCallbackFn;
    fn.glDebugMessageCallbackFn = Debug_glDebugMessageCallback;
  }
  if (!debug_fn.glDebugMessageControlFn) {
    debug_fn.glDebugMessageControlFn = fn.glDebugMessageControlFn;
    fn.glDebugMessageControlFn = Debug_glDebugMessageControl;
  }
  if (!debug_fn.glDebugMessageInsertFn) {
    debug_fn.glDebugMessageInsertFn = fn.glDebugMessageInsertFn;
    fn.glDebugMessageInsertFn = Debug_glDebugMessageInsert;
  }
  if (!debug_fn.glDeleteBuffersARBFn) {
    debug_fn.glDeleteBuffersARBFn = fn.glDeleteBuffersARBFn;
    fn.glDeleteBuffersARBFn = Debug_glDeleteBuffersARB;
  }
  if (!debug_fn.glDeleteFencesAPPLEFn) {
    debug_fn.glDeleteFencesAPPLEFn = fn.glDeleteFencesAPPLEFn;
    fn.glDeleteFencesAPPLEFn = Debug_glDeleteFencesAPPLE;
  }
  if (!debug_fn.glDeleteFencesNVFn) {
    debug_fn.glDeleteFencesNVFn = fn.glDeleteFencesNVFn;
    fn.glDeleteFencesNVFn = Debug_glDeleteFencesNV;
  }
  if (!debug_fn.glDeleteFramebuffersEXTFn) {
    debug_fn.glDeleteFramebuffersEXTFn = fn.glDeleteFramebuffersEXTFn;
    fn.glDeleteFramebuffersEXTFn = Debug_glDeleteFramebuffersEXT;
  }
  if (!debug_fn.glDeletePathsNVFn) {
    debug_fn.glDeletePathsNVFn = fn.glDeletePathsNVFn;
    fn.glDeletePathsNVFn = Debug_glDeletePathsNV;
  }
  if (!debug_fn.glDeleteProgramFn) {
    debug_fn.glDeleteProgramFn = fn.glDeleteProgramFn;
    fn.glDeleteProgramFn = Debug_glDeleteProgram;
  }
  if (!debug_fn.glDeleteQueriesFn) {
    debug_fn.glDeleteQueriesFn = fn.glDeleteQueriesFn;
    fn.glDeleteQueriesFn = Debug_glDeleteQueries;
  }
  if (!debug_fn.glDeleteRenderbuffersEXTFn) {
    debug_fn.glDeleteRenderbuffersEXTFn = fn.glDeleteRenderbuffersEXTFn;
    fn.glDeleteRenderbuffersEXTFn = Debug_glDeleteRenderbuffersEXT;
  }
  if (!debug_fn.glDeleteSamplersFn) {
    debug_fn.glDeleteSamplersFn = fn.glDeleteSamplersFn;
    fn.glDeleteSamplersFn = Debug_glDeleteSamplers;
  }
  if (!debug_fn.glDeleteShaderFn) {
    debug_fn.glDeleteShaderFn = fn.glDeleteShaderFn;
    fn.glDeleteShaderFn = Debug_glDeleteShader;
  }
  if (!debug_fn.glDeleteSyncFn) {
    debug_fn.glDeleteSyncFn = fn.glDeleteSyncFn;
    fn.glDeleteSyncFn = Debug_glDeleteSync;
  }
  if (!debug_fn.glDeleteTexturesFn) {
    debug_fn.glDeleteTexturesFn = fn.glDeleteTexturesFn;
    fn.glDeleteTexturesFn = Debug_glDeleteTextures;
  }
  if (!debug_fn.glDeleteTransformFeedbacksFn) {
    debug_fn.glDeleteTransformFeedbacksFn = fn.glDeleteTransformFeedbacksFn;
    fn.glDeleteTransformFeedbacksFn = Debug_glDeleteTransformFeedbacks;
  }
  if (!debug_fn.glDeleteVertexArraysOESFn) {
    debug_fn.glDeleteVertexArraysOESFn = fn.glDeleteVertexArraysOESFn;
    fn.glDeleteVertexArraysOESFn = Debug_glDeleteVertexArraysOES;
  }
  if (!debug_fn.glDepthFuncFn) {
    debug_fn.glDepthFuncFn = fn.glDepthFuncFn;
    fn.glDepthFuncFn = Debug_glDepthFunc;
  }
  if (!debug_fn.glDepthMaskFn) {
    debug_fn.glDepthMaskFn = fn.glDepthMaskFn;
    fn.glDepthMaskFn = Debug_glDepthMask;
  }
  if (!debug_fn.glDepthRangeFn) {
    debug_fn.glDepthRangeFn = fn.glDepthRangeFn;
    fn.glDepthRangeFn = Debug_glDepthRange;
  }
  if (!debug_fn.glDepthRangefFn) {
    debug_fn.glDepthRangefFn = fn.glDepthRangefFn;
    fn.glDepthRangefFn = Debug_glDepthRangef;
  }
  if (!debug_fn.glDetachShaderFn) {
    debug_fn.glDetachShaderFn = fn.glDetachShaderFn;
    fn.glDetachShaderFn = Debug_glDetachShader;
  }
  if (!debug_fn.glDisableFn) {
    debug_fn.glDisableFn = fn.glDisableFn;
    fn.glDisableFn = Debug_glDisable;
  }
  if (!debug_fn.glDisableVertexAttribArrayFn) {
    debug_fn.glDisableVertexAttribArrayFn = fn.glDisableVertexAttribArrayFn;
    fn.glDisableVertexAttribArrayFn = Debug_glDisableVertexAttribArray;
  }
  if (!debug_fn.glDiscardFramebufferEXTFn) {
    debug_fn.glDiscardFramebufferEXTFn = fn.glDiscardFramebufferEXTFn;
    fn.glDiscardFramebufferEXTFn = Debug_glDiscardFramebufferEXT;
  }
  if (!debug_fn.glDrawArraysFn) {
    debug_fn.glDrawArraysFn = fn.glDrawArraysFn;
    fn.glDrawArraysFn = Debug_glDrawArrays;
  }
  if (!debug_fn.glDrawArraysInstancedANGLEFn) {
    debug_fn.glDrawArraysInstancedANGLEFn = fn.glDrawArraysInstancedANGLEFn;
    fn.glDrawArraysInstancedANGLEFn = Debug_glDrawArraysInstancedANGLE;
  }
  if (!debug_fn.glDrawBufferFn) {
    debug_fn.glDrawBufferFn = fn.glDrawBufferFn;
    fn.glDrawBufferFn = Debug_glDrawBuffer;
  }
  if (!debug_fn.glDrawBuffersARBFn) {
    debug_fn.glDrawBuffersARBFn = fn.glDrawBuffersARBFn;
    fn.glDrawBuffersARBFn = Debug_glDrawBuffersARB;
  }
  if (!debug_fn.glDrawElementsFn) {
    debug_fn.glDrawElementsFn = fn.glDrawElementsFn;
    fn.glDrawElementsFn = Debug_glDrawElements;
  }
  if (!debug_fn.glDrawElementsInstancedANGLEFn) {
    debug_fn.glDrawElementsInstancedANGLEFn = fn.glDrawElementsInstancedANGLEFn;
    fn.glDrawElementsInstancedANGLEFn = Debug_glDrawElementsInstancedANGLE;
  }
  if (!debug_fn.glDrawRangeElementsFn) {
    debug_fn.glDrawRangeElementsFn = fn.glDrawRangeElementsFn;
    fn.glDrawRangeElementsFn = Debug_glDrawRangeElements;
  }
  if (!debug_fn.glEGLImageTargetRenderbufferStorageOESFn) {
    debug_fn.glEGLImageTargetRenderbufferStorageOESFn =
        fn.glEGLImageTargetRenderbufferStorageOESFn;
    fn.glEGLImageTargetRenderbufferStorageOESFn =
        Debug_glEGLImageTargetRenderbufferStorageOES;
  }
  if (!debug_fn.glEGLImageTargetTexture2DOESFn) {
    debug_fn.glEGLImageTargetTexture2DOESFn = fn.glEGLImageTargetTexture2DOESFn;
    fn.glEGLImageTargetTexture2DOESFn = Debug_glEGLImageTargetTexture2DOES;
  }
  if (!debug_fn.glEnableFn) {
    debug_fn.glEnableFn = fn.glEnableFn;
    fn.glEnableFn = Debug_glEnable;
  }
  if (!debug_fn.glEnableVertexAttribArrayFn) {
    debug_fn.glEnableVertexAttribArrayFn = fn.glEnableVertexAttribArrayFn;
    fn.glEnableVertexAttribArrayFn = Debug_glEnableVertexAttribArray;
  }
  if (!debug_fn.glEndQueryFn) {
    debug_fn.glEndQueryFn = fn.glEndQueryFn;
    fn.glEndQueryFn = Debug_glEndQuery;
  }
  if (!debug_fn.glEndTransformFeedbackFn) {
    debug_fn.glEndTransformFeedbackFn = fn.glEndTransformFeedbackFn;
    fn.glEndTransformFeedbackFn = Debug_glEndTransformFeedback;
  }
  if (!debug_fn.glFenceSyncFn) {
    debug_fn.glFenceSyncFn = fn.glFenceSyncFn;
    fn.glFenceSyncFn = Debug_glFenceSync;
  }
  if (!debug_fn.glFinishFn) {
    debug_fn.glFinishFn = fn.glFinishFn;
    fn.glFinishFn = Debug_glFinish;
  }
  if (!debug_fn.glFinishFenceAPPLEFn) {
    debug_fn.glFinishFenceAPPLEFn = fn.glFinishFenceAPPLEFn;
    fn.glFinishFenceAPPLEFn = Debug_glFinishFenceAPPLE;
  }
  if (!debug_fn.glFinishFenceNVFn) {
    debug_fn.glFinishFenceNVFn = fn.glFinishFenceNVFn;
    fn.glFinishFenceNVFn = Debug_glFinishFenceNV;
  }
  if (!debug_fn.glFlushFn) {
    debug_fn.glFlushFn = fn.glFlushFn;
    fn.glFlushFn = Debug_glFlush;
  }
  if (!debug_fn.glFlushMappedBufferRangeFn) {
    debug_fn.glFlushMappedBufferRangeFn = fn.glFlushMappedBufferRangeFn;
    fn.glFlushMappedBufferRangeFn = Debug_glFlushMappedBufferRange;
  }
  if (!debug_fn.glFramebufferRenderbufferEXTFn) {
    debug_fn.glFramebufferRenderbufferEXTFn = fn.glFramebufferRenderbufferEXTFn;
    fn.glFramebufferRenderbufferEXTFn = Debug_glFramebufferRenderbufferEXT;
  }
  if (!debug_fn.glFramebufferTexture2DEXTFn) {
    debug_fn.glFramebufferTexture2DEXTFn = fn.glFramebufferTexture2DEXTFn;
    fn.glFramebufferTexture2DEXTFn = Debug_glFramebufferTexture2DEXT;
  }
  if (!debug_fn.glFramebufferTexture2DMultisampleEXTFn) {
    debug_fn.glFramebufferTexture2DMultisampleEXTFn =
        fn.glFramebufferTexture2DMultisampleEXTFn;
    fn.glFramebufferTexture2DMultisampleEXTFn =
        Debug_glFramebufferTexture2DMultisampleEXT;
  }
  if (!debug_fn.glFramebufferTexture2DMultisampleIMGFn) {
    debug_fn.glFramebufferTexture2DMultisampleIMGFn =
        fn.glFramebufferTexture2DMultisampleIMGFn;
    fn.glFramebufferTexture2DMultisampleIMGFn =
        Debug_glFramebufferTexture2DMultisampleIMG;
  }
  if (!debug_fn.glFramebufferTextureLayerFn) {
    debug_fn.glFramebufferTextureLayerFn = fn.glFramebufferTextureLayerFn;
    fn.glFramebufferTextureLayerFn = Debug_glFramebufferTextureLayer;
  }
  if (!debug_fn.glFrontFaceFn) {
    debug_fn.glFrontFaceFn = fn.glFrontFaceFn;
    fn.glFrontFaceFn = Debug_glFrontFace;
  }
  if (!debug_fn.glGenBuffersARBFn) {
    debug_fn.glGenBuffersARBFn = fn.glGenBuffersARBFn;
    fn.glGenBuffersARBFn = Debug_glGenBuffersARB;
  }
  if (!debug_fn.glGenerateMipmapEXTFn) {
    debug_fn.glGenerateMipmapEXTFn = fn.glGenerateMipmapEXTFn;
    fn.glGenerateMipmapEXTFn = Debug_glGenerateMipmapEXT;
  }
  if (!debug_fn.glGenFencesAPPLEFn) {
    debug_fn.glGenFencesAPPLEFn = fn.glGenFencesAPPLEFn;
    fn.glGenFencesAPPLEFn = Debug_glGenFencesAPPLE;
  }
  if (!debug_fn.glGenFencesNVFn) {
    debug_fn.glGenFencesNVFn = fn.glGenFencesNVFn;
    fn.glGenFencesNVFn = Debug_glGenFencesNV;
  }
  if (!debug_fn.glGenFramebuffersEXTFn) {
    debug_fn.glGenFramebuffersEXTFn = fn.glGenFramebuffersEXTFn;
    fn.glGenFramebuffersEXTFn = Debug_glGenFramebuffersEXT;
  }
  if (!debug_fn.glGenPathsNVFn) {
    debug_fn.glGenPathsNVFn = fn.glGenPathsNVFn;
    fn.glGenPathsNVFn = Debug_glGenPathsNV;
  }
  if (!debug_fn.glGenQueriesFn) {
    debug_fn.glGenQueriesFn = fn.glGenQueriesFn;
    fn.glGenQueriesFn = Debug_glGenQueries;
  }
  if (!debug_fn.glGenRenderbuffersEXTFn) {
    debug_fn.glGenRenderbuffersEXTFn = fn.glGenRenderbuffersEXTFn;
    fn.glGenRenderbuffersEXTFn = Debug_glGenRenderbuffersEXT;
  }
  if (!debug_fn.glGenSamplersFn) {
    debug_fn.glGenSamplersFn = fn.glGenSamplersFn;
    fn.glGenSamplersFn = Debug_glGenSamplers;
  }
  if (!debug_fn.glGenTexturesFn) {
    debug_fn.glGenTexturesFn = fn.glGenTexturesFn;
    fn.glGenTexturesFn = Debug_glGenTextures;
  }
  if (!debug_fn.glGenTransformFeedbacksFn) {
    debug_fn.glGenTransformFeedbacksFn = fn.glGenTransformFeedbacksFn;
    fn.glGenTransformFeedbacksFn = Debug_glGenTransformFeedbacks;
  }
  if (!debug_fn.glGenVertexArraysOESFn) {
    debug_fn.glGenVertexArraysOESFn = fn.glGenVertexArraysOESFn;
    fn.glGenVertexArraysOESFn = Debug_glGenVertexArraysOES;
  }
  if (!debug_fn.glGetActiveAttribFn) {
    debug_fn.glGetActiveAttribFn = fn.glGetActiveAttribFn;
    fn.glGetActiveAttribFn = Debug_glGetActiveAttrib;
  }
  if (!debug_fn.glGetActiveUniformFn) {
    debug_fn.glGetActiveUniformFn = fn.glGetActiveUniformFn;
    fn.glGetActiveUniformFn = Debug_glGetActiveUniform;
  }
  if (!debug_fn.glGetActiveUniformBlockivFn) {
    debug_fn.glGetActiveUniformBlockivFn = fn.glGetActiveUniformBlockivFn;
    fn.glGetActiveUniformBlockivFn = Debug_glGetActiveUniformBlockiv;
  }
  if (!debug_fn.glGetActiveUniformBlockivRobustANGLEFn) {
    debug_fn.glGetActiveUniformBlockivRobustANGLEFn =
        fn.glGetActiveUniformBlockivRobustANGLEFn;
    fn.glGetActiveUniformBlockivRobustANGLEFn =
        Debug_glGetActiveUniformBlockivRobustANGLE;
  }
  if (!debug_fn.glGetActiveUniformBlockNameFn) {
    debug_fn.glGetActiveUniformBlockNameFn = fn.glGetActiveUniformBlockNameFn;
    fn.glGetActiveUniformBlockNameFn = Debug_glGetActiveUniformBlockName;
  }
  if (!debug_fn.glGetActiveUniformsivFn) {
    debug_fn.glGetActiveUniformsivFn = fn.glGetActiveUniformsivFn;
    fn.glGetActiveUniformsivFn = Debug_glGetActiveUniformsiv;
  }
  if (!debug_fn.glGetAttachedShadersFn) {
    debug_fn.glGetAttachedShadersFn = fn.glGetAttachedShadersFn;
    fn.glGetAttachedShadersFn = Debug_glGetAttachedShaders;
  }
  if (!debug_fn.glGetAttribLocationFn) {
    debug_fn.glGetAttribLocationFn = fn.glGetAttribLocationFn;
    fn.glGetAttribLocationFn = Debug_glGetAttribLocation;
  }
  if (!debug_fn.glGetBooleani_vRobustANGLEFn) {
    debug_fn.glGetBooleani_vRobustANGLEFn = fn.glGetBooleani_vRobustANGLEFn;
    fn.glGetBooleani_vRobustANGLEFn = Debug_glGetBooleani_vRobustANGLE;
  }
  if (!debug_fn.glGetBooleanvFn) {
    debug_fn.glGetBooleanvFn = fn.glGetBooleanvFn;
    fn.glGetBooleanvFn = Debug_glGetBooleanv;
  }
  if (!debug_fn.glGetBooleanvRobustANGLEFn) {
    debug_fn.glGetBooleanvRobustANGLEFn = fn.glGetBooleanvRobustANGLEFn;
    fn.glGetBooleanvRobustANGLEFn = Debug_glGetBooleanvRobustANGLE;
  }
  if (!debug_fn.glGetBufferParameteri64vRobustANGLEFn) {
    debug_fn.glGetBufferParameteri64vRobustANGLEFn =
        fn.glGetBufferParameteri64vRobustANGLEFn;
    fn.glGetBufferParameteri64vRobustANGLEFn =
        Debug_glGetBufferParameteri64vRobustANGLE;
  }
  if (!debug_fn.glGetBufferParameterivFn) {
    debug_fn.glGetBufferParameterivFn = fn.glGetBufferParameterivFn;
    fn.glGetBufferParameterivFn = Debug_glGetBufferParameteriv;
  }
  if (!debug_fn.glGetBufferParameterivRobustANGLEFn) {
    debug_fn.glGetBufferParameterivRobustANGLEFn =
        fn.glGetBufferParameterivRobustANGLEFn;
    fn.glGetBufferParameterivRobustANGLEFn =
        Debug_glGetBufferParameterivRobustANGLE;
  }
  if (!debug_fn.glGetBufferPointervRobustANGLEFn) {
    debug_fn.glGetBufferPointervRobustANGLEFn =
        fn.glGetBufferPointervRobustANGLEFn;
    fn.glGetBufferPointervRobustANGLEFn = Debug_glGetBufferPointervRobustANGLE;
  }
  if (!debug_fn.glGetDebugMessageLogFn) {
    debug_fn.glGetDebugMessageLogFn = fn.glGetDebugMessageLogFn;
    fn.glGetDebugMessageLogFn = Debug_glGetDebugMessageLog;
  }
  if (!debug_fn.glGetErrorFn) {
    debug_fn.glGetErrorFn = fn.glGetErrorFn;
    fn.glGetErrorFn = Debug_glGetError;
  }
  if (!debug_fn.glGetFenceivNVFn) {
    debug_fn.glGetFenceivNVFn = fn.glGetFenceivNVFn;
    fn.glGetFenceivNVFn = Debug_glGetFenceivNV;
  }
  if (!debug_fn.glGetFloatvFn) {
    debug_fn.glGetFloatvFn = fn.glGetFloatvFn;
    fn.glGetFloatvFn = Debug_glGetFloatv;
  }
  if (!debug_fn.glGetFloatvRobustANGLEFn) {
    debug_fn.glGetFloatvRobustANGLEFn = fn.glGetFloatvRobustANGLEFn;
    fn.glGetFloatvRobustANGLEFn = Debug_glGetFloatvRobustANGLE;
  }
  if (!debug_fn.glGetFragDataIndexFn) {
    debug_fn.glGetFragDataIndexFn = fn.glGetFragDataIndexFn;
    fn.glGetFragDataIndexFn = Debug_glGetFragDataIndex;
  }
  if (!debug_fn.glGetFragDataLocationFn) {
    debug_fn.glGetFragDataLocationFn = fn.glGetFragDataLocationFn;
    fn.glGetFragDataLocationFn = Debug_glGetFragDataLocation;
  }
  if (!debug_fn.glGetFramebufferAttachmentParameterivEXTFn) {
    debug_fn.glGetFramebufferAttachmentParameterivEXTFn =
        fn.glGetFramebufferAttachmentParameterivEXTFn;
    fn.glGetFramebufferAttachmentParameterivEXTFn =
        Debug_glGetFramebufferAttachmentParameterivEXT;
  }
  if (!debug_fn.glGetFramebufferAttachmentParameterivRobustANGLEFn) {
    debug_fn.glGetFramebufferAttachmentParameterivRobustANGLEFn =
        fn.glGetFramebufferAttachmentParameterivRobustANGLEFn;
    fn.glGetFramebufferAttachmentParameterivRobustANGLEFn =
        Debug_glGetFramebufferAttachmentParameterivRobustANGLE;
  }
  if (!debug_fn.glGetFramebufferParameterivRobustANGLEFn) {
    debug_fn.glGetFramebufferParameterivRobustANGLEFn =
        fn.glGetFramebufferParameterivRobustANGLEFn;
    fn.glGetFramebufferParameterivRobustANGLEFn =
        Debug_glGetFramebufferParameterivRobustANGLE;
  }
  if (!debug_fn.glGetGraphicsResetStatusARBFn) {
    debug_fn.glGetGraphicsResetStatusARBFn = fn.glGetGraphicsResetStatusARBFn;
    fn.glGetGraphicsResetStatusARBFn = Debug_glGetGraphicsResetStatusARB;
  }
  if (!debug_fn.glGetInteger64i_vFn) {
    debug_fn.glGetInteger64i_vFn = fn.glGetInteger64i_vFn;
    fn.glGetInteger64i_vFn = Debug_glGetInteger64i_v;
  }
  if (!debug_fn.glGetInteger64i_vRobustANGLEFn) {
    debug_fn.glGetInteger64i_vRobustANGLEFn = fn.glGetInteger64i_vRobustANGLEFn;
    fn.glGetInteger64i_vRobustANGLEFn = Debug_glGetInteger64i_vRobustANGLE;
  }
  if (!debug_fn.glGetInteger64vFn) {
    debug_fn.glGetInteger64vFn = fn.glGetInteger64vFn;
    fn.glGetInteger64vFn = Debug_glGetInteger64v;
  }
  if (!debug_fn.glGetInteger64vRobustANGLEFn) {
    debug_fn.glGetInteger64vRobustANGLEFn = fn.glGetInteger64vRobustANGLEFn;
    fn.glGetInteger64vRobustANGLEFn = Debug_glGetInteger64vRobustANGLE;
  }
  if (!debug_fn.glGetIntegeri_vFn) {
    debug_fn.glGetIntegeri_vFn = fn.glGetIntegeri_vFn;
    fn.glGetIntegeri_vFn = Debug_glGetIntegeri_v;
  }
  if (!debug_fn.glGetIntegeri_vRobustANGLEFn) {
    debug_fn.glGetIntegeri_vRobustANGLEFn = fn.glGetIntegeri_vRobustANGLEFn;
    fn.glGetIntegeri_vRobustANGLEFn = Debug_glGetIntegeri_vRobustANGLE;
  }
  if (!debug_fn.glGetIntegervFn) {
    debug_fn.glGetIntegervFn = fn.glGetIntegervFn;
    fn.glGetIntegervFn = Debug_glGetIntegerv;
  }
  if (!debug_fn.glGetIntegervRobustANGLEFn) {
    debug_fn.glGetIntegervRobustANGLEFn = fn.glGetIntegervRobustANGLEFn;
    fn.glGetIntegervRobustANGLEFn = Debug_glGetIntegervRobustANGLE;
  }
  if (!debug_fn.glGetInternalformativFn) {
    debug_fn.glGetInternalformativFn = fn.glGetInternalformativFn;
    fn.glGetInternalformativFn = Debug_glGetInternalformativ;
  }
  if (!debug_fn.glGetInternalformativRobustANGLEFn) {
    debug_fn.glGetInternalformativRobustANGLEFn =
        fn.glGetInternalformativRobustANGLEFn;
    fn.glGetInternalformativRobustANGLEFn =
        Debug_glGetInternalformativRobustANGLE;
  }
  if (!debug_fn.glGetMultisamplefvRobustANGLEFn) {
    debug_fn.glGetMultisamplefvRobustANGLEFn =
        fn.glGetMultisamplefvRobustANGLEFn;
    fn.glGetMultisamplefvRobustANGLEFn = Debug_glGetMultisamplefvRobustANGLE;
  }
  if (!debug_fn.glGetnUniformfvRobustANGLEFn) {
    debug_fn.glGetnUniformfvRobustANGLEFn = fn.glGetnUniformfvRobustANGLEFn;
    fn.glGetnUniformfvRobustANGLEFn = Debug_glGetnUniformfvRobustANGLE;
  }
  if (!debug_fn.glGetnUniformivRobustANGLEFn) {
    debug_fn.glGetnUniformivRobustANGLEFn = fn.glGetnUniformivRobustANGLEFn;
    fn.glGetnUniformivRobustANGLEFn = Debug_glGetnUniformivRobustANGLE;
  }
  if (!debug_fn.glGetnUniformuivRobustANGLEFn) {
    debug_fn.glGetnUniformuivRobustANGLEFn = fn.glGetnUniformuivRobustANGLEFn;
    fn.glGetnUniformuivRobustANGLEFn = Debug_glGetnUniformuivRobustANGLE;
  }
  if (!debug_fn.glGetObjectLabelFn) {
    debug_fn.glGetObjectLabelFn = fn.glGetObjectLabelFn;
    fn.glGetObjectLabelFn = Debug_glGetObjectLabel;
  }
  if (!debug_fn.glGetObjectPtrLabelFn) {
    debug_fn.glGetObjectPtrLabelFn = fn.glGetObjectPtrLabelFn;
    fn.glGetObjectPtrLabelFn = Debug_glGetObjectPtrLabel;
  }
  if (!debug_fn.glGetPointervFn) {
    debug_fn.glGetPointervFn = fn.glGetPointervFn;
    fn.glGetPointervFn = Debug_glGetPointerv;
  }
  if (!debug_fn.glGetPointervRobustANGLERobustANGLEFn) {
    debug_fn.glGetPointervRobustANGLERobustANGLEFn =
        fn.glGetPointervRobustANGLERobustANGLEFn;
    fn.glGetPointervRobustANGLERobustANGLEFn =
        Debug_glGetPointervRobustANGLERobustANGLE;
  }
  if (!debug_fn.glGetProgramBinaryFn) {
    debug_fn.glGetProgramBinaryFn = fn.glGetProgramBinaryFn;
    fn.glGetProgramBinaryFn = Debug_glGetProgramBinary;
  }
  if (!debug_fn.glGetProgramInfoLogFn) {
    debug_fn.glGetProgramInfoLogFn = fn.glGetProgramInfoLogFn;
    fn.glGetProgramInfoLogFn = Debug_glGetProgramInfoLog;
  }
  if (!debug_fn.glGetProgramInterfaceivFn) {
    debug_fn.glGetProgramInterfaceivFn = fn.glGetProgramInterfaceivFn;
    fn.glGetProgramInterfaceivFn = Debug_glGetProgramInterfaceiv;
  }
  if (!debug_fn.glGetProgramInterfaceivRobustANGLEFn) {
    debug_fn.glGetProgramInterfaceivRobustANGLEFn =
        fn.glGetProgramInterfaceivRobustANGLEFn;
    fn.glGetProgramInterfaceivRobustANGLEFn =
        Debug_glGetProgramInterfaceivRobustANGLE;
  }
  if (!debug_fn.glGetProgramivFn) {
    debug_fn.glGetProgramivFn = fn.glGetProgramivFn;
    fn.glGetProgramivFn = Debug_glGetProgramiv;
  }
  if (!debug_fn.glGetProgramivRobustANGLEFn) {
    debug_fn.glGetProgramivRobustANGLEFn = fn.glGetProgramivRobustANGLEFn;
    fn.glGetProgramivRobustANGLEFn = Debug_glGetProgramivRobustANGLE;
  }
  if (!debug_fn.glGetProgramResourceivFn) {
    debug_fn.glGetProgramResourceivFn = fn.glGetProgramResourceivFn;
    fn.glGetProgramResourceivFn = Debug_glGetProgramResourceiv;
  }
  if (!debug_fn.glGetProgramResourceLocationFn) {
    debug_fn.glGetProgramResourceLocationFn = fn.glGetProgramResourceLocationFn;
    fn.glGetProgramResourceLocationFn = Debug_glGetProgramResourceLocation;
  }
  if (!debug_fn.glGetProgramResourceNameFn) {
    debug_fn.glGetProgramResourceNameFn = fn.glGetProgramResourceNameFn;
    fn.glGetProgramResourceNameFn = Debug_glGetProgramResourceName;
  }
  if (!debug_fn.glGetQueryivFn) {
    debug_fn.glGetQueryivFn = fn.glGetQueryivFn;
    fn.glGetQueryivFn = Debug_glGetQueryiv;
  }
  if (!debug_fn.glGetQueryivRobustANGLEFn) {
    debug_fn.glGetQueryivRobustANGLEFn = fn.glGetQueryivRobustANGLEFn;
    fn.glGetQueryivRobustANGLEFn = Debug_glGetQueryivRobustANGLE;
  }
  if (!debug_fn.glGetQueryObjecti64vFn) {
    debug_fn.glGetQueryObjecti64vFn = fn.glGetQueryObjecti64vFn;
    fn.glGetQueryObjecti64vFn = Debug_glGetQueryObjecti64v;
  }
  if (!debug_fn.glGetQueryObjecti64vRobustANGLEFn) {
    debug_fn.glGetQueryObjecti64vRobustANGLEFn =
        fn.glGetQueryObjecti64vRobustANGLEFn;
    fn.glGetQueryObjecti64vRobustANGLEFn =
        Debug_glGetQueryObjecti64vRobustANGLE;
  }
  if (!debug_fn.glGetQueryObjectivFn) {
    debug_fn.glGetQueryObjectivFn = fn.glGetQueryObjectivFn;
    fn.glGetQueryObjectivFn = Debug_glGetQueryObjectiv;
  }
  if (!debug_fn.glGetQueryObjectivRobustANGLEFn) {
    debug_fn.glGetQueryObjectivRobustANGLEFn =
        fn.glGetQueryObjectivRobustANGLEFn;
    fn.glGetQueryObjectivRobustANGLEFn = Debug_glGetQueryObjectivRobustANGLE;
  }
  if (!debug_fn.glGetQueryObjectui64vFn) {
    debug_fn.glGetQueryObjectui64vFn = fn.glGetQueryObjectui64vFn;
    fn.glGetQueryObjectui64vFn = Debug_glGetQueryObjectui64v;
  }
  if (!debug_fn.glGetQueryObjectui64vRobustANGLEFn) {
    debug_fn.glGetQueryObjectui64vRobustANGLEFn =
        fn.glGetQueryObjectui64vRobustANGLEFn;
    fn.glGetQueryObjectui64vRobustANGLEFn =
        Debug_glGetQueryObjectui64vRobustANGLE;
  }
  if (!debug_fn.glGetQueryObjectuivFn) {
    debug_fn.glGetQueryObjectuivFn = fn.glGetQueryObjectuivFn;
    fn.glGetQueryObjectuivFn = Debug_glGetQueryObjectuiv;
  }
  if (!debug_fn.glGetQueryObjectuivRobustANGLEFn) {
    debug_fn.glGetQueryObjectuivRobustANGLEFn =
        fn.glGetQueryObjectuivRobustANGLEFn;
    fn.glGetQueryObjectuivRobustANGLEFn = Debug_glGetQueryObjectuivRobustANGLE;
  }
  if (!debug_fn.glGetRenderbufferParameterivEXTFn) {
    debug_fn.glGetRenderbufferParameterivEXTFn =
        fn.glGetRenderbufferParameterivEXTFn;
    fn.glGetRenderbufferParameterivEXTFn =
        Debug_glGetRenderbufferParameterivEXT;
  }
  if (!debug_fn.glGetRenderbufferParameterivRobustANGLEFn) {
    debug_fn.glGetRenderbufferParameterivRobustANGLEFn =
        fn.glGetRenderbufferParameterivRobustANGLEFn;
    fn.glGetRenderbufferParameterivRobustANGLEFn =
        Debug_glGetRenderbufferParameterivRobustANGLE;
  }
  if (!debug_fn.glGetSamplerParameterfvFn) {
    debug_fn.glGetSamplerParameterfvFn = fn.glGetSamplerParameterfvFn;
    fn.glGetSamplerParameterfvFn = Debug_glGetSamplerParameterfv;
  }
  if (!debug_fn.glGetSamplerParameterfvRobustANGLEFn) {
    debug_fn.glGetSamplerParameterfvRobustANGLEFn =
        fn.glGetSamplerParameterfvRobustANGLEFn;
    fn.glGetSamplerParameterfvRobustANGLEFn =
        Debug_glGetSamplerParameterfvRobustANGLE;
  }
  if (!debug_fn.glGetSamplerParameterIivRobustANGLEFn) {
    debug_fn.glGetSamplerParameterIivRobustANGLEFn =
        fn.glGetSamplerParameterIivRobustANGLEFn;
    fn.glGetSamplerParameterIivRobustANGLEFn =
        Debug_glGetSamplerParameterIivRobustANGLE;
  }
  if (!debug_fn.glGetSamplerParameterIuivRobustANGLEFn) {
    debug_fn.glGetSamplerParameterIuivRobustANGLEFn =
        fn.glGetSamplerParameterIuivRobustANGLEFn;
    fn.glGetSamplerParameterIuivRobustANGLEFn =
        Debug_glGetSamplerParameterIuivRobustANGLE;
  }
  if (!debug_fn.glGetSamplerParameterivFn) {
    debug_fn.glGetSamplerParameterivFn = fn.glGetSamplerParameterivFn;
    fn.glGetSamplerParameterivFn = Debug_glGetSamplerParameteriv;
  }
  if (!debug_fn.glGetSamplerParameterivRobustANGLEFn) {
    debug_fn.glGetSamplerParameterivRobustANGLEFn =
        fn.glGetSamplerParameterivRobustANGLEFn;
    fn.glGetSamplerParameterivRobustANGLEFn =
        Debug_glGetSamplerParameterivRobustANGLE;
  }
  if (!debug_fn.glGetShaderInfoLogFn) {
    debug_fn.glGetShaderInfoLogFn = fn.glGetShaderInfoLogFn;
    fn.glGetShaderInfoLogFn = Debug_glGetShaderInfoLog;
  }
  if (!debug_fn.glGetShaderivFn) {
    debug_fn.glGetShaderivFn = fn.glGetShaderivFn;
    fn.glGetShaderivFn = Debug_glGetShaderiv;
  }
  if (!debug_fn.glGetShaderivRobustANGLEFn) {
    debug_fn.glGetShaderivRobustANGLEFn = fn.glGetShaderivRobustANGLEFn;
    fn.glGetShaderivRobustANGLEFn = Debug_glGetShaderivRobustANGLE;
  }
  if (!debug_fn.glGetShaderPrecisionFormatFn) {
    debug_fn.glGetShaderPrecisionFormatFn = fn.glGetShaderPrecisionFormatFn;
    fn.glGetShaderPrecisionFormatFn = Debug_glGetShaderPrecisionFormat;
  }
  if (!debug_fn.glGetShaderSourceFn) {
    debug_fn.glGetShaderSourceFn = fn.glGetShaderSourceFn;
    fn.glGetShaderSourceFn = Debug_glGetShaderSource;
  }
  if (!debug_fn.glGetStringFn) {
    debug_fn.glGetStringFn = fn.glGetStringFn;
    fn.glGetStringFn = Debug_glGetString;
  }
  if (!debug_fn.glGetStringiFn) {
    debug_fn.glGetStringiFn = fn.glGetStringiFn;
    fn.glGetStringiFn = Debug_glGetStringi;
  }
  if (!debug_fn.glGetSyncivFn) {
    debug_fn.glGetSyncivFn = fn.glGetSyncivFn;
    fn.glGetSyncivFn = Debug_glGetSynciv;
  }
  if (!debug_fn.glGetTexLevelParameterfvFn) {
    debug_fn.glGetTexLevelParameterfvFn = fn.glGetTexLevelParameterfvFn;
    fn.glGetTexLevelParameterfvFn = Debug_glGetTexLevelParameterfv;
  }
  if (!debug_fn.glGetTexLevelParameterfvRobustANGLEFn) {
    debug_fn.glGetTexLevelParameterfvRobustANGLEFn =
        fn.glGetTexLevelParameterfvRobustANGLEFn;
    fn.glGetTexLevelParameterfvRobustANGLEFn =
        Debug_glGetTexLevelParameterfvRobustANGLE;
  }
  if (!debug_fn.glGetTexLevelParameterivFn) {
    debug_fn.glGetTexLevelParameterivFn = fn.glGetTexLevelParameterivFn;
    fn.glGetTexLevelParameterivFn = Debug_glGetTexLevelParameteriv;
  }
  if (!debug_fn.glGetTexLevelParameterivRobustANGLEFn) {
    debug_fn.glGetTexLevelParameterivRobustANGLEFn =
        fn.glGetTexLevelParameterivRobustANGLEFn;
    fn.glGetTexLevelParameterivRobustANGLEFn =
        Debug_glGetTexLevelParameterivRobustANGLE;
  }
  if (!debug_fn.glGetTexParameterfvFn) {
    debug_fn.glGetTexParameterfvFn = fn.glGetTexParameterfvFn;
    fn.glGetTexParameterfvFn = Debug_glGetTexParameterfv;
  }
  if (!debug_fn.glGetTexParameterfvRobustANGLEFn) {
    debug_fn.glGetTexParameterfvRobustANGLEFn =
        fn.glGetTexParameterfvRobustANGLEFn;
    fn.glGetTexParameterfvRobustANGLEFn = Debug_glGetTexParameterfvRobustANGLE;
  }
  if (!debug_fn.glGetTexParameterIivRobustANGLEFn) {
    debug_fn.glGetTexParameterIivRobustANGLEFn =
        fn.glGetTexParameterIivRobustANGLEFn;
    fn.glGetTexParameterIivRobustANGLEFn =
        Debug_glGetTexParameterIivRobustANGLE;
  }
  if (!debug_fn.glGetTexParameterIuivRobustANGLEFn) {
    debug_fn.glGetTexParameterIuivRobustANGLEFn =
        fn.glGetTexParameterIuivRobustANGLEFn;
    fn.glGetTexParameterIuivRobustANGLEFn =
        Debug_glGetTexParameterIuivRobustANGLE;
  }
  if (!debug_fn.glGetTexParameterivFn) {
    debug_fn.glGetTexParameterivFn = fn.glGetTexParameterivFn;
    fn.glGetTexParameterivFn = Debug_glGetTexParameteriv;
  }
  if (!debug_fn.glGetTexParameterivRobustANGLEFn) {
    debug_fn.glGetTexParameterivRobustANGLEFn =
        fn.glGetTexParameterivRobustANGLEFn;
    fn.glGetTexParameterivRobustANGLEFn = Debug_glGetTexParameterivRobustANGLE;
  }
  if (!debug_fn.glGetTransformFeedbackVaryingFn) {
    debug_fn.glGetTransformFeedbackVaryingFn =
        fn.glGetTransformFeedbackVaryingFn;
    fn.glGetTransformFeedbackVaryingFn = Debug_glGetTransformFeedbackVarying;
  }
  if (!debug_fn.glGetTranslatedShaderSourceANGLEFn) {
    debug_fn.glGetTranslatedShaderSourceANGLEFn =
        fn.glGetTranslatedShaderSourceANGLEFn;
    fn.glGetTranslatedShaderSourceANGLEFn =
        Debug_glGetTranslatedShaderSourceANGLE;
  }
  if (!debug_fn.glGetUniformBlockIndexFn) {
    debug_fn.glGetUniformBlockIndexFn = fn.glGetUniformBlockIndexFn;
    fn.glGetUniformBlockIndexFn = Debug_glGetUniformBlockIndex;
  }
  if (!debug_fn.glGetUniformfvFn) {
    debug_fn.glGetUniformfvFn = fn.glGetUniformfvFn;
    fn.glGetUniformfvFn = Debug_glGetUniformfv;
  }
  if (!debug_fn.glGetUniformfvRobustANGLEFn) {
    debug_fn.glGetUniformfvRobustANGLEFn = fn.glGetUniformfvRobustANGLEFn;
    fn.glGetUniformfvRobustANGLEFn = Debug_glGetUniformfvRobustANGLE;
  }
  if (!debug_fn.glGetUniformIndicesFn) {
    debug_fn.glGetUniformIndicesFn = fn.glGetUniformIndicesFn;
    fn.glGetUniformIndicesFn = Debug_glGetUniformIndices;
  }
  if (!debug_fn.glGetUniformivFn) {
    debug_fn.glGetUniformivFn = fn.glGetUniformivFn;
    fn.glGetUniformivFn = Debug_glGetUniformiv;
  }
  if (!debug_fn.glGetUniformivRobustANGLEFn) {
    debug_fn.glGetUniformivRobustANGLEFn = fn.glGetUniformivRobustANGLEFn;
    fn.glGetUniformivRobustANGLEFn = Debug_glGetUniformivRobustANGLE;
  }
  if (!debug_fn.glGetUniformLocationFn) {
    debug_fn.glGetUniformLocationFn = fn.glGetUniformLocationFn;
    fn.glGetUniformLocationFn = Debug_glGetUniformLocation;
  }
  if (!debug_fn.glGetUniformuivFn) {
    debug_fn.glGetUniformuivFn = fn.glGetUniformuivFn;
    fn.glGetUniformuivFn = Debug_glGetUniformuiv;
  }
  if (!debug_fn.glGetUniformuivRobustANGLEFn) {
    debug_fn.glGetUniformuivRobustANGLEFn = fn.glGetUniformuivRobustANGLEFn;
    fn.glGetUniformuivRobustANGLEFn = Debug_glGetUniformuivRobustANGLE;
  }
  if (!debug_fn.glGetVertexAttribfvFn) {
    debug_fn.glGetVertexAttribfvFn = fn.glGetVertexAttribfvFn;
    fn.glGetVertexAttribfvFn = Debug_glGetVertexAttribfv;
  }
  if (!debug_fn.glGetVertexAttribfvRobustANGLEFn) {
    debug_fn.glGetVertexAttribfvRobustANGLEFn =
        fn.glGetVertexAttribfvRobustANGLEFn;
    fn.glGetVertexAttribfvRobustANGLEFn = Debug_glGetVertexAttribfvRobustANGLE;
  }
  if (!debug_fn.glGetVertexAttribIivRobustANGLEFn) {
    debug_fn.glGetVertexAttribIivRobustANGLEFn =
        fn.glGetVertexAttribIivRobustANGLEFn;
    fn.glGetVertexAttribIivRobustANGLEFn =
        Debug_glGetVertexAttribIivRobustANGLE;
  }
  if (!debug_fn.glGetVertexAttribIuivRobustANGLEFn) {
    debug_fn.glGetVertexAttribIuivRobustANGLEFn =
        fn.glGetVertexAttribIuivRobustANGLEFn;
    fn.glGetVertexAttribIuivRobustANGLEFn =
        Debug_glGetVertexAttribIuivRobustANGLE;
  }
  if (!debug_fn.glGetVertexAttribivFn) {
    debug_fn.glGetVertexAttribivFn = fn.glGetVertexAttribivFn;
    fn.glGetVertexAttribivFn = Debug_glGetVertexAttribiv;
  }
  if (!debug_fn.glGetVertexAttribivRobustANGLEFn) {
    debug_fn.glGetVertexAttribivRobustANGLEFn =
        fn.glGetVertexAttribivRobustANGLEFn;
    fn.glGetVertexAttribivRobustANGLEFn = Debug_glGetVertexAttribivRobustANGLE;
  }
  if (!debug_fn.glGetVertexAttribPointervFn) {
    debug_fn.glGetVertexAttribPointervFn = fn.glGetVertexAttribPointervFn;
    fn.glGetVertexAttribPointervFn = Debug_glGetVertexAttribPointerv;
  }
  if (!debug_fn.glGetVertexAttribPointervRobustANGLEFn) {
    debug_fn.glGetVertexAttribPointervRobustANGLEFn =
        fn.glGetVertexAttribPointervRobustANGLEFn;
    fn.glGetVertexAttribPointervRobustANGLEFn =
        Debug_glGetVertexAttribPointervRobustANGLE;
  }
  if (!debug_fn.glHintFn) {
    debug_fn.glHintFn = fn.glHintFn;
    fn.glHintFn = Debug_glHint;
  }
  if (!debug_fn.glInsertEventMarkerEXTFn) {
    debug_fn.glInsertEventMarkerEXTFn = fn.glInsertEventMarkerEXTFn;
    fn.glInsertEventMarkerEXTFn = Debug_glInsertEventMarkerEXT;
  }
  if (!debug_fn.glInvalidateFramebufferFn) {
    debug_fn.glInvalidateFramebufferFn = fn.glInvalidateFramebufferFn;
    fn.glInvalidateFramebufferFn = Debug_glInvalidateFramebuffer;
  }
  if (!debug_fn.glInvalidateSubFramebufferFn) {
    debug_fn.glInvalidateSubFramebufferFn = fn.glInvalidateSubFramebufferFn;
    fn.glInvalidateSubFramebufferFn = Debug_glInvalidateSubFramebuffer;
  }
  if (!debug_fn.glIsBufferFn) {
    debug_fn.glIsBufferFn = fn.glIsBufferFn;
    fn.glIsBufferFn = Debug_glIsBuffer;
  }
  if (!debug_fn.glIsEnabledFn) {
    debug_fn.glIsEnabledFn = fn.glIsEnabledFn;
    fn.glIsEnabledFn = Debug_glIsEnabled;
  }
  if (!debug_fn.glIsFenceAPPLEFn) {
    debug_fn.glIsFenceAPPLEFn = fn.glIsFenceAPPLEFn;
    fn.glIsFenceAPPLEFn = Debug_glIsFenceAPPLE;
  }
  if (!debug_fn.glIsFenceNVFn) {
    debug_fn.glIsFenceNVFn = fn.glIsFenceNVFn;
    fn.glIsFenceNVFn = Debug_glIsFenceNV;
  }
  if (!debug_fn.glIsFramebufferEXTFn) {
    debug_fn.glIsFramebufferEXTFn = fn.glIsFramebufferEXTFn;
    fn.glIsFramebufferEXTFn = Debug_glIsFramebufferEXT;
  }
  if (!debug_fn.glIsPathNVFn) {
    debug_fn.glIsPathNVFn = fn.glIsPathNVFn;
    fn.glIsPathNVFn = Debug_glIsPathNV;
  }
  if (!debug_fn.glIsProgramFn) {
    debug_fn.glIsProgramFn = fn.glIsProgramFn;
    fn.glIsProgramFn = Debug_glIsProgram;
  }
  if (!debug_fn.glIsQueryFn) {
    debug_fn.glIsQueryFn = fn.glIsQueryFn;
    fn.glIsQueryFn = Debug_glIsQuery;
  }
  if (!debug_fn.glIsRenderbufferEXTFn) {
    debug_fn.glIsRenderbufferEXTFn = fn.glIsRenderbufferEXTFn;
    fn.glIsRenderbufferEXTFn = Debug_glIsRenderbufferEXT;
  }
  if (!debug_fn.glIsSamplerFn) {
    debug_fn.glIsSamplerFn = fn.glIsSamplerFn;
    fn.glIsSamplerFn = Debug_glIsSampler;
  }
  if (!debug_fn.glIsShaderFn) {
    debug_fn.glIsShaderFn = fn.glIsShaderFn;
    fn.glIsShaderFn = Debug_glIsShader;
  }
  if (!debug_fn.glIsSyncFn) {
    debug_fn.glIsSyncFn = fn.glIsSyncFn;
    fn.glIsSyncFn = Debug_glIsSync;
  }
  if (!debug_fn.glIsTextureFn) {
    debug_fn.glIsTextureFn = fn.glIsTextureFn;
    fn.glIsTextureFn = Debug_glIsTexture;
  }
  if (!debug_fn.glIsTransformFeedbackFn) {
    debug_fn.glIsTransformFeedbackFn = fn.glIsTransformFeedbackFn;
    fn.glIsTransformFeedbackFn = Debug_glIsTransformFeedback;
  }
  if (!debug_fn.glIsVertexArrayOESFn) {
    debug_fn.glIsVertexArrayOESFn = fn.glIsVertexArrayOESFn;
    fn.glIsVertexArrayOESFn = Debug_glIsVertexArrayOES;
  }
  if (!debug_fn.glLineWidthFn) {
    debug_fn.glLineWidthFn = fn.glLineWidthFn;
    fn.glLineWidthFn = Debug_glLineWidth;
  }
  if (!debug_fn.glLinkProgramFn) {
    debug_fn.glLinkProgramFn = fn.glLinkProgramFn;
    fn.glLinkProgramFn = Debug_glLinkProgram;
  }
  if (!debug_fn.glMapBufferFn) {
    debug_fn.glMapBufferFn = fn.glMapBufferFn;
    fn.glMapBufferFn = Debug_glMapBuffer;
  }
  if (!debug_fn.glMapBufferRangeFn) {
    debug_fn.glMapBufferRangeFn = fn.glMapBufferRangeFn;
    fn.glMapBufferRangeFn = Debug_glMapBufferRange;
  }
  if (!debug_fn.glMatrixLoadfEXTFn) {
    debug_fn.glMatrixLoadfEXTFn = fn.glMatrixLoadfEXTFn;
    fn.glMatrixLoadfEXTFn = Debug_glMatrixLoadfEXT;
  }
  if (!debug_fn.glMatrixLoadIdentityEXTFn) {
    debug_fn.glMatrixLoadIdentityEXTFn = fn.glMatrixLoadIdentityEXTFn;
    fn.glMatrixLoadIdentityEXTFn = Debug_glMatrixLoadIdentityEXT;
  }
  if (!debug_fn.glMemoryBarrierEXTFn) {
    debug_fn.glMemoryBarrierEXTFn = fn.glMemoryBarrierEXTFn;
    fn.glMemoryBarrierEXTFn = Debug_glMemoryBarrierEXT;
  }
  if (!debug_fn.glObjectLabelFn) {
    debug_fn.glObjectLabelFn = fn.glObjectLabelFn;
    fn.glObjectLabelFn = Debug_glObjectLabel;
  }
  if (!debug_fn.glObjectPtrLabelFn) {
    debug_fn.glObjectPtrLabelFn = fn.glObjectPtrLabelFn;
    fn.glObjectPtrLabelFn = Debug_glObjectPtrLabel;
  }
  if (!debug_fn.glPathCommandsNVFn) {
    debug_fn.glPathCommandsNVFn = fn.glPathCommandsNVFn;
    fn.glPathCommandsNVFn = Debug_glPathCommandsNV;
  }
  if (!debug_fn.glPathParameterfNVFn) {
    debug_fn.glPathParameterfNVFn = fn.glPathParameterfNVFn;
    fn.glPathParameterfNVFn = Debug_glPathParameterfNV;
  }
  if (!debug_fn.glPathParameteriNVFn) {
    debug_fn.glPathParameteriNVFn = fn.glPathParameteriNVFn;
    fn.glPathParameteriNVFn = Debug_glPathParameteriNV;
  }
  if (!debug_fn.glPathStencilFuncNVFn) {
    debug_fn.glPathStencilFuncNVFn = fn.glPathStencilFuncNVFn;
    fn.glPathStencilFuncNVFn = Debug_glPathStencilFuncNV;
  }
  if (!debug_fn.glPauseTransformFeedbackFn) {
    debug_fn.glPauseTransformFeedbackFn = fn.glPauseTransformFeedbackFn;
    fn.glPauseTransformFeedbackFn = Debug_glPauseTransformFeedback;
  }
  if (!debug_fn.glPixelStoreiFn) {
    debug_fn.glPixelStoreiFn = fn.glPixelStoreiFn;
    fn.glPixelStoreiFn = Debug_glPixelStorei;
  }
  if (!debug_fn.glPointParameteriFn) {
    debug_fn.glPointParameteriFn = fn.glPointParameteriFn;
    fn.glPointParameteriFn = Debug_glPointParameteri;
  }
  if (!debug_fn.glPolygonOffsetFn) {
    debug_fn.glPolygonOffsetFn = fn.glPolygonOffsetFn;
    fn.glPolygonOffsetFn = Debug_glPolygonOffset;
  }
  if (!debug_fn.glPopDebugGroupFn) {
    debug_fn.glPopDebugGroupFn = fn.glPopDebugGroupFn;
    fn.glPopDebugGroupFn = Debug_glPopDebugGroup;
  }
  if (!debug_fn.glPopGroupMarkerEXTFn) {
    debug_fn.glPopGroupMarkerEXTFn = fn.glPopGroupMarkerEXTFn;
    fn.glPopGroupMarkerEXTFn = Debug_glPopGroupMarkerEXT;
  }
  if (!debug_fn.glPrimitiveRestartIndexFn) {
    debug_fn.glPrimitiveRestartIndexFn = fn.glPrimitiveRestartIndexFn;
    fn.glPrimitiveRestartIndexFn = Debug_glPrimitiveRestartIndex;
  }
  if (!debug_fn.glProgramBinaryFn) {
    debug_fn.glProgramBinaryFn = fn.glProgramBinaryFn;
    fn.glProgramBinaryFn = Debug_glProgramBinary;
  }
  if (!debug_fn.glProgramParameteriFn) {
    debug_fn.glProgramParameteriFn = fn.glProgramParameteriFn;
    fn.glProgramParameteriFn = Debug_glProgramParameteri;
  }
  if (!debug_fn.glProgramPathFragmentInputGenNVFn) {
    debug_fn.glProgramPathFragmentInputGenNVFn =
        fn.glProgramPathFragmentInputGenNVFn;
    fn.glProgramPathFragmentInputGenNVFn =
        Debug_glProgramPathFragmentInputGenNV;
  }
  if (!debug_fn.glPushDebugGroupFn) {
    debug_fn.glPushDebugGroupFn = fn.glPushDebugGroupFn;
    fn.glPushDebugGroupFn = Debug_glPushDebugGroup;
  }
  if (!debug_fn.glPushGroupMarkerEXTFn) {
    debug_fn.glPushGroupMarkerEXTFn = fn.glPushGroupMarkerEXTFn;
    fn.glPushGroupMarkerEXTFn = Debug_glPushGroupMarkerEXT;
  }
  if (!debug_fn.glQueryCounterFn) {
    debug_fn.glQueryCounterFn = fn.glQueryCounterFn;
    fn.glQueryCounterFn = Debug_glQueryCounter;
  }
  if (!debug_fn.glReadBufferFn) {
    debug_fn.glReadBufferFn = fn.glReadBufferFn;
    fn.glReadBufferFn = Debug_glReadBuffer;
  }
  if (!debug_fn.glReadnPixelsRobustANGLEFn) {
    debug_fn.glReadnPixelsRobustANGLEFn = fn.glReadnPixelsRobustANGLEFn;
    fn.glReadnPixelsRobustANGLEFn = Debug_glReadnPixelsRobustANGLE;
  }
  if (!debug_fn.glReadPixelsFn) {
    debug_fn.glReadPixelsFn = fn.glReadPixelsFn;
    fn.glReadPixelsFn = Debug_glReadPixels;
  }
  if (!debug_fn.glReadPixelsRobustANGLEFn) {
    debug_fn.glReadPixelsRobustANGLEFn = fn.glReadPixelsRobustANGLEFn;
    fn.glReadPixelsRobustANGLEFn = Debug_glReadPixelsRobustANGLE;
  }
  if (!debug_fn.glReleaseShaderCompilerFn) {
    debug_fn.glReleaseShaderCompilerFn = fn.glReleaseShaderCompilerFn;
    fn.glReleaseShaderCompilerFn = Debug_glReleaseShaderCompiler;
  }
  if (!debug_fn.glRenderbufferStorageEXTFn) {
    debug_fn.glRenderbufferStorageEXTFn = fn.glRenderbufferStorageEXTFn;
    fn.glRenderbufferStorageEXTFn = Debug_glRenderbufferStorageEXT;
  }
  if (!debug_fn.glRenderbufferStorageMultisampleFn) {
    debug_fn.glRenderbufferStorageMultisampleFn =
        fn.glRenderbufferStorageMultisampleFn;
    fn.glRenderbufferStorageMultisampleFn =
        Debug_glRenderbufferStorageMultisample;
  }
  if (!debug_fn.glRenderbufferStorageMultisampleANGLEFn) {
    debug_fn.glRenderbufferStorageMultisampleANGLEFn =
        fn.glRenderbufferStorageMultisampleANGLEFn;
    fn.glRenderbufferStorageMultisampleANGLEFn =
        Debug_glRenderbufferStorageMultisampleANGLE;
  }
  if (!debug_fn.glRenderbufferStorageMultisampleEXTFn) {
    debug_fn.glRenderbufferStorageMultisampleEXTFn =
        fn.glRenderbufferStorageMultisampleEXTFn;
    fn.glRenderbufferStorageMultisampleEXTFn =
        Debug_glRenderbufferStorageMultisampleEXT;
  }
  if (!debug_fn.glRenderbufferStorageMultisampleIMGFn) {
    debug_fn.glRenderbufferStorageMultisampleIMGFn =
        fn.glRenderbufferStorageMultisampleIMGFn;
    fn.glRenderbufferStorageMultisampleIMGFn =
        Debug_glRenderbufferStorageMultisampleIMG;
  }
  if (!debug_fn.glResumeTransformFeedbackFn) {
    debug_fn.glResumeTransformFeedbackFn = fn.glResumeTransformFeedbackFn;
    fn.glResumeTransformFeedbackFn = Debug_glResumeTransformFeedback;
  }
  if (!debug_fn.glSampleCoverageFn) {
    debug_fn.glSampleCoverageFn = fn.glSampleCoverageFn;
    fn.glSampleCoverageFn = Debug_glSampleCoverage;
  }
  if (!debug_fn.glSamplerParameterfFn) {
    debug_fn.glSamplerParameterfFn = fn.glSamplerParameterfFn;
    fn.glSamplerParameterfFn = Debug_glSamplerParameterf;
  }
  if (!debug_fn.glSamplerParameterfvFn) {
    debug_fn.glSamplerParameterfvFn = fn.glSamplerParameterfvFn;
    fn.glSamplerParameterfvFn = Debug_glSamplerParameterfv;
  }
  if (!debug_fn.glSamplerParameterfvRobustANGLEFn) {
    debug_fn.glSamplerParameterfvRobustANGLEFn =
        fn.glSamplerParameterfvRobustANGLEFn;
    fn.glSamplerParameterfvRobustANGLEFn =
        Debug_glSamplerParameterfvRobustANGLE;
  }
  if (!debug_fn.glSamplerParameteriFn) {
    debug_fn.glSamplerParameteriFn = fn.glSamplerParameteriFn;
    fn.glSamplerParameteriFn = Debug_glSamplerParameteri;
  }
  if (!debug_fn.glSamplerParameterIivRobustANGLEFn) {
    debug_fn.glSamplerParameterIivRobustANGLEFn =
        fn.glSamplerParameterIivRobustANGLEFn;
    fn.glSamplerParameterIivRobustANGLEFn =
        Debug_glSamplerParameterIivRobustANGLE;
  }
  if (!debug_fn.glSamplerParameterIuivRobustANGLEFn) {
    debug_fn.glSamplerParameterIuivRobustANGLEFn =
        fn.glSamplerParameterIuivRobustANGLEFn;
    fn.glSamplerParameterIuivRobustANGLEFn =
        Debug_glSamplerParameterIuivRobustANGLE;
  }
  if (!debug_fn.glSamplerParameterivFn) {
    debug_fn.glSamplerParameterivFn = fn.glSamplerParameterivFn;
    fn.glSamplerParameterivFn = Debug_glSamplerParameteriv;
  }
  if (!debug_fn.glSamplerParameterivRobustANGLEFn) {
    debug_fn.glSamplerParameterivRobustANGLEFn =
        fn.glSamplerParameterivRobustANGLEFn;
    fn.glSamplerParameterivRobustANGLEFn =
        Debug_glSamplerParameterivRobustANGLE;
  }
  if (!debug_fn.glScissorFn) {
    debug_fn.glScissorFn = fn.glScissorFn;
    fn.glScissorFn = Debug_glScissor;
  }
  if (!debug_fn.glSetFenceAPPLEFn) {
    debug_fn.glSetFenceAPPLEFn = fn.glSetFenceAPPLEFn;
    fn.glSetFenceAPPLEFn = Debug_glSetFenceAPPLE;
  }
  if (!debug_fn.glSetFenceNVFn) {
    debug_fn.glSetFenceNVFn = fn.glSetFenceNVFn;
    fn.glSetFenceNVFn = Debug_glSetFenceNV;
  }
  if (!debug_fn.glShaderBinaryFn) {
    debug_fn.glShaderBinaryFn = fn.glShaderBinaryFn;
    fn.glShaderBinaryFn = Debug_glShaderBinary;
  }
  if (!debug_fn.glShaderSourceFn) {
    debug_fn.glShaderSourceFn = fn.glShaderSourceFn;
    fn.glShaderSourceFn = Debug_glShaderSource;
  }
  if (!debug_fn.glStencilFillPathInstancedNVFn) {
    debug_fn.glStencilFillPathInstancedNVFn = fn.glStencilFillPathInstancedNVFn;
    fn.glStencilFillPathInstancedNVFn = Debug_glStencilFillPathInstancedNV;
  }
  if (!debug_fn.glStencilFillPathNVFn) {
    debug_fn.glStencilFillPathNVFn = fn.glStencilFillPathNVFn;
    fn.glStencilFillPathNVFn = Debug_glStencilFillPathNV;
  }
  if (!debug_fn.glStencilFuncFn) {
    debug_fn.glStencilFuncFn = fn.glStencilFuncFn;
    fn.glStencilFuncFn = Debug_glStencilFunc;
  }
  if (!debug_fn.glStencilFuncSeparateFn) {
    debug_fn.glStencilFuncSeparateFn = fn.glStencilFuncSeparateFn;
    fn.glStencilFuncSeparateFn = Debug_glStencilFuncSeparate;
  }
  if (!debug_fn.glStencilMaskFn) {
    debug_fn.glStencilMaskFn = fn.glStencilMaskFn;
    fn.glStencilMaskFn = Debug_glStencilMask;
  }
  if (!debug_fn.glStencilMaskSeparateFn) {
    debug_fn.glStencilMaskSeparateFn = fn.glStencilMaskSeparateFn;
    fn.glStencilMaskSeparateFn = Debug_glStencilMaskSeparate;
  }
  if (!debug_fn.glStencilOpFn) {
    debug_fn.glStencilOpFn = fn.glStencilOpFn;
    fn.glStencilOpFn = Debug_glStencilOp;
  }
  if (!debug_fn.glStencilOpSeparateFn) {
    debug_fn.glStencilOpSeparateFn = fn.glStencilOpSeparateFn;
    fn.glStencilOpSeparateFn = Debug_glStencilOpSeparate;
  }
  if (!debug_fn.glStencilStrokePathInstancedNVFn) {
    debug_fn.glStencilStrokePathInstancedNVFn =
        fn.glStencilStrokePathInstancedNVFn;
    fn.glStencilStrokePathInstancedNVFn = Debug_glStencilStrokePathInstancedNV;
  }
  if (!debug_fn.glStencilStrokePathNVFn) {
    debug_fn.glStencilStrokePathNVFn = fn.glStencilStrokePathNVFn;
    fn.glStencilStrokePathNVFn = Debug_glStencilStrokePathNV;
  }
  if (!debug_fn.glStencilThenCoverFillPathInstancedNVFn) {
    debug_fn.glStencilThenCoverFillPathInstancedNVFn =
        fn.glStencilThenCoverFillPathInstancedNVFn;
    fn.glStencilThenCoverFillPathInstancedNVFn =
        Debug_glStencilThenCoverFillPathInstancedNV;
  }
  if (!debug_fn.glStencilThenCoverFillPathNVFn) {
    debug_fn.glStencilThenCoverFillPathNVFn = fn.glStencilThenCoverFillPathNVFn;
    fn.glStencilThenCoverFillPathNVFn = Debug_glStencilThenCoverFillPathNV;
  }
  if (!debug_fn.glStencilThenCoverStrokePathInstancedNVFn) {
    debug_fn.glStencilThenCoverStrokePathInstancedNVFn =
        fn.glStencilThenCoverStrokePathInstancedNVFn;
    fn.glStencilThenCoverStrokePathInstancedNVFn =
        Debug_glStencilThenCoverStrokePathInstancedNV;
  }
  if (!debug_fn.glStencilThenCoverStrokePathNVFn) {
    debug_fn.glStencilThenCoverStrokePathNVFn =
        fn.glStencilThenCoverStrokePathNVFn;
    fn.glStencilThenCoverStrokePathNVFn = Debug_glStencilThenCoverStrokePathNV;
  }
  if (!debug_fn.glTestFenceAPPLEFn) {
    debug_fn.glTestFenceAPPLEFn = fn.glTestFenceAPPLEFn;
    fn.glTestFenceAPPLEFn = Debug_glTestFenceAPPLE;
  }
  if (!debug_fn.glTestFenceNVFn) {
    debug_fn.glTestFenceNVFn = fn.glTestFenceNVFn;
    fn.glTestFenceNVFn = Debug_glTestFenceNV;
  }
  if (!debug_fn.glTexImage2DFn) {
    debug_fn.glTexImage2DFn = fn.glTexImage2DFn;
    fn.glTexImage2DFn = Debug_glTexImage2D;
  }
  if (!debug_fn.glTexImage2DRobustANGLEFn) {
    debug_fn.glTexImage2DRobustANGLEFn = fn.glTexImage2DRobustANGLEFn;
    fn.glTexImage2DRobustANGLEFn = Debug_glTexImage2DRobustANGLE;
  }
  if (!debug_fn.glTexImage3DFn) {
    debug_fn.glTexImage3DFn = fn.glTexImage3DFn;
    fn.glTexImage3DFn = Debug_glTexImage3D;
  }
  if (!debug_fn.glTexImage3DRobustANGLEFn) {
    debug_fn.glTexImage3DRobustANGLEFn = fn.glTexImage3DRobustANGLEFn;
    fn.glTexImage3DRobustANGLEFn = Debug_glTexImage3DRobustANGLE;
  }
  if (!debug_fn.glTexParameterfFn) {
    debug_fn.glTexParameterfFn = fn.glTexParameterfFn;
    fn.glTexParameterfFn = Debug_glTexParameterf;
  }
  if (!debug_fn.glTexParameterfvFn) {
    debug_fn.glTexParameterfvFn = fn.glTexParameterfvFn;
    fn.glTexParameterfvFn = Debug_glTexParameterfv;
  }
  if (!debug_fn.glTexParameterfvRobustANGLEFn) {
    debug_fn.glTexParameterfvRobustANGLEFn = fn.glTexParameterfvRobustANGLEFn;
    fn.glTexParameterfvRobustANGLEFn = Debug_glTexParameterfvRobustANGLE;
  }
  if (!debug_fn.glTexParameteriFn) {
    debug_fn.glTexParameteriFn = fn.glTexParameteriFn;
    fn.glTexParameteriFn = Debug_glTexParameteri;
  }
  if (!debug_fn.glTexParameterIivRobustANGLEFn) {
    debug_fn.glTexParameterIivRobustANGLEFn = fn.glTexParameterIivRobustANGLEFn;
    fn.glTexParameterIivRobustANGLEFn = Debug_glTexParameterIivRobustANGLE;
  }
  if (!debug_fn.glTexParameterIuivRobustANGLEFn) {
    debug_fn.glTexParameterIuivRobustANGLEFn =
        fn.glTexParameterIuivRobustANGLEFn;
    fn.glTexParameterIuivRobustANGLEFn = Debug_glTexParameterIuivRobustANGLE;
  }
  if (!debug_fn.glTexParameterivFn) {
    debug_fn.glTexParameterivFn = fn.glTexParameterivFn;
    fn.glTexParameterivFn = Debug_glTexParameteriv;
  }
  if (!debug_fn.glTexParameterivRobustANGLEFn) {
    debug_fn.glTexParameterivRobustANGLEFn = fn.glTexParameterivRobustANGLEFn;
    fn.glTexParameterivRobustANGLEFn = Debug_glTexParameterivRobustANGLE;
  }
  if (!debug_fn.glTexStorage2DEXTFn) {
    debug_fn.glTexStorage2DEXTFn = fn.glTexStorage2DEXTFn;
    fn.glTexStorage2DEXTFn = Debug_glTexStorage2DEXT;
  }
  if (!debug_fn.glTexStorage3DFn) {
    debug_fn.glTexStorage3DFn = fn.glTexStorage3DFn;
    fn.glTexStorage3DFn = Debug_glTexStorage3D;
  }
  if (!debug_fn.glTexSubImage2DFn) {
    debug_fn.glTexSubImage2DFn = fn.glTexSubImage2DFn;
    fn.glTexSubImage2DFn = Debug_glTexSubImage2D;
  }
  if (!debug_fn.glTexSubImage2DRobustANGLEFn) {
    debug_fn.glTexSubImage2DRobustANGLEFn = fn.glTexSubImage2DRobustANGLEFn;
    fn.glTexSubImage2DRobustANGLEFn = Debug_glTexSubImage2DRobustANGLE;
  }
  if (!debug_fn.glTexSubImage3DFn) {
    debug_fn.glTexSubImage3DFn = fn.glTexSubImage3DFn;
    fn.glTexSubImage3DFn = Debug_glTexSubImage3D;
  }
  if (!debug_fn.glTexSubImage3DRobustANGLEFn) {
    debug_fn.glTexSubImage3DRobustANGLEFn = fn.glTexSubImage3DRobustANGLEFn;
    fn.glTexSubImage3DRobustANGLEFn = Debug_glTexSubImage3DRobustANGLE;
  }
  if (!debug_fn.glTransformFeedbackVaryingsFn) {
    debug_fn.glTransformFeedbackVaryingsFn = fn.glTransformFeedbackVaryingsFn;
    fn.glTransformFeedbackVaryingsFn = Debug_glTransformFeedbackVaryings;
  }
  if (!debug_fn.glUniform1fFn) {
    debug_fn.glUniform1fFn = fn.glUniform1fFn;
    fn.glUniform1fFn = Debug_glUniform1f;
  }
  if (!debug_fn.glUniform1fvFn) {
    debug_fn.glUniform1fvFn = fn.glUniform1fvFn;
    fn.glUniform1fvFn = Debug_glUniform1fv;
  }
  if (!debug_fn.glUniform1iFn) {
    debug_fn.glUniform1iFn = fn.glUniform1iFn;
    fn.glUniform1iFn = Debug_glUniform1i;
  }
  if (!debug_fn.glUniform1ivFn) {
    debug_fn.glUniform1ivFn = fn.glUniform1ivFn;
    fn.glUniform1ivFn = Debug_glUniform1iv;
  }
  if (!debug_fn.glUniform1uiFn) {
    debug_fn.glUniform1uiFn = fn.glUniform1uiFn;
    fn.glUniform1uiFn = Debug_glUniform1ui;
  }
  if (!debug_fn.glUniform1uivFn) {
    debug_fn.glUniform1uivFn = fn.glUniform1uivFn;
    fn.glUniform1uivFn = Debug_glUniform1uiv;
  }
  if (!debug_fn.glUniform2fFn) {
    debug_fn.glUniform2fFn = fn.glUniform2fFn;
    fn.glUniform2fFn = Debug_glUniform2f;
  }
  if (!debug_fn.glUniform2fvFn) {
    debug_fn.glUniform2fvFn = fn.glUniform2fvFn;
    fn.glUniform2fvFn = Debug_glUniform2fv;
  }
  if (!debug_fn.glUniform2iFn) {
    debug_fn.glUniform2iFn = fn.glUniform2iFn;
    fn.glUniform2iFn = Debug_glUniform2i;
  }
  if (!debug_fn.glUniform2ivFn) {
    debug_fn.glUniform2ivFn = fn.glUniform2ivFn;
    fn.glUniform2ivFn = Debug_glUniform2iv;
  }
  if (!debug_fn.glUniform2uiFn) {
    debug_fn.glUniform2uiFn = fn.glUniform2uiFn;
    fn.glUniform2uiFn = Debug_glUniform2ui;
  }
  if (!debug_fn.glUniform2uivFn) {
    debug_fn.glUniform2uivFn = fn.glUniform2uivFn;
    fn.glUniform2uivFn = Debug_glUniform2uiv;
  }
  if (!debug_fn.glUniform3fFn) {
    debug_fn.glUniform3fFn = fn.glUniform3fFn;
    fn.glUniform3fFn = Debug_glUniform3f;
  }
  if (!debug_fn.glUniform3fvFn) {
    debug_fn.glUniform3fvFn = fn.glUniform3fvFn;
    fn.glUniform3fvFn = Debug_glUniform3fv;
  }
  if (!debug_fn.glUniform3iFn) {
    debug_fn.glUniform3iFn = fn.glUniform3iFn;
    fn.glUniform3iFn = Debug_glUniform3i;
  }
  if (!debug_fn.glUniform3ivFn) {
    debug_fn.glUniform3ivFn = fn.glUniform3ivFn;
    fn.glUniform3ivFn = Debug_glUniform3iv;
  }
  if (!debug_fn.glUniform3uiFn) {
    debug_fn.glUniform3uiFn = fn.glUniform3uiFn;
    fn.glUniform3uiFn = Debug_glUniform3ui;
  }
  if (!debug_fn.glUniform3uivFn) {
    debug_fn.glUniform3uivFn = fn.glUniform3uivFn;
    fn.glUniform3uivFn = Debug_glUniform3uiv;
  }
  if (!debug_fn.glUniform4fFn) {
    debug_fn.glUniform4fFn = fn.glUniform4fFn;
    fn.glUniform4fFn = Debug_glUniform4f;
  }
  if (!debug_fn.glUniform4fvFn) {
    debug_fn.glUniform4fvFn = fn.glUniform4fvFn;
    fn.glUniform4fvFn = Debug_glUniform4fv;
  }
  if (!debug_fn.glUniform4iFn) {
    debug_fn.glUniform4iFn = fn.glUniform4iFn;
    fn.glUniform4iFn = Debug_glUniform4i;
  }
  if (!debug_fn.glUniform4ivFn) {
    debug_fn.glUniform4ivFn = fn.glUniform4ivFn;
    fn.glUniform4ivFn = Debug_glUniform4iv;
  }
  if (!debug_fn.glUniform4uiFn) {
    debug_fn.glUniform4uiFn = fn.glUniform4uiFn;
    fn.glUniform4uiFn = Debug_glUniform4ui;
  }
  if (!debug_fn.glUniform4uivFn) {
    debug_fn.glUniform4uivFn = fn.glUniform4uivFn;
    fn.glUniform4uivFn = Debug_glUniform4uiv;
  }
  if (!debug_fn.glUniformBlockBindingFn) {
    debug_fn.glUniformBlockBindingFn = fn.glUniformBlockBindingFn;
    fn.glUniformBlockBindingFn = Debug_glUniformBlockBinding;
  }
  if (!debug_fn.glUniformMatrix2fvFn) {
    debug_fn.glUniformMatrix2fvFn = fn.glUniformMatrix2fvFn;
    fn.glUniformMatrix2fvFn = Debug_glUniformMatrix2fv;
  }
  if (!debug_fn.glUniformMatrix2x3fvFn) {
    debug_fn.glUniformMatrix2x3fvFn = fn.glUniformMatrix2x3fvFn;
    fn.glUniformMatrix2x3fvFn = Debug_glUniformMatrix2x3fv;
  }
  if (!debug_fn.glUniformMatrix2x4fvFn) {
    debug_fn.glUniformMatrix2x4fvFn = fn.glUniformMatrix2x4fvFn;
    fn.glUniformMatrix2x4fvFn = Debug_glUniformMatrix2x4fv;
  }
  if (!debug_fn.glUniformMatrix3fvFn) {
    debug_fn.glUniformMatrix3fvFn = fn.glUniformMatrix3fvFn;
    fn.glUniformMatrix3fvFn = Debug_glUniformMatrix3fv;
  }
  if (!debug_fn.glUniformMatrix3x2fvFn) {
    debug_fn.glUniformMatrix3x2fvFn = fn.glUniformMatrix3x2fvFn;
    fn.glUniformMatrix3x2fvFn = Debug_glUniformMatrix3x2fv;
  }
  if (!debug_fn.glUniformMatrix3x4fvFn) {
    debug_fn.glUniformMatrix3x4fvFn = fn.glUniformMatrix3x4fvFn;
    fn.glUniformMatrix3x4fvFn = Debug_glUniformMatrix3x4fv;
  }
  if (!debug_fn.glUniformMatrix4fvFn) {
    debug_fn.glUniformMatrix4fvFn = fn.glUniformMatrix4fvFn;
    fn.glUniformMatrix4fvFn = Debug_glUniformMatrix4fv;
  }
  if (!debug_fn.glUniformMatrix4x2fvFn) {
    debug_fn.glUniformMatrix4x2fvFn = fn.glUniformMatrix4x2fvFn;
    fn.glUniformMatrix4x2fvFn = Debug_glUniformMatrix4x2fv;
  }
  if (!debug_fn.glUniformMatrix4x3fvFn) {
    debug_fn.glUniformMatrix4x3fvFn = fn.glUniformMatrix4x3fvFn;
    fn.glUniformMatrix4x3fvFn = Debug_glUniformMatrix4x3fv;
  }
  if (!debug_fn.glUnmapBufferFn) {
    debug_fn.glUnmapBufferFn = fn.glUnmapBufferFn;
    fn.glUnmapBufferFn = Debug_glUnmapBuffer;
  }
  if (!debug_fn.glUseProgramFn) {
    debug_fn.glUseProgramFn = fn.glUseProgramFn;
    fn.glUseProgramFn = Debug_glUseProgram;
  }
  if (!debug_fn.glValidateProgramFn) {
    debug_fn.glValidateProgramFn = fn.glValidateProgramFn;
    fn.glValidateProgramFn = Debug_glValidateProgram;
  }
  if (!debug_fn.glVertexAttrib1fFn) {
    debug_fn.glVertexAttrib1fFn = fn.glVertexAttrib1fFn;
    fn.glVertexAttrib1fFn = Debug_glVertexAttrib1f;
  }
  if (!debug_fn.glVertexAttrib1fvFn) {
    debug_fn.glVertexAttrib1fvFn = fn.glVertexAttrib1fvFn;
    fn.glVertexAttrib1fvFn = Debug_glVertexAttrib1fv;
  }
  if (!debug_fn.glVertexAttrib2fFn) {
    debug_fn.glVertexAttrib2fFn = fn.glVertexAttrib2fFn;
    fn.glVertexAttrib2fFn = Debug_glVertexAttrib2f;
  }
  if (!debug_fn.glVertexAttrib2fvFn) {
    debug_fn.glVertexAttrib2fvFn = fn.glVertexAttrib2fvFn;
    fn.glVertexAttrib2fvFn = Debug_glVertexAttrib2fv;
  }
  if (!debug_fn.glVertexAttrib3fFn) {
    debug_fn.glVertexAttrib3fFn = fn.glVertexAttrib3fFn;
    fn.glVertexAttrib3fFn = Debug_glVertexAttrib3f;
  }
  if (!debug_fn.glVertexAttrib3fvFn) {
    debug_fn.glVertexAttrib3fvFn = fn.glVertexAttrib3fvFn;
    fn.glVertexAttrib3fvFn = Debug_glVertexAttrib3fv;
  }
  if (!debug_fn.glVertexAttrib4fFn) {
    debug_fn.glVertexAttrib4fFn = fn.glVertexAttrib4fFn;
    fn.glVertexAttrib4fFn = Debug_glVertexAttrib4f;
  }
  if (!debug_fn.glVertexAttrib4fvFn) {
    debug_fn.glVertexAttrib4fvFn = fn.glVertexAttrib4fvFn;
    fn.glVertexAttrib4fvFn = Debug_glVertexAttrib4fv;
  }
  if (!debug_fn.glVertexAttribDivisorANGLEFn) {
    debug_fn.glVertexAttribDivisorANGLEFn = fn.glVertexAttribDivisorANGLEFn;
    fn.glVertexAttribDivisorANGLEFn = Debug_glVertexAttribDivisorANGLE;
  }
  if (!debug_fn.glVertexAttribI4iFn) {
    debug_fn.glVertexAttribI4iFn = fn.glVertexAttribI4iFn;
    fn.glVertexAttribI4iFn = Debug_glVertexAttribI4i;
  }
  if (!debug_fn.glVertexAttribI4ivFn) {
    debug_fn.glVertexAttribI4ivFn = fn.glVertexAttribI4ivFn;
    fn.glVertexAttribI4ivFn = Debug_glVertexAttribI4iv;
  }
  if (!debug_fn.glVertexAttribI4uiFn) {
    debug_fn.glVertexAttribI4uiFn = fn.glVertexAttribI4uiFn;
    fn.glVertexAttribI4uiFn = Debug_glVertexAttribI4ui;
  }
  if (!debug_fn.glVertexAttribI4uivFn) {
    debug_fn.glVertexAttribI4uivFn = fn.glVertexAttribI4uivFn;
    fn.glVertexAttribI4uivFn = Debug_glVertexAttribI4uiv;
  }
  if (!debug_fn.glVertexAttribIPointerFn) {
    debug_fn.glVertexAttribIPointerFn = fn.glVertexAttribIPointerFn;
    fn.glVertexAttribIPointerFn = Debug_glVertexAttribIPointer;
  }
  if (!debug_fn.glVertexAttribPointerFn) {
    debug_fn.glVertexAttribPointerFn = fn.glVertexAttribPointerFn;
    fn.glVertexAttribPointerFn = Debug_glVertexAttribPointer;
  }
  if (!debug_fn.glViewportFn) {
    debug_fn.glViewportFn = fn.glViewportFn;
    fn.glViewportFn = Debug_glViewport;
  }
  if (!debug_fn.glWaitSyncFn) {
    debug_fn.glWaitSyncFn = fn.glWaitSyncFn;
    fn.glWaitSyncFn = Debug_glWaitSync;
  }
  g_debugBindingsInitialized = true;
}

void DriverGL::ClearBindings() {
  memset(this, 0, sizeof(*this));
}

void GLApiBase::glActiveTextureFn(GLenum texture) {
  driver_->fn.glActiveTextureFn(texture);
}

void GLApiBase::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  driver_->fn.glApplyFramebufferAttachmentCMAAINTELFn();
}

void GLApiBase::glAttachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glAttachShaderFn(program, shader);
}

void GLApiBase::glBeginQueryFn(GLenum target, GLuint id) {
  driver_->fn.glBeginQueryFn(target, id);
}

void GLApiBase::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  driver_->fn.glBeginTransformFeedbackFn(primitiveMode);
}

void GLApiBase::glBindAttribLocationFn(GLuint program,
                                       GLuint index,
                                       const char* name) {
  driver_->fn.glBindAttribLocationFn(program, index, name);
}

void GLApiBase::glBindBufferFn(GLenum target, GLuint buffer) {
  driver_->fn.glBindBufferFn(target, buffer);
}

void GLApiBase::glBindBufferBaseFn(GLenum target, GLuint index, GLuint buffer) {
  driver_->fn.glBindBufferBaseFn(target, index, buffer);
}

void GLApiBase::glBindBufferRangeFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer,
                                    GLintptr offset,
                                    GLsizeiptr size) {
  driver_->fn.glBindBufferRangeFn(target, index, buffer, offset, size);
}

void GLApiBase::glBindFragDataLocationFn(GLuint program,
                                         GLuint colorNumber,
                                         const char* name) {
  driver_->fn.glBindFragDataLocationFn(program, colorNumber, name);
}

void GLApiBase::glBindFragDataLocationIndexedFn(GLuint program,
                                                GLuint colorNumber,
                                                GLuint index,
                                                const char* name) {
  driver_->fn.glBindFragDataLocationIndexedFn(program, colorNumber, index,
                                              name);
}

void GLApiBase::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  driver_->fn.glBindFramebufferEXTFn(target, framebuffer);
}

void GLApiBase::glBindImageTextureEXTFn(GLuint index,
                                        GLuint texture,
                                        GLint level,
                                        GLboolean layered,
                                        GLint layer,
                                        GLenum access,
                                        GLint format) {
  driver_->fn.glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                      access, format);
}

void GLApiBase::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  driver_->fn.glBindRenderbufferEXTFn(target, renderbuffer);
}

void GLApiBase::glBindSamplerFn(GLuint unit, GLuint sampler) {
  driver_->fn.glBindSamplerFn(unit, sampler);
}

void GLApiBase::glBindTextureFn(GLenum target, GLuint texture) {
  driver_->fn.glBindTextureFn(target, texture);
}

void GLApiBase::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  driver_->fn.glBindTransformFeedbackFn(target, id);
}

void GLApiBase::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                GLint location,
                                                const char* name) {
  driver_->fn.glBindUniformLocationCHROMIUMFn(program, location, name);
}

void GLApiBase::glBindVertexArrayOESFn(GLuint array) {
  driver_->fn.glBindVertexArrayOESFn(array);
}

void GLApiBase::glBlendBarrierKHRFn(void) {
  driver_->fn.glBlendBarrierKHRFn();
}

void GLApiBase::glBlendColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glBlendColorFn(red, green, blue, alpha);
}

void GLApiBase::glBlendEquationFn(GLenum mode) {
  driver_->fn.glBlendEquationFn(mode);
}

void GLApiBase::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  driver_->fn.glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void GLApiBase::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  driver_->fn.glBlendFuncFn(sfactor, dfactor);
}

void GLApiBase::glBlendFuncSeparateFn(GLenum srcRGB,
                                      GLenum dstRGB,
                                      GLenum srcAlpha,
                                      GLenum dstAlpha) {
  driver_->fn.glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GLApiBase::glBlitFramebufferFn(GLint srcX0,
                                    GLint srcY0,
                                    GLint srcX1,
                                    GLint srcY1,
                                    GLint dstX0,
                                    GLint dstY0,
                                    GLint dstX1,
                                    GLint dstY1,
                                    GLbitfield mask,
                                    GLenum filter) {
  driver_->fn.glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                  dstX1, dstY1, mask, filter);
}

void GLApiBase::glBlitFramebufferANGLEFn(GLint srcX0,
                                         GLint srcY0,
                                         GLint srcX1,
                                         GLint srcY1,
                                         GLint dstX0,
                                         GLint dstY0,
                                         GLint dstX1,
                                         GLint dstY1,
                                         GLbitfield mask,
                                         GLenum filter) {
  driver_->fn.glBlitFramebufferANGLEFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                       dstX1, dstY1, mask, filter);
}

void GLApiBase::glBlitFramebufferEXTFn(GLint srcX0,
                                       GLint srcY0,
                                       GLint srcX1,
                                       GLint srcY1,
                                       GLint dstX0,
                                       GLint dstY0,
                                       GLint dstX1,
                                       GLint dstY1,
                                       GLbitfield mask,
                                       GLenum filter) {
  driver_->fn.glBlitFramebufferEXTFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                     dstX1, dstY1, mask, filter);
}

void GLApiBase::glBufferDataFn(GLenum target,
                               GLsizeiptr size,
                               const void* data,
                               GLenum usage) {
  driver_->fn.glBufferDataFn(target, size, data, usage);
}

void GLApiBase::glBufferSubDataFn(GLenum target,
                                  GLintptr offset,
                                  GLsizeiptr size,
                                  const void* data) {
  driver_->fn.glBufferSubDataFn(target, offset, size, data);
}

GLenum GLApiBase::glCheckFramebufferStatusEXTFn(GLenum target) {
  return driver_->fn.glCheckFramebufferStatusEXTFn(target);
}

void GLApiBase::glClearFn(GLbitfield mask) {
  driver_->fn.glClearFn(mask);
}

void GLApiBase::glClearBufferfiFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat depth,
                                  GLint stencil) {
  driver_->fn.glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void GLApiBase::glClearBufferfvFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLfloat* value) {
  driver_->fn.glClearBufferfvFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearBufferivFn(GLenum buffer,
                                  GLint drawbuffer,
                                  const GLint* value) {
  driver_->fn.glClearBufferivFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearBufferuivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLuint* value) {
  driver_->fn.glClearBufferuivFn(buffer, drawbuffer, value);
}

void GLApiBase::glClearColorFn(GLclampf red,
                               GLclampf green,
                               GLclampf blue,
                               GLclampf alpha) {
  driver_->fn.glClearColorFn(red, green, blue, alpha);
}

void GLApiBase::glClearDepthFn(GLclampd depth) {
  driver_->fn.glClearDepthFn(depth);
}

void GLApiBase::glClearDepthfFn(GLclampf depth) {
  driver_->fn.glClearDepthfFn(depth);
}

void GLApiBase::glClearStencilFn(GLint s) {
  driver_->fn.glClearStencilFn(s);
}

GLenum GLApiBase::glClientWaitSyncFn(GLsync sync,
                                     GLbitfield flags,
                                     GLuint64 timeout) {
  return driver_->fn.glClientWaitSyncFn(sync, flags, timeout);
}

void GLApiBase::glColorMaskFn(GLboolean red,
                              GLboolean green,
                              GLboolean blue,
                              GLboolean alpha) {
  driver_->fn.glColorMaskFn(red, green, blue, alpha);
}

void GLApiBase::glCompileShaderFn(GLuint shader) {
  driver_->fn.glCompileShaderFn(shader);
}

void GLApiBase::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                  GLuint destId) {
  driver_->fn.glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

void GLApiBase::glCompressedTexImage2DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage2DFn(target, level, internalformat, width,
                                       height, border, imageSize, data);
}

void GLApiBase::glCompressedTexImage3DFn(GLenum target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         const void* data) {
  driver_->fn.glCompressedTexImage3DFn(target, level, internalformat, width,
                                       height, depth, border, imageSize, data);
}

void GLApiBase::glCompressedTexSubImage2DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

void GLApiBase::glCompressedTexSubImage3DFn(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLsizei imageSize,
                                            const void* data) {
  driver_->fn.glCompressedTexSubImage3DFn(target, level, xoffset, yoffset,
                                          zoffset, width, height, depth, format,
                                          imageSize, data);
}

void GLApiBase::glCopyBufferSubDataFn(GLenum readTarget,
                                      GLenum writeTarget,
                                      GLintptr readOffset,
                                      GLintptr writeOffset,
                                      GLsizeiptr size) {
  driver_->fn.glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                    writeOffset, size);
}

void GLApiBase::glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                           GLuint destId,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLboolean unpackFlipY,
                                           GLboolean unpackPremultiplyAlpha,
                                           GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopySubTextureCHROMIUMFn(
      sourceId, destId, xoffset, yoffset, x, y, width, height, unpackFlipY,
      unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void GLApiBase::glCopyTexImage2DFn(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border) {
  driver_->fn.glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                                 height, border);
}

void GLApiBase::glCopyTexSubImage2DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y,
                                    width, height);
}

void GLApiBase::glCopyTexSubImage3DFn(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLint x,
                                      GLint y,
                                      GLsizei width,
                                      GLsizei height) {
  driver_->fn.glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x,
                                    y, width, height);
}

void GLApiBase::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                        GLuint destId,
                                        GLint internalFormat,
                                        GLenum destType,
                                        GLboolean unpackFlipY,
                                        GLboolean unpackPremultiplyAlpha,
                                        GLboolean unpackUnmultiplyAlpha) {
  driver_->fn.glCopyTextureCHROMIUMFn(
      sourceId, destId, internalFormat, destType, unpackFlipY,
      unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void GLApiBase::glCoverageModulationNVFn(GLenum components) {
  driver_->fn.glCoverageModulationNVFn(components);
}

void GLApiBase::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                             GLenum pathNameType,
                                             const void* paths,
                                             GLuint pathBase,
                                             GLenum coverMode,
                                             GLenum transformType,
                                             const GLfloat* transformValues) {
  driver_->fn.glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                           pathBase, coverMode, transformType,
                                           transformValues);
}

void GLApiBase::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  driver_->fn.glCoverFillPathNVFn(path, coverMode);
}

void GLApiBase::glCoverStrokePathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum coverMode,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, coverMode, transformType,
                                             transformValues);
}

void GLApiBase::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  driver_->fn.glCoverStrokePathNVFn(name, coverMode);
}

GLuint GLApiBase::glCreateProgramFn(void) {
  return driver_->fn.glCreateProgramFn();
}

GLuint GLApiBase::glCreateShaderFn(GLenum type) {
  return driver_->fn.glCreateShaderFn(type);
}

void GLApiBase::glCullFaceFn(GLenum mode) {
  driver_->fn.glCullFaceFn(mode);
}

void GLApiBase::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                         const void* userParam) {
  driver_->fn.glDebugMessageCallbackFn(callback, userParam);
}

void GLApiBase::glDebugMessageControlFn(GLenum source,
                                        GLenum type,
                                        GLenum severity,
                                        GLsizei count,
                                        const GLuint* ids,
                                        GLboolean enabled) {
  driver_->fn.glDebugMessageControlFn(source, type, severity, count, ids,
                                      enabled);
}

void GLApiBase::glDebugMessageInsertFn(GLenum source,
                                       GLenum type,
                                       GLuint id,
                                       GLenum severity,
                                       GLsizei length,
                                       const char* buf) {
  driver_->fn.glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

void GLApiBase::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  driver_->fn.glDeleteBuffersARBFn(n, buffers);
}

void GLApiBase::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesAPPLEFn(n, fences);
}

void GLApiBase::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  driver_->fn.glDeleteFencesNVFn(n, fences);
}

void GLApiBase::glDeleteFramebuffersEXTFn(GLsizei n,
                                          const GLuint* framebuffers) {
  driver_->fn.glDeleteFramebuffersEXTFn(n, framebuffers);
}

void GLApiBase::glDeletePathsNVFn(GLuint path, GLsizei range) {
  driver_->fn.glDeletePathsNVFn(path, range);
}

void GLApiBase::glDeleteProgramFn(GLuint program) {
  driver_->fn.glDeleteProgramFn(program);
}

void GLApiBase::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteQueriesFn(n, ids);
}

void GLApiBase::glDeleteRenderbuffersEXTFn(GLsizei n,
                                           const GLuint* renderbuffers) {
  driver_->fn.glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void GLApiBase::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  driver_->fn.glDeleteSamplersFn(n, samplers);
}

void GLApiBase::glDeleteShaderFn(GLuint shader) {
  driver_->fn.glDeleteShaderFn(shader);
}

void GLApiBase::glDeleteSyncFn(GLsync sync) {
  driver_->fn.glDeleteSyncFn(sync);
}

void GLApiBase::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  driver_->fn.glDeleteTexturesFn(n, textures);
}

void GLApiBase::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  driver_->fn.glDeleteTransformFeedbacksFn(n, ids);
}

void GLApiBase::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  driver_->fn.glDeleteVertexArraysOESFn(n, arrays);
}

void GLApiBase::glDepthFuncFn(GLenum func) {
  driver_->fn.glDepthFuncFn(func);
}

void GLApiBase::glDepthMaskFn(GLboolean flag) {
  driver_->fn.glDepthMaskFn(flag);
}

void GLApiBase::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  driver_->fn.glDepthRangeFn(zNear, zFar);
}

void GLApiBase::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  driver_->fn.glDepthRangefFn(zNear, zFar);
}

void GLApiBase::glDetachShaderFn(GLuint program, GLuint shader) {
  driver_->fn.glDetachShaderFn(program, shader);
}

void GLApiBase::glDisableFn(GLenum cap) {
  driver_->fn.glDisableFn(cap);
}

void GLApiBase::glDisableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glDisableVertexAttribArrayFn(index);
}

void GLApiBase::glDiscardFramebufferEXTFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void GLApiBase::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  driver_->fn.glDrawArraysFn(mode, first, count);
}

void GLApiBase::glDrawArraysInstancedANGLEFn(GLenum mode,
                                             GLint first,
                                             GLsizei count,
                                             GLsizei primcount) {
  driver_->fn.glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void GLApiBase::glDrawBufferFn(GLenum mode) {
  driver_->fn.glDrawBufferFn(mode);
}

void GLApiBase::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  driver_->fn.glDrawBuffersARBFn(n, bufs);
}

void GLApiBase::glDrawElementsFn(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void* indices) {
  driver_->fn.glDrawElementsFn(mode, count, type, indices);
}

void GLApiBase::glDrawElementsInstancedANGLEFn(GLenum mode,
                                               GLsizei count,
                                               GLenum type,
                                               const void* indices,
                                               GLsizei primcount) {
  driver_->fn.glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                             primcount);
}

void GLApiBase::glDrawRangeElementsFn(GLenum mode,
                                      GLuint start,
                                      GLuint end,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  driver_->fn.glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

void GLApiBase::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                         GLeglImageOES image) {
  driver_->fn.glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void GLApiBase::glEGLImageTargetTexture2DOESFn(GLenum target,
                                               GLeglImageOES image) {
  driver_->fn.glEGLImageTargetTexture2DOESFn(target, image);
}

void GLApiBase::glEnableFn(GLenum cap) {
  driver_->fn.glEnableFn(cap);
}

void GLApiBase::glEnableVertexAttribArrayFn(GLuint index) {
  driver_->fn.glEnableVertexAttribArrayFn(index);
}

void GLApiBase::glEndQueryFn(GLenum target) {
  driver_->fn.glEndQueryFn(target);
}

void GLApiBase::glEndTransformFeedbackFn(void) {
  driver_->fn.glEndTransformFeedbackFn();
}

GLsync GLApiBase::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  return driver_->fn.glFenceSyncFn(condition, flags);
}

void GLApiBase::glFinishFn(void) {
  driver_->fn.glFinishFn();
}

void GLApiBase::glFinishFenceAPPLEFn(GLuint fence) {
  driver_->fn.glFinishFenceAPPLEFn(fence);
}

void GLApiBase::glFinishFenceNVFn(GLuint fence) {
  driver_->fn.glFinishFenceNVFn(fence);
}

void GLApiBase::glFlushFn(void) {
  driver_->fn.glFlushFn();
}

void GLApiBase::glFlushMappedBufferRangeFn(GLenum target,
                                           GLintptr offset,
                                           GLsizeiptr length) {
  driver_->fn.glFlushMappedBufferRangeFn(target, offset, length);
}

void GLApiBase::glFramebufferRenderbufferEXTFn(GLenum target,
                                               GLenum attachment,
                                               GLenum renderbuffertarget,
                                               GLuint renderbuffer) {
  driver_->fn.glFramebufferRenderbufferEXTFn(target, attachment,
                                             renderbuffertarget, renderbuffer);
}

void GLApiBase::glFramebufferTexture2DEXTFn(GLenum target,
                                            GLenum attachment,
                                            GLenum textarget,
                                            GLuint texture,
                                            GLint level) {
  driver_->fn.glFramebufferTexture2DEXTFn(target, attachment, textarget,
                                          texture, level);
}

void GLApiBase::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                       GLenum attachment,
                                                       GLenum textarget,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLsizei samples) {
  driver_->fn.glFramebufferTexture2DMultisampleEXTFn(
      target, attachment, textarget, texture, level, samples);
}

void GLApiBase::glFramebufferTexture2DMultisampleIMGFn(GLenum target,
                                                       GLenum attachment,
                                                       GLenum textarget,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLsizei samples) {
  driver_->fn.glFramebufferTexture2DMultisampleIMGFn(
      target, attachment, textarget, texture, level, samples);
}

void GLApiBase::glFramebufferTextureLayerFn(GLenum target,
                                            GLenum attachment,
                                            GLuint texture,
                                            GLint level,
                                            GLint layer) {
  driver_->fn.glFramebufferTextureLayerFn(target, attachment, texture, level,
                                          layer);
}

void GLApiBase::glFrontFaceFn(GLenum mode) {
  driver_->fn.glFrontFaceFn(mode);
}

void GLApiBase::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  driver_->fn.glGenBuffersARBFn(n, buffers);
}

void GLApiBase::glGenerateMipmapEXTFn(GLenum target) {
  driver_->fn.glGenerateMipmapEXTFn(target);
}

void GLApiBase::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesAPPLEFn(n, fences);
}

void GLApiBase::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  driver_->fn.glGenFencesNVFn(n, fences);
}

void GLApiBase::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  driver_->fn.glGenFramebuffersEXTFn(n, framebuffers);
}

GLuint GLApiBase::glGenPathsNVFn(GLsizei range) {
  return driver_->fn.glGenPathsNVFn(range);
}

void GLApiBase::glGenQueriesFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenQueriesFn(n, ids);
}

void GLApiBase::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  driver_->fn.glGenRenderbuffersEXTFn(n, renderbuffers);
}

void GLApiBase::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  driver_->fn.glGenSamplersFn(n, samplers);
}

void GLApiBase::glGenTexturesFn(GLsizei n, GLuint* textures) {
  driver_->fn.glGenTexturesFn(n, textures);
}

void GLApiBase::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  driver_->fn.glGenTransformFeedbacksFn(n, ids);
}

void GLApiBase::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  driver_->fn.glGenVertexArraysOESFn(n, arrays);
}

void GLApiBase::glGetActiveAttribFn(GLuint program,
                                    GLuint index,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    GLint* size,
                                    GLenum* type,
                                    char* name) {
  driver_->fn.glGetActiveAttribFn(program, index, bufsize, length, size, type,
                                  name);
}

void GLApiBase::glGetActiveUniformFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  driver_->fn.glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                   name);
}

void GLApiBase::glGetActiveUniformBlockivFn(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLint* params) {
  driver_->fn.glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                          params);
}

void GLApiBase::glGetActiveUniformBlockivRobustANGLEFn(GLuint program,
                                                       GLuint uniformBlockIndex,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  driver_->fn.glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void GLApiBase::glGetActiveUniformBlockNameFn(GLuint program,
                                              GLuint uniformBlockIndex,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              char* uniformBlockName) {
  driver_->fn.glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                            length, uniformBlockName);
}

void GLApiBase::glGetActiveUniformsivFn(GLuint program,
                                        GLsizei uniformCount,
                                        const GLuint* uniformIndices,
                                        GLenum pname,
                                        GLint* params) {
  driver_->fn.glGetActiveUniformsivFn(program, uniformCount, uniformIndices,
                                      pname, params);
}

void GLApiBase::glGetAttachedShadersFn(GLuint program,
                                       GLsizei maxcount,
                                       GLsizei* count,
                                       GLuint* shaders) {
  driver_->fn.glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint GLApiBase::glGetAttribLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetAttribLocationFn(program, name);
}

void GLApiBase::glGetBooleani_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLboolean* data) {
  driver_->fn.glGetBooleani_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

void GLApiBase::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  driver_->fn.glGetBooleanvFn(pname, params);
}

void GLApiBase::glGetBooleanvRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLboolean* data) {
  driver_->fn.glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint64* params) {
  driver_->fn.glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

void GLApiBase::glGetBufferParameterivFn(GLenum target,
                                         GLenum pname,
                                         GLint* params) {
  driver_->fn.glGetBufferParameterivFn(target, pname, params);
}

void GLApiBase::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  driver_->fn.glGetBufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                  length, params);
}

void GLApiBase::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 void** params) {
  driver_->fn.glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetDebugMessageLogFn(GLuint count,
                                       GLsizei bufSize,
                                       GLenum* sources,
                                       GLenum* types,
                                       GLuint* ids,
                                       GLenum* severities,
                                       GLsizei* lengths,
                                       char* messageLog) {
  driver_->fn.glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                     severities, lengths, messageLog);
}

GLenum GLApiBase::glGetErrorFn(void) {
  return driver_->fn.glGetErrorFn();
}

void GLApiBase::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  driver_->fn.glGetFenceivNVFn(fence, pname, params);
}

void GLApiBase::glGetFloatvFn(GLenum pname, GLfloat* params) {
  driver_->fn.glGetFloatvFn(pname, params);
}

void GLApiBase::glGetFloatvRobustANGLEFn(GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLfloat* data) {
  driver_->fn.glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

GLint GLApiBase::glGetFragDataIndexFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataIndexFn(program, name);
}

GLint GLApiBase::glGetFragDataLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetFragDataLocationFn(program, name);
}

void GLApiBase::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                           GLenum attachment,
                                                           GLenum pname,
                                                           GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivEXTFn(target, attachment,
                                                         pname, params);
}

void GLApiBase::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  driver_->fn.glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

void GLApiBase::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  driver_->fn.glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                       length, params);
}

GLenum GLApiBase::glGetGraphicsResetStatusARBFn(void) {
  return driver_->fn.glGetGraphicsResetStatusARBFn();
}

void GLApiBase::glGetInteger64i_vFn(GLenum target,
                                    GLuint index,
                                    GLint64* data) {
  driver_->fn.glGetInteger64i_vFn(target, index, data);
}

void GLApiBase::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                               GLuint index,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint64* data) {
  driver_->fn.glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length,
                                             data);
}

void GLApiBase::glGetInteger64vFn(GLenum pname, GLint64* params) {
  driver_->fn.glGetInteger64vFn(pname, params);
}

void GLApiBase::glGetInteger64vRobustANGLEFn(GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint64* data) {
  driver_->fn.glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  driver_->fn.glGetIntegeri_vFn(target, index, data);
}

void GLApiBase::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                             GLuint index,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* data) {
  driver_->fn.glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length,
                                           data);
}

void GLApiBase::glGetIntegervFn(GLenum pname, GLint* params) {
  driver_->fn.glGetIntegervFn(pname, params);
}

void GLApiBase::glGetIntegervRobustANGLEFn(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* data) {
  driver_->fn.glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

void GLApiBase::glGetInternalformativFn(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLint* params) {
  driver_->fn.glGetInternalformativFn(target, internalformat, pname, bufSize,
                                      params);
}

void GLApiBase::glGetInternalformativRobustANGLEFn(GLenum target,
                                                   GLenum internalformat,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  driver_->fn.glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                                 bufSize, length, params);
}

void GLApiBase::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLfloat* val) {
  driver_->fn.glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length,
                                              val);
}

void GLApiBase::glGetnUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  driver_->fn.glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetnUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  driver_->fn.glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetnUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  driver_->fn.glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                            params);
}

void GLApiBase::glGetObjectLabelFn(GLenum identifier,
                                   GLuint name,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   char* label) {
  driver_->fn.glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

void GLApiBase::glGetObjectPtrLabelFn(void* ptr,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      char* label) {
  driver_->fn.glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

void GLApiBase::glGetPointervFn(GLenum pname, void** params) {
  driver_->fn.glGetPointervFn(pname, params);
}

void GLApiBase::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  driver_->fn.glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                    params);
}

void GLApiBase::glGetProgramBinaryFn(GLuint program,
                                     GLsizei bufSize,
                                     GLsizei* length,
                                     GLenum* binaryFormat,
                                     GLvoid* binary) {
  driver_->fn.glGetProgramBinaryFn(program, bufSize, length, binaryFormat,
                                   binary);
}

void GLApiBase::glGetProgramInfoLogFn(GLuint program,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  driver_->fn.glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void GLApiBase::glGetProgramInterfaceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetProgramInterfaceivFn(program, programInterface, pname,
                                        params);
}

void GLApiBase::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                     GLenum programInterface,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetProgramInterfaceivRobustANGLEFn(
      program, programInterface, pname, bufSize, length, params);
}

void GLApiBase::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  driver_->fn.glGetProgramivFn(program, pname, params);
}

void GLApiBase::glGetProgramivRobustANGLEFn(GLuint program,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetProgramivRobustANGLEFn(program, pname, bufSize, length,
                                          params);
}

void GLApiBase::glGetProgramResourceivFn(GLuint program,
                                         GLenum programInterface,
                                         GLuint index,
                                         GLsizei propCount,
                                         const GLenum* props,
                                         GLsizei bufSize,
                                         GLsizei* length,
                                         GLint* params) {
  driver_->fn.glGetProgramResourceivFn(program, programInterface, index,
                                       propCount, props, bufSize, length,
                                       params);
}

GLint GLApiBase::glGetProgramResourceLocationFn(GLuint program,
                                                GLenum programInterface,
                                                const char* name) {
  return driver_->fn.glGetProgramResourceLocationFn(program, programInterface,
                                                    name);
}

void GLApiBase::glGetProgramResourceNameFn(GLuint program,
                                           GLenum programInterface,
                                           GLuint index,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLchar* name) {
  driver_->fn.glGetProgramResourceNameFn(program, programInterface, index,
                                         bufSize, length, name);
}

void GLApiBase::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryivFn(target, pname, params);
}

void GLApiBase::glGetQueryivRobustANGLEFn(GLenum target,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  driver_->fn.glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

void GLApiBase::glGetQueryObjecti64vFn(GLuint id,
                                       GLenum pname,
                                       GLint64* params) {
  driver_->fn.glGetQueryObjecti64vFn(id, pname, params);
}

void GLApiBase::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* params) {
  driver_->fn.glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  driver_->fn.glGetQueryObjectivFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  driver_->fn.glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

void GLApiBase::glGetQueryObjectui64vFn(GLuint id,
                                        GLenum pname,
                                        GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint64* params) {
  driver_->fn.glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetQueryObjectuivFn(GLuint id, GLenum pname, GLuint* params) {
  driver_->fn.glGetQueryObjectuivFn(id, pname, params);
}

void GLApiBase::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLuint* params) {
  driver_->fn.glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                  GLenum pname,
                                                  GLint* params) {
  driver_->fn.glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void GLApiBase::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  driver_->fn.glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                        length, params);
}

void GLApiBase::glGetSamplerParameterfvFn(GLuint sampler,
                                          GLenum pname,
                                          GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvFn(sampler, pname, params);
}

void GLApiBase::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* params) {
  driver_->fn.glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

void GLApiBase::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                    length, params);
}

void GLApiBase::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLuint* params) {
  driver_->fn.glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                     length, params);
}

void GLApiBase::glGetSamplerParameterivFn(GLuint sampler,
                                          GLenum pname,
                                          GLint* params) {
  driver_->fn.glGetSamplerParameterivFn(sampler, pname, params);
}

void GLApiBase::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  driver_->fn.glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize,
                                                   length, params);
}

void GLApiBase::glGetShaderInfoLogFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* infolog) {
  driver_->fn.glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void GLApiBase::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  driver_->fn.glGetShaderivFn(shader, pname, params);
}

void GLApiBase::glGetShaderivRobustANGLEFn(GLuint shader,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  driver_->fn.glGetShaderivRobustANGLEFn(shader, pname, bufSize, length,
                                         params);
}

void GLApiBase::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                             GLenum precisiontype,
                                             GLint* range,
                                             GLint* precision) {
  driver_->fn.glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                           precision);
}

void GLApiBase::glGetShaderSourceFn(GLuint shader,
                                    GLsizei bufsize,
                                    GLsizei* length,
                                    char* source) {
  driver_->fn.glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* GLApiBase::glGetStringFn(GLenum name) {
  return driver_->fn.glGetStringFn(name);
}

const GLubyte* GLApiBase::glGetStringiFn(GLenum name, GLuint index) {
  return driver_->fn.glGetStringiFn(name, index);
}

void GLApiBase::glGetSyncivFn(GLsync sync,
                              GLenum pname,
                              GLsizei bufSize,
                              GLsizei* length,
                              GLint* values) {
  driver_->fn.glGetSyncivFn(sync, pname, bufSize, length, values);
}

void GLApiBase::glGetTexLevelParameterfvFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvFn(target, level, pname, params);
}

void GLApiBase::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  driver_->fn.glGetTexLevelParameterfvRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

void GLApiBase::glGetTexLevelParameterivFn(GLenum target,
                                           GLint level,
                                           GLenum pname,
                                           GLint* params) {
  driver_->fn.glGetTexLevelParameterivFn(target, level, pname, params);
}

void GLApiBase::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                      GLint level,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  driver_->fn.glGetTexLevelParameterivRobustANGLEFn(target, level, pname,
                                                    bufSize, length, params);
}

void GLApiBase::glGetTexParameterfvFn(GLenum target,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetTexParameterfvFn(target, pname, params);
}

void GLApiBase::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetTexParameterivFn(GLenum target,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetTexParameterivFn(target, pname, params);
}

void GLApiBase::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetTransformFeedbackVaryingFn(GLuint program,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLsizei* size,
                                                GLenum* type,
                                                char* name) {
  driver_->fn.glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                              size, type, name);
}

void GLApiBase::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                   GLsizei bufsize,
                                                   GLsizei* length,
                                                   char* source) {
  driver_->fn.glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length,
                                                 source);
}

GLuint GLApiBase::glGetUniformBlockIndexFn(GLuint program,
                                           const char* uniformBlockName) {
  return driver_->fn.glGetUniformBlockIndexFn(program, uniformBlockName);
}

void GLApiBase::glGetUniformfvFn(GLuint program,
                                 GLint location,
                                 GLfloat* params) {
  driver_->fn.glGetUniformfvFn(program, location, params);
}

void GLApiBase::glGetUniformfvRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLfloat* params) {
  driver_->fn.glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

void GLApiBase::glGetUniformIndicesFn(GLuint program,
                                      GLsizei uniformCount,
                                      const char* const* uniformNames,
                                      GLuint* uniformIndices) {
  driver_->fn.glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                    uniformIndices);
}

void GLApiBase::glGetUniformivFn(GLuint program,
                                 GLint location,
                                 GLint* params) {
  driver_->fn.glGetUniformivFn(program, location, params);
}

void GLApiBase::glGetUniformivRobustANGLEFn(GLuint program,
                                            GLint location,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  driver_->fn.glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                          params);
}

GLint GLApiBase::glGetUniformLocationFn(GLuint program, const char* name) {
  return driver_->fn.glGetUniformLocationFn(program, name);
}

void GLApiBase::glGetUniformuivFn(GLuint program,
                                  GLint location,
                                  GLuint* params) {
  driver_->fn.glGetUniformuivFn(program, location, params);
}

void GLApiBase::glGetUniformuivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLuint* params) {
  driver_->fn.glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                           params);
}

void GLApiBase::glGetVertexAttribfvFn(GLuint index,
                                      GLenum pname,
                                      GLfloat* params) {
  driver_->fn.glGetVertexAttribfvFn(index, pname, params);
}

void GLApiBase::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  driver_->fn.glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  driver_->fn.glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                                params);
}

void GLApiBase::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  driver_->fn.glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                                 params);
}

void GLApiBase::glGetVertexAttribivFn(GLuint index,
                                      GLenum pname,
                                      GLint* params) {
  driver_->fn.glGetVertexAttribivFn(index, pname, params);
}

void GLApiBase::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  driver_->fn.glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                               params);
}

void GLApiBase::glGetVertexAttribPointervFn(GLuint index,
                                            GLenum pname,
                                            void** pointer) {
  driver_->fn.glGetVertexAttribPointervFn(index, pname, pointer);
}

void GLApiBase::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** pointer) {
  driver_->fn.glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize,
                                                     length, pointer);
}

void GLApiBase::glHintFn(GLenum target, GLenum mode) {
  driver_->fn.glHintFn(target, mode);
}

void GLApiBase::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glInsertEventMarkerEXTFn(length, marker);
}

void GLApiBase::glInvalidateFramebufferFn(GLenum target,
                                          GLsizei numAttachments,
                                          const GLenum* attachments) {
  driver_->fn.glInvalidateFramebufferFn(target, numAttachments, attachments);
}

void GLApiBase::glInvalidateSubFramebufferFn(GLenum target,
                                             GLsizei numAttachments,
                                             const GLenum* attachments,
                                             GLint x,
                                             GLint y,
                                             GLint width,
                                             GLint height) {
  driver_->fn.glInvalidateSubFramebufferFn(target, numAttachments, attachments,
                                           x, y, width, height);
}

GLboolean GLApiBase::glIsBufferFn(GLuint buffer) {
  return driver_->fn.glIsBufferFn(buffer);
}

GLboolean GLApiBase::glIsEnabledFn(GLenum cap) {
  return driver_->fn.glIsEnabledFn(cap);
}

GLboolean GLApiBase::glIsFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glIsFenceAPPLEFn(fence);
}

GLboolean GLApiBase::glIsFenceNVFn(GLuint fence) {
  return driver_->fn.glIsFenceNVFn(fence);
}

GLboolean GLApiBase::glIsFramebufferEXTFn(GLuint framebuffer) {
  return driver_->fn.glIsFramebufferEXTFn(framebuffer);
}

GLboolean GLApiBase::glIsPathNVFn(GLuint path) {
  return driver_->fn.glIsPathNVFn(path);
}

GLboolean GLApiBase::glIsProgramFn(GLuint program) {
  return driver_->fn.glIsProgramFn(program);
}

GLboolean GLApiBase::glIsQueryFn(GLuint query) {
  return driver_->fn.glIsQueryFn(query);
}

GLboolean GLApiBase::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  return driver_->fn.glIsRenderbufferEXTFn(renderbuffer);
}

GLboolean GLApiBase::glIsSamplerFn(GLuint sampler) {
  return driver_->fn.glIsSamplerFn(sampler);
}

GLboolean GLApiBase::glIsShaderFn(GLuint shader) {
  return driver_->fn.glIsShaderFn(shader);
}

GLboolean GLApiBase::glIsSyncFn(GLsync sync) {
  return driver_->fn.glIsSyncFn(sync);
}

GLboolean GLApiBase::glIsTextureFn(GLuint texture) {
  return driver_->fn.glIsTextureFn(texture);
}

GLboolean GLApiBase::glIsTransformFeedbackFn(GLuint id) {
  return driver_->fn.glIsTransformFeedbackFn(id);
}

GLboolean GLApiBase::glIsVertexArrayOESFn(GLuint array) {
  return driver_->fn.glIsVertexArrayOESFn(array);
}

void GLApiBase::glLineWidthFn(GLfloat width) {
  driver_->fn.glLineWidthFn(width);
}

void GLApiBase::glLinkProgramFn(GLuint program) {
  driver_->fn.glLinkProgramFn(program);
}

void* GLApiBase::glMapBufferFn(GLenum target, GLenum access) {
  return driver_->fn.glMapBufferFn(target, access);
}

void* GLApiBase::glMapBufferRangeFn(GLenum target,
                                    GLintptr offset,
                                    GLsizeiptr length,
                                    GLbitfield access) {
  return driver_->fn.glMapBufferRangeFn(target, offset, length, access);
}

void GLApiBase::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  driver_->fn.glMatrixLoadfEXTFn(matrixMode, m);
}

void GLApiBase::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  driver_->fn.glMatrixLoadIdentityEXTFn(matrixMode);
}

void GLApiBase::glMemoryBarrierEXTFn(GLbitfield barriers) {
  driver_->fn.glMemoryBarrierEXTFn(barriers);
}

void GLApiBase::glObjectLabelFn(GLenum identifier,
                                GLuint name,
                                GLsizei length,
                                const char* label) {
  driver_->fn.glObjectLabelFn(identifier, name, length, label);
}

void GLApiBase::glObjectPtrLabelFn(void* ptr,
                                   GLsizei length,
                                   const char* label) {
  driver_->fn.glObjectPtrLabelFn(ptr, length, label);
}

void GLApiBase::glPathCommandsNVFn(GLuint path,
                                   GLsizei numCommands,
                                   const GLubyte* commands,
                                   GLsizei numCoords,
                                   GLenum coordType,
                                   const GLvoid* coords) {
  driver_->fn.glPathCommandsNVFn(path, numCommands, commands, numCoords,
                                 coordType, coords);
}

void GLApiBase::glPathParameterfNVFn(GLuint path, GLenum pname, GLfloat value) {
  driver_->fn.glPathParameterfNVFn(path, pname, value);
}

void GLApiBase::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  driver_->fn.glPathParameteriNVFn(path, pname, value);
}

void GLApiBase::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glPathStencilFuncNVFn(func, ref, mask);
}

void GLApiBase::glPauseTransformFeedbackFn(void) {
  driver_->fn.glPauseTransformFeedbackFn();
}

void GLApiBase::glPixelStoreiFn(GLenum pname, GLint param) {
  driver_->fn.glPixelStoreiFn(pname, param);
}

void GLApiBase::glPointParameteriFn(GLenum pname, GLint param) {
  driver_->fn.glPointParameteriFn(pname, param);
}

void GLApiBase::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  driver_->fn.glPolygonOffsetFn(factor, units);
}

void GLApiBase::glPopDebugGroupFn() {
  driver_->fn.glPopDebugGroupFn();
}

void GLApiBase::glPopGroupMarkerEXTFn(void) {
  driver_->fn.glPopGroupMarkerEXTFn();
}

void GLApiBase::glPrimitiveRestartIndexFn(GLuint index) {
  driver_->fn.glPrimitiveRestartIndexFn(index);
}

void GLApiBase::glProgramBinaryFn(GLuint program,
                                  GLenum binaryFormat,
                                  const GLvoid* binary,
                                  GLsizei length) {
  driver_->fn.glProgramBinaryFn(program, binaryFormat, binary, length);
}

void GLApiBase::glProgramParameteriFn(GLuint program,
                                      GLenum pname,
                                      GLint value) {
  driver_->fn.glProgramParameteriFn(program, pname, value);
}

void GLApiBase::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                  GLint location,
                                                  GLenum genMode,
                                                  GLint components,
                                                  const GLfloat* coeffs) {
  driver_->fn.glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                                components, coeffs);
}

void GLApiBase::glPushDebugGroupFn(GLenum source,
                                   GLuint id,
                                   GLsizei length,
                                   const char* message) {
  driver_->fn.glPushDebugGroupFn(source, id, length, message);
}

void GLApiBase::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  driver_->fn.glPushGroupMarkerEXTFn(length, marker);
}

void GLApiBase::glQueryCounterFn(GLuint id, GLenum target) {
  driver_->fn.glQueryCounterFn(id, target);
}

void GLApiBase::glReadBufferFn(GLenum src) {
  driver_->fn.glReadBufferFn(src);
}

void GLApiBase::glReadnPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           void* data) {
  driver_->fn.glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                         bufSize, length, data);
}

void GLApiBase::glReadPixelsFn(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               void* pixels) {
  driver_->fn.glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void GLApiBase::glReadPixelsRobustANGLEFn(GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          void* pixels) {
  driver_->fn.glReadPixelsRobustANGLEFn(x, y, width, height, format, type,
                                        bufSize, length, pixels);
}

void GLApiBase::glReleaseShaderCompilerFn(void) {
  driver_->fn.glReleaseShaderCompilerFn();
}

void GLApiBase::glRenderbufferStorageEXTFn(GLenum target,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) {
  driver_->fn.glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleFn(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleFn(target, samples,
                                                 internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleANGLEFn(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleANGLEFn(
      target, samples, internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleEXTFn(
      target, samples, internalformat, width, height);
}

void GLApiBase::glRenderbufferStorageMultisampleIMGFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  driver_->fn.glRenderbufferStorageMultisampleIMGFn(
      target, samples, internalformat, width, height);
}

void GLApiBase::glResumeTransformFeedbackFn(void) {
  driver_->fn.glResumeTransformFeedbackFn();
}

void GLApiBase::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  driver_->fn.glSampleCoverageFn(value, invert);
}

void GLApiBase::glSamplerParameterfFn(GLuint sampler,
                                      GLenum pname,
                                      GLfloat param) {
  driver_->fn.glSamplerParameterfFn(sampler, pname, param);
}

void GLApiBase::glSamplerParameterfvFn(GLuint sampler,
                                       GLenum pname,
                                       const GLfloat* params) {
  driver_->fn.glSamplerParameterfvFn(sampler, pname, params);
}

void GLApiBase::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLfloat* param) {
  driver_->fn.glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

void GLApiBase::glSamplerParameteriFn(GLuint sampler,
                                      GLenum pname,
                                      GLint param) {
  driver_->fn.glSamplerParameteriFn(sampler, pname, param);
}

void GLApiBase::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  driver_->fn.glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 param);
}

void GLApiBase::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLuint* param) {
  driver_->fn.glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  param);
}

void GLApiBase::glSamplerParameterivFn(GLuint sampler,
                                       GLenum pname,
                                       const GLint* params) {
  driver_->fn.glSamplerParameterivFn(sampler, pname, params);
}

void GLApiBase::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  const GLint* param) {
  driver_->fn.glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

void GLApiBase::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glScissorFn(x, y, width, height);
}

void GLApiBase::glSetFenceAPPLEFn(GLuint fence) {
  driver_->fn.glSetFenceAPPLEFn(fence);
}

void GLApiBase::glSetFenceNVFn(GLuint fence, GLenum condition) {
  driver_->fn.glSetFenceNVFn(fence, condition);
}

void GLApiBase::glShaderBinaryFn(GLsizei n,
                                 const GLuint* shaders,
                                 GLenum binaryformat,
                                 const void* binary,
                                 GLsizei length) {
  driver_->fn.glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void GLApiBase::glShaderSourceFn(GLuint shader,
                                 GLsizei count,
                                 const char* const* str,
                                 const GLint* length) {
  driver_->fn.glShaderSourceFn(shader, count, str, length);
}

void GLApiBase::glStencilFillPathInstancedNVFn(GLsizei numPaths,
                                               GLenum pathNameType,
                                               const void* paths,
                                               GLuint pathBase,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum transformType,
                                               const GLfloat* transformValues) {
  driver_->fn.glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                             pathBase, fillMode, mask,
                                             transformType, transformValues);
}

void GLApiBase::glStencilFillPathNVFn(GLuint path,
                                      GLenum fillMode,
                                      GLuint mask) {
  driver_->fn.glStencilFillPathNVFn(path, fillMode, mask);
}

void GLApiBase::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  driver_->fn.glStencilFuncFn(func, ref, mask);
}

void GLApiBase::glStencilFuncSeparateFn(GLenum face,
                                        GLenum func,
                                        GLint ref,
                                        GLuint mask) {
  driver_->fn.glStencilFuncSeparateFn(face, func, ref, mask);
}

void GLApiBase::glStencilMaskFn(GLuint mask) {
  driver_->fn.glStencilMaskFn(mask);
}

void GLApiBase::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  driver_->fn.glStencilMaskSeparateFn(face, mask);
}

void GLApiBase::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  driver_->fn.glStencilOpFn(fail, zfail, zpass);
}

void GLApiBase::glStencilOpSeparateFn(GLenum face,
                                      GLenum fail,
                                      GLenum zfail,
                                      GLenum zpass) {
  driver_->fn.glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void GLApiBase::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                               pathBase, ref, mask,
                                               transformType, transformValues);
}

void GLApiBase::glStencilStrokePathNVFn(GLuint path,
                                        GLint reference,
                                        GLuint mask) {
  driver_->fn.glStencilStrokePathNVFn(path, reference, mask);
}

void GLApiBase::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

void GLApiBase::glStencilThenCoverFillPathNVFn(GLuint path,
                                               GLenum fillMode,
                                               GLuint mask,
                                               GLenum coverMode) {
  driver_->fn.glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

void GLApiBase::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  driver_->fn.glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

void GLApiBase::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                 GLint reference,
                                                 GLuint mask,
                                                 GLenum coverMode) {
  driver_->fn.glStencilThenCoverStrokePathNVFn(path, reference, mask,
                                               coverMode);
}

GLboolean GLApiBase::glTestFenceAPPLEFn(GLuint fence) {
  return driver_->fn.glTestFenceAPPLEFn(fence);
}

GLboolean GLApiBase::glTestFenceNVFn(GLuint fence) {
  return driver_->fn.glTestFenceNVFn(fence);
}

void GLApiBase::glTexImage2DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage2DFn(target, level, internalformat, width, height,
                             border, format, type, pixels);
}

void GLApiBase::glTexImage2DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                        height, border, format, type, bufSize,
                                        pixels);
}

void GLApiBase::glTexImage3DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  driver_->fn.glTexImage3DFn(target, level, internalformat, width, height,
                             depth, border, format, type, pixels);
}

void GLApiBase::glTexImage3DRobustANGLEFn(GLenum target,
                                          GLint level,
                                          GLint internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLenum format,
                                          GLenum type,
                                          GLsizei bufSize,
                                          const void* pixels) {
  driver_->fn.glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                        height, depth, border, format, type,
                                        bufSize, pixels);
}

void GLApiBase::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  driver_->fn.glTexParameterfFn(target, pname, param);
}

void GLApiBase::glTexParameterfvFn(GLenum target,
                                   GLenum pname,
                                   const GLfloat* params) {
  driver_->fn.glTexParameterfvFn(target, pname, params);
}

void GLApiBase::glTexParameterfvRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLfloat* params) {
  driver_->fn.glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  driver_->fn.glTexParameteriFn(target, pname, param);
}

void GLApiBase::glTexParameterIivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  driver_->fn.glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLuint* params) {
  driver_->fn.glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexParameterivFn(GLenum target,
                                   GLenum pname,
                                   const GLint* params) {
  driver_->fn.glTexParameterivFn(target, pname, params);
}

void GLApiBase::glTexParameterivRobustANGLEFn(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              const GLint* params) {
  driver_->fn.glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

void GLApiBase::glTexStorage2DEXTFn(GLenum target,
                                    GLsizei levels,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height) {
  driver_->fn.glTexStorage2DEXTFn(target, levels, internalformat, width,
                                  height);
}

void GLApiBase::glTexStorage3DFn(GLenum target,
                                 GLsizei levels,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth) {
  driver_->fn.glTexStorage3DFn(target, levels, internalformat, width, height,
                               depth);
}

void GLApiBase::glTexSubImage2DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                                format, type, pixels);
}

void GLApiBase::glTexSubImage2DRobustANGLEFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             const void* pixels) {
  driver_->fn.glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset,
                                           width, height, format, type, bufSize,
                                           pixels);
}

void GLApiBase::glTexSubImage3DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  driver_->fn.glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                                height, depth, format, type, pixels);
}

void GLApiBase::glTexSubImage3DRobustANGLEFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLenum type,
                                             GLsizei bufSize,
                                             const void* pixels) {
  driver_->fn.glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                           zoffset, width, height, depth,
                                           format, type, bufSize, pixels);
}

void GLApiBase::glTransformFeedbackVaryingsFn(GLuint program,
                                              GLsizei count,
                                              const char* const* varyings,
                                              GLenum bufferMode) {
  driver_->fn.glTransformFeedbackVaryingsFn(program, count, varyings,
                                            bufferMode);
}

void GLApiBase::glUniform1fFn(GLint location, GLfloat x) {
  driver_->fn.glUniform1fFn(location, x);
}

void GLApiBase::glUniform1fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform1fvFn(location, count, v);
}

void GLApiBase::glUniform1iFn(GLint location, GLint x) {
  driver_->fn.glUniform1iFn(location, x);
}

void GLApiBase::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform1ivFn(location, count, v);
}

void GLApiBase::glUniform1uiFn(GLint location, GLuint v0) {
  driver_->fn.glUniform1uiFn(location, v0);
}

void GLApiBase::glUniform1uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform1uivFn(location, count, v);
}

void GLApiBase::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  driver_->fn.glUniform2fFn(location, x, y);
}

void GLApiBase::glUniform2fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform2fvFn(location, count, v);
}

void GLApiBase::glUniform2iFn(GLint location, GLint x, GLint y) {
  driver_->fn.glUniform2iFn(location, x, y);
}

void GLApiBase::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform2ivFn(location, count, v);
}

void GLApiBase::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  driver_->fn.glUniform2uiFn(location, v0, v1);
}

void GLApiBase::glUniform2uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform2uivFn(location, count, v);
}

void GLApiBase::glUniform3fFn(GLint location, GLfloat x, GLfloat y, GLfloat z) {
  driver_->fn.glUniform3fFn(location, x, y, z);
}

void GLApiBase::glUniform3fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform3fvFn(location, count, v);
}

void GLApiBase::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  driver_->fn.glUniform3iFn(location, x, y, z);
}

void GLApiBase::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform3ivFn(location, count, v);
}

void GLApiBase::glUniform3uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2) {
  driver_->fn.glUniform3uiFn(location, v0, v1, v2);
}

void GLApiBase::glUniform3uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform3uivFn(location, count, v);
}

void GLApiBase::glUniform4fFn(GLint location,
                              GLfloat x,
                              GLfloat y,
                              GLfloat z,
                              GLfloat w) {
  driver_->fn.glUniform4fFn(location, x, y, z, w);
}

void GLApiBase::glUniform4fvFn(GLint location,
                               GLsizei count,
                               const GLfloat* v) {
  driver_->fn.glUniform4fvFn(location, count, v);
}

void GLApiBase::glUniform4iFn(GLint location,
                              GLint x,
                              GLint y,
                              GLint z,
                              GLint w) {
  driver_->fn.glUniform4iFn(location, x, y, z, w);
}

void GLApiBase::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  driver_->fn.glUniform4ivFn(location, count, v);
}

void GLApiBase::glUniform4uiFn(GLint location,
                               GLuint v0,
                               GLuint v1,
                               GLuint v2,
                               GLuint v3) {
  driver_->fn.glUniform4uiFn(location, v0, v1, v2, v3);
}

void GLApiBase::glUniform4uivFn(GLint location,
                                GLsizei count,
                                const GLuint* v) {
  driver_->fn.glUniform4uivFn(location, count, v);
}

void GLApiBase::glUniformBlockBindingFn(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLuint uniformBlockBinding) {
  driver_->fn.glUniformBlockBindingFn(program, uniformBlockIndex,
                                      uniformBlockBinding);
}

void GLApiBase::glUniformMatrix2fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix2x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x3fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix2x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix2x4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix3fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix3x4fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix3x4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4fvFn(GLint location,
                                     GLsizei count,
                                     GLboolean transpose,
                                     const GLfloat* value) {
  driver_->fn.glUniformMatrix4fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4x2fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x2fvFn(location, count, transpose, value);
}

void GLApiBase::glUniformMatrix4x3fvFn(GLint location,
                                       GLsizei count,
                                       GLboolean transpose,
                                       const GLfloat* value) {
  driver_->fn.glUniformMatrix4x3fvFn(location, count, transpose, value);
}

GLboolean GLApiBase::glUnmapBufferFn(GLenum target) {
  return driver_->fn.glUnmapBufferFn(target);
}

void GLApiBase::glUseProgramFn(GLuint program) {
  driver_->fn.glUseProgramFn(program);
}

void GLApiBase::glValidateProgramFn(GLuint program) {
  driver_->fn.glValidateProgramFn(program);
}

void GLApiBase::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  driver_->fn.glVertexAttrib1fFn(indx, x);
}

void GLApiBase::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib1fvFn(indx, values);
}

void GLApiBase::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  driver_->fn.glVertexAttrib2fFn(indx, x, y);
}

void GLApiBase::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib2fvFn(indx, values);
}

void GLApiBase::glVertexAttrib3fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  driver_->fn.glVertexAttrib3fFn(indx, x, y, z);
}

void GLApiBase::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib3fvFn(indx, values);
}

void GLApiBase::glVertexAttrib4fFn(GLuint indx,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  driver_->fn.glVertexAttrib4fFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  driver_->fn.glVertexAttrib4fvFn(indx, values);
}

void GLApiBase::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  driver_->fn.glVertexAttribDivisorANGLEFn(index, divisor);
}

void GLApiBase::glVertexAttribI4iFn(GLuint indx,
                                    GLint x,
                                    GLint y,
                                    GLint z,
                                    GLint w) {
  driver_->fn.glVertexAttribI4iFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  driver_->fn.glVertexAttribI4ivFn(indx, values);
}

void GLApiBase::glVertexAttribI4uiFn(GLuint indx,
                                     GLuint x,
                                     GLuint y,
                                     GLuint z,
                                     GLuint w) {
  driver_->fn.glVertexAttribI4uiFn(indx, x, y, z, w);
}

void GLApiBase::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  driver_->fn.glVertexAttribI4uivFn(indx, values);
}

void GLApiBase::glVertexAttribIPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLsizei stride,
                                         const void* ptr) {
  driver_->fn.glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

void GLApiBase::glVertexAttribPointerFn(GLuint indx,
                                        GLint size,
                                        GLenum type,
                                        GLboolean normalized,
                                        GLsizei stride,
                                        const void* ptr) {
  driver_->fn.glVertexAttribPointerFn(indx, size, type, normalized, stride,
                                      ptr);
}

void GLApiBase::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  driver_->fn.glViewportFn(x, y, width, height);
}

GLenum GLApiBase::glWaitSyncFn(GLsync sync,
                               GLbitfield flags,
                               GLuint64 timeout) {
  return driver_->fn.glWaitSyncFn(sync, flags, timeout);
}

void TraceGLApi::glActiveTextureFn(GLenum texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glActiveTexture")
  gl_api_->glActiveTextureFn(texture);
}

void TraceGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glApplyFramebufferAttachmentCMAAINTEL")
  gl_api_->glApplyFramebufferAttachmentCMAAINTELFn();
}

void TraceGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glAttachShader")
  gl_api_->glAttachShaderFn(program, shader);
}

void TraceGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginQuery")
  gl_api_->glBeginQueryFn(target, id);
}

void TraceGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBeginTransformFeedback")
  gl_api_->glBeginTransformFeedbackFn(primitiveMode);
}

void TraceGLApi::glBindAttribLocationFn(GLuint program,
                                        GLuint index,
                                        const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindAttribLocation")
  gl_api_->glBindAttribLocationFn(program, index, name);
}

void TraceGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBuffer")
  gl_api_->glBindBufferFn(target, buffer);
}

void TraceGLApi::glBindBufferBaseFn(GLenum target,
                                    GLuint index,
                                    GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferBase")
  gl_api_->glBindBufferBaseFn(target, index, buffer);
}

void TraceGLApi::glBindBufferRangeFn(GLenum target,
                                     GLuint index,
                                     GLuint buffer,
                                     GLintptr offset,
                                     GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindBufferRange")
  gl_api_->glBindBufferRangeFn(target, index, buffer, offset, size);
}

void TraceGLApi::glBindFragDataLocationFn(GLuint program,
                                          GLuint colorNumber,
                                          const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFragDataLocation")
  gl_api_->glBindFragDataLocationFn(program, colorNumber, name);
}

void TraceGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                 GLuint colorNumber,
                                                 GLuint index,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindFragDataLocationIndexed")
  gl_api_->glBindFragDataLocationIndexedFn(program, colorNumber, index, name);
}

void TraceGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindFramebufferEXT")
  gl_api_->glBindFramebufferEXTFn(target, framebuffer);
}

void TraceGLApi::glBindImageTextureEXTFn(GLuint index,
                                         GLuint texture,
                                         GLint level,
                                         GLboolean layered,
                                         GLint layer,
                                         GLenum access,
                                         GLint format) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindImageTextureEXT")
  gl_api_->glBindImageTextureEXTFn(index, texture, level, layered, layer,
                                   access, format);
}

void TraceGLApi::glBindRenderbufferEXTFn(GLenum target, GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindRenderbufferEXT")
  gl_api_->glBindRenderbufferEXTFn(target, renderbuffer);
}

void TraceGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindSampler")
  gl_api_->glBindSamplerFn(unit, sampler);
}

void TraceGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTexture")
  gl_api_->glBindTextureFn(target, texture);
}

void TraceGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindTransformFeedback")
  gl_api_->glBindTransformFeedbackFn(target, id);
}

void TraceGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                 GLint location,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glBindUniformLocationCHROMIUM")
  gl_api_->glBindUniformLocationCHROMIUMFn(program, location, name);
}

void TraceGLApi::glBindVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBindVertexArrayOES")
  gl_api_->glBindVertexArrayOESFn(array);
}

void TraceGLApi::glBlendBarrierKHRFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendBarrierKHR")
  gl_api_->glBlendBarrierKHRFn();
}

void TraceGLApi::glBlendColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendColor")
  gl_api_->glBlendColorFn(red, green, blue, alpha);
}

void TraceGLApi::glBlendEquationFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquation")
  gl_api_->glBlendEquationFn(mode);
}

void TraceGLApi::glBlendEquationSeparateFn(GLenum modeRGB, GLenum modeAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendEquationSeparate")
  gl_api_->glBlendEquationSeparateFn(modeRGB, modeAlpha);
}

void TraceGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFunc")
  gl_api_->glBlendFuncFn(sfactor, dfactor);
}

void TraceGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                       GLenum dstRGB,
                                       GLenum srcAlpha,
                                       GLenum dstAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlendFuncSeparate")
  gl_api_->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void TraceGLApi::glBlitFramebufferFn(GLint srcX0,
                                     GLint srcY0,
                                     GLint srcX1,
                                     GLint srcY1,
                                     GLint dstX0,
                                     GLint dstY0,
                                     GLint dstX1,
                                     GLint dstY1,
                                     GLbitfield mask,
                                     GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlitFramebuffer")
  gl_api_->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                               dstY1, mask, filter);
}

void TraceGLApi::glBlitFramebufferANGLEFn(GLint srcX0,
                                          GLint srcY0,
                                          GLint srcX1,
                                          GLint srcY1,
                                          GLint dstX0,
                                          GLint dstY0,
                                          GLint dstX1,
                                          GLint dstY1,
                                          GLbitfield mask,
                                          GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlitFramebufferANGLE")
  gl_api_->glBlitFramebufferANGLEFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                    dstX1, dstY1, mask, filter);
}

void TraceGLApi::glBlitFramebufferEXTFn(GLint srcX0,
                                        GLint srcY0,
                                        GLint srcX1,
                                        GLint srcY1,
                                        GLint dstX0,
                                        GLint dstY0,
                                        GLint dstX1,
                                        GLint dstY1,
                                        GLbitfield mask,
                                        GLenum filter) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBlitFramebufferEXT")
  gl_api_->glBlitFramebufferEXTFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                                  dstX1, dstY1, mask, filter);
}

void TraceGLApi::glBufferDataFn(GLenum target,
                                GLsizeiptr size,
                                const void* data,
                                GLenum usage) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferData")
  gl_api_->glBufferDataFn(target, size, data, usage);
}

void TraceGLApi::glBufferSubDataFn(GLenum target,
                                   GLintptr offset,
                                   GLsizeiptr size,
                                   const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glBufferSubData")
  gl_api_->glBufferSubDataFn(target, offset, size, data);
}

GLenum TraceGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCheckFramebufferStatusEXT")
  return gl_api_->glCheckFramebufferStatusEXTFn(target);
}

void TraceGLApi::glClearFn(GLbitfield mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClear")
  gl_api_->glClearFn(mask);
}

void TraceGLApi::glClearBufferfiFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat depth,
                                   GLint stencil) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfi")
  gl_api_->glClearBufferfiFn(buffer, drawbuffer, depth, stencil);
}

void TraceGLApi::glClearBufferfvFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferfv")
  gl_api_->glClearBufferfvFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearBufferivFn(GLenum buffer,
                                   GLint drawbuffer,
                                   const GLint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferiv")
  gl_api_->glClearBufferivFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearBufferuivFn(GLenum buffer,
                                    GLint drawbuffer,
                                    const GLuint* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearBufferuiv")
  gl_api_->glClearBufferuivFn(buffer, drawbuffer, value);
}

void TraceGLApi::glClearColorFn(GLclampf red,
                                GLclampf green,
                                GLclampf blue,
                                GLclampf alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearColor")
  gl_api_->glClearColorFn(red, green, blue, alpha);
}

void TraceGLApi::glClearDepthFn(GLclampd depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepth")
  gl_api_->glClearDepthFn(depth);
}

void TraceGLApi::glClearDepthfFn(GLclampf depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearDepthf")
  gl_api_->glClearDepthfFn(depth);
}

void TraceGLApi::glClearStencilFn(GLint s) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClearStencil")
  gl_api_->glClearStencilFn(s);
}

GLenum TraceGLApi::glClientWaitSyncFn(GLsync sync,
                                      GLbitfield flags,
                                      GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glClientWaitSync")
  return gl_api_->glClientWaitSyncFn(sync, flags, timeout);
}

void TraceGLApi::glColorMaskFn(GLboolean red,
                               GLboolean green,
                               GLboolean blue,
                               GLboolean alpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glColorMask")
  gl_api_->glColorMaskFn(red, green, blue, alpha);
}

void TraceGLApi::glCompileShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompileShader")
  gl_api_->glCompileShaderFn(shader);
}

void TraceGLApi::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                   GLuint destId) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCompressedCopyTextureCHROMIUM")
  gl_api_->glCompressedCopyTextureCHROMIUMFn(sourceId, destId);
}

void TraceGLApi::glCompressedTexImage2DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage2D")
  gl_api_->glCompressedTexImage2DFn(target, level, internalformat, width,
                                    height, border, imageSize, data);
}

void TraceGLApi::glCompressedTexImage3DFn(GLenum target,
                                          GLint level,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height,
                                          GLsizei depth,
                                          GLint border,
                                          GLsizei imageSize,
                                          const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexImage3D")
  gl_api_->glCompressedTexImage3DFn(target, level, internalformat, width,
                                    height, depth, border, imageSize, data);
}

void TraceGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage2D")
  gl_api_->glCompressedTexSubImage2DFn(target, level, xoffset, yoffset, width,
                                       height, format, imageSize, data);
}

void TraceGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                             GLint level,
                                             GLint xoffset,
                                             GLint yoffset,
                                             GLint zoffset,
                                             GLsizei width,
                                             GLsizei height,
                                             GLsizei depth,
                                             GLenum format,
                                             GLsizei imageSize,
                                             const void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCompressedTexSubImage3D")
  gl_api_->glCompressedTexSubImage3DFn(target, level, xoffset, yoffset, zoffset,
                                       width, height, depth, format, imageSize,
                                       data);
}

void TraceGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                       GLenum writeTarget,
                                       GLintptr readOffset,
                                       GLintptr writeOffset,
                                       GLsizeiptr size) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyBufferSubData")
  gl_api_->glCopyBufferSubDataFn(readTarget, writeTarget, readOffset,
                                 writeOffset, size);
}

void TraceGLApi::glCopySubTextureCHROMIUMFn(GLuint sourceId,
                                            GLuint destId,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLboolean unpackFlipY,
                                            GLboolean unpackPremultiplyAlpha,
                                            GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopySubTextureCHROMIUM")
  gl_api_->glCopySubTextureCHROMIUMFn(
      sourceId, destId, xoffset, yoffset, x, y, width, height, unpackFlipY,
      unpackPremultiplyAlpha, unpackUnmultiplyAlpha);
}

void TraceGLApi::glCopyTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLenum internalformat,
                                    GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexImage2D")
  gl_api_->glCopyTexImage2DFn(target, level, internalformat, x, y, width,
                              height, border);
}

void TraceGLApi::glCopyTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage2D")
  gl_api_->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                                 height);
}

void TraceGLApi::glCopyTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTexSubImage3D")
  gl_api_->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                                 width, height);
}

void TraceGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                         GLuint destId,
                                         GLint internalFormat,
                                         GLenum destType,
                                         GLboolean unpackFlipY,
                                         GLboolean unpackPremultiplyAlpha,
                                         GLboolean unpackUnmultiplyAlpha) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCopyTextureCHROMIUM")
  gl_api_->glCopyTextureCHROMIUMFn(sourceId, destId, internalFormat, destType,
                                   unpackFlipY, unpackPremultiplyAlpha,
                                   unpackUnmultiplyAlpha);
}

void TraceGLApi::glCoverageModulationNVFn(GLenum components) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverageModulationNV")
  gl_api_->glCoverageModulationNVFn(components);
}

void TraceGLApi::glCoverFillPathInstancedNVFn(GLsizei numPaths,
                                              GLenum pathNameType,
                                              const void* paths,
                                              GLuint pathBase,
                                              GLenum coverMode,
                                              GLenum transformType,
                                              const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathInstancedNV")
  gl_api_->glCoverFillPathInstancedNVFn(numPaths, pathNameType, paths, pathBase,
                                        coverMode, transformType,
                                        transformValues);
}

void TraceGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverFillPathNV")
  gl_api_->glCoverFillPathNVFn(path, coverMode);
}

void TraceGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glCoverStrokePathInstancedNV")
  gl_api_->glCoverStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, coverMode, transformType,
                                          transformValues);
}

void TraceGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCoverStrokePathNV")
  gl_api_->glCoverStrokePathNVFn(name, coverMode);
}

GLuint TraceGLApi::glCreateProgramFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateProgram")
  return gl_api_->glCreateProgramFn();
}

GLuint TraceGLApi::glCreateShaderFn(GLenum type) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCreateShader")
  return gl_api_->glCreateShaderFn(type);
}

void TraceGLApi::glCullFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glCullFace")
  gl_api_->glCullFaceFn(mode);
}

void TraceGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                          const void* userParam) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageCallback")
  gl_api_->glDebugMessageCallbackFn(callback, userParam);
}

void TraceGLApi::glDebugMessageControlFn(GLenum source,
                                         GLenum type,
                                         GLenum severity,
                                         GLsizei count,
                                         const GLuint* ids,
                                         GLboolean enabled) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageControl")
  gl_api_->glDebugMessageControlFn(source, type, severity, count, ids, enabled);
}

void TraceGLApi::glDebugMessageInsertFn(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const char* buf) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDebugMessageInsert")
  gl_api_->glDebugMessageInsertFn(source, type, id, severity, length, buf);
}

void TraceGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteBuffersARB")
  gl_api_->glDeleteBuffersARBFn(n, buffers);
}

void TraceGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesAPPLE")
  gl_api_->glDeleteFencesAPPLEFn(n, fences);
}

void TraceGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFencesNV")
  gl_api_->glDeleteFencesNVFn(n, fences);
}

void TraceGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                           const GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteFramebuffersEXT")
  gl_api_->glDeleteFramebuffersEXTFn(n, framebuffers);
}

void TraceGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeletePathsNV")
  gl_api_->glDeletePathsNVFn(path, range);
}

void TraceGLApi::glDeleteProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteProgram")
  gl_api_->glDeleteProgramFn(program);
}

void TraceGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteQueries")
  gl_api_->glDeleteQueriesFn(n, ids);
}

void TraceGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                            const GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteRenderbuffersEXT")
  gl_api_->glDeleteRenderbuffersEXTFn(n, renderbuffers);
}

void TraceGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSamplers")
  gl_api_->glDeleteSamplersFn(n, samplers);
}

void TraceGLApi::glDeleteShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteShader")
  gl_api_->glDeleteShaderFn(shader);
}

void TraceGLApi::glDeleteSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteSync")
  gl_api_->glDeleteSyncFn(sync);
}

void TraceGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTextures")
  gl_api_->glDeleteTexturesFn(n, textures);
}

void TraceGLApi::glDeleteTransformFeedbacksFn(GLsizei n, const GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteTransformFeedbacks")
  gl_api_->glDeleteTransformFeedbacksFn(n, ids);
}

void TraceGLApi::glDeleteVertexArraysOESFn(GLsizei n, const GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDeleteVertexArraysOES")
  gl_api_->glDeleteVertexArraysOESFn(n, arrays);
}

void TraceGLApi::glDepthFuncFn(GLenum func) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthFunc")
  gl_api_->glDepthFuncFn(func);
}

void TraceGLApi::glDepthMaskFn(GLboolean flag) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthMask")
  gl_api_->glDepthMaskFn(flag);
}

void TraceGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRange")
  gl_api_->glDepthRangeFn(zNear, zFar);
}

void TraceGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDepthRangef")
  gl_api_->glDepthRangefFn(zNear, zFar);
}

void TraceGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDetachShader")
  gl_api_->glDetachShaderFn(program, shader);
}

void TraceGLApi::glDisableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisable")
  gl_api_->glDisableFn(cap);
}

void TraceGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDisableVertexAttribArray")
  gl_api_->glDisableVertexAttribArrayFn(index);
}

void TraceGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDiscardFramebufferEXT")
  gl_api_->glDiscardFramebufferEXTFn(target, numAttachments, attachments);
}

void TraceGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArrays")
  gl_api_->glDrawArraysFn(mode, first, count);
}

void TraceGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                              GLint first,
                                              GLsizei count,
                                              GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawArraysInstancedANGLE")
  gl_api_->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
}

void TraceGLApi::glDrawBufferFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffer")
  gl_api_->glDrawBufferFn(mode);
}

void TraceGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawBuffersARB")
  gl_api_->glDrawBuffersARBFn(n, bufs);
}

void TraceGLApi::glDrawElementsFn(GLenum mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawElements")
  gl_api_->glDrawElementsFn(mode, count, type, indices);
}

void TraceGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                GLsizei count,
                                                GLenum type,
                                                const void* indices,
                                                GLsizei primcount) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glDrawElementsInstancedANGLE")
  gl_api_->glDrawElementsInstancedANGLEFn(mode, count, type, indices,
                                          primcount);
}

void TraceGLApi::glDrawRangeElementsFn(GLenum mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glDrawRangeElements")
  gl_api_->glDrawRangeElementsFn(mode, start, end, count, type, indices);
}

void TraceGLApi::glEGLImageTargetRenderbufferStorageOESFn(GLenum target,
                                                          GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glEGLImageTargetRenderbufferStorageOES")
  gl_api_->glEGLImageTargetRenderbufferStorageOESFn(target, image);
}

void TraceGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                GLeglImageOES image) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glEGLImageTargetTexture2DOES")
  gl_api_->glEGLImageTargetTexture2DOESFn(target, image);
}

void TraceGLApi::glEnableFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnable")
  gl_api_->glEnableFn(cap);
}

void TraceGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEnableVertexAttribArray")
  gl_api_->glEnableVertexAttribArrayFn(index);
}

void TraceGLApi::glEndQueryFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndQuery")
  gl_api_->glEndQueryFn(target);
}

void TraceGLApi::glEndTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glEndTransformFeedback")
  gl_api_->glEndTransformFeedbackFn();
}

GLsync TraceGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFenceSync")
  return gl_api_->glFenceSyncFn(condition, flags);
}

void TraceGLApi::glFinishFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinish")
  gl_api_->glFinishFn();
}

void TraceGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceAPPLE")
  gl_api_->glFinishFenceAPPLEFn(fence);
}

void TraceGLApi::glFinishFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFinishFenceNV")
  gl_api_->glFinishFenceNVFn(fence);
}

void TraceGLApi::glFlushFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlush")
  gl_api_->glFlushFn();
}

void TraceGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                            GLintptr offset,
                                            GLsizeiptr length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFlushMappedBufferRange")
  gl_api_->glFlushMappedBufferRangeFn(target, offset, length);
}

void TraceGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                GLenum attachment,
                                                GLenum renderbuffertarget,
                                                GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glFramebufferRenderbufferEXT")
  gl_api_->glFramebufferRenderbufferEXTFn(target, attachment,
                                          renderbuffertarget, renderbuffer);
}

void TraceGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                             GLenum attachment,
                                             GLenum textarget,
                                             GLuint texture,
                                             GLint level) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTexture2DEXT")
  gl_api_->glFramebufferTexture2DEXTFn(target, attachment, textarget, texture,
                                       level);
}

void TraceGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glFramebufferTexture2DMultisampleEXT")
  gl_api_->glFramebufferTexture2DMultisampleEXTFn(target, attachment, textarget,
                                                  texture, level, samples);
}

void TraceGLApi::glFramebufferTexture2DMultisampleIMGFn(GLenum target,
                                                        GLenum attachment,
                                                        GLenum textarget,
                                                        GLuint texture,
                                                        GLint level,
                                                        GLsizei samples) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glFramebufferTexture2DMultisampleIMG")
  gl_api_->glFramebufferTexture2DMultisampleIMGFn(target, attachment, textarget,
                                                  texture, level, samples);
}

void TraceGLApi::glFramebufferTextureLayerFn(GLenum target,
                                             GLenum attachment,
                                             GLuint texture,
                                             GLint level,
                                             GLint layer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFramebufferTextureLayer")
  gl_api_->glFramebufferTextureLayerFn(target, attachment, texture, level,
                                       layer);
}

void TraceGLApi::glFrontFaceFn(GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glFrontFace")
  gl_api_->glFrontFaceFn(mode);
}

void TraceGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenBuffersARB")
  gl_api_->glGenBuffersARBFn(n, buffers);
}

void TraceGLApi::glGenerateMipmapEXTFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenerateMipmapEXT")
  gl_api_->glGenerateMipmapEXTFn(target);
}

void TraceGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesAPPLE")
  gl_api_->glGenFencesAPPLEFn(n, fences);
}

void TraceGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFencesNV")
  gl_api_->glGenFencesNVFn(n, fences);
}

void TraceGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenFramebuffersEXT")
  gl_api_->glGenFramebuffersEXTFn(n, framebuffers);
}

GLuint TraceGLApi::glGenPathsNVFn(GLsizei range) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenPathsNV")
  return gl_api_->glGenPathsNVFn(range);
}

void TraceGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenQueries")
  gl_api_->glGenQueriesFn(n, ids);
}

void TraceGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenRenderbuffersEXT")
  gl_api_->glGenRenderbuffersEXTFn(n, renderbuffers);
}

void TraceGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenSamplers")
  gl_api_->glGenSamplersFn(n, samplers);
}

void TraceGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTextures")
  gl_api_->glGenTexturesFn(n, textures);
}

void TraceGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenTransformFeedbacks")
  gl_api_->glGenTransformFeedbacksFn(n, ids);
}

void TraceGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGenVertexArraysOES")
  gl_api_->glGenVertexArraysOESFn(n, arrays);
}

void TraceGLApi::glGetActiveAttribFn(GLuint program,
                                     GLuint index,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     GLint* size,
                                     GLenum* type,
                                     char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveAttrib")
  gl_api_->glGetActiveAttribFn(program, index, bufsize, length, size, type,
                               name);
}

void TraceGLApi::glGetActiveUniformFn(GLuint program,
                                      GLuint index,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      GLint* size,
                                      GLenum* type,
                                      char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniform")
  gl_api_->glGetActiveUniformFn(program, index, bufsize, length, size, type,
                                name);
}

void TraceGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLenum pname,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformBlockiv")
  gl_api_->glGetActiveUniformBlockivFn(program, uniformBlockIndex, pname,
                                       params);
}

void TraceGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetActiveUniformBlockivRobustANGLE")
  gl_api_->glGetActiveUniformBlockivRobustANGLEFn(
      program, uniformBlockIndex, pname, bufSize, length, params);
}

void TraceGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                               GLuint uniformBlockIndex,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetActiveUniformBlockName")
  gl_api_->glGetActiveUniformBlockNameFn(program, uniformBlockIndex, bufSize,
                                         length, uniformBlockName);
}

void TraceGLApi::glGetActiveUniformsivFn(GLuint program,
                                         GLsizei uniformCount,
                                         const GLuint* uniformIndices,
                                         GLenum pname,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetActiveUniformsiv")
  gl_api_->glGetActiveUniformsivFn(program, uniformCount, uniformIndices, pname,
                                   params);
}

void TraceGLApi::glGetAttachedShadersFn(GLuint program,
                                        GLsizei maxcount,
                                        GLsizei* count,
                                        GLuint* shaders) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttachedShaders")
  gl_api_->glGetAttachedShadersFn(program, maxcount, count, shaders);
}

GLint TraceGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetAttribLocation")
  return gl_api_->glGetAttribLocationFn(program, name);
}

void TraceGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleani_vRobustANGLE")
  gl_api_->glGetBooleani_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanv")
  gl_api_->glGetBooleanvFn(pname, params);
}

void TraceGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLboolean* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBooleanvRobustANGLE")
  gl_api_->glGetBooleanvRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetBufferParameteri64vRobustANGLE")
  gl_api_->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufSize, length,
                                                 params);
}

void TraceGLApi::glGetBufferParameterivFn(GLenum target,
                                          GLenum pname,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetBufferParameteriv")
  gl_api_->glGetBufferParameterivFn(target, pname, params);
}

void TraceGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferParameterivRobustANGLE")
  gl_api_->glGetBufferParameterivRobustANGLEFn(target, pname, bufSize, length,
                                               params);
}

void TraceGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetBufferPointervRobustANGLE")
  gl_api_->glGetBufferPointervRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetDebugMessageLogFn(GLuint count,
                                        GLsizei bufSize,
                                        GLenum* sources,
                                        GLenum* types,
                                        GLuint* ids,
                                        GLenum* severities,
                                        GLsizei* lengths,
                                        char* messageLog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetDebugMessageLog")
  gl_api_->glGetDebugMessageLogFn(count, bufSize, sources, types, ids,
                                  severities, lengths, messageLog);
}

GLenum TraceGLApi::glGetErrorFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetError")
  return gl_api_->glGetErrorFn();
}

void TraceGLApi::glGetFenceivNVFn(GLuint fence, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFenceivNV")
  gl_api_->glGetFenceivNVFn(fence, pname, params);
}

void TraceGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatv")
  gl_api_->glGetFloatvFn(pname, params);
}

void TraceGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLfloat* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFloatvRobustANGLE")
  gl_api_->glGetFloatvRobustANGLEFn(pname, bufSize, length, data);
}

GLint TraceGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataIndex")
  return gl_api_->glGetFragDataIndexFn(program, name);
}

GLint TraceGLApi::glGetFragDataLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetFragDataLocation")
  return gl_api_->glGetFragDataLocationFn(program, name);
}

void TraceGLApi::glGetFramebufferAttachmentParameterivEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum pname,
                                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivEXT")
  gl_api_->glGetFramebufferAttachmentParameterivEXTFn(target, attachment, pname,
                                                      params);
}

void TraceGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferAttachmentParameterivRobustANGLE")
  gl_api_->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, attachment, pname, bufSize, length, params);
}

void TraceGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetFramebufferParameterivRobustANGLE")
  gl_api_->glGetFramebufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                    length, params);
}

GLenum TraceGLApi::glGetGraphicsResetStatusARBFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetGraphicsResetStatusARB")
  return gl_api_->glGetGraphicsResetStatusARBFn();
}

void TraceGLApi::glGetInteger64i_vFn(GLenum target,
                                     GLuint index,
                                     GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64i_v")
  gl_api_->glGetInteger64i_vFn(target, index, data);
}

void TraceGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInteger64i_vRobustANGLE")
  gl_api_->glGetInteger64i_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64v")
  gl_api_->glGetInteger64vFn(pname, params);
}

void TraceGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint64* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInteger64vRobustANGLE")
  gl_api_->glGetInteger64vRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetIntegeri_vFn(GLenum target, GLuint index, GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_v")
  gl_api_->glGetIntegeri_vFn(target, index, data);
}

void TraceGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                              GLuint index,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegeri_vRobustANGLE")
  gl_api_->glGetIntegeri_vRobustANGLEFn(target, index, bufSize, length, data);
}

void TraceGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegerv")
  gl_api_->glGetIntegervFn(pname, params);
}

void TraceGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetIntegervRobustANGLE")
  gl_api_->glGetIntegervRobustANGLEFn(pname, bufSize, length, data);
}

void TraceGLApi::glGetInternalformativFn(GLenum target,
                                         GLenum internalformat,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetInternalformativ")
  gl_api_->glGetInternalformativFn(target, internalformat, pname, bufSize,
                                   params);
}

void TraceGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                    GLenum internalformat,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetInternalformativRobustANGLE")
  gl_api_->glGetInternalformativRobustANGLEFn(target, internalformat, pname,
                                              bufSize, length, params);
}

void TraceGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* val) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetMultisamplefvRobustANGLE")
  gl_api_->glGetMultisamplefvRobustANGLEFn(pname, index, bufSize, length, val);
}

void TraceGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformfvRobustANGLE")
  gl_api_->glGetnUniformfvRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetnUniformivRobustANGLE")
  gl_api_->glGetnUniformivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                               GLint location,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetnUniformuivRobustANGLE")
  gl_api_->glGetnUniformuivRobustANGLEFn(program, location, bufSize, length,
                                         params);
}

void TraceGLApi::glGetObjectLabelFn(GLenum identifier,
                                    GLuint name,
                                    GLsizei bufSize,
                                    GLsizei* length,
                                    char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectLabel")
  gl_api_->glGetObjectLabelFn(identifier, name, bufSize, length, label);
}

void TraceGLApi::glGetObjectPtrLabelFn(void* ptr,
                                       GLsizei bufSize,
                                       GLsizei* length,
                                       char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetObjectPtrLabel")
  gl_api_->glGetObjectPtrLabelFn(ptr, bufSize, length, label);
}

void TraceGLApi::glGetPointervFn(GLenum pname, void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetPointerv")
  gl_api_->glGetPointervFn(pname, params);
}

void TraceGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       void** params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetPointervRobustANGLERobustANGLE")
  gl_api_->glGetPointervRobustANGLERobustANGLEFn(pname, bufSize, length,
                                                 params);
}

void TraceGLApi::glGetProgramBinaryFn(GLuint program,
                                      GLsizei bufSize,
                                      GLsizei* length,
                                      GLenum* binaryFormat,
                                      GLvoid* binary) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramBinary")
  gl_api_->glGetProgramBinaryFn(program, bufSize, length, binaryFormat, binary);
}

void TraceGLApi::glGetProgramInfoLogFn(GLuint program,
                                       GLsizei bufsize,
                                       GLsizei* length,
                                       char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInfoLog")
  gl_api_->glGetProgramInfoLogFn(program, bufsize, length, infolog);
}

void TraceGLApi::glGetProgramInterfaceivFn(GLuint program,
                                           GLenum programInterface,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramInterfaceiv")
  gl_api_->glGetProgramInterfaceivFn(program, programInterface, pname, params);
}

void TraceGLApi::glGetProgramInterfaceivRobustANGLEFn(GLuint program,
                                                      GLenum programInterface,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetProgramInterfaceivRobustANGLE")
  gl_api_->glGetProgramInterfaceivRobustANGLEFn(program, programInterface,
                                                pname, bufSize, length, params);
}

void TraceGLApi::glGetProgramivFn(GLuint program, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramiv")
  gl_api_->glGetProgramivFn(program, pname, params);
}

void TraceGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramivRobustANGLE")
  gl_api_->glGetProgramivRobustANGLEFn(program, pname, bufSize, length, params);
}

void TraceGLApi::glGetProgramResourceivFn(GLuint program,
                                          GLenum programInterface,
                                          GLuint index,
                                          GLsizei propCount,
                                          const GLenum* props,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceiv")
  gl_api_->glGetProgramResourceivFn(program, programInterface, index, propCount,
                                    props, bufSize, length, params);
}

GLint TraceGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                 GLenum programInterface,
                                                 const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetProgramResourceLocation")
  return gl_api_->glGetProgramResourceLocationFn(program, programInterface,
                                                 name);
}

void TraceGLApi::glGetProgramResourceNameFn(GLuint program,
                                            GLenum programInterface,
                                            GLuint index,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLchar* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetProgramResourceName")
  gl_api_->glGetProgramResourceNameFn(program, programInterface, index, bufSize,
                                      length, name);
}

void TraceGLApi::glGetQueryivFn(GLenum target, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryiv")
  gl_api_->glGetQueryivFn(target, pname, params);
}

void TraceGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryivRobustANGLE")
  gl_api_->glGetQueryivRobustANGLEFn(target, pname, bufSize, length, params);
}

void TraceGLApi::glGetQueryObjecti64vFn(GLuint id,
                                        GLenum pname,
                                        GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjecti64v")
  gl_api_->glGetQueryObjecti64vFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjecti64vRobustANGLE")
  gl_api_->glGetQueryObjecti64vRobustANGLEFn(id, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetQueryObjectivFn(GLuint id, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectiv")
  gl_api_->glGetQueryObjectivFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectivRobustANGLE")
  gl_api_->glGetQueryObjectivRobustANGLEFn(id, pname, bufSize, length, params);
}

void TraceGLApi::glGetQueryObjectui64vFn(GLuint id,
                                         GLenum pname,
                                         GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectui64v")
  gl_api_->glGetQueryObjectui64vFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint64* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectui64vRobustANGLE")
  gl_api_->glGetQueryObjectui64vRobustANGLEFn(id, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetQueryObjectuivFn(GLuint id,
                                       GLenum pname,
                                       GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetQueryObjectuiv")
  gl_api_->glGetQueryObjectuivFn(id, pname, params);
}

void TraceGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetQueryObjectuivRobustANGLE")
  gl_api_->glGetQueryObjectuivRobustANGLEFn(id, pname, bufSize, length, params);
}

void TraceGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                   GLenum pname,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetRenderbufferParameterivEXT")
  gl_api_->glGetRenderbufferParameterivEXTFn(target, pname, params);
}

void TraceGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetRenderbufferParameterivRobustANGLE")
  gl_api_->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufSize,
                                                     length, params);
}

void TraceGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameterfv")
  gl_api_->glGetSamplerParameterfvFn(sampler, pname, params);
}

void TraceGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterfvRobustANGLE")
  gl_api_->glGetSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void TraceGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIivRobustANGLE")
  gl_api_->glGetSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterIuivRobustANGLE")
  gl_api_->glGetSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize,
                                                  length, params);
}

void TraceGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                           GLenum pname,
                                           GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSamplerParameteriv")
  gl_api_->glGetSamplerParameterivFn(sampler, pname, params);
}

void TraceGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetSamplerParameterivRobustANGLE")
  gl_api_->glGetSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, length,
                                                params);
}

void TraceGLApi::glGetShaderInfoLogFn(GLuint shader,
                                      GLsizei bufsize,
                                      GLsizei* length,
                                      char* infolog) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderInfoLog")
  gl_api_->glGetShaderInfoLogFn(shader, bufsize, length, infolog);
}

void TraceGLApi::glGetShaderivFn(GLuint shader, GLenum pname, GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderiv")
  gl_api_->glGetShaderivFn(shader, pname, params);
}

void TraceGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderivRobustANGLE")
  gl_api_->glGetShaderivRobustANGLEFn(shader, pname, bufSize, length, params);
}

void TraceGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                              GLenum precisiontype,
                                              GLint* range,
                                              GLint* precision) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderPrecisionFormat")
  gl_api_->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                        precision);
}

void TraceGLApi::glGetShaderSourceFn(GLuint shader,
                                     GLsizei bufsize,
                                     GLsizei* length,
                                     char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetShaderSource")
  gl_api_->glGetShaderSourceFn(shader, bufsize, length, source);
}

const GLubyte* TraceGLApi::glGetStringFn(GLenum name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetString")
  return gl_api_->glGetStringFn(name);
}

const GLubyte* TraceGLApi::glGetStringiFn(GLenum name, GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetStringi")
  return gl_api_->glGetStringiFn(name, index);
}

void TraceGLApi::glGetSyncivFn(GLsync sync,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei* length,
                               GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetSynciv")
  gl_api_->glGetSyncivFn(sync, pname, bufSize, length, values);
}

void TraceGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameterfv")
  gl_api_->glGetTexLevelParameterfvFn(target, level, pname, params);
}

void TraceGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterfvRobustANGLE")
  gl_api_->glGetTexLevelParameterfvRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetTexLevelParameterivFn(GLenum target,
                                            GLint level,
                                            GLenum pname,
                                            GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexLevelParameteriv")
  gl_api_->glGetTexLevelParameterivFn(target, level, pname, params);
}

void TraceGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                       GLint level,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetTexLevelParameterivRobustANGLE")
  gl_api_->glGetTexLevelParameterivRobustANGLEFn(target, level, pname, bufSize,
                                                 length, params);
}

void TraceGLApi::glGetTexParameterfvFn(GLenum target,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameterfv")
  gl_api_->glGetTexParameterfvFn(target, pname, params);
}

void TraceGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterfvRobustANGLE")
  gl_api_->glGetTexParameterfvRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIivRobustANGLE")
  gl_api_->glGetTexParameterIivRobustANGLEFn(target, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterIuivRobustANGLE")
  gl_api_->glGetTexParameterIuivRobustANGLEFn(target, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetTexParameterivFn(GLenum target,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetTexParameteriv")
  gl_api_->glGetTexParameterivFn(target, pname, params);
}

void TraceGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTexParameterivRobustANGLE")
  gl_api_->glGetTexParameterivRobustANGLEFn(target, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLsizei* size,
                                                 GLenum* type,
                                                 char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTransformFeedbackVarying")
  gl_api_->glGetTransformFeedbackVaryingFn(program, index, bufSize, length,
                                           size, type, name);
}

void TraceGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* source) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetTranslatedShaderSourceANGLE")
  gl_api_->glGetTranslatedShaderSourceANGLEFn(shader, bufsize, length, source);
}

GLuint TraceGLApi::glGetUniformBlockIndexFn(GLuint program,
                                            const char* uniformBlockName) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformBlockIndex")
  return gl_api_->glGetUniformBlockIndexFn(program, uniformBlockName);
}

void TraceGLApi::glGetUniformfvFn(GLuint program,
                                  GLint location,
                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfv")
  gl_api_->glGetUniformfvFn(program, location, params);
}

void TraceGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformfvRobustANGLE")
  gl_api_->glGetUniformfvRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

void TraceGLApi::glGetUniformIndicesFn(GLuint program,
                                       GLsizei uniformCount,
                                       const char* const* uniformNames,
                                       GLuint* uniformIndices) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformIndices")
  gl_api_->glGetUniformIndicesFn(program, uniformCount, uniformNames,
                                 uniformIndices);
}

void TraceGLApi::glGetUniformivFn(GLuint program,
                                  GLint location,
                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformiv")
  gl_api_->glGetUniformivFn(program, location, params);
}

void TraceGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                             GLint location,
                                             GLsizei bufSize,
                                             GLsizei* length,
                                             GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformivRobustANGLE")
  gl_api_->glGetUniformivRobustANGLEFn(program, location, bufSize, length,
                                       params);
}

GLint TraceGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformLocation")
  return gl_api_->glGetUniformLocationFn(program, name);
}

void TraceGLApi::glGetUniformuivFn(GLuint program,
                                   GLint location,
                                   GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuiv")
  gl_api_->glGetUniformuivFn(program, location, params);
}

void TraceGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                              GLint location,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetUniformuivRobustANGLE")
  gl_api_->glGetUniformuivRobustANGLEFn(program, location, bufSize, length,
                                        params);
}

void TraceGLApi::glGetVertexAttribfvFn(GLuint index,
                                       GLenum pname,
                                       GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribfv")
  gl_api_->glGetVertexAttribfvFn(index, pname, params);
}

void TraceGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribfvRobustANGLE")
  gl_api_->glGetVertexAttribfvRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIivRobustANGLE")
  gl_api_->glGetVertexAttribIivRobustANGLEFn(index, pname, bufSize, length,
                                             params);
}

void TraceGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribIuivRobustANGLE")
  gl_api_->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufSize, length,
                                              params);
}

void TraceGLApi::glGetVertexAttribivFn(GLuint index,
                                       GLenum pname,
                                       GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribiv")
  gl_api_->glGetVertexAttribivFn(index, pname, params);
}

void TraceGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                  GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glGetVertexAttribivRobustANGLE")
  gl_api_->glGetVertexAttribivRobustANGLEFn(index, pname, bufSize, length,
                                            params);
}

void TraceGLApi::glGetVertexAttribPointervFn(GLuint index,
                                             GLenum pname,
                                             void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glGetVertexAttribPointerv")
  gl_api_->glGetVertexAttribPointervFn(index, pname, pointer);
}

void TraceGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        void** pointer) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glGetVertexAttribPointervRobustANGLE")
  gl_api_->glGetVertexAttribPointervRobustANGLEFn(index, pname, bufSize, length,
                                                  pointer);
}

void TraceGLApi::glHintFn(GLenum target, GLenum mode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glHint")
  gl_api_->glHintFn(target, mode);
}

void TraceGLApi::glInsertEventMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInsertEventMarkerEXT")
  gl_api_->glInsertEventMarkerEXTFn(length, marker);
}

void TraceGLApi::glInvalidateFramebufferFn(GLenum target,
                                           GLsizei numAttachments,
                                           const GLenum* attachments) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateFramebuffer")
  gl_api_->glInvalidateFramebufferFn(target, numAttachments, attachments);
}

void TraceGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments,
                                              GLint x,
                                              GLint y,
                                              GLint width,
                                              GLint height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glInvalidateSubFramebuffer")
  gl_api_->glInvalidateSubFramebufferFn(target, numAttachments, attachments, x,
                                        y, width, height);
}

GLboolean TraceGLApi::glIsBufferFn(GLuint buffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsBuffer")
  return gl_api_->glIsBufferFn(buffer);
}

GLboolean TraceGLApi::glIsEnabledFn(GLenum cap) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsEnabled")
  return gl_api_->glIsEnabledFn(cap);
}

GLboolean TraceGLApi::glIsFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceAPPLE")
  return gl_api_->glIsFenceAPPLEFn(fence);
}

GLboolean TraceGLApi::glIsFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFenceNV")
  return gl_api_->glIsFenceNVFn(fence);
}

GLboolean TraceGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsFramebufferEXT")
  return gl_api_->glIsFramebufferEXTFn(framebuffer);
}

GLboolean TraceGLApi::glIsPathNVFn(GLuint path) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsPathNV")
  return gl_api_->glIsPathNVFn(path);
}

GLboolean TraceGLApi::glIsProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsProgram")
  return gl_api_->glIsProgramFn(program);
}

GLboolean TraceGLApi::glIsQueryFn(GLuint query) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsQuery")
  return gl_api_->glIsQueryFn(query);
}

GLboolean TraceGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsRenderbufferEXT")
  return gl_api_->glIsRenderbufferEXTFn(renderbuffer);
}

GLboolean TraceGLApi::glIsSamplerFn(GLuint sampler) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSampler")
  return gl_api_->glIsSamplerFn(sampler);
}

GLboolean TraceGLApi::glIsShaderFn(GLuint shader) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsShader")
  return gl_api_->glIsShaderFn(shader);
}

GLboolean TraceGLApi::glIsSyncFn(GLsync sync) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsSync")
  return gl_api_->glIsSyncFn(sync);
}

GLboolean TraceGLApi::glIsTextureFn(GLuint texture) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTexture")
  return gl_api_->glIsTextureFn(texture);
}

GLboolean TraceGLApi::glIsTransformFeedbackFn(GLuint id) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsTransformFeedback")
  return gl_api_->glIsTransformFeedbackFn(id);
}

GLboolean TraceGLApi::glIsVertexArrayOESFn(GLuint array) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glIsVertexArrayOES")
  return gl_api_->glIsVertexArrayOESFn(array);
}

void TraceGLApi::glLineWidthFn(GLfloat width) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLineWidth")
  gl_api_->glLineWidthFn(width);
}

void TraceGLApi::glLinkProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glLinkProgram")
  gl_api_->glLinkProgramFn(program);
}

void* TraceGLApi::glMapBufferFn(GLenum target, GLenum access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBuffer")
  return gl_api_->glMapBufferFn(target, access);
}

void* TraceGLApi::glMapBufferRangeFn(GLenum target,
                                     GLintptr offset,
                                     GLsizeiptr length,
                                     GLbitfield access) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMapBufferRange")
  return gl_api_->glMapBufferRangeFn(target, offset, length, access);
}

void TraceGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadfEXT")
  gl_api_->glMatrixLoadfEXTFn(matrixMode, m);
}

void TraceGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMatrixLoadIdentityEXT")
  gl_api_->glMatrixLoadIdentityEXTFn(matrixMode);
}

void TraceGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glMemoryBarrierEXT")
  gl_api_->glMemoryBarrierEXTFn(barriers);
}

void TraceGLApi::glObjectLabelFn(GLenum identifier,
                                 GLuint name,
                                 GLsizei length,
                                 const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectLabel")
  gl_api_->glObjectLabelFn(identifier, name, length, label);
}

void TraceGLApi::glObjectPtrLabelFn(void* ptr,
                                    GLsizei length,
                                    const char* label) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glObjectPtrLabel")
  gl_api_->glObjectPtrLabelFn(ptr, length, label);
}

void TraceGLApi::glPathCommandsNVFn(GLuint path,
                                    GLsizei numCommands,
                                    const GLubyte* commands,
                                    GLsizei numCoords,
                                    GLenum coordType,
                                    const GLvoid* coords) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathCommandsNV")
  gl_api_->glPathCommandsNVFn(path, numCommands, commands, numCoords, coordType,
                              coords);
}

void TraceGLApi::glPathParameterfNVFn(GLuint path,
                                      GLenum pname,
                                      GLfloat value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameterfNV")
  gl_api_->glPathParameterfNVFn(path, pname, value);
}

void TraceGLApi::glPathParameteriNVFn(GLuint path, GLenum pname, GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathParameteriNV")
  gl_api_->glPathParameteriNVFn(path, pname, value);
}

void TraceGLApi::glPathStencilFuncNVFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPathStencilFuncNV")
  gl_api_->glPathStencilFuncNVFn(func, ref, mask);
}

void TraceGLApi::glPauseTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPauseTransformFeedback")
  gl_api_->glPauseTransformFeedbackFn();
}

void TraceGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPixelStorei")
  gl_api_->glPixelStoreiFn(pname, param);
}

void TraceGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPointParameteri")
  gl_api_->glPointParameteriFn(pname, param);
}

void TraceGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPolygonOffset")
  gl_api_->glPolygonOffsetFn(factor, units);
}

void TraceGLApi::glPopDebugGroupFn() {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopDebugGroup")
  gl_api_->glPopDebugGroupFn();
}

void TraceGLApi::glPopGroupMarkerEXTFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPopGroupMarkerEXT")
  gl_api_->glPopGroupMarkerEXTFn();
}

void TraceGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPrimitiveRestartIndex")
  gl_api_->glPrimitiveRestartIndexFn(index);
}

void TraceGLApi::glProgramBinaryFn(GLuint program,
                                   GLenum binaryFormat,
                                   const GLvoid* binary,
                                   GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramBinary")
  gl_api_->glProgramBinaryFn(program, binaryFormat, binary, length);
}

void TraceGLApi::glProgramParameteriFn(GLuint program,
                                       GLenum pname,
                                       GLint value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glProgramParameteri")
  gl_api_->glProgramParameteriFn(program, pname, value);
}

void TraceGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                   GLint location,
                                                   GLenum genMode,
                                                   GLint components,
                                                   const GLfloat* coeffs) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glProgramPathFragmentInputGenNV")
  gl_api_->glProgramPathFragmentInputGenNVFn(program, location, genMode,
                                             components, coeffs);
}

void TraceGLApi::glPushDebugGroupFn(GLenum source,
                                    GLuint id,
                                    GLsizei length,
                                    const char* message) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushDebugGroup")
  gl_api_->glPushDebugGroupFn(source, id, length, message);
}

void TraceGLApi::glPushGroupMarkerEXTFn(GLsizei length, const char* marker) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glPushGroupMarkerEXT")
  gl_api_->glPushGroupMarkerEXTFn(length, marker);
}

void TraceGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glQueryCounter")
  gl_api_->glQueryCounterFn(id, target);
}

void TraceGLApi::glReadBufferFn(GLenum src) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadBuffer")
  gl_api_->glReadBufferFn(src);
}

void TraceGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                            GLint y,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            GLsizei bufSize,
                                            GLsizei* length,
                                            void* data) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadnPixelsRobustANGLE")
  gl_api_->glReadnPixelsRobustANGLEFn(x, y, width, height, format, type,
                                      bufSize, length, data);
}

void TraceGLApi::glReadPixelsFn(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixels")
  gl_api_->glReadPixelsFn(x, y, width, height, format, type, pixels);
}

void TraceGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReadPixelsRobustANGLE")
  gl_api_->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufSize,
                                     length, pixels);
}

void TraceGLApi::glReleaseShaderCompilerFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glReleaseShaderCompiler")
  gl_api_->glReleaseShaderCompilerFn();
}

void TraceGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glRenderbufferStorageEXT")
  gl_api_->glRenderbufferStorageEXTFn(target, internalformat, width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                    GLsizei samples,
                                                    GLenum internalformat,
                                                    GLsizei width,
                                                    GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glRenderbufferStorageMultisample")
  gl_api_->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                              width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleANGLEFn(GLenum target,
                                                         GLsizei samples,
                                                         GLenum internalformat,
                                                         GLsizei width,
                                                         GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glRenderbufferStorageMultisampleANGLE")
  gl_api_->glRenderbufferStorageMultisampleANGLEFn(
      target, samples, internalformat, width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glRenderbufferStorageMultisampleEXT")
  gl_api_->glRenderbufferStorageMultisampleEXTFn(target, samples,
                                                 internalformat, width, height);
}

void TraceGLApi::glRenderbufferStorageMultisampleIMGFn(GLenum target,
                                                       GLsizei samples,
                                                       GLenum internalformat,
                                                       GLsizei width,
                                                       GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glRenderbufferStorageMultisampleIMG")
  gl_api_->glRenderbufferStorageMultisampleIMGFn(target, samples,
                                                 internalformat, width, height);
}

void TraceGLApi::glResumeTransformFeedbackFn(void) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glResumeTransformFeedback")
  gl_api_->glResumeTransformFeedbackFn();
}

void TraceGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSampleCoverage")
  gl_api_->glSampleCoverageFn(value, invert);
}

void TraceGLApi::glSamplerParameterfFn(GLuint sampler,
                                       GLenum pname,
                                       GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterf")
  gl_api_->glSamplerParameterfFn(sampler, pname, param);
}

void TraceGLApi::glSamplerParameterfvFn(GLuint sampler,
                                        GLenum pname,
                                        const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameterfv")
  gl_api_->glSamplerParameterfvFn(sampler, pname, params);
}

void TraceGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterfvRobustANGLE")
  gl_api_->glSamplerParameterfvRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameteriFn(GLuint sampler,
                                       GLenum pname,
                                       GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteri")
  gl_api_->glSamplerParameteriFn(sampler, pname, param);
}

void TraceGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIivRobustANGLE")
  gl_api_->glSamplerParameterIivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterIuivRobustANGLE")
  gl_api_->glSamplerParameterIuivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glSamplerParameterivFn(GLuint sampler,
                                        GLenum pname,
                                        const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSamplerParameteriv")
  gl_api_->glSamplerParameterivFn(sampler, pname, params);
}

void TraceGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glSamplerParameterivRobustANGLE")
  gl_api_->glSamplerParameterivRobustANGLEFn(sampler, pname, bufSize, param);
}

void TraceGLApi::glScissorFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glScissor")
  gl_api_->glScissorFn(x, y, width, height);
}

void TraceGLApi::glSetFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceAPPLE")
  gl_api_->glSetFenceAPPLEFn(fence);
}

void TraceGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glSetFenceNV")
  gl_api_->glSetFenceNVFn(fence, condition);
}

void TraceGLApi::glShaderBinaryFn(GLsizei n,
                                  const GLuint* shaders,
                                  GLenum binaryformat,
                                  const void* binary,
                                  GLsizei length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderBinary")
  gl_api_->glShaderBinaryFn(n, shaders, binaryformat, binary, length);
}

void TraceGLApi::glShaderSourceFn(GLuint shader,
                                  GLsizei count,
                                  const char* const* str,
                                  const GLint* length) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glShaderSource")
  gl_api_->glShaderSourceFn(shader, count, str, length);
}

void TraceGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilFillPathInstancedNV")
  gl_api_->glStencilFillPathInstancedNVFn(numPaths, pathNameType, paths,
                                          pathBase, fillMode, mask,
                                          transformType, transformValues);
}

void TraceGLApi::glStencilFillPathNVFn(GLuint path,
                                       GLenum fillMode,
                                       GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFillPathNV")
  gl_api_->glStencilFillPathNVFn(path, fillMode, mask);
}

void TraceGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFunc")
  gl_api_->glStencilFuncFn(func, ref, mask);
}

void TraceGLApi::glStencilFuncSeparateFn(GLenum face,
                                         GLenum func,
                                         GLint ref,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilFuncSeparate")
  gl_api_->glStencilFuncSeparateFn(face, func, ref, mask);
}

void TraceGLApi::glStencilMaskFn(GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMask")
  gl_api_->glStencilMaskFn(mask);
}

void TraceGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilMaskSeparate")
  gl_api_->glStencilMaskSeparateFn(face, mask);
}

void TraceGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOp")
  gl_api_->glStencilOpFn(fail, zfail, zpass);
}

void TraceGLApi::glStencilOpSeparateFn(GLenum face,
                                       GLenum fail,
                                       GLenum zfail,
                                       GLenum zpass) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilOpSeparate")
  gl_api_->glStencilOpSeparateFn(face, fail, zfail, zpass);
}

void TraceGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilStrokePathInstancedNV")
  gl_api_->glStencilStrokePathInstancedNVFn(numPaths, pathNameType, paths,
                                            pathBase, ref, mask, transformType,
                                            transformValues);
}

void TraceGLApi::glStencilStrokePathNVFn(GLuint path,
                                         GLint reference,
                                         GLuint mask) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glStencilStrokePathNV")
  gl_api_->glStencilStrokePathNVFn(path, reference, mask);
}

void TraceGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverFillPathInstancedNV")
  gl_api_->glStencilThenCoverFillPathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, fillMode, mask, coverMode,
      transformType, transformValues);
}

void TraceGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverFillPathNV")
  gl_api_->glStencilThenCoverFillPathNVFn(path, fillMode, mask, coverMode);
}

void TraceGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  TRACE_EVENT_BINARY_EFFICIENT0(
      "gpu", "TraceGLAPI::glStencilThenCoverStrokePathInstancedNV")
  gl_api_->glStencilThenCoverStrokePathInstancedNVFn(
      numPaths, pathNameType, paths, pathBase, ref, mask, coverMode,
      transformType, transformValues);
}

void TraceGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glStencilThenCoverStrokePathNV")
  gl_api_->glStencilThenCoverStrokePathNVFn(path, reference, mask, coverMode);
}

GLboolean TraceGLApi::glTestFenceAPPLEFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceAPPLE")
  return gl_api_->glTestFenceAPPLEFn(fence);
}

GLboolean TraceGLApi::glTestFenceNVFn(GLuint fence) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTestFenceNV")
  return gl_api_->glTestFenceNVFn(fence);
}

void TraceGLApi::glTexImage2DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2D")
  gl_api_->glTexImage2DFn(target, level, internalformat, width, height, border,
                          format, type, pixels);
}

void TraceGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage2DRobustANGLE")
  gl_api_->glTexImage2DRobustANGLEFn(target, level, internalformat, width,
                                     height, border, format, type, bufSize,
                                     pixels);
}

void TraceGLApi::glTexImage3DFn(GLenum target,
                                GLint level,
                                GLint internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLenum format,
                                GLenum type,
                                const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3D")
  gl_api_->glTexImage3DFn(target, level, internalformat, width, height, depth,
                          border, format, type, pixels);
}

void TraceGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                           GLint level,
                                           GLint internalformat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLint border,
                                           GLenum format,
                                           GLenum type,
                                           GLsizei bufSize,
                                           const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexImage3DRobustANGLE")
  gl_api_->glTexImage3DRobustANGLEFn(target, level, internalformat, width,
                                     height, depth, border, format, type,
                                     bufSize, pixels);
}

void TraceGLApi::glTexParameterfFn(GLenum target, GLenum pname, GLfloat param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterf")
  gl_api_->glTexParameterfFn(target, pname, param);
}

void TraceGLApi::glTexParameterfvFn(GLenum target,
                                    GLenum pname,
                                    const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameterfv")
  gl_api_->glTexParameterfvFn(target, pname, params);
}

void TraceGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLfloat* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterfvRobustANGLE")
  gl_api_->glTexParameterfvRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameteriFn(GLenum target, GLenum pname, GLint param) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteri")
  gl_api_->glTexParameteriFn(target, pname, param);
}

void TraceGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIivRobustANGLE")
  gl_api_->glTexParameterIivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 const GLuint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterIuivRobustANGLE")
  gl_api_->glTexParameterIuivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexParameterivFn(GLenum target,
                                    GLenum pname,
                                    const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexParameteriv")
  gl_api_->glTexParameterivFn(target, pname, params);
}

void TraceGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               const GLint* params) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTexParameterivRobustANGLE")
  gl_api_->glTexParameterivRobustANGLEFn(target, pname, bufSize, params);
}

void TraceGLApi::glTexStorage2DEXTFn(GLenum target,
                                     GLsizei levels,
                                     GLenum internalformat,
                                     GLsizei width,
                                     GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage2DEXT")
  gl_api_->glTexStorage2DEXTFn(target, levels, internalformat, width, height);
}

void TraceGLApi::glTexStorage3DFn(GLenum target,
                                  GLsizei levels,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexStorage3D")
  gl_api_->glTexStorage3DFn(target, levels, internalformat, width, height,
                            depth);
}

void TraceGLApi::glTexSubImage2DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2D")
  gl_api_->glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                             format, type, pixels);
}

void TraceGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage2DRobustANGLE")
  gl_api_->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                        height, format, type, bufSize, pixels);
}

void TraceGLApi::glTexSubImage3DFn(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLenum type,
                                   const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3D")
  gl_api_->glTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, pixels);
}

void TraceGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
                                              GLint level,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLint zoffset,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLenum format,
                                              GLenum type,
                                              GLsizei bufSize,
                                              const void* pixels) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glTexSubImage3DRobustANGLE")
  gl_api_->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset,
                                        zoffset, width, height, depth, format,
                                        type, bufSize, pixels);
}

void TraceGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                               GLsizei count,
                                               const char* const* varyings,
                                               GLenum bufferMode) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu",
                                "TraceGLAPI::glTransformFeedbackVaryings")
  gl_api_->glTransformFeedbackVaryingsFn(program, count, varyings, bufferMode);
}

void TraceGLApi::glUniform1fFn(GLint location, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1f")
  gl_api_->glUniform1fFn(location, x);
}

void TraceGLApi::glUniform1fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1fv")
  gl_api_->glUniform1fvFn(location, count, v);
}

void TraceGLApi::glUniform1iFn(GLint location, GLint x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1i")
  gl_api_->glUniform1iFn(location, x);
}

void TraceGLApi::glUniform1ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1iv")
  gl_api_->glUniform1ivFn(location, count, v);
}

void TraceGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1ui")
  gl_api_->glUniform1uiFn(location, v0);
}

void TraceGLApi::glUniform1uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform1uiv")
  gl_api_->glUniform1uivFn(location, count, v);
}

void TraceGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2f")
  gl_api_->glUniform2fFn(location, x, y);
}

void TraceGLApi::glUniform2fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2fv")
  gl_api_->glUniform2fvFn(location, count, v);
}

void TraceGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2i")
  gl_api_->glUniform2iFn(location, x, y);
}

void TraceGLApi::glUniform2ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2iv")
  gl_api_->glUniform2ivFn(location, count, v);
}

void TraceGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2ui")
  gl_api_->glUniform2uiFn(location, v0, v1);
}

void TraceGLApi::glUniform2uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform2uiv")
  gl_api_->glUniform2uivFn(location, count, v);
}

void TraceGLApi::glUniform3fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3f")
  gl_api_->glUniform3fFn(location, x, y, z);
}

void TraceGLApi::glUniform3fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3fv")
  gl_api_->glUniform3fvFn(location, count, v);
}

void TraceGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3i")
  gl_api_->glUniform3iFn(location, x, y, z);
}

void TraceGLApi::glUniform3ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3iv")
  gl_api_->glUniform3ivFn(location, count, v);
}

void TraceGLApi::glUniform3uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3ui")
  gl_api_->glUniform3uiFn(location, v0, v1, v2);
}

void TraceGLApi::glUniform3uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform3uiv")
  gl_api_->glUniform3uivFn(location, count, v);
}

void TraceGLApi::glUniform4fFn(GLint location,
                               GLfloat x,
                               GLfloat y,
                               GLfloat z,
                               GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4f")
  gl_api_->glUniform4fFn(location, x, y, z, w);
}

void TraceGLApi::glUniform4fvFn(GLint location,
                                GLsizei count,
                                const GLfloat* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4fv")
  gl_api_->glUniform4fvFn(location, count, v);
}

void TraceGLApi::glUniform4iFn(GLint location,
                               GLint x,
                               GLint y,
                               GLint z,
                               GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4i")
  gl_api_->glUniform4iFn(location, x, y, z, w);
}

void TraceGLApi::glUniform4ivFn(GLint location, GLsizei count, const GLint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4iv")
  gl_api_->glUniform4ivFn(location, count, v);
}

void TraceGLApi::glUniform4uiFn(GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4ui")
  gl_api_->glUniform4uiFn(location, v0, v1, v2, v3);
}

void TraceGLApi::glUniform4uivFn(GLint location,
                                 GLsizei count,
                                 const GLuint* v) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniform4uiv")
  gl_api_->glUniform4uivFn(location, count, v);
}

void TraceGLApi::glUniformBlockBindingFn(GLuint program,
                                         GLuint uniformBlockIndex,
                                         GLuint uniformBlockBinding) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformBlockBinding")
  gl_api_->glUniformBlockBindingFn(program, uniformBlockIndex,
                                   uniformBlockBinding);
}

void TraceGLApi::glUniformMatrix2fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2fv")
  gl_api_->glUniformMatrix2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix2x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x3fv")
  gl_api_->glUniformMatrix2x3fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix2x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix2x4fv")
  gl_api_->glUniformMatrix2x4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3fv")
  gl_api_->glUniformMatrix3fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x2fv")
  gl_api_->glUniformMatrix3x2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix3x4fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix3x4fv")
  gl_api_->glUniformMatrix3x4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4fvFn(GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4fv")
  gl_api_->glUniformMatrix4fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4x2fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x2fv")
  gl_api_->glUniformMatrix4x2fvFn(location, count, transpose, value);
}

void TraceGLApi::glUniformMatrix4x3fvFn(GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat* value) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUniformMatrix4x3fv")
  gl_api_->glUniformMatrix4x3fvFn(location, count, transpose, value);
}

GLboolean TraceGLApi::glUnmapBufferFn(GLenum target) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUnmapBuffer")
  return gl_api_->glUnmapBufferFn(target);
}

void TraceGLApi::glUseProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glUseProgram")
  gl_api_->glUseProgramFn(program);
}

void TraceGLApi::glValidateProgramFn(GLuint program) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glValidateProgram")
  gl_api_->glValidateProgramFn(program);
}

void TraceGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1f")
  gl_api_->glVertexAttrib1fFn(indx, x);
}

void TraceGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib1fv")
  gl_api_->glVertexAttrib1fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2f")
  gl_api_->glVertexAttrib2fFn(indx, x, y);
}

void TraceGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib2fv")
  gl_api_->glVertexAttrib2fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib3fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3f")
  gl_api_->glVertexAttrib3fFn(indx, x, y, z);
}

void TraceGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib3fv")
  gl_api_->glVertexAttrib3fvFn(indx, values);
}

void TraceGLApi::glVertexAttrib4fFn(GLuint indx,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4f")
  gl_api_->glVertexAttrib4fFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttrib4fv")
  gl_api_->glVertexAttrib4fvFn(indx, values);
}

void TraceGLApi::glVertexAttribDivisorANGLEFn(GLuint index, GLuint divisor) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribDivisorANGLE")
  gl_api_->glVertexAttribDivisorANGLEFn(index, divisor);
}

void TraceGLApi::glVertexAttribI4iFn(GLuint indx,
                                     GLint x,
                                     GLint y,
                                     GLint z,
                                     GLint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4i")
  gl_api_->glVertexAttribI4iFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4iv")
  gl_api_->glVertexAttribI4ivFn(indx, values);
}

void TraceGLApi::glVertexAttribI4uiFn(GLuint indx,
                                      GLuint x,
                                      GLuint y,
                                      GLuint z,
                                      GLuint w) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4ui")
  gl_api_->glVertexAttribI4uiFn(indx, x, y, z, w);
}

void TraceGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribI4uiv")
  gl_api_->glVertexAttribI4uivFn(indx, values);
}

void TraceGLApi::glVertexAttribIPointerFn(GLuint indx,
                                          GLint size,
                                          GLenum type,
                                          GLsizei stride,
                                          const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribIPointer")
  gl_api_->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
}

void TraceGLApi::glVertexAttribPointerFn(GLuint indx,
                                         GLint size,
                                         GLenum type,
                                         GLboolean normalized,
                                         GLsizei stride,
                                         const void* ptr) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glVertexAttribPointer")
  gl_api_->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
}

void TraceGLApi::glViewportFn(GLint x, GLint y, GLsizei width, GLsizei height) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glViewport")
  gl_api_->glViewportFn(x, y, width, height);
}

GLenum TraceGLApi::glWaitSyncFn(GLsync sync,
                                GLbitfield flags,
                                GLuint64 timeout) {
  TRACE_EVENT_BINARY_EFFICIENT0("gpu", "TraceGLAPI::glWaitSync")
  return gl_api_->glWaitSyncFn(sync, flags, timeout);
}

void NoContextGLApi::glActiveTextureFn(GLenum texture) {
  NOTREACHED() << "Trying to call glActiveTexture() without current GL context";
  LOG(ERROR) << "Trying to call glActiveTexture() without current GL context";
}

void NoContextGLApi::glApplyFramebufferAttachmentCMAAINTELFn(void) {
  NOTREACHED() << "Trying to call glApplyFramebufferAttachmentCMAAINTEL() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glApplyFramebufferAttachmentCMAAINTEL() "
                "without current GL context";
}

void NoContextGLApi::glAttachShaderFn(GLuint program, GLuint shader) {
  NOTREACHED() << "Trying to call glAttachShader() without current GL context";
  LOG(ERROR) << "Trying to call glAttachShader() without current GL context";
}

void NoContextGLApi::glBeginQueryFn(GLenum target, GLuint id) {
  NOTREACHED() << "Trying to call glBeginQuery() without current GL context";
  LOG(ERROR) << "Trying to call glBeginQuery() without current GL context";
}

void NoContextGLApi::glBeginTransformFeedbackFn(GLenum primitiveMode) {
  NOTREACHED()
      << "Trying to call glBeginTransformFeedback() without current GL context";
  LOG(ERROR)
      << "Trying to call glBeginTransformFeedback() without current GL context";
}

void NoContextGLApi::glBindAttribLocationFn(GLuint program,
                                            GLuint index,
                                            const char* name) {
  NOTREACHED()
      << "Trying to call glBindAttribLocation() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindAttribLocation() without current GL context";
}

void NoContextGLApi::glBindBufferFn(GLenum target, GLuint buffer) {
  NOTREACHED() << "Trying to call glBindBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glBindBuffer() without current GL context";
}

void NoContextGLApi::glBindBufferBaseFn(GLenum target,
                                        GLuint index,
                                        GLuint buffer) {
  NOTREACHED()
      << "Trying to call glBindBufferBase() without current GL context";
  LOG(ERROR) << "Trying to call glBindBufferBase() without current GL context";
}

void NoContextGLApi::glBindBufferRangeFn(GLenum target,
                                         GLuint index,
                                         GLuint buffer,
                                         GLintptr offset,
                                         GLsizeiptr size) {
  NOTREACHED()
      << "Trying to call glBindBufferRange() without current GL context";
  LOG(ERROR) << "Trying to call glBindBufferRange() without current GL context";
}

void NoContextGLApi::glBindFragDataLocationFn(GLuint program,
                                              GLuint colorNumber,
                                              const char* name) {
  NOTREACHED()
      << "Trying to call glBindFragDataLocation() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindFragDataLocation() without current GL context";
}

void NoContextGLApi::glBindFragDataLocationIndexedFn(GLuint program,
                                                     GLuint colorNumber,
                                                     GLuint index,
                                                     const char* name) {
  NOTREACHED() << "Trying to call glBindFragDataLocationIndexed() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glBindFragDataLocationIndexed() without "
                "current GL context";
}

void NoContextGLApi::glBindFramebufferEXTFn(GLenum target, GLuint framebuffer) {
  NOTREACHED()
      << "Trying to call glBindFramebufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindFramebufferEXT() without current GL context";
}

void NoContextGLApi::glBindImageTextureEXTFn(GLuint index,
                                             GLuint texture,
                                             GLint level,
                                             GLboolean layered,
                                             GLint layer,
                                             GLenum access,
                                             GLint format) {
  NOTREACHED()
      << "Trying to call glBindImageTextureEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindImageTextureEXT() without current GL context";
}

void NoContextGLApi::glBindRenderbufferEXTFn(GLenum target,
                                             GLuint renderbuffer) {
  NOTREACHED()
      << "Trying to call glBindRenderbufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindRenderbufferEXT() without current GL context";
}

void NoContextGLApi::glBindSamplerFn(GLuint unit, GLuint sampler) {
  NOTREACHED() << "Trying to call glBindSampler() without current GL context";
  LOG(ERROR) << "Trying to call glBindSampler() without current GL context";
}

void NoContextGLApi::glBindTextureFn(GLenum target, GLuint texture) {
  NOTREACHED() << "Trying to call glBindTexture() without current GL context";
  LOG(ERROR) << "Trying to call glBindTexture() without current GL context";
}

void NoContextGLApi::glBindTransformFeedbackFn(GLenum target, GLuint id) {
  NOTREACHED()
      << "Trying to call glBindTransformFeedback() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindTransformFeedback() without current GL context";
}

void NoContextGLApi::glBindUniformLocationCHROMIUMFn(GLuint program,
                                                     GLint location,
                                                     const char* name) {
  NOTREACHED() << "Trying to call glBindUniformLocationCHROMIUM() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glBindUniformLocationCHROMIUM() without "
                "current GL context";
}

void NoContextGLApi::glBindVertexArrayOESFn(GLuint array) {
  NOTREACHED()
      << "Trying to call glBindVertexArrayOES() without current GL context";
  LOG(ERROR)
      << "Trying to call glBindVertexArrayOES() without current GL context";
}

void NoContextGLApi::glBlendBarrierKHRFn(void) {
  NOTREACHED()
      << "Trying to call glBlendBarrierKHR() without current GL context";
  LOG(ERROR) << "Trying to call glBlendBarrierKHR() without current GL context";
}

void NoContextGLApi::glBlendColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NOTREACHED() << "Trying to call glBlendColor() without current GL context";
  LOG(ERROR) << "Trying to call glBlendColor() without current GL context";
}

void NoContextGLApi::glBlendEquationFn(GLenum mode) {
  NOTREACHED() << "Trying to call glBlendEquation() without current GL context";
  LOG(ERROR) << "Trying to call glBlendEquation() without current GL context";
}

void NoContextGLApi::glBlendEquationSeparateFn(GLenum modeRGB,
                                               GLenum modeAlpha) {
  NOTREACHED()
      << "Trying to call glBlendEquationSeparate() without current GL context";
  LOG(ERROR)
      << "Trying to call glBlendEquationSeparate() without current GL context";
}

void NoContextGLApi::glBlendFuncFn(GLenum sfactor, GLenum dfactor) {
  NOTREACHED() << "Trying to call glBlendFunc() without current GL context";
  LOG(ERROR) << "Trying to call glBlendFunc() without current GL context";
}

void NoContextGLApi::glBlendFuncSeparateFn(GLenum srcRGB,
                                           GLenum dstRGB,
                                           GLenum srcAlpha,
                                           GLenum dstAlpha) {
  NOTREACHED()
      << "Trying to call glBlendFuncSeparate() without current GL context";
  LOG(ERROR)
      << "Trying to call glBlendFuncSeparate() without current GL context";
}

void NoContextGLApi::glBlitFramebufferFn(GLint srcX0,
                                         GLint srcY0,
                                         GLint srcX1,
                                         GLint srcY1,
                                         GLint dstX0,
                                         GLint dstY0,
                                         GLint dstX1,
                                         GLint dstY1,
                                         GLbitfield mask,
                                         GLenum filter) {
  NOTREACHED()
      << "Trying to call glBlitFramebuffer() without current GL context";
  LOG(ERROR) << "Trying to call glBlitFramebuffer() without current GL context";
}

void NoContextGLApi::glBlitFramebufferANGLEFn(GLint srcX0,
                                              GLint srcY0,
                                              GLint srcX1,
                                              GLint srcY1,
                                              GLint dstX0,
                                              GLint dstY0,
                                              GLint dstX1,
                                              GLint dstY1,
                                              GLbitfield mask,
                                              GLenum filter) {
  NOTREACHED()
      << "Trying to call glBlitFramebufferANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glBlitFramebufferANGLE() without current GL context";
}

void NoContextGLApi::glBlitFramebufferEXTFn(GLint srcX0,
                                            GLint srcY0,
                                            GLint srcX1,
                                            GLint srcY1,
                                            GLint dstX0,
                                            GLint dstY0,
                                            GLint dstX1,
                                            GLint dstY1,
                                            GLbitfield mask,
                                            GLenum filter) {
  NOTREACHED()
      << "Trying to call glBlitFramebufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glBlitFramebufferEXT() without current GL context";
}

void NoContextGLApi::glBufferDataFn(GLenum target,
                                    GLsizeiptr size,
                                    const void* data,
                                    GLenum usage) {
  NOTREACHED() << "Trying to call glBufferData() without current GL context";
  LOG(ERROR) << "Trying to call glBufferData() without current GL context";
}

void NoContextGLApi::glBufferSubDataFn(GLenum target,
                                       GLintptr offset,
                                       GLsizeiptr size,
                                       const void* data) {
  NOTREACHED() << "Trying to call glBufferSubData() without current GL context";
  LOG(ERROR) << "Trying to call glBufferSubData() without current GL context";
}

GLenum NoContextGLApi::glCheckFramebufferStatusEXTFn(GLenum target) {
  NOTREACHED() << "Trying to call glCheckFramebufferStatusEXT() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glCheckFramebufferStatusEXT() without current "
                "GL context";
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glClearFn(GLbitfield mask) {
  NOTREACHED() << "Trying to call glClear() without current GL context";
  LOG(ERROR) << "Trying to call glClear() without current GL context";
}

void NoContextGLApi::glClearBufferfiFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat depth,
                                       GLint stencil) {
  NOTREACHED() << "Trying to call glClearBufferfi() without current GL context";
  LOG(ERROR) << "Trying to call glClearBufferfi() without current GL context";
}

void NoContextGLApi::glClearBufferfvFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLfloat* value) {
  NOTREACHED() << "Trying to call glClearBufferfv() without current GL context";
  LOG(ERROR) << "Trying to call glClearBufferfv() without current GL context";
}

void NoContextGLApi::glClearBufferivFn(GLenum buffer,
                                       GLint drawbuffer,
                                       const GLint* value) {
  NOTREACHED() << "Trying to call glClearBufferiv() without current GL context";
  LOG(ERROR) << "Trying to call glClearBufferiv() without current GL context";
}

void NoContextGLApi::glClearBufferuivFn(GLenum buffer,
                                        GLint drawbuffer,
                                        const GLuint* value) {
  NOTREACHED()
      << "Trying to call glClearBufferuiv() without current GL context";
  LOG(ERROR) << "Trying to call glClearBufferuiv() without current GL context";
}

void NoContextGLApi::glClearColorFn(GLclampf red,
                                    GLclampf green,
                                    GLclampf blue,
                                    GLclampf alpha) {
  NOTREACHED() << "Trying to call glClearColor() without current GL context";
  LOG(ERROR) << "Trying to call glClearColor() without current GL context";
}

void NoContextGLApi::glClearDepthFn(GLclampd depth) {
  NOTREACHED() << "Trying to call glClearDepth() without current GL context";
  LOG(ERROR) << "Trying to call glClearDepth() without current GL context";
}

void NoContextGLApi::glClearDepthfFn(GLclampf depth) {
  NOTREACHED() << "Trying to call glClearDepthf() without current GL context";
  LOG(ERROR) << "Trying to call glClearDepthf() without current GL context";
}

void NoContextGLApi::glClearStencilFn(GLint s) {
  NOTREACHED() << "Trying to call glClearStencil() without current GL context";
  LOG(ERROR) << "Trying to call glClearStencil() without current GL context";
}

GLenum NoContextGLApi::glClientWaitSyncFn(GLsync sync,
                                          GLbitfield flags,
                                          GLuint64 timeout) {
  NOTREACHED()
      << "Trying to call glClientWaitSync() without current GL context";
  LOG(ERROR) << "Trying to call glClientWaitSync() without current GL context";
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glColorMaskFn(GLboolean red,
                                   GLboolean green,
                                   GLboolean blue,
                                   GLboolean alpha) {
  NOTREACHED() << "Trying to call glColorMask() without current GL context";
  LOG(ERROR) << "Trying to call glColorMask() without current GL context";
}

void NoContextGLApi::glCompileShaderFn(GLuint shader) {
  NOTREACHED() << "Trying to call glCompileShader() without current GL context";
  LOG(ERROR) << "Trying to call glCompileShader() without current GL context";
}

void NoContextGLApi::glCompressedCopyTextureCHROMIUMFn(GLuint sourceId,
                                                       GLuint destId) {
  NOTREACHED() << "Trying to call glCompressedCopyTextureCHROMIUM() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glCompressedCopyTextureCHROMIUM() without "
                "current GL context";
}

void NoContextGLApi::glCompressedTexImage2DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NOTREACHED()
      << "Trying to call glCompressedTexImage2D() without current GL context";
  LOG(ERROR)
      << "Trying to call glCompressedTexImage2D() without current GL context";
}

void NoContextGLApi::glCompressedTexImage3DFn(GLenum target,
                                              GLint level,
                                              GLenum internalformat,
                                              GLsizei width,
                                              GLsizei height,
                                              GLsizei depth,
                                              GLint border,
                                              GLsizei imageSize,
                                              const void* data) {
  NOTREACHED()
      << "Trying to call glCompressedTexImage3D() without current GL context";
  LOG(ERROR)
      << "Trying to call glCompressedTexImage3D() without current GL context";
}

void NoContextGLApi::glCompressedTexSubImage2DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NOTREACHED() << "Trying to call glCompressedTexSubImage2D() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glCompressedTexSubImage2D() without current GL "
                "context";
}

void NoContextGLApi::glCompressedTexSubImage3DFn(GLenum target,
                                                 GLint level,
                                                 GLint xoffset,
                                                 GLint yoffset,
                                                 GLint zoffset,
                                                 GLsizei width,
                                                 GLsizei height,
                                                 GLsizei depth,
                                                 GLenum format,
                                                 GLsizei imageSize,
                                                 const void* data) {
  NOTREACHED() << "Trying to call glCompressedTexSubImage3D() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glCompressedTexSubImage3D() without current GL "
                "context";
}

void NoContextGLApi::glCopyBufferSubDataFn(GLenum readTarget,
                                           GLenum writeTarget,
                                           GLintptr readOffset,
                                           GLintptr writeOffset,
                                           GLsizeiptr size) {
  NOTREACHED()
      << "Trying to call glCopyBufferSubData() without current GL context";
  LOG(ERROR)
      << "Trying to call glCopyBufferSubData() without current GL context";
}

void NoContextGLApi::glCopySubTextureCHROMIUMFn(
    GLuint sourceId,
    GLuint destId,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpackFlipY,
    GLboolean unpackPremultiplyAlpha,
    GLboolean unpackUnmultiplyAlpha) {
  NOTREACHED()
      << "Trying to call glCopySubTextureCHROMIUM() without current GL context";
  LOG(ERROR)
      << "Trying to call glCopySubTextureCHROMIUM() without current GL context";
}

void NoContextGLApi::glCopyTexImage2DFn(GLenum target,
                                        GLint level,
                                        GLenum internalformat,
                                        GLint x,
                                        GLint y,
                                        GLsizei width,
                                        GLsizei height,
                                        GLint border) {
  NOTREACHED()
      << "Trying to call glCopyTexImage2D() without current GL context";
  LOG(ERROR) << "Trying to call glCopyTexImage2D() without current GL context";
}

void NoContextGLApi::glCopyTexSubImage2DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NOTREACHED()
      << "Trying to call glCopyTexSubImage2D() without current GL context";
  LOG(ERROR)
      << "Trying to call glCopyTexSubImage2D() without current GL context";
}

void NoContextGLApi::glCopyTexSubImage3DFn(GLenum target,
                                           GLint level,
                                           GLint xoffset,
                                           GLint yoffset,
                                           GLint zoffset,
                                           GLint x,
                                           GLint y,
                                           GLsizei width,
                                           GLsizei height) {
  NOTREACHED()
      << "Trying to call glCopyTexSubImage3D() without current GL context";
  LOG(ERROR)
      << "Trying to call glCopyTexSubImage3D() without current GL context";
}

void NoContextGLApi::glCopyTextureCHROMIUMFn(GLuint sourceId,
                                             GLuint destId,
                                             GLint internalFormat,
                                             GLenum destType,
                                             GLboolean unpackFlipY,
                                             GLboolean unpackPremultiplyAlpha,
                                             GLboolean unpackUnmultiplyAlpha) {
  NOTREACHED()
      << "Trying to call glCopyTextureCHROMIUM() without current GL context";
  LOG(ERROR)
      << "Trying to call glCopyTextureCHROMIUM() without current GL context";
}

void NoContextGLApi::glCoverageModulationNVFn(GLenum components) {
  NOTREACHED()
      << "Trying to call glCoverageModulationNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glCoverageModulationNV() without current GL context";
}

void NoContextGLApi::glCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glCoverFillPathInstancedNV() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glCoverFillPathInstancedNV() without current "
                "GL context";
}

void NoContextGLApi::glCoverFillPathNVFn(GLuint path, GLenum coverMode) {
  NOTREACHED()
      << "Trying to call glCoverFillPathNV() without current GL context";
  LOG(ERROR) << "Trying to call glCoverFillPathNV() without current GL context";
}

void NoContextGLApi::glCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glCoverStrokePathInstancedNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glCoverStrokePathInstancedNV() without current "
                "GL context";
}

void NoContextGLApi::glCoverStrokePathNVFn(GLuint name, GLenum coverMode) {
  NOTREACHED()
      << "Trying to call glCoverStrokePathNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glCoverStrokePathNV() without current GL context";
}

GLuint NoContextGLApi::glCreateProgramFn(void) {
  NOTREACHED() << "Trying to call glCreateProgram() without current GL context";
  LOG(ERROR) << "Trying to call glCreateProgram() without current GL context";
  return 0U;
}

GLuint NoContextGLApi::glCreateShaderFn(GLenum type) {
  NOTREACHED() << "Trying to call glCreateShader() without current GL context";
  LOG(ERROR) << "Trying to call glCreateShader() without current GL context";
  return 0U;
}

void NoContextGLApi::glCullFaceFn(GLenum mode) {
  NOTREACHED() << "Trying to call glCullFace() without current GL context";
  LOG(ERROR) << "Trying to call glCullFace() without current GL context";
}

void NoContextGLApi::glDebugMessageCallbackFn(GLDEBUGPROC callback,
                                              const void* userParam) {
  NOTREACHED()
      << "Trying to call glDebugMessageCallback() without current GL context";
  LOG(ERROR)
      << "Trying to call glDebugMessageCallback() without current GL context";
}

void NoContextGLApi::glDebugMessageControlFn(GLenum source,
                                             GLenum type,
                                             GLenum severity,
                                             GLsizei count,
                                             const GLuint* ids,
                                             GLboolean enabled) {
  NOTREACHED()
      << "Trying to call glDebugMessageControl() without current GL context";
  LOG(ERROR)
      << "Trying to call glDebugMessageControl() without current GL context";
}

void NoContextGLApi::glDebugMessageInsertFn(GLenum source,
                                            GLenum type,
                                            GLuint id,
                                            GLenum severity,
                                            GLsizei length,
                                            const char* buf) {
  NOTREACHED()
      << "Trying to call glDebugMessageInsert() without current GL context";
  LOG(ERROR)
      << "Trying to call glDebugMessageInsert() without current GL context";
}

void NoContextGLApi::glDeleteBuffersARBFn(GLsizei n, const GLuint* buffers) {
  NOTREACHED()
      << "Trying to call glDeleteBuffersARB() without current GL context";
  LOG(ERROR)
      << "Trying to call glDeleteBuffersARB() without current GL context";
}

void NoContextGLApi::glDeleteFencesAPPLEFn(GLsizei n, const GLuint* fences) {
  NOTREACHED()
      << "Trying to call glDeleteFencesAPPLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glDeleteFencesAPPLE() without current GL context";
}

void NoContextGLApi::glDeleteFencesNVFn(GLsizei n, const GLuint* fences) {
  NOTREACHED()
      << "Trying to call glDeleteFencesNV() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteFencesNV() without current GL context";
}

void NoContextGLApi::glDeleteFramebuffersEXTFn(GLsizei n,
                                               const GLuint* framebuffers) {
  NOTREACHED()
      << "Trying to call glDeleteFramebuffersEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glDeleteFramebuffersEXT() without current GL context";
}

void NoContextGLApi::glDeletePathsNVFn(GLuint path, GLsizei range) {
  NOTREACHED() << "Trying to call glDeletePathsNV() without current GL context";
  LOG(ERROR) << "Trying to call glDeletePathsNV() without current GL context";
}

void NoContextGLApi::glDeleteProgramFn(GLuint program) {
  NOTREACHED() << "Trying to call glDeleteProgram() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteProgram() without current GL context";
}

void NoContextGLApi::glDeleteQueriesFn(GLsizei n, const GLuint* ids) {
  NOTREACHED() << "Trying to call glDeleteQueries() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteQueries() without current GL context";
}

void NoContextGLApi::glDeleteRenderbuffersEXTFn(GLsizei n,
                                                const GLuint* renderbuffers) {
  NOTREACHED()
      << "Trying to call glDeleteRenderbuffersEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glDeleteRenderbuffersEXT() without current GL context";
}

void NoContextGLApi::glDeleteSamplersFn(GLsizei n, const GLuint* samplers) {
  NOTREACHED()
      << "Trying to call glDeleteSamplers() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteSamplers() without current GL context";
}

void NoContextGLApi::glDeleteShaderFn(GLuint shader) {
  NOTREACHED() << "Trying to call glDeleteShader() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteShader() without current GL context";
}

void NoContextGLApi::glDeleteSyncFn(GLsync sync) {
  NOTREACHED() << "Trying to call glDeleteSync() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteSync() without current GL context";
}

void NoContextGLApi::glDeleteTexturesFn(GLsizei n, const GLuint* textures) {
  NOTREACHED()
      << "Trying to call glDeleteTextures() without current GL context";
  LOG(ERROR) << "Trying to call glDeleteTextures() without current GL context";
}

void NoContextGLApi::glDeleteTransformFeedbacksFn(GLsizei n,
                                                  const GLuint* ids) {
  NOTREACHED() << "Trying to call glDeleteTransformFeedbacks() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glDeleteTransformFeedbacks() without current "
                "GL context";
}

void NoContextGLApi::glDeleteVertexArraysOESFn(GLsizei n,
                                               const GLuint* arrays) {
  NOTREACHED()
      << "Trying to call glDeleteVertexArraysOES() without current GL context";
  LOG(ERROR)
      << "Trying to call glDeleteVertexArraysOES() without current GL context";
}

void NoContextGLApi::glDepthFuncFn(GLenum func) {
  NOTREACHED() << "Trying to call glDepthFunc() without current GL context";
  LOG(ERROR) << "Trying to call glDepthFunc() without current GL context";
}

void NoContextGLApi::glDepthMaskFn(GLboolean flag) {
  NOTREACHED() << "Trying to call glDepthMask() without current GL context";
  LOG(ERROR) << "Trying to call glDepthMask() without current GL context";
}

void NoContextGLApi::glDepthRangeFn(GLclampd zNear, GLclampd zFar) {
  NOTREACHED() << "Trying to call glDepthRange() without current GL context";
  LOG(ERROR) << "Trying to call glDepthRange() without current GL context";
}

void NoContextGLApi::glDepthRangefFn(GLclampf zNear, GLclampf zFar) {
  NOTREACHED() << "Trying to call glDepthRangef() without current GL context";
  LOG(ERROR) << "Trying to call glDepthRangef() without current GL context";
}

void NoContextGLApi::glDetachShaderFn(GLuint program, GLuint shader) {
  NOTREACHED() << "Trying to call glDetachShader() without current GL context";
  LOG(ERROR) << "Trying to call glDetachShader() without current GL context";
}

void NoContextGLApi::glDisableFn(GLenum cap) {
  NOTREACHED() << "Trying to call glDisable() without current GL context";
  LOG(ERROR) << "Trying to call glDisable() without current GL context";
}

void NoContextGLApi::glDisableVertexAttribArrayFn(GLuint index) {
  NOTREACHED() << "Trying to call glDisableVertexAttribArray() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glDisableVertexAttribArray() without current "
                "GL context";
}

void NoContextGLApi::glDiscardFramebufferEXTFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NOTREACHED()
      << "Trying to call glDiscardFramebufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glDiscardFramebufferEXT() without current GL context";
}

void NoContextGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  NOTREACHED() << "Trying to call glDrawArrays() without current GL context";
  LOG(ERROR) << "Trying to call glDrawArrays() without current GL context";
}

void NoContextGLApi::glDrawArraysInstancedANGLEFn(GLenum mode,
                                                  GLint first,
                                                  GLsizei count,
                                                  GLsizei primcount) {
  NOTREACHED() << "Trying to call glDrawArraysInstancedANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glDrawArraysInstancedANGLE() without current "
                "GL context";
}

void NoContextGLApi::glDrawBufferFn(GLenum mode) {
  NOTREACHED() << "Trying to call glDrawBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glDrawBuffer() without current GL context";
}

void NoContextGLApi::glDrawBuffersARBFn(GLsizei n, const GLenum* bufs) {
  NOTREACHED()
      << "Trying to call glDrawBuffersARB() without current GL context";
  LOG(ERROR) << "Trying to call glDrawBuffersARB() without current GL context";
}

void NoContextGLApi::glDrawElementsFn(GLenum mode,
                                      GLsizei count,
                                      GLenum type,
                                      const void* indices) {
  NOTREACHED() << "Trying to call glDrawElements() without current GL context";
  LOG(ERROR) << "Trying to call glDrawElements() without current GL context";
}

void NoContextGLApi::glDrawElementsInstancedANGLEFn(GLenum mode,
                                                    GLsizei count,
                                                    GLenum type,
                                                    const void* indices,
                                                    GLsizei primcount) {
  NOTREACHED() << "Trying to call glDrawElementsInstancedANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glDrawElementsInstancedANGLE() without current "
                "GL context";
}

void NoContextGLApi::glDrawRangeElementsFn(GLenum mode,
                                           GLuint start,
                                           GLuint end,
                                           GLsizei count,
                                           GLenum type,
                                           const void* indices) {
  NOTREACHED()
      << "Trying to call glDrawRangeElements() without current GL context";
  LOG(ERROR)
      << "Trying to call glDrawRangeElements() without current GL context";
}

void NoContextGLApi::glEGLImageTargetRenderbufferStorageOESFn(
    GLenum target,
    GLeglImageOES image) {
  NOTREACHED() << "Trying to call glEGLImageTargetRenderbufferStorageOES() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glEGLImageTargetRenderbufferStorageOES() "
                "without current GL context";
}

void NoContextGLApi::glEGLImageTargetTexture2DOESFn(GLenum target,
                                                    GLeglImageOES image) {
  NOTREACHED() << "Trying to call glEGLImageTargetTexture2DOES() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glEGLImageTargetTexture2DOES() without current "
                "GL context";
}

void NoContextGLApi::glEnableFn(GLenum cap) {
  NOTREACHED() << "Trying to call glEnable() without current GL context";
  LOG(ERROR) << "Trying to call glEnable() without current GL context";
}

void NoContextGLApi::glEnableVertexAttribArrayFn(GLuint index) {
  NOTREACHED() << "Trying to call glEnableVertexAttribArray() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glEnableVertexAttribArray() without current GL "
                "context";
}

void NoContextGLApi::glEndQueryFn(GLenum target) {
  NOTREACHED() << "Trying to call glEndQuery() without current GL context";
  LOG(ERROR) << "Trying to call glEndQuery() without current GL context";
}

void NoContextGLApi::glEndTransformFeedbackFn(void) {
  NOTREACHED()
      << "Trying to call glEndTransformFeedback() without current GL context";
  LOG(ERROR)
      << "Trying to call glEndTransformFeedback() without current GL context";
}

GLsync NoContextGLApi::glFenceSyncFn(GLenum condition, GLbitfield flags) {
  NOTREACHED() << "Trying to call glFenceSync() without current GL context";
  LOG(ERROR) << "Trying to call glFenceSync() without current GL context";
  return NULL;
}

void NoContextGLApi::glFinishFn(void) {
  NOTREACHED() << "Trying to call glFinish() without current GL context";
  LOG(ERROR) << "Trying to call glFinish() without current GL context";
}

void NoContextGLApi::glFinishFenceAPPLEFn(GLuint fence) {
  NOTREACHED()
      << "Trying to call glFinishFenceAPPLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glFinishFenceAPPLE() without current GL context";
}

void NoContextGLApi::glFinishFenceNVFn(GLuint fence) {
  NOTREACHED() << "Trying to call glFinishFenceNV() without current GL context";
  LOG(ERROR) << "Trying to call glFinishFenceNV() without current GL context";
}

void NoContextGLApi::glFlushFn(void) {
  NOTREACHED() << "Trying to call glFlush() without current GL context";
  LOG(ERROR) << "Trying to call glFlush() without current GL context";
}

void NoContextGLApi::glFlushMappedBufferRangeFn(GLenum target,
                                                GLintptr offset,
                                                GLsizeiptr length) {
  NOTREACHED()
      << "Trying to call glFlushMappedBufferRange() without current GL context";
  LOG(ERROR)
      << "Trying to call glFlushMappedBufferRange() without current GL context";
}

void NoContextGLApi::glFramebufferRenderbufferEXTFn(GLenum target,
                                                    GLenum attachment,
                                                    GLenum renderbuffertarget,
                                                    GLuint renderbuffer) {
  NOTREACHED() << "Trying to call glFramebufferRenderbufferEXT() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glFramebufferRenderbufferEXT() without current "
                "GL context";
}

void NoContextGLApi::glFramebufferTexture2DEXTFn(GLenum target,
                                                 GLenum attachment,
                                                 GLenum textarget,
                                                 GLuint texture,
                                                 GLint level) {
  NOTREACHED() << "Trying to call glFramebufferTexture2DEXT() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glFramebufferTexture2DEXT() without current GL "
                "context";
}

void NoContextGLApi::glFramebufferTexture2DMultisampleEXTFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum textarget,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLsizei samples) {
  NOTREACHED() << "Trying to call glFramebufferTexture2DMultisampleEXT() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glFramebufferTexture2DMultisampleEXT() without "
                "current GL context";
}

void NoContextGLApi::glFramebufferTexture2DMultisampleIMGFn(GLenum target,
                                                            GLenum attachment,
                                                            GLenum textarget,
                                                            GLuint texture,
                                                            GLint level,
                                                            GLsizei samples) {
  NOTREACHED() << "Trying to call glFramebufferTexture2DMultisampleIMG() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glFramebufferTexture2DMultisampleIMG() without "
                "current GL context";
}

void NoContextGLApi::glFramebufferTextureLayerFn(GLenum target,
                                                 GLenum attachment,
                                                 GLuint texture,
                                                 GLint level,
                                                 GLint layer) {
  NOTREACHED() << "Trying to call glFramebufferTextureLayer() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glFramebufferTextureLayer() without current GL "
                "context";
}

void NoContextGLApi::glFrontFaceFn(GLenum mode) {
  NOTREACHED() << "Trying to call glFrontFace() without current GL context";
  LOG(ERROR) << "Trying to call glFrontFace() without current GL context";
}

void NoContextGLApi::glGenBuffersARBFn(GLsizei n, GLuint* buffers) {
  NOTREACHED() << "Trying to call glGenBuffersARB() without current GL context";
  LOG(ERROR) << "Trying to call glGenBuffersARB() without current GL context";
}

void NoContextGLApi::glGenerateMipmapEXTFn(GLenum target) {
  NOTREACHED()
      << "Trying to call glGenerateMipmapEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glGenerateMipmapEXT() without current GL context";
}

void NoContextGLApi::glGenFencesAPPLEFn(GLsizei n, GLuint* fences) {
  NOTREACHED()
      << "Trying to call glGenFencesAPPLE() without current GL context";
  LOG(ERROR) << "Trying to call glGenFencesAPPLE() without current GL context";
}

void NoContextGLApi::glGenFencesNVFn(GLsizei n, GLuint* fences) {
  NOTREACHED() << "Trying to call glGenFencesNV() without current GL context";
  LOG(ERROR) << "Trying to call glGenFencesNV() without current GL context";
}

void NoContextGLApi::glGenFramebuffersEXTFn(GLsizei n, GLuint* framebuffers) {
  NOTREACHED()
      << "Trying to call glGenFramebuffersEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glGenFramebuffersEXT() without current GL context";
}

GLuint NoContextGLApi::glGenPathsNVFn(GLsizei range) {
  NOTREACHED() << "Trying to call glGenPathsNV() without current GL context";
  LOG(ERROR) << "Trying to call glGenPathsNV() without current GL context";
  return 0U;
}

void NoContextGLApi::glGenQueriesFn(GLsizei n, GLuint* ids) {
  NOTREACHED() << "Trying to call glGenQueries() without current GL context";
  LOG(ERROR) << "Trying to call glGenQueries() without current GL context";
}

void NoContextGLApi::glGenRenderbuffersEXTFn(GLsizei n, GLuint* renderbuffers) {
  NOTREACHED()
      << "Trying to call glGenRenderbuffersEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glGenRenderbuffersEXT() without current GL context";
}

void NoContextGLApi::glGenSamplersFn(GLsizei n, GLuint* samplers) {
  NOTREACHED() << "Trying to call glGenSamplers() without current GL context";
  LOG(ERROR) << "Trying to call glGenSamplers() without current GL context";
}

void NoContextGLApi::glGenTexturesFn(GLsizei n, GLuint* textures) {
  NOTREACHED() << "Trying to call glGenTextures() without current GL context";
  LOG(ERROR) << "Trying to call glGenTextures() without current GL context";
}

void NoContextGLApi::glGenTransformFeedbacksFn(GLsizei n, GLuint* ids) {
  NOTREACHED()
      << "Trying to call glGenTransformFeedbacks() without current GL context";
  LOG(ERROR)
      << "Trying to call glGenTransformFeedbacks() without current GL context";
}

void NoContextGLApi::glGenVertexArraysOESFn(GLsizei n, GLuint* arrays) {
  NOTREACHED()
      << "Trying to call glGenVertexArraysOES() without current GL context";
  LOG(ERROR)
      << "Trying to call glGenVertexArraysOES() without current GL context";
}

void NoContextGLApi::glGetActiveAttribFn(GLuint program,
                                         GLuint index,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         GLint* size,
                                         GLenum* type,
                                         char* name) {
  NOTREACHED()
      << "Trying to call glGetActiveAttrib() without current GL context";
  LOG(ERROR) << "Trying to call glGetActiveAttrib() without current GL context";
}

void NoContextGLApi::glGetActiveUniformFn(GLuint program,
                                          GLuint index,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* size,
                                          GLenum* type,
                                          char* name) {
  NOTREACHED()
      << "Trying to call glGetActiveUniform() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetActiveUniform() without current GL context";
}

void NoContextGLApi::glGetActiveUniformBlockivFn(GLuint program,
                                                 GLuint uniformBlockIndex,
                                                 GLenum pname,
                                                 GLint* params) {
  NOTREACHED() << "Trying to call glGetActiveUniformBlockiv() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetActiveUniformBlockiv() without current GL "
                "context";
}

void NoContextGLApi::glGetActiveUniformBlockivRobustANGLEFn(
    GLuint program,
    GLuint uniformBlockIndex,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NOTREACHED() << "Trying to call glGetActiveUniformBlockivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetActiveUniformBlockivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetActiveUniformBlockNameFn(GLuint program,
                                                   GLuint uniformBlockIndex,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   char* uniformBlockName) {
  NOTREACHED() << "Trying to call glGetActiveUniformBlockName() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetActiveUniformBlockName() without current "
                "GL context";
}

void NoContextGLApi::glGetActiveUniformsivFn(GLuint program,
                                             GLsizei uniformCount,
                                             const GLuint* uniformIndices,
                                             GLenum pname,
                                             GLint* params) {
  NOTREACHED()
      << "Trying to call glGetActiveUniformsiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetActiveUniformsiv() without current GL context";
}

void NoContextGLApi::glGetAttachedShadersFn(GLuint program,
                                            GLsizei maxcount,
                                            GLsizei* count,
                                            GLuint* shaders) {
  NOTREACHED()
      << "Trying to call glGetAttachedShaders() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetAttachedShaders() without current GL context";
}

GLint NoContextGLApi::glGetAttribLocationFn(GLuint program, const char* name) {
  NOTREACHED()
      << "Trying to call glGetAttribLocation() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetAttribLocation() without current GL context";
  return 0;
}

void NoContextGLApi::glGetBooleani_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLboolean* data) {
  NOTREACHED() << "Trying to call glGetBooleani_vRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetBooleani_vRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetBooleanvFn(GLenum pname, GLboolean* params) {
  NOTREACHED() << "Trying to call glGetBooleanv() without current GL context";
  LOG(ERROR) << "Trying to call glGetBooleanv() without current GL context";
}

void NoContextGLApi::glGetBooleanvRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLboolean* data) {
  NOTREACHED()
      << "Trying to call glGetBooleanvRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetBooleanvRobustANGLE() without current GL context";
}

void NoContextGLApi::glGetBufferParameteri64vRobustANGLEFn(GLenum target,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint64* params) {
  NOTREACHED() << "Trying to call glGetBufferParameteri64vRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetBufferParameteri64vRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetBufferParameterivFn(GLenum target,
                                              GLenum pname,
                                              GLint* params) {
  NOTREACHED()
      << "Trying to call glGetBufferParameteriv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetBufferParameteriv() without current GL context";
}

void NoContextGLApi::glGetBufferParameterivRobustANGLEFn(GLenum target,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         GLsizei* length,
                                                         GLint* params) {
  NOTREACHED() << "Trying to call glGetBufferParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetBufferParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetBufferPointervRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      void** params) {
  NOTREACHED() << "Trying to call glGetBufferPointervRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetBufferPointervRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetDebugMessageLogFn(GLuint count,
                                            GLsizei bufSize,
                                            GLenum* sources,
                                            GLenum* types,
                                            GLuint* ids,
                                            GLenum* severities,
                                            GLsizei* lengths,
                                            char* messageLog) {
  NOTREACHED()
      << "Trying to call glGetDebugMessageLog() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetDebugMessageLog() without current GL context";
}

GLenum NoContextGLApi::glGetErrorFn(void) {
  NOTREACHED() << "Trying to call glGetError() without current GL context";
  LOG(ERROR) << "Trying to call glGetError() without current GL context";
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetFenceivNVFn(GLuint fence,
                                      GLenum pname,
                                      GLint* params) {
  NOTREACHED() << "Trying to call glGetFenceivNV() without current GL context";
  LOG(ERROR) << "Trying to call glGetFenceivNV() without current GL context";
}

void NoContextGLApi::glGetFloatvFn(GLenum pname, GLfloat* params) {
  NOTREACHED() << "Trying to call glGetFloatv() without current GL context";
  LOG(ERROR) << "Trying to call glGetFloatv() without current GL context";
}

void NoContextGLApi::glGetFloatvRobustANGLEFn(GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLfloat* data) {
  NOTREACHED()
      << "Trying to call glGetFloatvRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetFloatvRobustANGLE() without current GL context";
}

GLint NoContextGLApi::glGetFragDataIndexFn(GLuint program, const char* name) {
  NOTREACHED()
      << "Trying to call glGetFragDataIndex() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetFragDataIndex() without current GL context";
  return 0;
}

GLint NoContextGLApi::glGetFragDataLocationFn(GLuint program,
                                              const char* name) {
  NOTREACHED()
      << "Trying to call glGetFragDataLocation() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetFragDataLocation() without current GL context";
  return 0;
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivEXTFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLint* params) {
  NOTREACHED() << "Trying to call glGetFramebufferAttachmentParameterivEXT() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetFramebufferAttachmentParameterivEXT() "
                "without current GL context";
}

void NoContextGLApi::glGetFramebufferAttachmentParameterivRobustANGLEFn(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NOTREACHED() << "Trying to call "
                  "glGetFramebufferAttachmentParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call "
                "glGetFramebufferAttachmentParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetFramebufferParameterivRobustANGLEFn(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufSize,
                                                              GLsizei* length,
                                                              GLint* params) {
  NOTREACHED() << "Trying to call glGetFramebufferParameterivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetFramebufferParameterivRobustANGLE() "
                "without current GL context";
}

GLenum NoContextGLApi::glGetGraphicsResetStatusARBFn(void) {
  NOTREACHED() << "Trying to call glGetGraphicsResetStatusARB() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetGraphicsResetStatusARB() without current "
                "GL context";
  return static_cast<GLenum>(0);
}

void NoContextGLApi::glGetInteger64i_vFn(GLenum target,
                                         GLuint index,
                                         GLint64* data) {
  NOTREACHED()
      << "Trying to call glGetInteger64i_v() without current GL context";
  LOG(ERROR) << "Trying to call glGetInteger64i_v() without current GL context";
}

void NoContextGLApi::glGetInteger64i_vRobustANGLEFn(GLenum target,
                                                    GLuint index,
                                                    GLsizei bufSize,
                                                    GLsizei* length,
                                                    GLint64* data) {
  NOTREACHED() << "Trying to call glGetInteger64i_vRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetInteger64i_vRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetInteger64vFn(GLenum pname, GLint64* params) {
  NOTREACHED() << "Trying to call glGetInteger64v() without current GL context";
  LOG(ERROR) << "Trying to call glGetInteger64v() without current GL context";
}

void NoContextGLApi::glGetInteger64vRobustANGLEFn(GLenum pname,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint64* data) {
  NOTREACHED() << "Trying to call glGetInteger64vRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetInteger64vRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetIntegeri_vFn(GLenum target,
                                       GLuint index,
                                       GLint* data) {
  NOTREACHED() << "Trying to call glGetIntegeri_v() without current GL context";
  LOG(ERROR) << "Trying to call glGetIntegeri_v() without current GL context";
}

void NoContextGLApi::glGetIntegeri_vRobustANGLEFn(GLenum target,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* data) {
  NOTREACHED() << "Trying to call glGetIntegeri_vRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetIntegeri_vRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  NOTREACHED() << "Trying to call glGetIntegerv() without current GL context";
  LOG(ERROR) << "Trying to call glGetIntegerv() without current GL context";
}

void NoContextGLApi::glGetIntegervRobustANGLEFn(GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* data) {
  NOTREACHED()
      << "Trying to call glGetIntegervRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetIntegervRobustANGLE() without current GL context";
}

void NoContextGLApi::glGetInternalformativFn(GLenum target,
                                             GLenum internalformat,
                                             GLenum pname,
                                             GLsizei bufSize,
                                             GLint* params) {
  NOTREACHED()
      << "Trying to call glGetInternalformativ() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetInternalformativ() without current GL context";
}

void NoContextGLApi::glGetInternalformativRobustANGLEFn(GLenum target,
                                                        GLenum internalformat,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLint* params) {
  NOTREACHED() << "Trying to call glGetInternalformativRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetInternalformativRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetMultisamplefvRobustANGLEFn(GLenum pname,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLfloat* val) {
  NOTREACHED() << "Trying to call glGetMultisamplefvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetMultisamplefvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetnUniformfvRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLfloat* params) {
  NOTREACHED() << "Trying to call glGetnUniformfvRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetnUniformfvRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetnUniformivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLint* params) {
  NOTREACHED() << "Trying to call glGetnUniformivRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetnUniformivRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetnUniformuivRobustANGLEFn(GLuint program,
                                                   GLint location,
                                                   GLsizei bufSize,
                                                   GLsizei* length,
                                                   GLuint* params) {
  NOTREACHED() << "Trying to call glGetnUniformuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetnUniformuivRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetObjectLabelFn(GLenum identifier,
                                        GLuint name,
                                        GLsizei bufSize,
                                        GLsizei* length,
                                        char* label) {
  NOTREACHED()
      << "Trying to call glGetObjectLabel() without current GL context";
  LOG(ERROR) << "Trying to call glGetObjectLabel() without current GL context";
}

void NoContextGLApi::glGetObjectPtrLabelFn(void* ptr,
                                           GLsizei bufSize,
                                           GLsizei* length,
                                           char* label) {
  NOTREACHED()
      << "Trying to call glGetObjectPtrLabel() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetObjectPtrLabel() without current GL context";
}

void NoContextGLApi::glGetPointervFn(GLenum pname, void** params) {
  NOTREACHED() << "Trying to call glGetPointerv() without current GL context";
  LOG(ERROR) << "Trying to call glGetPointerv() without current GL context";
}

void NoContextGLApi::glGetPointervRobustANGLERobustANGLEFn(GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           void** params) {
  NOTREACHED() << "Trying to call glGetPointervRobustANGLERobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetPointervRobustANGLERobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetProgramBinaryFn(GLuint program,
                                          GLsizei bufSize,
                                          GLsizei* length,
                                          GLenum* binaryFormat,
                                          GLvoid* binary) {
  NOTREACHED()
      << "Trying to call glGetProgramBinary() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetProgramBinary() without current GL context";
}

void NoContextGLApi::glGetProgramInfoLogFn(GLuint program,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           char* infolog) {
  NOTREACHED()
      << "Trying to call glGetProgramInfoLog() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetProgramInfoLog() without current GL context";
}

void NoContextGLApi::glGetProgramInterfaceivFn(GLuint program,
                                               GLenum programInterface,
                                               GLenum pname,
                                               GLint* params) {
  NOTREACHED()
      << "Trying to call glGetProgramInterfaceiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetProgramInterfaceiv() without current GL context";
}

void NoContextGLApi::glGetProgramInterfaceivRobustANGLEFn(
    GLuint program,
    GLenum programInterface,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  NOTREACHED() << "Trying to call glGetProgramInterfaceivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetProgramInterfaceivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetProgramivFn(GLuint program,
                                      GLenum pname,
                                      GLint* params) {
  NOTREACHED() << "Trying to call glGetProgramiv() without current GL context";
  LOG(ERROR) << "Trying to call glGetProgramiv() without current GL context";
}

void NoContextGLApi::glGetProgramivRobustANGLEFn(GLuint program,
                                                 GLenum pname,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NOTREACHED() << "Trying to call glGetProgramivRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetProgramivRobustANGLE() without current GL "
                "context";
}

void NoContextGLApi::glGetProgramResourceivFn(GLuint program,
                                              GLenum programInterface,
                                              GLuint index,
                                              GLsizei propCount,
                                              const GLenum* props,
                                              GLsizei bufSize,
                                              GLsizei* length,
                                              GLint* params) {
  NOTREACHED()
      << "Trying to call glGetProgramResourceiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetProgramResourceiv() without current GL context";
}

GLint NoContextGLApi::glGetProgramResourceLocationFn(GLuint program,
                                                     GLenum programInterface,
                                                     const char* name) {
  NOTREACHED() << "Trying to call glGetProgramResourceLocation() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetProgramResourceLocation() without current "
                "GL context";
  return 0;
}

void NoContextGLApi::glGetProgramResourceNameFn(GLuint program,
                                                GLenum programInterface,
                                                GLuint index,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLchar* name) {
  NOTREACHED()
      << "Trying to call glGetProgramResourceName() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetProgramResourceName() without current GL context";
}

void NoContextGLApi::glGetQueryivFn(GLenum target,
                                    GLenum pname,
                                    GLint* params) {
  NOTREACHED() << "Trying to call glGetQueryiv() without current GL context";
  LOG(ERROR) << "Trying to call glGetQueryiv() without current GL context";
}

void NoContextGLApi::glGetQueryivRobustANGLEFn(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               GLint* params) {
  NOTREACHED()
      << "Trying to call glGetQueryivRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetQueryivRobustANGLE() without current GL context";
}

void NoContextGLApi::glGetQueryObjecti64vFn(GLuint id,
                                            GLenum pname,
                                            GLint64* params) {
  NOTREACHED()
      << "Trying to call glGetQueryObjecti64v() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetQueryObjecti64v() without current GL context";
}

void NoContextGLApi::glGetQueryObjecti64vRobustANGLEFn(GLuint id,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint64* params) {
  NOTREACHED() << "Trying to call glGetQueryObjecti64vRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetQueryObjecti64vRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetQueryObjectivFn(GLuint id,
                                          GLenum pname,
                                          GLint* params) {
  NOTREACHED()
      << "Trying to call glGetQueryObjectiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetQueryObjectiv() without current GL context";
}

void NoContextGLApi::glGetQueryObjectivRobustANGLEFn(GLuint id,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLint* params) {
  NOTREACHED() << "Trying to call glGetQueryObjectivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetQueryObjectivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetQueryObjectui64vFn(GLuint id,
                                             GLenum pname,
                                             GLuint64* params) {
  NOTREACHED()
      << "Trying to call glGetQueryObjectui64v() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetQueryObjectui64v() without current GL context";
}

void NoContextGLApi::glGetQueryObjectui64vRobustANGLEFn(GLuint id,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint64* params) {
  NOTREACHED() << "Trying to call glGetQueryObjectui64vRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetQueryObjectui64vRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetQueryObjectuivFn(GLuint id,
                                           GLenum pname,
                                           GLuint* params) {
  NOTREACHED()
      << "Trying to call glGetQueryObjectuiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetQueryObjectuiv() without current GL context";
}

void NoContextGLApi::glGetQueryObjectuivRobustANGLEFn(GLuint id,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLuint* params) {
  NOTREACHED() << "Trying to call glGetQueryObjectuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetQueryObjectuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetRenderbufferParameterivEXTFn(GLenum target,
                                                       GLenum pname,
                                                       GLint* params) {
  NOTREACHED() << "Trying to call glGetRenderbufferParameterivEXT() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetRenderbufferParameterivEXT() without "
                "current GL context";
}

void NoContextGLApi::glGetRenderbufferParameterivRobustANGLEFn(GLenum target,
                                                               GLenum pname,
                                                               GLsizei bufSize,
                                                               GLsizei* length,
                                                               GLint* params) {
  NOTREACHED() << "Trying to call glGetRenderbufferParameterivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetRenderbufferParameterivRobustANGLE() "
                "without current GL context";
}

void NoContextGLApi::glGetSamplerParameterfvFn(GLuint sampler,
                                               GLenum pname,
                                               GLfloat* params) {
  NOTREACHED()
      << "Trying to call glGetSamplerParameterfv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetSamplerParameterfv() without current GL context";
}

void NoContextGLApi::glGetSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLfloat* params) {
  NOTREACHED() << "Trying to call glGetSamplerParameterfvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetSamplerParameterfvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NOTREACHED() << "Trying to call glGetSamplerParameterIivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetSamplerParameterIivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            GLuint* params) {
  NOTREACHED() << "Trying to call glGetSamplerParameterIuivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetSamplerParameterIuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetSamplerParameterivFn(GLuint sampler,
                                               GLenum pname,
                                               GLint* params) {
  NOTREACHED()
      << "Trying to call glGetSamplerParameteriv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetSamplerParameteriv() without current GL context";
}

void NoContextGLApi::glGetSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                          GLenum pname,
                                                          GLsizei bufSize,
                                                          GLsizei* length,
                                                          GLint* params) {
  NOTREACHED() << "Trying to call glGetSamplerParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetSamplerParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetShaderInfoLogFn(GLuint shader,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          char* infolog) {
  NOTREACHED()
      << "Trying to call glGetShaderInfoLog() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetShaderInfoLog() without current GL context";
}

void NoContextGLApi::glGetShaderivFn(GLuint shader,
                                     GLenum pname,
                                     GLint* params) {
  NOTREACHED() << "Trying to call glGetShaderiv() without current GL context";
  LOG(ERROR) << "Trying to call glGetShaderiv() without current GL context";
}

void NoContextGLApi::glGetShaderivRobustANGLEFn(GLuint shader,
                                                GLenum pname,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                GLint* params) {
  NOTREACHED()
      << "Trying to call glGetShaderivRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetShaderivRobustANGLE() without current GL context";
}

void NoContextGLApi::glGetShaderPrecisionFormatFn(GLenum shadertype,
                                                  GLenum precisiontype,
                                                  GLint* range,
                                                  GLint* precision) {
  NOTREACHED() << "Trying to call glGetShaderPrecisionFormat() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetShaderPrecisionFormat() without current "
                "GL context";
}

void NoContextGLApi::glGetShaderSourceFn(GLuint shader,
                                         GLsizei bufsize,
                                         GLsizei* length,
                                         char* source) {
  NOTREACHED()
      << "Trying to call glGetShaderSource() without current GL context";
  LOG(ERROR) << "Trying to call glGetShaderSource() without current GL context";
}

const GLubyte* NoContextGLApi::glGetStringFn(GLenum name) {
  NOTREACHED() << "Trying to call glGetString() without current GL context";
  LOG(ERROR) << "Trying to call glGetString() without current GL context";
  return NULL;
}

const GLubyte* NoContextGLApi::glGetStringiFn(GLenum name, GLuint index) {
  NOTREACHED() << "Trying to call glGetStringi() without current GL context";
  LOG(ERROR) << "Trying to call glGetStringi() without current GL context";
  return NULL;
}

void NoContextGLApi::glGetSyncivFn(GLsync sync,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   GLsizei* length,
                                   GLint* values) {
  NOTREACHED() << "Trying to call glGetSynciv() without current GL context";
  LOG(ERROR) << "Trying to call glGetSynciv() without current GL context";
}

void NoContextGLApi::glGetTexLevelParameterfvFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLfloat* params) {
  NOTREACHED()
      << "Trying to call glGetTexLevelParameterfv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetTexLevelParameterfv() without current GL context";
}

void NoContextGLApi::glGetTexLevelParameterfvRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLfloat* params) {
  NOTREACHED() << "Trying to call glGetTexLevelParameterfvRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetTexLevelParameterfvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTexLevelParameterivFn(GLenum target,
                                                GLint level,
                                                GLenum pname,
                                                GLint* params) {
  NOTREACHED()
      << "Trying to call glGetTexLevelParameteriv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetTexLevelParameteriv() without current GL context";
}

void NoContextGLApi::glGetTexLevelParameterivRobustANGLEFn(GLenum target,
                                                           GLint level,
                                                           GLenum pname,
                                                           GLsizei bufSize,
                                                           GLsizei* length,
                                                           GLint* params) {
  NOTREACHED() << "Trying to call glGetTexLevelParameterivRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetTexLevelParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTexParameterfvFn(GLenum target,
                                           GLenum pname,
                                           GLfloat* params) {
  NOTREACHED()
      << "Trying to call glGetTexParameterfv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetTexParameterfv() without current GL context";
}

void NoContextGLApi::glGetTexParameterfvRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NOTREACHED() << "Trying to call glGetTexParameterfvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTexParameterfvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTexParameterIivRobustANGLEFn(GLenum target,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NOTREACHED() << "Trying to call glGetTexParameterIivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTexParameterIivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTexParameterIuivRobustANGLEFn(GLenum target,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NOTREACHED() << "Trying to call glGetTexParameterIuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTexParameterIuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTexParameterivFn(GLenum target,
                                           GLenum pname,
                                           GLint* params) {
  NOTREACHED()
      << "Trying to call glGetTexParameteriv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetTexParameteriv() without current GL context";
}

void NoContextGLApi::glGetTexParameterivRobustANGLEFn(GLenum target,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NOTREACHED() << "Trying to call glGetTexParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTexParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetTransformFeedbackVaryingFn(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLsizei* size,
                                                     GLenum* type,
                                                     char* name) {
  NOTREACHED() << "Trying to call glGetTransformFeedbackVarying() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTransformFeedbackVarying() without "
                "current GL context";
}

void NoContextGLApi::glGetTranslatedShaderSourceANGLEFn(GLuint shader,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        char* source) {
  NOTREACHED() << "Trying to call glGetTranslatedShaderSourceANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetTranslatedShaderSourceANGLE() without "
                "current GL context";
}

GLuint NoContextGLApi::glGetUniformBlockIndexFn(GLuint program,
                                                const char* uniformBlockName) {
  NOTREACHED()
      << "Trying to call glGetUniformBlockIndex() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetUniformBlockIndex() without current GL context";
  return 0U;
}

void NoContextGLApi::glGetUniformfvFn(GLuint program,
                                      GLint location,
                                      GLfloat* params) {
  NOTREACHED() << "Trying to call glGetUniformfv() without current GL context";
  LOG(ERROR) << "Trying to call glGetUniformfv() without current GL context";
}

void NoContextGLApi::glGetUniformfvRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLfloat* params) {
  NOTREACHED() << "Trying to call glGetUniformfvRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetUniformfvRobustANGLE() without current GL "
                "context";
}

void NoContextGLApi::glGetUniformIndicesFn(GLuint program,
                                           GLsizei uniformCount,
                                           const char* const* uniformNames,
                                           GLuint* uniformIndices) {
  NOTREACHED()
      << "Trying to call glGetUniformIndices() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetUniformIndices() without current GL context";
}

void NoContextGLApi::glGetUniformivFn(GLuint program,
                                      GLint location,
                                      GLint* params) {
  NOTREACHED() << "Trying to call glGetUniformiv() without current GL context";
  LOG(ERROR) << "Trying to call glGetUniformiv() without current GL context";
}

void NoContextGLApi::glGetUniformivRobustANGLEFn(GLuint program,
                                                 GLint location,
                                                 GLsizei bufSize,
                                                 GLsizei* length,
                                                 GLint* params) {
  NOTREACHED() << "Trying to call glGetUniformivRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetUniformivRobustANGLE() without current GL "
                "context";
}

GLint NoContextGLApi::glGetUniformLocationFn(GLuint program, const char* name) {
  NOTREACHED()
      << "Trying to call glGetUniformLocation() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetUniformLocation() without current GL context";
  return 0;
}

void NoContextGLApi::glGetUniformuivFn(GLuint program,
                                       GLint location,
                                       GLuint* params) {
  NOTREACHED() << "Trying to call glGetUniformuiv() without current GL context";
  LOG(ERROR) << "Trying to call glGetUniformuiv() without current GL context";
}

void NoContextGLApi::glGetUniformuivRobustANGLEFn(GLuint program,
                                                  GLint location,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  GLuint* params) {
  NOTREACHED() << "Trying to call glGetUniformuivRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetUniformuivRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glGetVertexAttribfvFn(GLuint index,
                                           GLenum pname,
                                           GLfloat* params) {
  NOTREACHED()
      << "Trying to call glGetVertexAttribfv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetVertexAttribfv() without current GL context";
}

void NoContextGLApi::glGetVertexAttribfvRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  NOTREACHED() << "Trying to call glGetVertexAttribfvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribfvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetVertexAttribIivRobustANGLEFn(GLuint index,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       GLsizei* length,
                                                       GLint* params) {
  NOTREACHED() << "Trying to call glGetVertexAttribIivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribIivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetVertexAttribIuivRobustANGLEFn(GLuint index,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei* length,
                                                        GLuint* params) {
  NOTREACHED() << "Trying to call glGetVertexAttribIuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribIuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetVertexAttribivFn(GLuint index,
                                           GLenum pname,
                                           GLint* params) {
  NOTREACHED()
      << "Trying to call glGetVertexAttribiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glGetVertexAttribiv() without current GL context";
}

void NoContextGLApi::glGetVertexAttribivRobustANGLEFn(GLuint index,
                                                      GLenum pname,
                                                      GLsizei bufSize,
                                                      GLsizei* length,
                                                      GLint* params) {
  NOTREACHED() << "Trying to call glGetVertexAttribivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glGetVertexAttribPointervFn(GLuint index,
                                                 GLenum pname,
                                                 void** pointer) {
  NOTREACHED() << "Trying to call glGetVertexAttribPointerv() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribPointerv() without current GL "
                "context";
}

void NoContextGLApi::glGetVertexAttribPointervRobustANGLEFn(GLuint index,
                                                            GLenum pname,
                                                            GLsizei bufSize,
                                                            GLsizei* length,
                                                            void** pointer) {
  NOTREACHED() << "Trying to call glGetVertexAttribPointervRobustANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glGetVertexAttribPointervRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glHintFn(GLenum target, GLenum mode) {
  NOTREACHED() << "Trying to call glHint() without current GL context";
  LOG(ERROR) << "Trying to call glHint() without current GL context";
}

void NoContextGLApi::glInsertEventMarkerEXTFn(GLsizei length,
                                              const char* marker) {
  NOTREACHED()
      << "Trying to call glInsertEventMarkerEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glInsertEventMarkerEXT() without current GL context";
}

void NoContextGLApi::glInvalidateFramebufferFn(GLenum target,
                                               GLsizei numAttachments,
                                               const GLenum* attachments) {
  NOTREACHED()
      << "Trying to call glInvalidateFramebuffer() without current GL context";
  LOG(ERROR)
      << "Trying to call glInvalidateFramebuffer() without current GL context";
}

void NoContextGLApi::glInvalidateSubFramebufferFn(GLenum target,
                                                  GLsizei numAttachments,
                                                  const GLenum* attachments,
                                                  GLint x,
                                                  GLint y,
                                                  GLint width,
                                                  GLint height) {
  NOTREACHED() << "Trying to call glInvalidateSubFramebuffer() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glInvalidateSubFramebuffer() without current "
                "GL context";
}

GLboolean NoContextGLApi::glIsBufferFn(GLuint buffer) {
  NOTREACHED() << "Trying to call glIsBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glIsBuffer() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsEnabledFn(GLenum cap) {
  NOTREACHED() << "Trying to call glIsEnabled() without current GL context";
  LOG(ERROR) << "Trying to call glIsEnabled() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceAPPLEFn(GLuint fence) {
  NOTREACHED() << "Trying to call glIsFenceAPPLE() without current GL context";
  LOG(ERROR) << "Trying to call glIsFenceAPPLE() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFenceNVFn(GLuint fence) {
  NOTREACHED() << "Trying to call glIsFenceNV() without current GL context";
  LOG(ERROR) << "Trying to call glIsFenceNV() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsFramebufferEXTFn(GLuint framebuffer) {
  NOTREACHED()
      << "Trying to call glIsFramebufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glIsFramebufferEXT() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsPathNVFn(GLuint path) {
  NOTREACHED() << "Trying to call glIsPathNV() without current GL context";
  LOG(ERROR) << "Trying to call glIsPathNV() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsProgramFn(GLuint program) {
  NOTREACHED() << "Trying to call glIsProgram() without current GL context";
  LOG(ERROR) << "Trying to call glIsProgram() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsQueryFn(GLuint query) {
  NOTREACHED() << "Trying to call glIsQuery() without current GL context";
  LOG(ERROR) << "Trying to call glIsQuery() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsRenderbufferEXTFn(GLuint renderbuffer) {
  NOTREACHED()
      << "Trying to call glIsRenderbufferEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glIsRenderbufferEXT() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSamplerFn(GLuint sampler) {
  NOTREACHED() << "Trying to call glIsSampler() without current GL context";
  LOG(ERROR) << "Trying to call glIsSampler() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsShaderFn(GLuint shader) {
  NOTREACHED() << "Trying to call glIsShader() without current GL context";
  LOG(ERROR) << "Trying to call glIsShader() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsSyncFn(GLsync sync) {
  NOTREACHED() << "Trying to call glIsSync() without current GL context";
  LOG(ERROR) << "Trying to call glIsSync() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTextureFn(GLuint texture) {
  NOTREACHED() << "Trying to call glIsTexture() without current GL context";
  LOG(ERROR) << "Trying to call glIsTexture() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsTransformFeedbackFn(GLuint id) {
  NOTREACHED()
      << "Trying to call glIsTransformFeedback() without current GL context";
  LOG(ERROR)
      << "Trying to call glIsTransformFeedback() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glIsVertexArrayOESFn(GLuint array) {
  NOTREACHED()
      << "Trying to call glIsVertexArrayOES() without current GL context";
  LOG(ERROR)
      << "Trying to call glIsVertexArrayOES() without current GL context";
  return GL_FALSE;
}

void NoContextGLApi::glLineWidthFn(GLfloat width) {
  NOTREACHED() << "Trying to call glLineWidth() without current GL context";
  LOG(ERROR) << "Trying to call glLineWidth() without current GL context";
}

void NoContextGLApi::glLinkProgramFn(GLuint program) {
  NOTREACHED() << "Trying to call glLinkProgram() without current GL context";
  LOG(ERROR) << "Trying to call glLinkProgram() without current GL context";
}

void* NoContextGLApi::glMapBufferFn(GLenum target, GLenum access) {
  NOTREACHED() << "Trying to call glMapBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glMapBuffer() without current GL context";
  return NULL;
}

void* NoContextGLApi::glMapBufferRangeFn(GLenum target,
                                         GLintptr offset,
                                         GLsizeiptr length,
                                         GLbitfield access) {
  NOTREACHED()
      << "Trying to call glMapBufferRange() without current GL context";
  LOG(ERROR) << "Trying to call glMapBufferRange() without current GL context";
  return NULL;
}

void NoContextGLApi::glMatrixLoadfEXTFn(GLenum matrixMode, const GLfloat* m) {
  NOTREACHED()
      << "Trying to call glMatrixLoadfEXT() without current GL context";
  LOG(ERROR) << "Trying to call glMatrixLoadfEXT() without current GL context";
}

void NoContextGLApi::glMatrixLoadIdentityEXTFn(GLenum matrixMode) {
  NOTREACHED()
      << "Trying to call glMatrixLoadIdentityEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glMatrixLoadIdentityEXT() without current GL context";
}

void NoContextGLApi::glMemoryBarrierEXTFn(GLbitfield barriers) {
  NOTREACHED()
      << "Trying to call glMemoryBarrierEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glMemoryBarrierEXT() without current GL context";
}

void NoContextGLApi::glObjectLabelFn(GLenum identifier,
                                     GLuint name,
                                     GLsizei length,
                                     const char* label) {
  NOTREACHED() << "Trying to call glObjectLabel() without current GL context";
  LOG(ERROR) << "Trying to call glObjectLabel() without current GL context";
}

void NoContextGLApi::glObjectPtrLabelFn(void* ptr,
                                        GLsizei length,
                                        const char* label) {
  NOTREACHED()
      << "Trying to call glObjectPtrLabel() without current GL context";
  LOG(ERROR) << "Trying to call glObjectPtrLabel() without current GL context";
}

void NoContextGLApi::glPathCommandsNVFn(GLuint path,
                                        GLsizei numCommands,
                                        const GLubyte* commands,
                                        GLsizei numCoords,
                                        GLenum coordType,
                                        const GLvoid* coords) {
  NOTREACHED()
      << "Trying to call glPathCommandsNV() without current GL context";
  LOG(ERROR) << "Trying to call glPathCommandsNV() without current GL context";
}

void NoContextGLApi::glPathParameterfNVFn(GLuint path,
                                          GLenum pname,
                                          GLfloat value) {
  NOTREACHED()
      << "Trying to call glPathParameterfNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glPathParameterfNV() without current GL context";
}

void NoContextGLApi::glPathParameteriNVFn(GLuint path,
                                          GLenum pname,
                                          GLint value) {
  NOTREACHED()
      << "Trying to call glPathParameteriNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glPathParameteriNV() without current GL context";
}

void NoContextGLApi::glPathStencilFuncNVFn(GLenum func,
                                           GLint ref,
                                           GLuint mask) {
  NOTREACHED()
      << "Trying to call glPathStencilFuncNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glPathStencilFuncNV() without current GL context";
}

void NoContextGLApi::glPauseTransformFeedbackFn(void) {
  NOTREACHED()
      << "Trying to call glPauseTransformFeedback() without current GL context";
  LOG(ERROR)
      << "Trying to call glPauseTransformFeedback() without current GL context";
}

void NoContextGLApi::glPixelStoreiFn(GLenum pname, GLint param) {
  NOTREACHED() << "Trying to call glPixelStorei() without current GL context";
  LOG(ERROR) << "Trying to call glPixelStorei() without current GL context";
}

void NoContextGLApi::glPointParameteriFn(GLenum pname, GLint param) {
  NOTREACHED()
      << "Trying to call glPointParameteri() without current GL context";
  LOG(ERROR) << "Trying to call glPointParameteri() without current GL context";
}

void NoContextGLApi::glPolygonOffsetFn(GLfloat factor, GLfloat units) {
  NOTREACHED() << "Trying to call glPolygonOffset() without current GL context";
  LOG(ERROR) << "Trying to call glPolygonOffset() without current GL context";
}

void NoContextGLApi::glPopDebugGroupFn() {
  NOTREACHED() << "Trying to call glPopDebugGroup() without current GL context";
  LOG(ERROR) << "Trying to call glPopDebugGroup() without current GL context";
}

void NoContextGLApi::glPopGroupMarkerEXTFn(void) {
  NOTREACHED()
      << "Trying to call glPopGroupMarkerEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glPopGroupMarkerEXT() without current GL context";
}

void NoContextGLApi::glPrimitiveRestartIndexFn(GLuint index) {
  NOTREACHED()
      << "Trying to call glPrimitiveRestartIndex() without current GL context";
  LOG(ERROR)
      << "Trying to call glPrimitiveRestartIndex() without current GL context";
}

void NoContextGLApi::glProgramBinaryFn(GLuint program,
                                       GLenum binaryFormat,
                                       const GLvoid* binary,
                                       GLsizei length) {
  NOTREACHED() << "Trying to call glProgramBinary() without current GL context";
  LOG(ERROR) << "Trying to call glProgramBinary() without current GL context";
}

void NoContextGLApi::glProgramParameteriFn(GLuint program,
                                           GLenum pname,
                                           GLint value) {
  NOTREACHED()
      << "Trying to call glProgramParameteri() without current GL context";
  LOG(ERROR)
      << "Trying to call glProgramParameteri() without current GL context";
}

void NoContextGLApi::glProgramPathFragmentInputGenNVFn(GLuint program,
                                                       GLint location,
                                                       GLenum genMode,
                                                       GLint components,
                                                       const GLfloat* coeffs) {
  NOTREACHED() << "Trying to call glProgramPathFragmentInputGenNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glProgramPathFragmentInputGenNV() without "
                "current GL context";
}

void NoContextGLApi::glPushDebugGroupFn(GLenum source,
                                        GLuint id,
                                        GLsizei length,
                                        const char* message) {
  NOTREACHED()
      << "Trying to call glPushDebugGroup() without current GL context";
  LOG(ERROR) << "Trying to call glPushDebugGroup() without current GL context";
}

void NoContextGLApi::glPushGroupMarkerEXTFn(GLsizei length,
                                            const char* marker) {
  NOTREACHED()
      << "Trying to call glPushGroupMarkerEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glPushGroupMarkerEXT() without current GL context";
}

void NoContextGLApi::glQueryCounterFn(GLuint id, GLenum target) {
  NOTREACHED() << "Trying to call glQueryCounter() without current GL context";
  LOG(ERROR) << "Trying to call glQueryCounter() without current GL context";
}

void NoContextGLApi::glReadBufferFn(GLenum src) {
  NOTREACHED() << "Trying to call glReadBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glReadBuffer() without current GL context";
}

void NoContextGLApi::glReadnPixelsRobustANGLEFn(GLint x,
                                                GLint y,
                                                GLsizei width,
                                                GLsizei height,
                                                GLenum format,
                                                GLenum type,
                                                GLsizei bufSize,
                                                GLsizei* length,
                                                void* data) {
  NOTREACHED()
      << "Trying to call glReadnPixelsRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glReadnPixelsRobustANGLE() without current GL context";
}

void NoContextGLApi::glReadPixelsFn(GLint x,
                                    GLint y,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLenum type,
                                    void* pixels) {
  NOTREACHED() << "Trying to call glReadPixels() without current GL context";
  LOG(ERROR) << "Trying to call glReadPixels() without current GL context";
}

void NoContextGLApi::glReadPixelsRobustANGLEFn(GLint x,
                                               GLint y,
                                               GLsizei width,
                                               GLsizei height,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               GLsizei* length,
                                               void* pixels) {
  NOTREACHED()
      << "Trying to call glReadPixelsRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glReadPixelsRobustANGLE() without current GL context";
}

void NoContextGLApi::glReleaseShaderCompilerFn(void) {
  NOTREACHED()
      << "Trying to call glReleaseShaderCompiler() without current GL context";
  LOG(ERROR)
      << "Trying to call glReleaseShaderCompiler() without current GL context";
}

void NoContextGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height) {
  NOTREACHED()
      << "Trying to call glRenderbufferStorageEXT() without current GL context";
  LOG(ERROR)
      << "Trying to call glRenderbufferStorageEXT() without current GL context";
}

void NoContextGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                        GLsizei samples,
                                                        GLenum internalformat,
                                                        GLsizei width,
                                                        GLsizei height) {
  NOTREACHED() << "Trying to call glRenderbufferStorageMultisample() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glRenderbufferStorageMultisample() without "
                "current GL context";
}

void NoContextGLApi::glRenderbufferStorageMultisampleANGLEFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NOTREACHED() << "Trying to call glRenderbufferStorageMultisampleANGLE() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glRenderbufferStorageMultisampleANGLE() "
                "without current GL context";
}

void NoContextGLApi::glRenderbufferStorageMultisampleEXTFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NOTREACHED() << "Trying to call glRenderbufferStorageMultisampleEXT() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glRenderbufferStorageMultisampleEXT() without "
                "current GL context";
}

void NoContextGLApi::glRenderbufferStorageMultisampleIMGFn(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  NOTREACHED() << "Trying to call glRenderbufferStorageMultisampleIMG() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glRenderbufferStorageMultisampleIMG() without "
                "current GL context";
}

void NoContextGLApi::glResumeTransformFeedbackFn(void) {
  NOTREACHED() << "Trying to call glResumeTransformFeedback() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glResumeTransformFeedback() without current GL "
                "context";
}

void NoContextGLApi::glSampleCoverageFn(GLclampf value, GLboolean invert) {
  NOTREACHED()
      << "Trying to call glSampleCoverage() without current GL context";
  LOG(ERROR) << "Trying to call glSampleCoverage() without current GL context";
}

void NoContextGLApi::glSamplerParameterfFn(GLuint sampler,
                                           GLenum pname,
                                           GLfloat param) {
  NOTREACHED()
      << "Trying to call glSamplerParameterf() without current GL context";
  LOG(ERROR)
      << "Trying to call glSamplerParameterf() without current GL context";
}

void NoContextGLApi::glSamplerParameterfvFn(GLuint sampler,
                                            GLenum pname,
                                            const GLfloat* params) {
  NOTREACHED()
      << "Trying to call glSamplerParameterfv() without current GL context";
  LOG(ERROR)
      << "Trying to call glSamplerParameterfv() without current GL context";
}

void NoContextGLApi::glSamplerParameterfvRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLfloat* param) {
  NOTREACHED() << "Trying to call glSamplerParameterfvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glSamplerParameterfvRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glSamplerParameteriFn(GLuint sampler,
                                           GLenum pname,
                                           GLint param) {
  NOTREACHED()
      << "Trying to call glSamplerParameteri() without current GL context";
  LOG(ERROR)
      << "Trying to call glSamplerParameteri() without current GL context";
}

void NoContextGLApi::glSamplerParameterIivRobustANGLEFn(GLuint sampler,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        const GLint* param) {
  NOTREACHED() << "Trying to call glSamplerParameterIivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glSamplerParameterIivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glSamplerParameterIuivRobustANGLEFn(GLuint sampler,
                                                         GLenum pname,
                                                         GLsizei bufSize,
                                                         const GLuint* param) {
  NOTREACHED() << "Trying to call glSamplerParameterIuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glSamplerParameterIuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glSamplerParameterivFn(GLuint sampler,
                                            GLenum pname,
                                            const GLint* params) {
  NOTREACHED()
      << "Trying to call glSamplerParameteriv() without current GL context";
  LOG(ERROR)
      << "Trying to call glSamplerParameteriv() without current GL context";
}

void NoContextGLApi::glSamplerParameterivRobustANGLEFn(GLuint sampler,
                                                       GLenum pname,
                                                       GLsizei bufSize,
                                                       const GLint* param) {
  NOTREACHED() << "Trying to call glSamplerParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glSamplerParameterivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glScissorFn(GLint x,
                                 GLint y,
                                 GLsizei width,
                                 GLsizei height) {
  NOTREACHED() << "Trying to call glScissor() without current GL context";
  LOG(ERROR) << "Trying to call glScissor() without current GL context";
}

void NoContextGLApi::glSetFenceAPPLEFn(GLuint fence) {
  NOTREACHED() << "Trying to call glSetFenceAPPLE() without current GL context";
  LOG(ERROR) << "Trying to call glSetFenceAPPLE() without current GL context";
}

void NoContextGLApi::glSetFenceNVFn(GLuint fence, GLenum condition) {
  NOTREACHED() << "Trying to call glSetFenceNV() without current GL context";
  LOG(ERROR) << "Trying to call glSetFenceNV() without current GL context";
}

void NoContextGLApi::glShaderBinaryFn(GLsizei n,
                                      const GLuint* shaders,
                                      GLenum binaryformat,
                                      const void* binary,
                                      GLsizei length) {
  NOTREACHED() << "Trying to call glShaderBinary() without current GL context";
  LOG(ERROR) << "Trying to call glShaderBinary() without current GL context";
}

void NoContextGLApi::glShaderSourceFn(GLuint shader,
                                      GLsizei count,
                                      const char* const* str,
                                      const GLint* length) {
  NOTREACHED() << "Trying to call glShaderSource() without current GL context";
  LOG(ERROR) << "Trying to call glShaderSource() without current GL context";
}

void NoContextGLApi::glStencilFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glStencilFillPathInstancedNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glStencilFillPathInstancedNV() without current "
                "GL context";
}

void NoContextGLApi::glStencilFillPathNVFn(GLuint path,
                                           GLenum fillMode,
                                           GLuint mask) {
  NOTREACHED()
      << "Trying to call glStencilFillPathNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glStencilFillPathNV() without current GL context";
}

void NoContextGLApi::glStencilFuncFn(GLenum func, GLint ref, GLuint mask) {
  NOTREACHED() << "Trying to call glStencilFunc() without current GL context";
  LOG(ERROR) << "Trying to call glStencilFunc() without current GL context";
}

void NoContextGLApi::glStencilFuncSeparateFn(GLenum face,
                                             GLenum func,
                                             GLint ref,
                                             GLuint mask) {
  NOTREACHED()
      << "Trying to call glStencilFuncSeparate() without current GL context";
  LOG(ERROR)
      << "Trying to call glStencilFuncSeparate() without current GL context";
}

void NoContextGLApi::glStencilMaskFn(GLuint mask) {
  NOTREACHED() << "Trying to call glStencilMask() without current GL context";
  LOG(ERROR) << "Trying to call glStencilMask() without current GL context";
}

void NoContextGLApi::glStencilMaskSeparateFn(GLenum face, GLuint mask) {
  NOTREACHED()
      << "Trying to call glStencilMaskSeparate() without current GL context";
  LOG(ERROR)
      << "Trying to call glStencilMaskSeparate() without current GL context";
}

void NoContextGLApi::glStencilOpFn(GLenum fail, GLenum zfail, GLenum zpass) {
  NOTREACHED() << "Trying to call glStencilOp() without current GL context";
  LOG(ERROR) << "Trying to call glStencilOp() without current GL context";
}

void NoContextGLApi::glStencilOpSeparateFn(GLenum face,
                                           GLenum fail,
                                           GLenum zfail,
                                           GLenum zpass) {
  NOTREACHED()
      << "Trying to call glStencilOpSeparate() without current GL context";
  LOG(ERROR)
      << "Trying to call glStencilOpSeparate() without current GL context";
}

void NoContextGLApi::glStencilStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glStencilStrokePathInstancedNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glStencilStrokePathInstancedNV() without "
                "current GL context";
}

void NoContextGLApi::glStencilStrokePathNVFn(GLuint path,
                                             GLint reference,
                                             GLuint mask) {
  NOTREACHED()
      << "Trying to call glStencilStrokePathNV() without current GL context";
  LOG(ERROR)
      << "Trying to call glStencilStrokePathNV() without current GL context";
}

void NoContextGLApi::glStencilThenCoverFillPathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLenum fillMode,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glStencilThenCoverFillPathInstancedNV() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glStencilThenCoverFillPathInstancedNV() "
                "without current GL context";
}

void NoContextGLApi::glStencilThenCoverFillPathNVFn(GLuint path,
                                                    GLenum fillMode,
                                                    GLuint mask,
                                                    GLenum coverMode) {
  NOTREACHED() << "Trying to call glStencilThenCoverFillPathNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glStencilThenCoverFillPathNV() without current "
                "GL context";
}

void NoContextGLApi::glStencilThenCoverStrokePathInstancedNVFn(
    GLsizei numPaths,
    GLenum pathNameType,
    const void* paths,
    GLuint pathBase,
    GLint ref,
    GLuint mask,
    GLenum coverMode,
    GLenum transformType,
    const GLfloat* transformValues) {
  NOTREACHED() << "Trying to call glStencilThenCoverStrokePathInstancedNV() "
                  "without current GL context";
  LOG(ERROR) << "Trying to call glStencilThenCoverStrokePathInstancedNV() "
                "without current GL context";
}

void NoContextGLApi::glStencilThenCoverStrokePathNVFn(GLuint path,
                                                      GLint reference,
                                                      GLuint mask,
                                                      GLenum coverMode) {
  NOTREACHED() << "Trying to call glStencilThenCoverStrokePathNV() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glStencilThenCoverStrokePathNV() without "
                "current GL context";
}

GLboolean NoContextGLApi::glTestFenceAPPLEFn(GLuint fence) {
  NOTREACHED()
      << "Trying to call glTestFenceAPPLE() without current GL context";
  LOG(ERROR) << "Trying to call glTestFenceAPPLE() without current GL context";
  return GL_FALSE;
}

GLboolean NoContextGLApi::glTestFenceNVFn(GLuint fence) {
  NOTREACHED() << "Trying to call glTestFenceNV() without current GL context";
  LOG(ERROR) << "Trying to call glTestFenceNV() without current GL context";
  return GL_FALSE;
}

void NoContextGLApi::glTexImage2DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NOTREACHED() << "Trying to call glTexImage2D() without current GL context";
  LOG(ERROR) << "Trying to call glTexImage2D() without current GL context";
}

void NoContextGLApi::glTexImage2DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NOTREACHED()
      << "Trying to call glTexImage2DRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glTexImage2DRobustANGLE() without current GL context";
}

void NoContextGLApi::glTexImage3DFn(GLenum target,
                                    GLint level,
                                    GLint internalformat,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth,
                                    GLint border,
                                    GLenum format,
                                    GLenum type,
                                    const void* pixels) {
  NOTREACHED() << "Trying to call glTexImage3D() without current GL context";
  LOG(ERROR) << "Trying to call glTexImage3D() without current GL context";
}

void NoContextGLApi::glTexImage3DRobustANGLEFn(GLenum target,
                                               GLint level,
                                               GLint internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               GLint border,
                                               GLenum format,
                                               GLenum type,
                                               GLsizei bufSize,
                                               const void* pixels) {
  NOTREACHED()
      << "Trying to call glTexImage3DRobustANGLE() without current GL context";
  LOG(ERROR)
      << "Trying to call glTexImage3DRobustANGLE() without current GL context";
}

void NoContextGLApi::glTexParameterfFn(GLenum target,
                                       GLenum pname,
                                       GLfloat param) {
  NOTREACHED() << "Trying to call glTexParameterf() without current GL context";
  LOG(ERROR) << "Trying to call glTexParameterf() without current GL context";
}

void NoContextGLApi::glTexParameterfvFn(GLenum target,
                                        GLenum pname,
                                        const GLfloat* params) {
  NOTREACHED()
      << "Trying to call glTexParameterfv() without current GL context";
  LOG(ERROR) << "Trying to call glTexParameterfv() without current GL context";
}

void NoContextGLApi::glTexParameterfvRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLfloat* params) {
  NOTREACHED() << "Trying to call glTexParameterfvRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glTexParameterfvRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glTexParameteriFn(GLenum target,
                                       GLenum pname,
                                       GLint param) {
  NOTREACHED() << "Trying to call glTexParameteri() without current GL context";
  LOG(ERROR) << "Trying to call glTexParameteri() without current GL context";
}

void NoContextGLApi::glTexParameterIivRobustANGLEFn(GLenum target,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    const GLint* params) {
  NOTREACHED() << "Trying to call glTexParameterIivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glTexParameterIivRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glTexParameterIuivRobustANGLEFn(GLenum target,
                                                     GLenum pname,
                                                     GLsizei bufSize,
                                                     const GLuint* params) {
  NOTREACHED() << "Trying to call glTexParameterIuivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glTexParameterIuivRobustANGLE() without "
                "current GL context";
}

void NoContextGLApi::glTexParameterivFn(GLenum target,
                                        GLenum pname,
                                        const GLint* params) {
  NOTREACHED()
      << "Trying to call glTexParameteriv() without current GL context";
  LOG(ERROR) << "Trying to call glTexParameteriv() without current GL context";
}

void NoContextGLApi::glTexParameterivRobustANGLEFn(GLenum target,
                                                   GLenum pname,
                                                   GLsizei bufSize,
                                                   const GLint* params) {
  NOTREACHED() << "Trying to call glTexParameterivRobustANGLE() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glTexParameterivRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glTexStorage2DEXTFn(GLenum target,
                                         GLsizei levels,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height) {
  NOTREACHED()
      << "Trying to call glTexStorage2DEXT() without current GL context";
  LOG(ERROR) << "Trying to call glTexStorage2DEXT() without current GL context";
}

void NoContextGLApi::glTexStorage3DFn(GLenum target,
                                      GLsizei levels,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth) {
  NOTREACHED() << "Trying to call glTexStorage3D() without current GL context";
  LOG(ERROR) << "Trying to call glTexStorage3D() without current GL context";
}

void NoContextGLApi::glTexSubImage2DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NOTREACHED() << "Trying to call glTexSubImage2D() without current GL context";
  LOG(ERROR) << "Trying to call glTexSubImage2D() without current GL context";
}

void NoContextGLApi::glTexSubImage2DRobustANGLEFn(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLenum format,
                                                  GLenum type,
                                                  GLsizei bufSize,
                                                  const void* pixels) {
  NOTREACHED() << "Trying to call glTexSubImage2DRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glTexSubImage2DRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glTexSubImage3DFn(GLenum target,
                                       GLint level,
                                       GLint xoffset,
                                       GLint yoffset,
                                       GLint zoffset,
                                       GLsizei width,
                                       GLsizei height,
                                       GLsizei depth,
                                       GLenum format,
                                       GLenum type,
                                       const void* pixels) {
  NOTREACHED() << "Trying to call glTexSubImage3D() without current GL context";
  LOG(ERROR) << "Trying to call glTexSubImage3D() without current GL context";
}

void NoContextGLApi::glTexSubImage3DRobustANGLEFn(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint zoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLenum format,
                                                  GLenum type,
                                                  GLsizei bufSize,
                                                  const void* pixels) {
  NOTREACHED() << "Trying to call glTexSubImage3DRobustANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glTexSubImage3DRobustANGLE() without current "
                "GL context";
}

void NoContextGLApi::glTransformFeedbackVaryingsFn(GLuint program,
                                                   GLsizei count,
                                                   const char* const* varyings,
                                                   GLenum bufferMode) {
  NOTREACHED() << "Trying to call glTransformFeedbackVaryings() without "
                  "current GL context";
  LOG(ERROR) << "Trying to call glTransformFeedbackVaryings() without current "
                "GL context";
}

void NoContextGLApi::glUniform1fFn(GLint location, GLfloat x) {
  NOTREACHED() << "Trying to call glUniform1f() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1f() without current GL context";
}

void NoContextGLApi::glUniform1fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NOTREACHED() << "Trying to call glUniform1fv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1fv() without current GL context";
}

void NoContextGLApi::glUniform1iFn(GLint location, GLint x) {
  NOTREACHED() << "Trying to call glUniform1i() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1i() without current GL context";
}

void NoContextGLApi::glUniform1ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NOTREACHED() << "Trying to call glUniform1iv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1iv() without current GL context";
}

void NoContextGLApi::glUniform1uiFn(GLint location, GLuint v0) {
  NOTREACHED() << "Trying to call glUniform1ui() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1ui() without current GL context";
}

void NoContextGLApi::glUniform1uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NOTREACHED() << "Trying to call glUniform1uiv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform1uiv() without current GL context";
}

void NoContextGLApi::glUniform2fFn(GLint location, GLfloat x, GLfloat y) {
  NOTREACHED() << "Trying to call glUniform2f() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2f() without current GL context";
}

void NoContextGLApi::glUniform2fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NOTREACHED() << "Trying to call glUniform2fv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2fv() without current GL context";
}

void NoContextGLApi::glUniform2iFn(GLint location, GLint x, GLint y) {
  NOTREACHED() << "Trying to call glUniform2i() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2i() without current GL context";
}

void NoContextGLApi::glUniform2ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NOTREACHED() << "Trying to call glUniform2iv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2iv() without current GL context";
}

void NoContextGLApi::glUniform2uiFn(GLint location, GLuint v0, GLuint v1) {
  NOTREACHED() << "Trying to call glUniform2ui() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2ui() without current GL context";
}

void NoContextGLApi::glUniform2uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NOTREACHED() << "Trying to call glUniform2uiv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform2uiv() without current GL context";
}

void NoContextGLApi::glUniform3fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z) {
  NOTREACHED() << "Trying to call glUniform3f() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3f() without current GL context";
}

void NoContextGLApi::glUniform3fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NOTREACHED() << "Trying to call glUniform3fv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3fv() without current GL context";
}

void NoContextGLApi::glUniform3iFn(GLint location, GLint x, GLint y, GLint z) {
  NOTREACHED() << "Trying to call glUniform3i() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3i() without current GL context";
}

void NoContextGLApi::glUniform3ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NOTREACHED() << "Trying to call glUniform3iv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3iv() without current GL context";
}

void NoContextGLApi::glUniform3uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2) {
  NOTREACHED() << "Trying to call glUniform3ui() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3ui() without current GL context";
}

void NoContextGLApi::glUniform3uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NOTREACHED() << "Trying to call glUniform3uiv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform3uiv() without current GL context";
}

void NoContextGLApi::glUniform4fFn(GLint location,
                                   GLfloat x,
                                   GLfloat y,
                                   GLfloat z,
                                   GLfloat w) {
  NOTREACHED() << "Trying to call glUniform4f() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4f() without current GL context";
}

void NoContextGLApi::glUniform4fvFn(GLint location,
                                    GLsizei count,
                                    const GLfloat* v) {
  NOTREACHED() << "Trying to call glUniform4fv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4fv() without current GL context";
}

void NoContextGLApi::glUniform4iFn(GLint location,
                                   GLint x,
                                   GLint y,
                                   GLint z,
                                   GLint w) {
  NOTREACHED() << "Trying to call glUniform4i() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4i() without current GL context";
}

void NoContextGLApi::glUniform4ivFn(GLint location,
                                    GLsizei count,
                                    const GLint* v) {
  NOTREACHED() << "Trying to call glUniform4iv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4iv() without current GL context";
}

void NoContextGLApi::glUniform4uiFn(GLint location,
                                    GLuint v0,
                                    GLuint v1,
                                    GLuint v2,
                                    GLuint v3) {
  NOTREACHED() << "Trying to call glUniform4ui() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4ui() without current GL context";
}

void NoContextGLApi::glUniform4uivFn(GLint location,
                                     GLsizei count,
                                     const GLuint* v) {
  NOTREACHED() << "Trying to call glUniform4uiv() without current GL context";
  LOG(ERROR) << "Trying to call glUniform4uiv() without current GL context";
}

void NoContextGLApi::glUniformBlockBindingFn(GLuint program,
                                             GLuint uniformBlockIndex,
                                             GLuint uniformBlockBinding) {
  NOTREACHED()
      << "Trying to call glUniformBlockBinding() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformBlockBinding() without current GL context";
}

void NoContextGLApi::glUniformMatrix2fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix2fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix2fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix2x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix2x3fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix2x3fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix2x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix2x4fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix2x4fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix3fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix3fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix3fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix3x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix3x2fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix3x2fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix3x4fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix3x4fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix3x4fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix4fvFn(GLint location,
                                          GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix4fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix4fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix4x2fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix4x2fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix4x2fv() without current GL context";
}

void NoContextGLApi::glUniformMatrix4x3fvFn(GLint location,
                                            GLsizei count,
                                            GLboolean transpose,
                                            const GLfloat* value) {
  NOTREACHED()
      << "Trying to call glUniformMatrix4x3fv() without current GL context";
  LOG(ERROR)
      << "Trying to call glUniformMatrix4x3fv() without current GL context";
}

GLboolean NoContextGLApi::glUnmapBufferFn(GLenum target) {
  NOTREACHED() << "Trying to call glUnmapBuffer() without current GL context";
  LOG(ERROR) << "Trying to call glUnmapBuffer() without current GL context";
  return GL_FALSE;
}

void NoContextGLApi::glUseProgramFn(GLuint program) {
  NOTREACHED() << "Trying to call glUseProgram() without current GL context";
  LOG(ERROR) << "Trying to call glUseProgram() without current GL context";
}

void NoContextGLApi::glValidateProgramFn(GLuint program) {
  NOTREACHED()
      << "Trying to call glValidateProgram() without current GL context";
  LOG(ERROR) << "Trying to call glValidateProgram() without current GL context";
}

void NoContextGLApi::glVertexAttrib1fFn(GLuint indx, GLfloat x) {
  NOTREACHED()
      << "Trying to call glVertexAttrib1f() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib1f() without current GL context";
}

void NoContextGLApi::glVertexAttrib1fvFn(GLuint indx, const GLfloat* values) {
  NOTREACHED()
      << "Trying to call glVertexAttrib1fv() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib1fv() without current GL context";
}

void NoContextGLApi::glVertexAttrib2fFn(GLuint indx, GLfloat x, GLfloat y) {
  NOTREACHED()
      << "Trying to call glVertexAttrib2f() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib2f() without current GL context";
}

void NoContextGLApi::glVertexAttrib2fvFn(GLuint indx, const GLfloat* values) {
  NOTREACHED()
      << "Trying to call glVertexAttrib2fv() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib2fv() without current GL context";
}

void NoContextGLApi::glVertexAttrib3fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z) {
  NOTREACHED()
      << "Trying to call glVertexAttrib3f() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib3f() without current GL context";
}

void NoContextGLApi::glVertexAttrib3fvFn(GLuint indx, const GLfloat* values) {
  NOTREACHED()
      << "Trying to call glVertexAttrib3fv() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib3fv() without current GL context";
}

void NoContextGLApi::glVertexAttrib4fFn(GLuint indx,
                                        GLfloat x,
                                        GLfloat y,
                                        GLfloat z,
                                        GLfloat w) {
  NOTREACHED()
      << "Trying to call glVertexAttrib4f() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib4f() without current GL context";
}

void NoContextGLApi::glVertexAttrib4fvFn(GLuint indx, const GLfloat* values) {
  NOTREACHED()
      << "Trying to call glVertexAttrib4fv() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttrib4fv() without current GL context";
}

void NoContextGLApi::glVertexAttribDivisorANGLEFn(GLuint index,
                                                  GLuint divisor) {
  NOTREACHED() << "Trying to call glVertexAttribDivisorANGLE() without current "
                  "GL context";
  LOG(ERROR) << "Trying to call glVertexAttribDivisorANGLE() without current "
                "GL context";
}

void NoContextGLApi::glVertexAttribI4iFn(GLuint indx,
                                         GLint x,
                                         GLint y,
                                         GLint z,
                                         GLint w) {
  NOTREACHED()
      << "Trying to call glVertexAttribI4i() without current GL context";
  LOG(ERROR) << "Trying to call glVertexAttribI4i() without current GL context";
}

void NoContextGLApi::glVertexAttribI4ivFn(GLuint indx, const GLint* values) {
  NOTREACHED()
      << "Trying to call glVertexAttribI4iv() without current GL context";
  LOG(ERROR)
      << "Trying to call glVertexAttribI4iv() without current GL context";
}

void NoContextGLApi::glVertexAttribI4uiFn(GLuint indx,
                                          GLuint x,
                                          GLuint y,
                                          GLuint z,
                                          GLuint w) {
  NOTREACHED()
      << "Trying to call glVertexAttribI4ui() without current GL context";
  LOG(ERROR)
      << "Trying to call glVertexAttribI4ui() without current GL context";
}

void NoContextGLApi::glVertexAttribI4uivFn(GLuint indx, const GLuint* values) {
  NOTREACHED()
      << "Trying to call glVertexAttribI4uiv() without current GL context";
  LOG(ERROR)
      << "Trying to call glVertexAttribI4uiv() without current GL context";
}

void NoContextGLApi::glVertexAttribIPointerFn(GLuint indx,
                                              GLint size,
                                              GLenum type,
                                              GLsizei stride,
                                              const void* ptr) {
  NOTREACHED()
      << "Trying to call glVertexAttribIPointer() without current GL context";
  LOG(ERROR)
      << "Trying to call glVertexAttribIPointer() without current GL context";
}

void NoContextGLApi::glVertexAttribPointerFn(GLuint indx,
                                             GLint size,
                                             GLenum type,
                                             GLboolean normalized,
                                             GLsizei stride,
                                             const void* ptr) {
  NOTREACHED()
      << "Trying to call glVertexAttribPointer() without current GL context";
  LOG(ERROR)
      << "Trying to call glVertexAttribPointer() without current GL context";
}

void NoContextGLApi::glViewportFn(GLint x,
                                  GLint y,
                                  GLsizei width,
                                  GLsizei height) {
  NOTREACHED() << "Trying to call glViewport() without current GL context";
  LOG(ERROR) << "Trying to call glViewport() without current GL context";
}

GLenum NoContextGLApi::glWaitSyncFn(GLsync sync,
                                    GLbitfield flags,
                                    GLuint64 timeout) {
  NOTREACHED() << "Trying to call glWaitSync() without current GL context";
  LOG(ERROR) << "Trying to call glWaitSync() without current GL context";
  return static_cast<GLenum>(0);
}

}  // namespace gl
