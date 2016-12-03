# This file is read into the GN build.

# Files are relative to third_party/skia.
{
  'skia_library_sources': [
    '<(skia_src_path)/ports/SkImageGenerator_none.cpp',

    '<(skia_src_path)/fonts/SkFontMgr_indirect.cpp',
    '<(skia_src_path)/fonts/SkRemotableFontMgr.cpp',
    '<(skia_src_path)/ports/SkFontHost_FreeType_common.cpp',
    '<(skia_src_path)/ports/SkFontHost_FreeType.cpp',
    '<(skia_src_path)/ports/SkFontHost_win.cpp',
    '<(skia_src_path)/ports/SkFontMgr_android.cpp',
    '<(skia_src_path)/ports/SkFontMgr_android_factory.cpp',
    '<(skia_src_path)/ports/SkFontMgr_android_parser.cpp',
    '<(skia_src_path)/ports/SkGlobalInitialization_default.cpp',
    '<(skia_src_path)/ports/SkImageEncoder_none.cpp',
    '<(skia_src_path)/ports/SkOSFile_posix.cpp',
    '<(skia_src_path)/ports/SkOSFile_stdio.cpp',
    '<(skia_src_path)/ports/SkOSFile_win.cpp',
    '<(skia_src_path)/ports/SkRemotableFontMgr_win_dw.cpp',
    '<(skia_src_path)/ports/SkScalerContext_win_dw.cpp',
    '<(skia_src_path)/ports/SkTLS_pthread.cpp',
    '<(skia_src_path)/ports/SkTLS_win.cpp',
    '<(skia_src_path)/ports/SkTypeface_win_dw.cpp',
    '<(skia_src_path)/sfnt/SkOTTable_name.cpp',
    '<(skia_src_path)/sfnt/SkOTUtils.cpp',

    #mac
    '<(skia_src_path)/utils/mac/SkStream_mac.cpp',

    #windows

    #testing
    '<(skia_src_path)/fonts/SkGScalerContext.cpp',

    #pdfium
    '<(skia_src_path)/ports/SkDiscardableMemory_none.cpp',
    '<(skia_src_path)/ports/SkFontMgr_custom.cpp',
    '<(skia_src_path)/ports/SkFontMgr_custom_empty_factory.cpp',
    '<(skia_src_path)/ports/SkMemory_malloc.cpp',
  ],
}
