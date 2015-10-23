// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/javascript/JavaScript.h"
#include "../../include/javascript/JS_Define.h"
#include "../../include/javascript/JS_Object.h"
#include "../../include/javascript/JS_Value.h"
#include "../../include/javascript/Document.h"

/* ---------------------------- CJS_Value ---------------------------- */

CJS_Value::CJS_Value(v8::Isolate* isolate)
    : m_eType(VT_unknown), m_isolate(isolate) {}
CJS_Value::CJS_Value(v8::Isolate* isolate,
                     v8::Local<v8::Value> pValue,
                     FXJSVALUETYPE t)
    : m_pValue(pValue), m_eType(t), m_isolate(isolate) {}

CJS_Value::CJS_Value(v8::Isolate* isolate, const int& iValue)
    : m_isolate(isolate) {
  operator=(iValue);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, const bool& bValue)
    : m_isolate(isolate) {
  operator=(bValue);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, const float& fValue)
    : m_isolate(isolate) {
  operator=(fValue);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, const double& dValue)
    : m_isolate(isolate) {
  operator=(dValue);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, JSFXObject pJsObj)
    : m_isolate(isolate) {
  operator=(pJsObj);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, CJS_Object* pJsObj)
    : m_isolate(isolate) {
  operator=(pJsObj);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, CJS_Document* pJsDoc)
    : m_isolate(isolate) {
  m_eType = VT_object;
  if (pJsDoc)
    m_pValue = (JSFXObject)*pJsDoc;
}

CJS_Value::CJS_Value(v8::Isolate* isolate, const FX_WCHAR* pWstr)
    : m_isolate(isolate) {
  operator=(pWstr);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, const FX_CHAR* pStr)
    : m_isolate(isolate) {
  operator=(pStr);
}

CJS_Value::CJS_Value(v8::Isolate* isolate, CJS_Array& array)
    : m_isolate(isolate) {
  operator=(array);
}

CJS_Value::~CJS_Value() {}

void CJS_Value::Attach(v8::Local<v8::Value> pValue, FXJSVALUETYPE t) {
  m_pValue = pValue;
  m_eType = t;
}

void CJS_Value::Attach(CJS_Value* pValue) {
  if (pValue)
    Attach(pValue->ToV8Value(), pValue->GetType());
}

void CJS_Value::Detach() {
  m_pValue = v8::Local<v8::Value>();
  m_eType = VT_unknown;
}

/* ----------------------------------------------------------------------------------------
 */

int CJS_Value::ToInt() const {
  return JS_ToInt32(m_isolate, m_pValue);
}

bool CJS_Value::ToBool() const {
  return JS_ToBoolean(m_isolate, m_pValue);
}

double CJS_Value::ToDouble() const {
  return JS_ToNumber(m_isolate, m_pValue);
}

float CJS_Value::ToFloat() const {
  return (float)ToDouble();
}

CJS_Object* CJS_Value::ToCJSObject() const {
  v8::Local<v8::Object> pObj = JS_ToObject(m_isolate, m_pValue);
  return (CJS_Object*)JS_GetPrivate(m_isolate, pObj);
}

v8::Local<v8::Object> CJS_Value::ToV8Object() const {
  return JS_ToObject(m_isolate, m_pValue);
}

CFX_WideString CJS_Value::ToCFXWideString() const {
  return JS_ToString(m_isolate, m_pValue);
}

CFX_ByteString CJS_Value::ToCFXByteString() const {
  return CFX_ByteString::FromUnicode(ToCFXWideString());
}

v8::Local<v8::Value> CJS_Value::ToV8Value() const {
  return m_pValue;
}

v8::Local<v8::Array> CJS_Value::ToV8Array() const {
  if (IsArrayObject())
    return v8::Local<v8::Array>::Cast(JS_ToObject(m_isolate, m_pValue));
  return v8::Local<v8::Array>();
}

/* ----------------------------------------------------------------------------------------
 */

void CJS_Value::operator=(int iValue) {
  m_pValue = JS_NewNumber(m_isolate, iValue);

  m_eType = VT_number;
}

