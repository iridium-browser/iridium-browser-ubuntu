// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/javascript/JavaScript.h"
#include "../../include/javascript/IJavaScript.h"
#include "../../include/javascript/JS_Define.h"
#include "../../include/javascript/JS_Object.h"
#include "../../include/javascript/JS_Value.h"
#include "../../include/javascript/console.h"
#include "../../include/javascript/JS_EventHandler.h"
#include "../../include/javascript/JS_Context.h"

/* ------------------------ console ------------------------ */

BEGIN_JS_STATIC_CONST(CJS_Console)
END_JS_STATIC_CONST()

BEGIN_JS_STATIC_PROP(CJS_Console)
END_JS_STATIC_PROP()

BEGIN_JS_STATIC_METHOD(CJS_Console)
	JS_STATIC_METHOD_ENTRY(clear)
	JS_STATIC_METHOD_ENTRY(hide)
	JS_STATIC_METHOD_ENTRY(println)
	JS_STATIC_METHOD_ENTRY(show)
END_JS_STATIC_METHOD()

IMPLEMENT_JS_CLASS(CJS_Console,console)

console::console(CJS_Object* pJSObject): CJS_EmbedObj(pJSObject)
{
}

console::~console()
{
}

FX_BOOL console::clear(IFXJS_Context* cc, const CJS_Parameters& params, CJS_Value& vRet, CFX_WideString& sError)
{
	return TRUE;
}

FX_BOOL console::hide(IFXJS_Context* cc, const CJS_Parameters& params, CJS_Value& vRet, CFX_WideString& sError)
{
	return TRUE;
}

FX_BOOL console::println(IFXJS_Context* cc, const CJS_Parameters& params, CJS_Value& vRet, CFX_WideString& sError)
{
	if (params.size() < 1)
	{
		return FALSE;
	}
	return TRUE;
}

FX_BOOL console::show(IFXJS_Context* cc, const CJS_Parameters& params, CJS_Value& vRet, CFX_WideString& sError)
{
	return TRUE;
}



