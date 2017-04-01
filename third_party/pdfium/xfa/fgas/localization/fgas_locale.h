// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FGAS_LOCALIZATION_FGAS_LOCALE_H_
#define XFA_FGAS_LOCALIZATION_FGAS_LOCALE_H_

#include <memory>

#include "core/fxcrt/fx_xml.h"
#include "xfa/fgas/localization/fgas_datetime.h"

class CFX_Unitime;

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
  virtual void GetNumbericSymbol(FX_LOCALENUMSYMBOL eType,
                                 CFX_WideString& wsNumSymbol) const = 0;
  virtual void GetDateTimeSymbols(CFX_WideString& wsDtSymbol) const = 0;
  virtual void GetMonthName(int32_t nMonth,
                            CFX_WideString& wsMonthName,
                            bool bAbbr = true) const = 0;
  virtual void GetDayName(int32_t nWeek,
                          CFX_WideString& wsDayName,
                          bool bAbbr = true) const = 0;
  virtual void GetMeridiemName(CFX_WideString& wsMeridiemName,
                               bool bAM = true) const = 0;
  virtual void GetTimeZone(FX_TIMEZONE* tz) const = 0;
  virtual void GetEraName(CFX_WideString& wsEraName, bool bAD = true) const = 0;
  virtual void GetDatePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                              CFX_WideString& wsPattern) const = 0;
  virtual void GetTimePattern(FX_LOCALEDATETIMESUBCATEGORY eType,
                              CFX_WideString& wsPattern) const = 0;
  virtual void GetNumPattern(FX_LOCALENUMSUBCATEGORY eType,
                             CFX_WideString& wsPattern) const = 0;
};

class IFX_LocaleMgr {
 public:
  virtual ~IFX_LocaleMgr() {}

  virtual uint16_t GetDefLocaleID() const = 0;
  virtual IFX_Locale* GetDefLocale() = 0;
  virtual IFX_Locale* GetLocaleByName(const CFX_WideString& wsLocaleName) = 0;

 protected:
  virtual std::unique_ptr<IFX_Locale> GetLocale(uint16_t lcid) = 0;
};

bool FX_DateFromCanonical(const CFX_WideString& wsDate, CFX_Unitime& datetime);
bool FX_TimeFromCanonical(const CFX_WideStringC& wsTime,
                          CFX_Unitime& datetime,
                          IFX_Locale* pLocale);
class CFX_Decimal {
 public:
  CFX_Decimal();
  explicit CFX_Decimal(uint32_t val);
  explicit CFX_Decimal(uint64_t val);
  explicit CFX_Decimal(int32_t val);
  explicit CFX_Decimal(int64_t val);
  explicit CFX_Decimal(FX_FLOAT val, uint8_t scale = 3);
  explicit CFX_Decimal(const CFX_WideStringC& str);
  explicit CFX_Decimal(const CFX_ByteStringC& str);
  operator CFX_WideString() const;
  operator double() const;
  bool operator==(const CFX_Decimal& val) const;
  bool operator<=(const CFX_Decimal& val) const;
  bool operator>=(const CFX_Decimal& val) const;
  bool operator!=(const CFX_Decimal& val) const;
  bool operator<(const CFX_Decimal& val) const;
  bool operator>(const CFX_Decimal& val) const;
  CFX_Decimal operator+(const CFX_Decimal& val) const;
  CFX_Decimal operator-(const CFX_Decimal& val) const;
  CFX_Decimal operator*(const CFX_Decimal& val) const;
  CFX_Decimal operator/(const CFX_Decimal& val) const;
  CFX_Decimal operator%(const CFX_Decimal& val) const;
  void SetScale(uint8_t newScale);
  uint8_t GetScale();
  void SetAbs();
  void SetNegate();
  void SetFloor();
  void SetCeiling();
  void SetTruncate();

 protected:
  CFX_Decimal(uint32_t hi, uint32_t mid, uint32_t lo, bool neg, uint8_t scale);
  inline bool IsNotZero() const { return m_uHi || m_uMid || m_uLo; }
  inline int8_t Compare(const CFX_Decimal& val) const;
  inline void Swap(CFX_Decimal& val);
  inline void FloorOrCeil(bool bFloor);
  CFX_Decimal AddOrMinus(const CFX_Decimal& val, bool isAdding) const;
  CFX_Decimal Multiply(const CFX_Decimal& val) const;
  CFX_Decimal Divide(const CFX_Decimal& val) const;
  CFX_Decimal Modulus(const CFX_Decimal& val) const;
  uint32_t m_uFlags;
  uint32_t m_uHi;
  uint32_t m_uLo;
  uint32_t m_uMid;
};

#endif  // XFA_FGAS_LOCALIZATION_FGAS_LOCALE_H_
