// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FXCRT_IFX_LOCALE_H_
#define CORE_FXCRT_IFX_LOCALE_H_

#include "core/fxcrt/cfx_datetime.h"
#include "core/fxcrt/fx_string.h"

enum FX_LOCALENUMSYMBOL {
  FX_LOCALENUMSYMBOL_Decimal,
  FX_LOCALENUMSYMBOL_Grouping,
  FX_LOCALENUMSYMBOL_Percent,
  FX_LOCALENUMSYMBOL_Minus,
  FX_LOCALENUMSYMBOL_Zero,
  FX_LOCALENUMSYMBOL_CurrencySymbol,
  FX_LOCALENUMSYMBOL_CurrencyName,
};

enum FX_LOCALEDATETIMESUBCATEGORY {
  FX_LOCALEDATETIMESUBCATEGORY_Default,
  FX_LOCALEDATETIMESUBCATEGORY_Short,
  FX_LOCALEDATETIMESUBCATEGORY_Medium,
  FX_LOCALEDATETIMESUBCATEGORY_Full,
  FX_LOCALEDATETIMESUBCATEGORY_Long,
};

enum FX_LOCALENUMSUBCATEGORY {
  FX_LOCALENUMPATTERN_Percent,
  FX_LOCALENUMPATTERN_Currency,
  FX_LOCALENUMPATTERN_Decimal,
  FX_LOCALENUMPATTERN_Integer,
};

enum FX_LOCALECATEGORY {
  FX_LOCALECATEGORY_Unknown,
  FX_LOCALECATEGORY_Date,
  FX_LOCALECATEGORY_Time,
  FX_LOCALECATEGORY_DateTime,
  FX_LOCALECATEGORY_Num,
  FX_LOCALECATEGORY_Text,
  FX_LOCALECATEGORY_Zero,
  FX_LOCALECATEGORY_Null,
};

enum FX_DATETIMETYPE {
  FX_DATETIMETYPE_Unknown,
  FX_DATETIMETYPE_Date,
  FX_DATETIMETYPE_Time,
  FX_DATETIMETYPE_DateTime,
  FX_DATETIMETYPE_TimeDate,
};

class IFX_Locale {
 public:
  virtual ~IFX_Locale() {}

  virtual CFX_WideString GetName() const = 0;
  virtual CFX_WideString GetNumbericSymbol(FX_LOCALENUMSYMBOL eType) const = 0;
  virtual CFX_WideString GetDateTimeSymbols() const = 0;
  virtual CFX_WideString GetMonthName(int32_t nMonth, bool bAbbr) const = 0;
  virtual CFX_WideString GetDayName(int32_t nWeek, bool bAbbr) const = 0;
  virtual CFX_WideString GetMeridiemName(bool bAM) const = 0;
  virtual FX_TIMEZONE GetTimeZone() const = 0;
  virtual CFX_WideString GetEraName(bool bAD) const = 0;
  virtual CFX_WideString GetDatePattern(
      FX_LOCALEDATETIMESUBCATEGORY eType) const = 0;
  virtual CFX_WideString GetTimePattern(
      FX_LOCALEDATETIMESUBCATEGORY eType) const = 0;
  virtual CFX_WideString GetNumPattern(FX_LOCALENUMSUBCATEGORY eType) const = 0;
};

#endif  // CORE_FXCRT_IFX_LOCALE_H_
