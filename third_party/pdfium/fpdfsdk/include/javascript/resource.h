// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef FPDFSDK_INCLUDE_JAVASCRIPT_RESOURCE_H_
#define FPDFSDK_INCLUDE_JAVASCRIPT_RESOURCE_H_

#include "../../../core/include/fxcrt/fx_basic.h"  // For CFX_WideString.
#include "../fsdk_define.h"  // For FX_UINT.

class CJS_Context;

#define IDS_STRING_JSALERT              25613
#define IDS_STRING_JSPARAMERROR         25614
#define IDS_STRING_JSAFNUMBER_KEYSTROKE 25615
#define IDS_STRING_JSPARAM_TOOLONG      25617
#define IDS_STRING_JSPARSEDATE          25618
#define IDS_STRING_JSRANGE1             25619
#define IDS_STRING_JSRANGE2             25620
#define IDS_STRING_JSRANGE3             25621
#define IDS_STRING_NOTSUPPORT           25627
#define IDS_STRING_JSBUSY               25628
#define IDS_STRING_JSEVENT              25629
#define IDS_STRING_RUN                  25630
#define IDS_STRING_JSPRINT1             25632
#define IDS_STRING_JSPRINT2             25633
#define IDS_STRING_JSNOGLOBAL           25635
#define IDS_STRING_JSREADONLY           25636
#define IDS_STRING_JSTYPEERROR          25637
#define IDS_STRING_JSVALUEERROR         25638

CFX_WideString JSGetStringFromID(CJS_Context* pContext, FX_UINT id);
CFX_WideString JSFormatErrorString(const char* class_name,
                                   const char* property_name,
                                   const CFX_WideString& details);

#endif  // FPDFSDK_INCLUDE_JAVASCRIPT_RESOURCE_H_