void CJS_Value::operator=(bool bValue) {
  m_pValue = JS_NewBoolean(m_isolate, bValue);

  m_eType = VT_boolean;
}

void CJS_Value::operator=(double dValue) {
  m_pValue = JS_NewNumber(m_isolate, dValue);

  m_eType = VT_number;
}

void CJS_Value::operator=(float fValue) {
  m_pValue = JS_NewNumber(m_isolate, fValue);
  m_eType = VT_number;
}

void CJS_Value::operator=(v8::Local<v8::Object> pObj) {
  m_pValue = JS_NewObject(m_isolate, pObj);

  m_eType = VT_fxobject;
}

void CJS_Value::operator=(CJS_Object* pObj) {
  if (pObj)
    operator=((JSFXObject)*pObj);
}

void CJS_Value::operator=(CJS_Document* pJsDoc) {
  m_eType = VT_object;
  if (pJsDoc) {
    m_pValue = static_cast<JSFXObject>(*pJsDoc);
  }
}

void CJS_Value::operator=(const FX_WCHAR* pWstr) {
  m_pValue = JS_NewString(m_isolate, (wchar_t*)pWstr);

  m_eType = VT_string;
}

void CJS_Value::SetNull() {
  m_pValue = JS_NewNull();

  m_eType = VT_null;
}

void CJS_Value::operator=(const FX_CHAR* pStr) {
  operator=(CFX_WideString::FromLocal(pStr).c_str());
}

void CJS_Value::operator=(CJS_Array& array) {
  m_pValue = JS_NewObject2(m_isolate, (v8::Local<v8::Array>)array);

  m_eType = VT_object;
}

void CJS_Value::operator=(CJS_Date& date) {
  m_pValue = JS_NewDate(m_isolate, (double)date);

  m_eType = VT_date;
}

void CJS_Value::operator=(CJS_Value value) {
  m_pValue = value.ToV8Value();

  m_eType = value.m_eType;
  m_isolate = value.m_isolate;
}

/* ----------------------------------------------------------------------------------------
 */

FXJSVALUETYPE CJS_Value::GetType() const {
  if (m_pValue.IsEmpty())
    return VT_unknown;
  if (m_pValue->IsString())
    return VT_string;
  if (m_pValue->IsNumber())
    return VT_number;
  if (m_pValue->IsBoolean())
    return VT_boolean;
  if (m_pValue->IsDate())
    return VT_date;
  if (m_pValue->IsObject())
    return VT_object;
  if (m_pValue->IsNull())
    return VT_null;
  if (m_pValue->IsUndefined())
    return VT_undefined;
  return VT_unknown;
}

FX_BOOL CJS_Value::IsArrayObject() const {
  if (m_pValue.IsEmpty())
    return FALSE;
  return m_pValue->IsArray();
}

FX_BOOL CJS_Value::IsDateObject() const {
  if (m_pValue.IsEmpty())
    return FALSE;
  return m_pValue->IsDate();
}

// CJS_Value::operator CJS_Array()
FX_BOOL CJS_Value::ConvertToArray(CJS_Array& array) const {
  if (IsArrayObject()) {
    array.Attach(JS_ToArray(m_isolate, m_pValue));
    return TRUE;
  }

  return FALSE;
}

FX_BOOL CJS_Value::ConvertToDate(CJS_Date& date) const {
  // 	if (GetType() == VT_date)
  // 	{
  // 		date = (double)(*this);
  // 		return TRUE;
  // 	}

  if (IsDateObject()) {
    date.Attach(m_pValue);
    return TRUE;
  }

  return FALSE;
}

/* ---------------------------- CJS_PropValue ---------------------------- */

CJS_PropValue::CJS_PropValue(const CJS_Value& value)
    : CJS_Value(value), m_bIsSetting(0) {}

CJS_PropValue::CJS_PropValue(v8::Isolate* isolate)
    : CJS_Value(isolate), m_bIsSetting(0) {}

CJS_PropValue::~CJS_PropValue() {}

