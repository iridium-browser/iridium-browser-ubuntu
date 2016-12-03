# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gfx_geometry',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'includes': [
        'gfx_geometry.gypi',
        '../../build/android/increase_size_for_speed.gypi',
      ],
    },
    {
      # GN version: //ui/gfx/geometry/mojo
      'target_name': 'mojo_geometry_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'geometry/mojo/geometry.mojom',
        ],
        'mojom_typemaps': [
          'geometry/mojo/geometry.typemap',
        ],
        'use_new_wrapper_types': 'false',
      },
      'includes': [ '../../mojo/mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //ui/gfx/geometry/mojo
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'dependencies': [
        'mojo_geometry_bindings_mojom',
        '../../mojo/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'gfx_range',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'defines': [
        'GFX_RANGE_IMPLEMENTATION',
      ],
      'sources': [
        'range/gfx_range_export.h',
        'range/range.cc',
        'range/range.h',
        'range/range_f.cc',
        'range/range_f.h',
        'range/range_mac.mm',
        'range/range_win.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          # TODO(jschuh): C4267: http://crbug.com/167187 size_t -> int
          # C4324 is structure was padded due to __declspec(align()), which is
          # uninteresting.
          'msvs_disabled_warnings': [ 4267, 4324 ],
        }],
      ]
    },
    {
      'target_name': 'gfx',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)//third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
        'gfx_geometry',
        'gfx_range',
      ],
      # text_elider.h includes ICU headers.
      'export_dependent_settings': [
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      ],
      'defines': [
        'GFX_IMPLEMENTATION',
      ],
      'include_dirs': [
        '<(DEPTH)/third_party/icu/source/common',
      ],
      'sources': [
        'android/device_display_info.cc',
        'android/device_display_info.h',
        'android/gfx_jni_registrar.cc',
        'android/gfx_jni_registrar.h',
        'android/java_bitmap.cc',
        'android/java_bitmap.h',
        'android/shared_device_display_info.cc',
        'android/shared_device_display_info.h',
        'android/view_configuration.cc',
        'android/view_configuration.h',
        'animation/animation.cc',
        'animation/animation.h',
        'animation/animation_container.cc',
        'animation/animation_container.h',
        'animation/animation_container_element.h',
        'animation/animation_container_observer.h',
        'animation/animation_delegate.h',
        'animation/animation_mac.mm',
        'animation/animation_win.cc',
        'animation/linear_animation.cc',
        'animation/linear_animation.h',
        'animation/multi_animation.cc',
        'animation/multi_animation.h',
        'animation/slide_animation.cc',
        'animation/slide_animation.h',
        'animation/throb_animation.cc',
        'animation/throb_animation.h',
        'animation/tween.cc',
        'animation/tween.h',
        'blit.cc',
        'blit.h',
        'break_list.h',
        'break_list.h',
        'buffer_format_util.cc',
        'buffer_format_util.h',
        'canvas.cc',
        'canvas.h',
        'canvas_notimplemented.cc',
        'canvas_paint_mac.h',
        'canvas_paint_mac.mm',
        'canvas_skia.cc',
        'canvas_skia_paint.h',
        'codec/jpeg_codec.cc',
        'codec/jpeg_codec.h',
        'codec/png_codec.cc',
        'codec/png_codec.h',
        'color_analysis.cc',
        'color_analysis.h',
        'color_palette.h',
        'color_space.cc',
        'color_space.h',
        'color_transform.cc',
        'color_transform.h',
        'color_utils.cc',
        'color_utils.h',
        'favicon_size.cc',
        'favicon_size.h',
        'font.cc',
        'font.h',
        'font_fallback.h',
        'font_fallback_linux.cc',
        'font_fallback_linux.h',
        'font_fallback_mac.mm',
        'font_fallback_win.cc',
        'font_fallback_win.h',
        'font_list.cc',
        'font_list.h',
        'font_list_impl.cc',
        'font_list_impl.h',
        'font_render_params.cc',
        'font_render_params.h',
        'font_render_params_android.cc',
        'font_render_params_linux.cc',
        'font_render_params_mac.cc',
        'font_render_params_win.cc',
        'generic_shared_memory_id.cc',
        'generic_shared_memory_id.h',
        'gfx_export.h',
        'gfx_paths.cc',
        'gfx_paths.h',
        'harfbuzz_font_skia.cc',
        'harfbuzz_font_skia.h',
        'image/canvas_image_source.cc',
        'image/canvas_image_source.h',
        'icc_profile.cc',
        'icc_profile.h',
        'icc_profile_mac.mm',
        'icc_profile_win.cc',
        'image/image.cc',
        'image/image.h',
        'image/image_family.cc',
        'image/image_family.h',
        'image/image_ios.mm',
        'image/image_mac.mm',
        'image/image_png_rep.cc',
        'image/image_png_rep.h',
        'image/image_skia.cc',
        'image/image_skia.h',
        'image/image_skia_operations.cc',
        'image/image_skia_operations.h',
        'image/image_skia_rep.cc',
        'image/image_skia_rep.h',
        'image/image_skia_source.cc',
        'image/image_skia_source.h',
        'image/image_skia_util_ios.h',
        'image/image_skia_util_ios.mm',
        'image/image_skia_util_mac.h',
        'image/image_skia_util_mac.mm',
        'image/image_util.cc',
        'image/image_util.h',
        'image/image_util_ios.mm',
        'interpolated_transform.cc',
        'interpolated_transform.h',
        'ios/NSString+CrStringDrawing.h',
        'ios/NSString+CrStringDrawing.mm',
        'ios/uikit_util.h',
        'ios/uikit_util.mm',
        'linux_font_delegate.cc',
        'linux_font_delegate.h',
        'mac/coordinate_conversion.h',
        'mac/coordinate_conversion.mm',
        'mac/io_surface.cc',
        'mac/io_surface.h',
        'mac/nswindow_frame_controls.h',
        'mac/nswindow_frame_controls.mm',
        'mac/scoped_cocoa_disable_screen_updates.h',
        'native_pixmap_handle_ozone.cc',
        'native_pixmap_handle_ozone.h',
        'native_widget_types.h',
        'nine_image_painter.cc',
        'nine_image_painter.h',
        'overlay_transform.h',
        'paint_throbber.cc',
        'paint_throbber.h',
        'path.cc',
        'path.h',
        'path_mac.h',
        'path_mac.mm',
        'path_win.cc',
        'path_win.h',
        'path_x11.cc',
        'path_x11.h',
        'platform_font.h',
        'platform_font_android.cc',
        'platform_font_ios.h',
        'platform_font_ios.mm',
        'platform_font_linux.cc',
        'platform_font_linux.h',
        'platform_font_mac.h',
        'platform_font_mac.mm',
        'platform_font_win.cc',
        'platform_font_win.h',
        'render_text.cc',
        'render_text.h',
        'render_text_harfbuzz.cc',
        'render_text_harfbuzz.h',
        'render_text_mac.h',
        'render_text_mac.mm',
        'scoped_canvas.cc',
        'scoped_canvas.h',
        'scoped_cg_context_save_gstate_mac.h',
        'scoped_ns_graphics_context_save_gstate_mac.h',
        'scoped_ns_graphics_context_save_gstate_mac.mm',
        'scoped_ui_graphics_push_context_ios.h',
        'scoped_ui_graphics_push_context_ios.mm',
        'scrollbar_size.cc',
        'scrollbar_size.h',
        'selection_bound.cc',
        'selection_bound.h',
        'selection_model.cc',
        'selection_model.h',
        'sequential_id_generator.cc',
        'sequential_id_generator.h',
        'shadow_value.cc',
        'shadow_value.h',
        'skbitmap_operations.cc',
        'skbitmap_operations.h',
        'skia_util.cc',
        'skia_util.h',
        'swap_result.h',
        'switches.cc',
        'switches.h',
        'text_constants.h',
        'text_elider.cc',
        'text_elider.h',
        'text_utils.cc',
        'text_utils.h',
        'text_utils_android.cc',
        'text_utils_ios.mm',
        'text_utils_skia.cc',
        'transform.cc',
        'transform.h',
        'transform_util.cc',
        'transform_util.h',
        'ui_gfx_exports.cc',
        'utf16_indexing.cc',
        'utf16_indexing.h',
        'vsync_provider.h',
        'win/direct_manipulation.cc',
        'win/direct_manipulation.h',
        'win/direct_write.cc',
        'win/direct_write.h',
        'win/hwnd_util.cc',
        'win/hwnd_util.h',
        "win/physical_size.cc",
        "win/physical_size.h",
        'win/rendering_window_manager.cc',
        'win/rendering_window_manager.h',
        'win/scoped_set_map_mode.h',
        'win/singleton_hwnd.cc',
        'win/singleton_hwnd.h',
        'win/singleton_hwnd_observer.cc',
        'win/singleton_hwnd_observer.h',
        'win/text_analysis_source.cc',
        'win/text_analysis_source.h',
        'win/window_impl.cc',
        'win/window_impl.h',
      ],
      'includes': [
        '../../build/android/increase_size_for_speed.gypi',
      ],
      'conditions': [
        ['OS=="ios"', {
          # Linkable dependents need to set the linker flag '-ObjC' in order to
          # use the categories in this target (e.g. NSString+CrStringDrawing.h).
          'link_settings': {
            'xcode_settings': {'OTHER_LDFLAGS': ['-ObjC']},
          },
          'sources!': [
            'blit.cc',
            'blit.h',
            'canvas.cc',
            'canvas.h',
            'canvas_notimplemented.cc',
            'canvas_paint_mac.h',
            'canvas_paint_mac.mm',
            'canvas_skia.cc',
            'canvas_skia_paint.h',
            'codec/jpeg_codec.cc',
          ],
        }, {
          'sources':[
            'gpu_memory_buffer.cc',
            'gpu_memory_buffer.h',
          ],
          'dependencies': [
            '<(libjpeg_gyp_path):libjpeg',
          ],
        }],
        # TODO(asvitkine): Switch all platforms to use canvas_skia.cc.
        #                  http://crbug.com/105550
        ['use_canvas_skia==1', {
          'sources!': [
            'canvas_notimplemented.cc',
          ],
        }, {  # use_canvas_skia!=1
          'sources!': [
            'canvas_skia.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'gdi_util.cc',
            'gdi_util.h',
            'icon_util.cc',
            'icon_util.h',
            'sys_color_change_listener.cc',
            'sys_color_change_listener.h',
          ],
          # TODO(jschuh): C4267: http://crbug.com/167187 size_t -> int
          # C4324 is structure was padded due to __declspec(align()), which is
          # uninteresting.
          'msvs_disabled_warnings': [ 4267, 4324 ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOSurface.framework',
            ],
          },
        }],
        ['OS=="android"', {
          'sources!': [
            'animation/throb_animation.cc',
            'selection_model.cc',
          ],
          'dependencies': [
            'gfx_jni_headers',
            '<(DEPTH)/base/base.gyp:base_java',
          ],
          'link_settings': {
            'libraries': [
              '-landroid',
              '-ljnigraphics',
            ],
          },
        }],
        ['chromeos==1', {
          # Chrome OS requires robust JPEG decoding for the login screen.
          'sources': [
            'chromeos/codec/jpeg_codec_robust_slow.cc',
            'chromeos/codec/jpeg_codec_robust_slow.h',
          ],
          'dependencies': [
            '<(libjpeg_ijg_gyp_path):libjpeg',
          ],
        }],
        ['use_aura==0 and toolkit_views==0', {
          'sources!': [
            'nine_image_painter.cc',
            'nine_image_painter.h',
          ],
        }],
        ['OS=="android" and use_aura==0', {
          'sources!': [
            'path.cc',
          ],
        }],
        ['OS=="android" or OS=="ios"', {
          'sources!': [
            'harfbuzz_font_skia.cc',
            'harfbuzz_font_skia.h',
            'render_text.cc',
            'render_text.h',
            'render_text_harfbuzz.cc',
            'render_text_harfbuzz.h',
            'text_utils_skia.cc',
          ],
        }, {  # desktop platforms
        }],
        ['use_x11==1', {
          'dependencies': [
            '../../build/linux/system.gyp:x11',
            'x/gfx_x11.gyp:gfx_x11',
          ],
        }],
        ['use_cairo==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:pangocairo',
          ],
        }],
        ['desktop_linux==1 or chromeos==1', {
          'dependencies': [
            # font_render_params_linux.cc uses fontconfig
            '<(DEPTH)/build/linux/system.gyp:fontconfig',
          ],
        }],
      ],
      'target_conditions': [
        # Need 'target_conditions' to override default filename_rules to include
        # the file on iOS.
        ['OS == "ios"', {
          'sources/': [
            ['include', '^scoped_cg_context_save_gstate_mac\\.h$'],
          ],
        }],
      ],
    },
    # Separate from gfx to limit the impact of the hard_dependency.
    {
      'target_name': 'gfx_vector_icons',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/skia/skia.gyp:skia',
        'gfx',
        'gfx_geometry',
      ],
      'defines': [
        'GFX_IMPLEMENTATION',
      ],
      'sources': [
        'paint_vector_icon.cc',
        'paint_vector_icon.h',
        'vector_icons_public.h',
      ],
      'variables': {
        'vector_icons_cc_file': '<(INTERMEDIATE_DIR)/ui/gfx/vector_icons.cc',
        'vector_icons_h_file': '<(SHARED_INTERMEDIATE_DIR)/ui/gfx/vector_icons.h',
      },
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'actions': [
        {
          # GN version: //ui/gfx:aggregate_vector_icons
          'action_name': 'aggregate_vector_icons',
          'inputs': [
            'vector_icons/',
          ],
          'outputs': [
            '<(vector_icons_cc_file)',
            '<(vector_icons_h_file)',
          ],
          'action': [ 'python',
                      'vector_icons/aggregate_vector_icons.py',
                      '--working_directory=vector_icons/',
                      '--output_cc=<(vector_icons_cc_file)',
                      '--output_h=<(vector_icons_h_file)',
          ],
          'message': 'Aggregating vector resources.',
          'process_outputs_as_sources': 1,
        },
      ],
      # Export a hard dependency because of generated header files.
      'hard_dependency': 1,
    },
    {
      'target_name': 'gfx_test_support',
      'type': 'static_library',
      'sources': [
        'image/image_unittest_util.cc',
        'image/image_unittest_util.h',
        'image/image_unittest_util_ios.mm',
        'image/image_unittest_util_mac.mm',
        'test/fontconfig_util_linux.cc',
        'test/fontconfig_util_linux.h',
        'test/gfx_util.cc',
        'test/gfx_util.h',
        'test/ui_cocoa_test_helper.h',
        'test/ui_cocoa_test_helper.mm',
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../testing/gtest.gyp:gtest',
        'gfx',
        'gfx_geometry',
      ],
      'conditions': [
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
            ],
          },
        }],
        ['OS=="ios"', {
          # The cocoa files don't apply to iOS.
          'sources/': [
            ['exclude', 'cocoa']
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../../build/linux/system.gyp:fontconfig',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="android"' , {
     'targets': [
       {
         'target_name': 'gfx_jni_headers',
         'type': 'none',
         'sources': [
           '../android/java/src/org/chromium/ui/gfx/BitmapHelper.java',
           '../android/java/src/org/chromium/ui/gfx/DeviceDisplayInfo.java',
           '../android/java/src/org/chromium/ui/gfx/ViewConfigurationHelper.java',
         ],
         'variables': {
           'jni_gen_package': 'ui/gfx',
         },
         'includes': [ '../../build/jni_generator.gypi' ],
       },
     ],
    }],
    ['disable_nacl!=1 and OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'gfx_geometry_win64',
          'type': '<(component)',
          'dependencies': [
            '<(DEPTH)/base/base.gyp:base_win64',
          ],
          'includes': [
            'gfx_geometry.gypi',
          ],
          'defines': [
            '<@(nacl_win64_defines)',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}
