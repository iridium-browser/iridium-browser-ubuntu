// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/javascript/JavaScript.h"
#include "../../include/javascript/IJavaScript.h"
#include "../../include/javascript/JS_Define.h"
#include "../../include/javascript/JS_Object.h"
#include "../../include/javascript/JS_Value.h"
#include "../../include/javascript/Consts.h"

/* ------------------------------ border ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Border)
JS_STATIC_CONST_ENTRY_STRING(L"s", L"solid")
JS_STATIC_CONST_ENTRY_STRING(L"b", L"beveled")
JS_STATIC_CONST_ENTRY_STRING(L"d", L"dashed")
JS_STATIC_CONST_ENTRY_STRING(L"i", L"inset")
JS_STATIC_CONST_ENTRY_STRING(L"u", L"underline")
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Border, border)

/* ------------------------------ display ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Display)
JS_STATIC_CONST_ENTRY_NUMBER(L"visible", 0)
JS_STATIC_CONST_ENTRY_NUMBER(L"hidden", 1)
JS_STATIC_CONST_ENTRY_NUMBER(L"noPrint", 2)
JS_STATIC_CONST_ENTRY_NUMBER(L"noView", 3)
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Display, display)

/* ------------------------------ font ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Font)
JS_STATIC_CONST_ENTRY_STRING(L"Times", L"Times-Roman")
JS_STATIC_CONST_ENTRY_STRING(L"TimesB", L"Times-Bold")
JS_STATIC_CONST_ENTRY_STRING(L"TimesI", L"Times-Italic")
JS_STATIC_CONST_ENTRY_STRING(L"TimesBI", L"Times-BoldItalic")
JS_STATIC_CONST_ENTRY_STRING(L"Helv", L"Helvetica")
JS_STATIC_CONST_ENTRY_STRING(L"HelvB", L"Helvetica-Bold")
JS_STATIC_CONST_ENTRY_STRING(L"HelvI", L"Helvetica-Oblique")
JS_STATIC_CONST_ENTRY_STRING(L"HelvBI", L"Helvetica-BoldOblique")
JS_STATIC_CONST_ENTRY_STRING(L"Cour", L"Courier")
JS_STATIC_CONST_ENTRY_STRING(L"CourB", L"Courier-Bold")
JS_STATIC_CONST_ENTRY_STRING(L"CourI", L"Courier-Oblique")
JS_STATIC_CONST_ENTRY_STRING(L"CourBI", L"Courier-BoldOblique")
JS_STATIC_CONST_ENTRY_STRING(L"Symbol", L"Symbol")
JS_STATIC_CONST_ENTRY_STRING(L"ZapfD", L"ZapfDingbats")
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Font, font)

/* ------------------------------ highlight ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Highlight)
JS_STATIC_CONST_ENTRY_STRING(L"n", L"none")
JS_STATIC_CONST_ENTRY_STRING(L"i", L"invert")
JS_STATIC_CONST_ENTRY_STRING(L"p", L"push")
JS_STATIC_CONST_ENTRY_STRING(L"o", L"outline")
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Highlight, highlight)

/* ------------------------------ position ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Position)
JS_STATIC_CONST_ENTRY_NUMBER(L"textOnly", 0)
JS_STATIC_CONST_ENTRY_NUMBER(L"iconOnly", 1)
JS_STATIC_CONST_ENTRY_NUMBER(L"iconTextV", 2)
JS_STATIC_CONST_ENTRY_NUMBER(L"textIconV", 3)
JS_STATIC_CONST_ENTRY_NUMBER(L"iconTextH", 4)
JS_STATIC_CONST_ENTRY_NUMBER(L"textIconH", 5)
JS_STATIC_CONST_ENTRY_NUMBER(L"overlay", 6)
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Position, position)

/* ------------------------------ scaleHow ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_ScaleHow)
JS_STATIC_CONST_ENTRY_NUMBER(L"proportional", 0)
JS_STATIC_CONST_ENTRY_NUMBER(L"anamorphic", 1)
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_ScaleHow, scaleHow)

/* ------------------------------ scaleWhen ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_ScaleWhen)
JS_STATIC_CONST_ENTRY_NUMBER(L"always", 0)
JS_STATIC_CONST_ENTRY_NUMBER(L"never", 1)
JS_STATIC_CONST_ENTRY_NUMBER(L"tooBig", 2)
JS_STATIC_CONST_ENTRY_NUMBER(L"tooSmall", 3)
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_ScaleWhen, scaleWhen)

/* ------------------------------ style ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Style)
JS_STATIC_CONST_ENTRY_STRING(L"ch", L"check")
JS_STATIC_CONST_ENTRY_STRING(L"cr", L"cross")
JS_STATIC_CONST_ENTRY_STRING(L"di", L"diamond")
JS_STATIC_CONST_ENTRY_STRING(L"ci", L"circle")
JS_STATIC_CONST_ENTRY_STRING(L"st", L"star")
JS_STATIC_CONST_ENTRY_STRING(L"sq", L"square")
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Style, style)

/* ------------------------------ zoomtype ------------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Zoomtype)
JS_STATIC_CONST_ENTRY_STRING(L"none", L"NoVary")
JS_STATIC_CONST_ENTRY_STRING(L"fitP", L"FitPage")
JS_STATIC_CONST_ENTRY_STRING(L"fitW", L"FitWidth")
JS_STATIC_CONST_ENTRY_STRING(L"fitH", L"FitHeight")
JS_STATIC_CONST_ENTRY_STRING(L"fitV", L"FitVisibleWidth")
JS_STATIC_CONST_ENTRY_STRING(L"pref", L"Preferred")
JS_STATIC_CONST_ENTRY_STRING(L"refW", L"ReflowWidth")
END_JS_STATIC_CONST()

IMPLEMENT_JS_CLASS_CONST(CJS_Zoomtype, zoomtype)

/* ------------------------------ CJS_GlobalConsts
 * ------------------------------ */