FX_BOOL CJS_PropValue::IsSetting() {
  return m_bIsSetting;
}

FX_BOOL CJS_PropValue::IsGetting() {
  return !m_bIsSetting;
}

void CJS_PropValue::operator<<(int iValue) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(iValue);
}

void CJS_PropValue::operator>>(int& iValue) const {
  ASSERT(m_bIsSetting);
  iValue = CJS_Value::ToInt();
}

void CJS_PropValue::operator<<(bool bValue) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(bValue);
}

void CJS_PropValue::operator>>(bool& bValue) const {
  ASSERT(m_bIsSetting);
  bValue = CJS_Value::ToBool();
}

void CJS_PropValue::operator<<(double dValue) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(dValue);
}

void CJS_PropValue::operator>>(double& dValue) const {
  ASSERT(m_bIsSetting);
  dValue = CJS_Value::ToDouble();
}

void CJS_PropValue::operator<<(CJS_Object* pObj) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(pObj);
}

void CJS_PropValue::operator>>(CJS_Object*& ppObj) const {
  ASSERT(m_bIsSetting);
  ppObj = CJS_Value::ToCJSObject();
}

void CJS_PropValue::operator<<(CJS_Document* pJsDoc) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(pJsDoc);
}

void CJS_PropValue::operator>>(CJS_Document*& ppJsDoc) const {
  ASSERT(m_bIsSetting);
  ppJsDoc = static_cast<CJS_Document*>(CJS_Value::ToCJSObject());
}

void CJS_PropValue::operator<<(JSFXObject pObj) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(pObj);
}

void CJS_PropValue::operator>>(JSFXObject& ppObj) const {
  ASSERT(m_bIsSetting);
  ppObj = CJS_Value::ToV8Object();
}

void CJS_PropValue::StartSetting() {
  m_bIsSetting = 1;
}

void CJS_PropValue::StartGetting() {
  m_bIsSetting = 0;
}
void CJS_PropValue::operator<<(CFX_ByteString string) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(string.c_str());
}

void CJS_PropValue::operator>>(CFX_ByteString& string) const {
  ASSERT(m_bIsSetting);
  string = CJS_Value::ToCFXByteString();
}

void CJS_PropValue::operator<<(const FX_WCHAR* c_string) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(c_string);
}

void CJS_PropValue::operator>>(CFX_WideString& wide_string) const {
  ASSERT(m_bIsSetting);
  wide_string = CJS_Value::ToCFXWideString();
}

void CJS_PropValue::operator<<(CFX_WideString wide_string) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(wide_string.c_str());
}

void CJS_PropValue::operator>>(CJS_Array& array) const {
  ASSERT(m_bIsSetting);
  ConvertToArray(array);
}

void CJS_PropValue::operator<<(CJS_Array& array) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(array);
}

void CJS_PropValue::operator>>(CJS_Date& date) const {
  ASSERT(m_bIsSetting);
  ConvertToDate(date);
}

void CJS_PropValue::operator<<(CJS_Date& date) {
  ASSERT(!m_bIsSetting);
  CJS_Value::operator=(date);
}

CJS_PropValue::operator v8::Local<v8::Value>() const {
  return m_pValue;
}

/* ======================================== CJS_Array
 * ========================================= */
CJS_Array::CJS_Array(v8::Isolate* isolate) : m_isolate(isolate) {}

CJS_Array::~CJS_Array() {}

void CJS_Array::Attach(v8::Local<v8::Array> pArray) {
  m_pArray = pArray;
}

FX_BOOL CJS_Array::IsAttached() {
  return FALSE;
}

void CJS_Array::GetElement(unsigned index, CJS_Value& value) {
  if (m_pArray.IsEmpty())
    return;
  v8::Local<v8::Value> p = JS_GetArrayElement(m_isolate, m_pArray, index);
  value.Attach(p, VT_object);
}

void CJS_Array::SetElement(unsigned index, CJS_Value value) {
  if (m_pArray.IsEmpty())
    m_pArray = JS_NewArray(m_isolate);

  JS_PutArrayElement(m_isolate, m_pArray, index, value.ToV8Value(),
                     value.GetType());
}

