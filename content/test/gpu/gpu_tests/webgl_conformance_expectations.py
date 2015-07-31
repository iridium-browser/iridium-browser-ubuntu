# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class WebGLConformanceExpectations(GpuTestExpectations):
  def __init__(self, conformance_path):
    self.conformance_path = conformance_path
    GpuTestExpectations.__init__(self)

  def Fail(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Fail(self, pattern, condition, bug)

  def Skip(self, pattern, condition=None, bug=None):
    self.CheckPatternIsValid(pattern)
    GpuTestExpectations.Skip(self, pattern, condition, bug)

  def CheckPatternIsValid(self, pattern):
    full_path = os.path.normpath(os.path.join(self.conformance_path, pattern))

    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified in' +
        'expectation does not exist: ' + full_path)

  def SetExpectations(self):
    # Fails on all platforms
    self.Fail('deqp/data/gles2/shaders/constant_expressions.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/fragdata.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/functions.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/preprocessor.html',
        bug=478572)
    self.Fail('deqp/data/gles2/shaders/scoping.html',
        bug=478572)
    self.Fail('conformance/glsl/misc/const-variable-initialization.html',
        bug=485632)
    self.Fail('conformance/misc/expando-loss.html',
        bug=485634)

    # Win failures
    self.Fail('conformance/glsl/misc/' +
              'ternary-operators-in-global-initializers.html',
        ['win'], bug=415694)
    self.Fail('conformance/glsl/bugs/' +
              'pow-of-small-constant-in-user-defined-function.html',
        ['win'], bug=485641)
    self.Fail('conformance/glsl/bugs/sampler-struct-function-arg.html',
        ['win'], bug=485642)

    # Win7 / Intel failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['win7', 'intel'], bug=314997)
    self.Fail('conformance/context/premultiplyalpha-test.html',
        ['win7', 'intel'])
    self.Fail('conformance/textures/copy-tex-image-and-sub-image-2d.html',
        ['win7', 'intel'])
    self.Fail('conformance/rendering/gl-viewport-test.html',
        ['win7', 'intel'], bug=372511)
    self.Fail('conformance/glsl/misc/shader-with-array-of-structs-uniform.html',
        ['win7', 'intel', 'nvidia'], bug=373972)

    # Win / AMD failures
    self.Fail('conformance/textures/texparameter-test.html',
        ['win', 'amd', 'd3d9'], bug=839) # angle bug ID
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['win', 'amd', 'd3d9'], bug=475095)
    self.Fail('conformance/rendering/more-than-65536-indices.html',
        ['win', 'amd', 'd3d9'], bug=475095)

    # Win / D3D9 failures
    # Skipping these tests because they're causing assertion failures.
    self.Skip('conformance/extensions/oes-texture-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Skip('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Fail('conformance/glsl/bugs/conditional-discard-optimization.html',
        ['win', 'd3d9'], bug=488552)

    # Mac failures
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['mac'], bug=421710)

    # Mac / Intel failures
    # Radar 13499466
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['mac', 'intel'], bug=225642)
    # Radar 13499623
    self.Fail('conformance/textures/texture-size.html',
        ['mac', 'intel'], bug=225642)

    # Mac / Intel HD 3000 failures
    self.Skip('conformance/ogles/GL/control_flow/control_flow_009_to_010.html',
        ['mac', ('intel', 0x116)], bug=322795)
    # Radar 13499677
    self.Fail('conformance/glsl/functions/' +
        'glsl-function-smoothstep-gentype.html',
        ['mac', ('intel', 0x116)], bug=225642)
    self.Fail('conformance/extensions/webgl-draw-buffers.html',
        ['mac', ('intel', 0x116)], bug=369349)

    # Mac 10.8 / Intel HD 3000 failures
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mountainlion', ('intel', 0x116)], bug=314997)
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mountainlion', ('intel', 0x116)], bug=322795)

    # Mac 10.9 / Intel HD 3000 failures
    self.Fail('conformance/ogles/GL/operators/operators_009_to_016.html',
        ['mavericks', ('intel', 0x116)], bug=417415)
    self.Fail('conformance/rendering/gl-scissor-test.html',
        ['mavericks', ('intel', 0x116)], bug=417415)

    # Mac Retina failures
    self.Fail(
        'conformance/glsl/bugs/array-of-struct-with-int-first-position.html',
        ['mac', ('nvidia', 0xfd5), ('nvidia', 0xfe9)], bug=368912)

    # Mac / AMD Failures
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['mac', 'amd'], bug=478572)

    # Mac 10.8 / ATI failures
    self.Fail(
        'conformance/rendering/' +
        'point-with-gl-pointcoord-in-fragment-shader.html',
        ['mountainlion', 'amd'])

    # Mac 10.7 / Intel failures
    self.Skip('conformance/glsl/functions/glsl-function-asin.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-dot.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-faceforward.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-length.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-normalize.html',
        ['lion', 'intel'])
    self.Skip('conformance/glsl/functions/glsl-function-reflect.html',
        ['lion', 'intel'])
    self.Skip('conformance/rendering/line-loop-tri-fan.html',
        ['lion', 'intel'])
    self.Skip('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['lion', 'intel'], bug=345575)
    self.Skip('conformance/ogles/GL/dot/dot_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/faceforward/faceforward_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/length/length_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/normalize/normalize_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/reflect/reflect_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/refract/refract_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    self.Skip('conformance/ogles/GL/tan/tan_001_to_006.html',
        ['lion', 'intel'], bug=323736)
    # Two flaky tests.
    self.Fail('conformance/ogles/GL/functions/functions_049_to_056.html',
        ['lion', 'intel'], bug=393331)
    self.Fail('conformance/extensions/webgl-compressed-texture-size-limit.html',
        ['lion', 'intel'], bug=393331)

    # Linux failures
    # NVIDIA
    self.Fail('conformance/textures/default-texture.html',
        ['linux', ('nvidia', 0x104a)], bug=422152)
    # AMD Radeon 5450
    self.Fail('conformance/programs/program-test.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/rendering/multisample-corruption.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/default-texture.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-webgl-canvas.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-webgl-canvas-rgb565.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-webgl-canvas-rgba4444.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-webgl-canvas-rgba5551.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/texture-mips.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/texture-npot-video.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/textures/texture-size.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/copyTexSubImage2D.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/drawArraysOutOfBounds.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/texImage2DHTML.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    self.Fail('conformance/more/functions/texSubImage2DHTML.html',
        ['linux', ('amd', 0x68f9)], bug=436212)
    # AMD Radeon 6450
    self.Fail('conformance/extensions/angle-instanced-arrays.html',
        ['linux', ('amd', 0x6779)], bug=479260)
    self.Fail('conformance/extensions/ext-texture-filter-anisotropic.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/shader-struct-scope.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/struct-nesting-of-variable-names.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/rendering/point-size.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/textures/texture-sub-image-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/more/functions/uniformf.html',
        ['linux', ('amd', 0x6779)], bug=436212)
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        ['linux', ('amd', 0x6779)], bug=479952)
    self.Fail('conformance/textures/texture-mips.html',
        ['linux', ('amd', 0x6779)], bug=479981)
    self.Fail('conformance/textures/texture-size-cube-maps.html',
        ['linux', ('amd', 0x6779)], bug=479983)
    self.Fail('conformance/uniforms/uniform-default-values.html',
        ['linux', ('amd', 0x6779)], bug=482013)

    # Android failures
    self.Fail('deqp/data/gles2/shaders/constants.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/conversions.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/declarations.html',
        ['android'], bug=478572)
    self.Fail('deqp/data/gles2/shaders/linkage.html',
        ['android'], bug=478572)

    # The following test is very slow and therefore times out on Android bot.
    self.Skip('conformance/rendering/multisample-corruption.html',
        ['android'])
    # The following test times out on Android bot.
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['android'], bug=369300)
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['android'], bug=315976)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['android'], bug=315976)
    # The following tests are disabled due to security issues.
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-video.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-video-rgb565.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-video-rgba4444.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/' +
        'tex-image-and-sub-image-2d-with-video-rgba5551.html',
        ['android'], bug=334204)
    self.Fail('conformance/textures/texture-npot-video.html',
        ['android'], bug=334204)

    # ChromeOS: affecting all devices.
    self.Fail('conformance/extensions/webgl-depth-texture.html',
        ['chromeos'], bug=382651)

    # ChromeOS: all Intel except for pinetrail (stumpy, parrot, peppy,...)
    # We will just include pinetrail here for now as we don't want to list
    # every single Intel device ID.
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/glsl/misc/shaders-with-varyings.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/renderbuffers/framebuffer-object-attachment.html',
        ['chromeos', 'intel'], bug=375556)
    self.Fail('conformance/textures/texture-size-limit.html',
        ['chromeos', 'intel'], bug=385361)

    # ChromeOS: pinetrail (alex, mario, zgb).
    self.Fail('conformance/attribs/gl-vertex-attrib-render.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-atan-xy.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-cos.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/functions/glsl-function-sin.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/misc/empty_main.vert.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/misc/gl_position_unset.vert.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/misc/shaders-with-varyings.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/glsl/variables/gl-frontfacing.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/limits/gl-max-texture-dimensions.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/acos/acos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/asin/asin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/atan/atan_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/build/build_009_to_016.html',
        ['chromeos', ('intel', 0xa011)], bug=378938)
    self.Fail('conformance/ogles/GL/control_flow/control_flow_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/cos/cos_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/discard/discard_001_to_002.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_065_to_072.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_081_to_088.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_097_to_104.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_105_to_112.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_113_to_120.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/functions/functions_121_to_126.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail(
        'conformance/ogles/GL/gl_FrontFacing/gl_FrontFacing_001_to_001.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log/log_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/log2/log2_001_to_008.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/normalize/normalize_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/ogles/GL/sin/sin_001_to_006.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/point-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/rendering/polygon-offset.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-mips.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-npot.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-npot-video.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-size.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/textures/texture-size-limit.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)
    self.Skip('conformance/uniforms/uniform-default-values.html',
        ['chromeos', ('intel', 0xa011)], bug=375554)

    # Flaky on Mac & Linux
    self.Fail('conformance/textures/texture-upload-size.html',
        ['mac'], bug=436493)
    self.Fail('conformance/textures/texture-upload-size.html',
        ['linux'], bug=436493)

    ##############################################################
    # WEBGL 2 TESTS FAILURES
    ##############################################################

    self.Fail('deqp/data/gles3/shaders/arrays.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/constants.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/constant_expressions.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/conversions.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/fragdata.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/functions.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/linkage.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/preprocessor.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/qualification_order.html', bug=483282)
    self.Fail('deqp/data/gles3/shaders/scoping.html', bug=483282)

    self.Fail('deqp/functional/gles3/textureformat.html', bug=483282)
    self.Fail('deqp/functional/gles3/transformfeedback.html', bug=483282)
    self.Fail('deqp/functional/gles3/uniformbuffers.html', bug=483282)

    self.Fail('conformance2/attribs/gl-vertex-attrib.html', bug=483282)
    self.Fail('conformance2/attribs/gl-vertex-attrib-i-render.html', bug=483282)
    self.Fail('conformance2/attribs/gl-vertexattribipointer.html', bug=483282)
    self.Fail('conformance2/attribs/gl-vertexattribipointer-offsets.html',
        bug=483282)

    self.Fail('conformance2/context/constants-and-properties-2.html',
        bug=483282)

    self.Fail('conformance2/core/draw-buffers.html', bug=483282)
    self.Fail('conformance2/core/frag-depth.html', bug=483282)
    self.Fail('conformance2/core/tex-mipmap-levels.html', bug=483282)

    self.Fail('conformance2/core/tex-new-formats.html', bug=483282)
    self.Fail('conformance2/core/tex-storage-2d.html', bug=483282)
    self.Fail('conformance2/core/tex-storage-and-subimage-3d.html', bug=483282)
    self.Fail('conformance2/core/texture-npot.html', bug=483282)

    self.Fail('conformance2/glsl3/misplaced-version-directive.html', bug=483282)

    self.Fail('conformance2/state/gl-get-calls.html', bug=483282)
    self.Fail('conformance2/state/gl-object-get-calls.html', bug=483282)

    self.Fail('conformance2/buffers/buffer-copying-contents.html', bug=483282)
    self.Fail('conformance2/buffers/buffer-copying-restrictions.html',
        bug=483282)
    self.Fail('conformance2/buffers/buffer-type-restrictions.html', bug=483282)
    self.Fail('conformance2/buffers/getBufferSubData.html', bug=483282)