int CJS_GlobalConsts::Init(IJS_Runtime* pRuntime) {
  DEFINE_GLOBAL_CONST(pRuntime, IDS_GREATER_THAN, Invalid value
                      : must be greater than or equal to % s.);
  DEFINE_GLOBAL_CONST(
      pRuntime, IDS_GT_AND_LT, Invalid value
      : must be greater than or equal to % s and less than or equal to % s.);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_LESS_THAN, Invalid value
                      : must be less than or equal to % s.);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_INVALID_MONTH, **Invalid**);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_INVALID_DATE, Invalid date / time
                      : please ensure that the date / time exists.Field);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_INVALID_VALUE,
                      The value entered does not match the format of the field);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_AM, am);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_PM, pm);
  DEFINE_GLOBAL_CONST(
      pRuntime, IDS_MONTH_INFO,
      January[1] February[2] March[3] April[4] May[5] June[6] July[7] August
          [8] September[9] October[10] November[11] December[12] Sept[9] Jan
              [1] Feb[2] Mar[3] Apr[4] Jun[6] Jul[7] Aug[8] Sep[9] Oct[10] Nov
                  [11] Dec[12]);
  DEFINE_GLOBAL_CONST(pRuntime, IDS_STARTUP_CONSOLE_MSG, ** ^ _ ^ **);

  return 0;
}

/* ------------------------------ CJS_GlobalArrays
 * ------------------------------ */

int CJS_GlobalArrays::Init(IJS_Runtime* pRuntime) {
  {
    const FX_WCHAR* ArrayName = L"RE_NUMBER_ENTRY_DOT_SEP";
    const FX_WCHAR* ArrayContent[] = {L"[+-]?\\d*\\.?\\d*"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_NUMBER_COMMIT_DOT_SEP";
    const FX_WCHAR* ArrayContent[] = {
        L"[+-]?\\d+(\\.\\d+)?", /* -1.0 or -1 */
        L"[+-]?\\.\\d+",        /* -.1 */
        L"[+-]?\\d+\\."         /* -1. */
    };
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_NUMBER_ENTRY_COMMA_SEP";
    const FX_WCHAR* ArrayContent[] = {L"[+-]?\\d*,?\\d*"};

    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_NUMBER_COMMIT_COMMA_SEP";
    const FX_WCHAR* ArrayContent[] = {
        L"[+-]?\\d+([.,]\\d+)?", /* -1,0 or -1 */
        L"[+-]?[.,]\\d+",        /* -,1 */
        L"[+-]?\\d+[.,]"         /* -1, */
    };
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_ZIP_ENTRY";
    const FX_WCHAR* ArrayContent[] = {L"\\d{0,5}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_ZIP_COMMIT";
    const FX_WCHAR* ArrayContent[] = {L"\\d{5}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_ZIP4_ENTRY";
    const FX_WCHAR* ArrayContent[] = {L"\\d{0,5}(\\.|[- ])?\\d{0,4}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_ZIP4_COMMIT";
    const FX_WCHAR* ArrayContent[] = {L"\\d{5}(\\.|[- ])?\\d{4}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_PHONE_ENTRY";
    const FX_WCHAR* ArrayContent[] = {
        L"\\d{0,3}(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}", /* 555-1234 or 408
                                                              555-1234 */
        L"\\(\\d{0,3}",                                    /* (408 */
        L"\\(\\d{0,3}\\)(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}", /* (408)
                                                                    555-1234 */
        /* (allow the addition of parens as an afterthought) */
        L"\\(\\d{0,3}(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}", /* (408 555-1234
                                                                 */
        L"\\d{0,3}\\)(\\.|[- ])?\\d{0,3}(\\.|[- ])?\\d{0,4}", /* 408) 555-1234
                                                                 */
        L"011(\\.|[- \\d])*" /* international */
    };
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_PHONE_COMMIT";
    const FX_WCHAR* ArrayContent[] = {
        L"\\d{3}(\\.|[- ])?\\d{4}",                        /* 555-1234 */
        L"\\d{3}(\\.|[- ])?\\d{3}(\\.|[- ])?\\d{4}",       /* 408 555-1234 */
        L"\\(\\d{3}\\)(\\.|[- ])?\\d{3}(\\.|[- ])?\\d{4}", /* (408) 555-1234 */
        L"011(\\.|[- \\d])*"                               /* international */
    };
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_SSN_ENTRY";
    const FX_WCHAR* ArrayContent[] = {
        L"\\d{0,3}(\\.|[- ])?\\d{0,2}(\\.|[- ])?\\d{0,4}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  {
    const FX_WCHAR* ArrayName = L"RE_SSN_COMMIT";
    const FX_WCHAR* ArrayContent[] = {
        L"\\d{3}(\\.|[- ])?\\d{2}(\\.|[- ])?\\d{4}"};
    DEFINE_GLOBAL_ARRAY(pRuntime);
  }

  return 0;
}
