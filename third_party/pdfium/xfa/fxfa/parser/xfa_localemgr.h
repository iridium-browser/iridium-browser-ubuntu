// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_XFA_LOCALEMGR_H_
#define XFA_FXFA_PARSER_XFA_LOCALEMGR_H_

#include <memory>
#include <vector>

#include "xfa/fgas/localization/fgas_datetime.h"
#include "xfa/fgas/localization/fgas_locale.h"
#include "xfa/fxfa/parser/xfa_localemgr.h"

class CXFA_Node;
class IFX_Locale;

#define XFA_LANGID_zh_CN 0x0804
#define XFA_LANGID_zh_TW 0x0404
#define XFA_LANGID_zh_HK 0x0c04
#define XFA_LANGID_ja_JP 0x0411
#define XFA_LANGID_ko_KR 0x0412
#define XFA_LANGID_en_US 0x0409
#define XFA_LANGID_en_GB 0x0809
#define XFA_LANGID_es_ES 0x0c0a
#define XFA_LANGID_es_LA 0x080a
#define XFA_LANGID_de_DE 0x0407
#define XFA_LANGID_fr_FR 0x040c
#define XFA_LANGID_it_IT 0x0410
#define XFA_LANGID_pt_BR 0x0416
#define XFA_LANGID_nl_NL 0x0413
#define XFA_LANGID_ru_RU 0x0419

class CXFA_LocaleMgr : public IFX_LocaleMgr {
 public:
  CXFA_LocaleMgr(CXFA_Node* pLocaleSet, CFX_WideString wsDeflcid);
  ~CXFA_LocaleMgr() override;

  // IFX_LocaleMgr
  uint16_t GetDefLocaleID() const override;
  IFX_Locale* GetDefLocale() override;
  IFX_Locale* GetLocaleByName(const CFX_WideString& wsLocaleName) override;

  void SetDefLocale(IFX_Locale* pLocale);
  CFX_WideStringC GetConfigLocaleName(CXFA_Node* pConfig);

 protected:
  std::unique_ptr<IFX_Locale> GetLocale(uint16_t lcid) override;

  std::vector<std::unique_ptr<IFX_Locale>> m_LocaleArray;
  std::vector<std::unique_ptr<IFX_Locale>> m_XMLLocaleArray;
  IFX_Locale* m_pDefLocale;  // owned by m_LocaleArray or m_XMLLocaleArray.
  CFX_WideString m_wsConfigLocale;
  uint16_t m_dwDeflcid;
  uint16_t m_dwLocaleFlags;
};

class CXFA_TimeZoneProvider {
 public:
  CXFA_TimeZoneProvider();
  ~CXFA_TimeZoneProvider();

  void GetTimeZone(FX_TIMEZONE* tz) const;

 private:
  FX_TIMEZONE m_tz;
};

#endif  // XFA_FXFA_PARSER_XFA_LOCALEMGR_H_
