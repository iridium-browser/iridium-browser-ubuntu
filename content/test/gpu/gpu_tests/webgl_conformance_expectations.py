# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_test_expectations import GpuTestExpectations

# See the GpuTestExpectations class for documentation.

class WebGLConformanceExpectations(GpuTestExpectations):
  def SetExpectations(self):
    # Fails on all platforms
    self.Fail('conformance/glsl/misc/shaders-with-invariance.html',
        bug=421710)
    self.Fail('conformance/glsl/bugs/essl3-shaders-with-webgl1.html',
        bug=428845)
    self.Fail('conformance/glsl/misc/expression-list-in-declarator-initializer.html',
        bug=428845)
    self.Fail('conformance/uniforms/gl-uniform-arrays.html',
        bug=433385)

    # Win failures
    self.Fail('conformance/glsl/misc/struct-equals.html',
        ['win'], bug=391957)
    self.Fail('conformance/glsl/bugs/conditional-discard-in-loop.html',
        ['win'], bug=402195)
    self.Fail('conformance/glsl/misc/ternary-operators-in-global-initializers.html',
        ['win'], bug=415694)
    self.Fail('conformance/glsl/misc/struct-specifiers-in-uniforms.html',
        ['win'], bug=433412)
    # This test still causes itself and any tests afterwards to time out
    # in Win Debug bots.
    self.Skip('conformance/textures/texture-copying-feedback-loops.html',
        ['Win'], bug=421695)

    self.Fail('conformance/rendering/framebuffer-switch.html',
        ['win'], bug=428849)
    self.Fail('conformance/rendering/framebuffer-texture-switch.html',
        ['win'], bug=428849)

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

    # Win8 / NVIDIA failures
    self.Fail('conformance/textures/tex-image-and-sub-image-2d-with-array-buffer-view.html',
        ['win', 'nvidia'], bug=459265)

    # Win / AMD failures
    self.Fail('conformance/textures/texparameter-test.html',
        ['win', 'amd', 'd3d9'], bug=839) # angle bug ID

    # Win / D3D9 failures
    # Skipping these tests because they're causing assertion failures.
    self.Skip('conformance/extensions/oes-texture-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID
    self.Skip('conformance/extensions/oes-texture-half-float-with-canvas.html',
        ['win', 'd3d9'], bug=896) # angle bug ID

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
    self.Fail('conformance/textures/default-texture.html',
        ['linux', ('nvidia', 0x104a)], bug=422152)
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

    # Android failures
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