int CJS_Array::GetLength() {
  if (m_pArray.IsEmpty())
    return 0;
  return JS_GetArrayLength(m_pArray);
}

CJS_Array::operator v8::Local<v8::Array>() {
  if (m_pArray.IsEmpty())
    m_pArray = JS_NewArray(m_isolate);

  return m_pArray;
}

/* ======================================== CJS_Date
 * ========================================= */

CJS_Date::CJS_Date(v8::Isolate* isolate) : m_isolate(isolate) {}

CJS_Date::CJS_Date(v8::Isolate* isolate, double dMsec_time) {
  m_isolate = isolate;
  m_pDate = JS_NewDate(isolate, dMsec_time);
}

CJS_Date::CJS_Date(v8::Isolate* isolate,
                   int year,
                   int mon,
                   int day,
                   int hour,
                   int min,
                   int sec) {
  m_isolate = isolate;
  m_pDate = JS_NewDate(isolate, MakeDate(year, mon, day, hour, min, sec, 0));
}

double CJS_Date::MakeDate(int year,
                          int mon,
                          int day,
                          int hour,
                          int min,
                          int sec,
                          int ms) {
  return JS_MakeDate(JS_MakeDay(year, mon, day),
                     JS_MakeTime(hour, min, sec, ms));
}

CJS_Date::~CJS_Date() {}

FX_BOOL CJS_Date::IsValidDate() {
  if (m_pDate.IsEmpty())
    return FALSE;
  return !JS_PortIsNan(JS_ToNumber(m_isolate, m_pDate));
}

void CJS_Date::Attach(v8::Local<v8::Value> pDate) {
  m_pDate = pDate;
}

int CJS_Date::GetYear() {
  if (IsValidDate())
    return JS_GetYearFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetYear(int iYear) {
  double date = MakeDate(iYear, GetMonth(), GetDay(), GetHours(), GetMinutes(),
                         GetSeconds(), 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

int CJS_Date::GetMonth() {
  if (IsValidDate())
    return JS_GetMonthFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetMonth(int iMonth) {
  double date = MakeDate(GetYear(), iMonth, GetDay(), GetHours(), GetMinutes(),
                         GetSeconds(), 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

int CJS_Date::GetDay() {
  if (IsValidDate())
    return JS_GetDayFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetDay(int iDay) {
  double date = MakeDate(GetYear(), GetMonth(), iDay, GetHours(), GetMinutes(),
                         GetSeconds(), 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

int CJS_Date::GetHours() {
  if (IsValidDate())
    return JS_GetHourFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetHours(int iHours) {
  double date = MakeDate(GetYear(), GetMonth(), GetDay(), iHours, GetMinutes(),
                         GetSeconds(), 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

int CJS_Date::GetMinutes() {
  if (IsValidDate())
    return JS_GetMinFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetMinutes(int minutes) {
  double date = MakeDate(GetYear(), GetMonth(), GetDay(), GetHours(), minutes,
                         GetSeconds(), 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

int CJS_Date::GetSeconds() {
  if (IsValidDate())
    return JS_GetSecFromTime(JS_LocalTime(JS_ToNumber(m_isolate, m_pDate)));

  return 0;
}

void CJS_Date::SetSeconds(int seconds) {
  double date = MakeDate(GetYear(), GetMonth(), GetDay(), GetHours(),
                         GetMinutes(), seconds, 0);
  JS_ValueCopy(m_pDate, JS_NewDate(m_isolate, date));
}

CJS_Date::operator v8::Local<v8::Value>() {
  return m_pDate;
}

CJS_Date::operator double() const {
  if (m_pDate.IsEmpty())
    return 0.0;
  return JS_ToNumber(m_isolate, m_pDate);
}

CFX_WideString CJS_Date::ToString() const {
  if (m_pDate.IsEmpty())
    return L"";
  return JS_ToString(m_isolate, m_pDate);
}
