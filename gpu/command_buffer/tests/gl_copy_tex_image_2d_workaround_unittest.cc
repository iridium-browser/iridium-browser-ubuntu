// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

#if defined(OS_MACOSX)
// A collection of tests that exercise the glCopyTexImage2D workaround. The
// parameter expresses different formats of the destination texture.
class GLCopyTexImage2DWorkaroundTest : public testing::TestWithParam<GLenum> {
 public:
  GLCopyTexImage2DWorkaroundTest() {}

 protected:
  void SetUp() override {
      base::CommandLine command_line(0, NULL);
      command_line.AppendSwitchASCII(
          switches::kGpuDriverBugWorkarounds,
          base::IntToString(gpu::USE_INTERMEDIARY_FOR_COPY_TEXTURE_IMAGE));
      gl_.InitializeWithCommandLine(GLManager::Options(), command_line);
      gl_.set_use_iosurface_memory_buffers(true);
      DCHECK(gl_.workarounds().use_intermediary_for_copy_texture_image);
  }

  void TearDown() override {
    GLTestHelper::CheckGLError("no errors", __LINE__);
    gl_.Destroy();
  }

  GLManager gl_;
};

INSTANTIATE_TEST_CASE_P(GLCopyTexImage2DWorkaroundTestWithParam,
                        GLCopyTexImage2DWorkaroundTest,
                        ::testing::Values(GL_RGBA));

TEST_P(GLCopyTexImage2DWorkaroundTest, UseIntermediaryTexture) {
  int width = 1;
  int height = 1;
  GLuint source_texture = 0;
  GLenum source_target = GL_TEXTURE_RECTANGLE_ARB;
  glGenTextures(1, &source_texture);
  glBindTexture(source_target, source_texture);
  glTexParameteri(source_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(source_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GLuint image_id = glCreateGpuMemoryBufferImageCHROMIUM(
      width, height, GL_RGBA, GL_READ_WRITE_CHROMIUM);
  ASSERT_NE(0u, image_id);
  glBindTexImage2DCHROMIUM(source_target, image_id);

  GLuint framebuffer = 0;
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, source_target, source_texture, 0);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            glCheckFramebufferStatus(GL_FRAMEBUFFER));


  GLenum dest_formats[] = {GL_RGBA, GL_RGB, GL_ALPHA, GL_LUMINANCE};
  const uint8_t expectations[4][4] = {
      {33, 44, 55, 66}, {33, 44, 55, 255}, {0, 0, 0, 66}, {33, 33, 33, 255}};
  for (size_t i = 0; i < sizeof(dest_formats) / sizeof(GLenum); ++i) {
    glClearColor(33.0 / 255.0, 44.0 / 255.0, 55.0 / 255.0, 66.0 / 255.0);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));

    GLuint dest_texture = 0;
    GLenum dest_target = GL_TEXTURE_2D;
    GLenum dest_format = dest_formats[i];
    glGenTextures(1, &dest_texture);
    glBindTexture(dest_target, dest_texture);
    glTexParameteri(dest_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(dest_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glCopyTexImage2D(dest_target, 0, dest_format, 0, 0, width, height, 0);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));

    // Check that bound textures haven't changed.
    GLint boundTexture = -1;
    glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &boundTexture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    EXPECT_EQ(static_cast<GLint>(source_texture), boundTexture);

    boundTexture = -1;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTexture);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    EXPECT_EQ(static_cast<GLint>(dest_texture), boundTexture);

    glClearColor(1.0 / 255.0, 2.0 / 255.0, 3.0 / 255.0, 4.0 / 255.0);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));

    glViewport(0, 0, width, height);
    GLTestHelper::DrawTextureQuad(dest_target, gfx::Size(width, height));

    // Verify.
    const uint8_t* expected = expectations[i];
    EXPECT_TRUE(
        GLTestHelper::CheckPixels(0, 0, 1, 1, 1 /* tolerance */, expected));
  }
}

#endif  // defined(OS_MACOSX)

}  // namespace gpu
