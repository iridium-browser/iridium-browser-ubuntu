//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class TransformFeedbackTest : public ANGLETest
{
  protected:
    TransformFeedbackTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        const std::string vertexShaderSource = SHADER_SOURCE
        (
            precision highp float;
            attribute vec4 position;

            void main()
            {
                gl_Position = position;
            }
        );

        const std::string fragmentShaderSource = SHADER_SOURCE
        (
            precision highp float;

            void main()
            {
                gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            }
        );

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        glGenBuffers(1, &mTransformFeedbackBuffer);
        mTransformFeedbackBufferSize = 1 << 24; // ~16MB
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBufferSize, NULL, GL_STATIC_DRAW);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteBuffers(1, &mTransformFeedbackBuffer);
        ANGLETest::TearDown();
    }

    GLuint mProgram;

    size_t mTransformFeedbackBufferSize;
    GLuint mTransformFeedbackBuffer;
};

TEST_P(TransformFeedbackTest, ZeroSizedViewport)
{
    // Set the program's transform feedback varyings (just gl_Position)
    const GLchar* transformFeedbackVaryings[] =
    {
        "gl_Position"
    };
    glTransformFeedbackVaryings(mProgram, ArraySize(transformFeedbackVaryings), transformFeedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(mProgram);

    // Re-link the program
    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    ASSERT_NE(linkStatus, 0);

    glUseProgram(mProgram);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_TRIANGLES);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    // Set a viewport that would result in no pixels being written to the framebuffer and draw
    // a quad
    glViewport(0, 0, 0, 0);

    drawQuad(mProgram, "position", 0.5f);

    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(primitivesWritten, 2);
}

// Test that XFB can write back vertices to a buffer and that we can draw from this buffer afterward.
TEST_P(TransformFeedbackTest, RecordAndDraw)
{
    // Set the program's transform feedback varyings (just gl_Position)
    const GLchar* transformFeedbackVaryings[] =
    {
        "gl_Position"
    };
    glTransformFeedbackVaryings(mProgram, ArraySize(transformFeedbackVaryings), transformFeedbackVaryings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(mProgram);

    // Re-link the program
    GLint linkStatus;
    glGetProgramiv(mProgram, GL_LINK_STATUS, &linkStatus);
    ASSERT_NE(linkStatus, 0);

    glUseProgram(mProgram);

    GLint positionLocation = glGetAttribLocation(mProgram, "position");

    // First pass: draw 6 points to the XFB buffer
    glEnable(GL_RASTERIZER_DISCARD);

    const GLfloat vertices[] =
    {
        -1.0f,  1.0f, 0.5f,
        -1.0f, -1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,

        -1.0f,  1.0f, 0.5f,
         1.0f, -1.0f, 0.5f,
         1.0f,  1.0f, 0.5f,
    };

    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(positionLocation);

    // Bind the buffer for transform feedback output and start transform feedback
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, mTransformFeedbackBuffer);
    glBeginTransformFeedback(GL_POINTS);

    // Create a query to check how many primitives were written
    GLuint primitivesWrittenQuery = 0;
    glGenQueries(1, &primitivesWrittenQuery);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, primitivesWrittenQuery);

    glDrawArrays(GL_POINTS, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    // End the query and transform feedkback
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndTransformFeedback();

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

    glDisable(GL_RASTERIZER_DISCARD);

    // Check how many primitives were written and verify that some were written even if
    // no pixels were rendered
    GLuint primitivesWritten = 0;
    glGetQueryObjectuiv(primitivesWrittenQuery, GL_QUERY_RESULT_EXT, &primitivesWritten);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(primitivesWritten, 6);

    // Nothing should have been drawn to the framebuffer
    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 0, 0, 0, 0);

    // Second pass: draw from the feedback buffer

    glBindBuffer(GL_ARRAY_BUFFER, mTransformFeedbackBuffer);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    EXPECT_PIXEL_EQ(getWindowWidth() / 2, getWindowHeight() / 2, 255,   0,   0, 255);
    EXPECT_GL_NO_ERROR();
}

// Test that buffer binding happens only on the current transform feedback object
TEST_P(TransformFeedbackTest, BufferBinding)
{
    // Reset any state
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    // Generate a new transform feedback and buffer
    GLuint transformFeedbackObject = 0;
    glGenTransformFeedbacks(1, &transformFeedbackObject);

    GLuint scratchBuffer = 0;
    glGenBuffers(1, &scratchBuffer);

    EXPECT_GL_NO_ERROR();

    // Bind TF 0 and a buffer
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID matches the one that was just bound
    GLint currentBufferBinding = 0;
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(currentBufferBinding, mTransformFeedbackBuffer);

    EXPECT_GL_NO_ERROR();

    // Check that the buffer ID for the newly bound transform feedback is zero
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, transformFeedbackObject);

    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &currentBufferBinding);
    EXPECT_EQ(currentBufferBinding, 0);

    EXPECT_GL_NO_ERROR();

    // Bind a buffer to this TF
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, scratchBuffer, 0, 32);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(scratchBuffer, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Rebind the original TF and check it's bindings
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &currentBufferBinding);
    EXPECT_EQ(0, currentBufferBinding);

    EXPECT_GL_NO_ERROR();

    // Clean up
    glDeleteTransformFeedbacks(1, &transformFeedbackObject);
    glDeleteBuffers(1, &scratchBuffer);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(TransformFeedbackTest, ES3_D3D11());

} // namespace
