// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fwl/cfwl_monthcalendar.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/base/ptr_util.h"
#include "xfa/fde/tto/fde_textout.h"
#include "xfa/fwl/cfwl_datetimepicker.h"
#include "xfa/fwl/cfwl_formproxy.h"
#include "xfa/fwl/cfwl_messagemouse.h"
#include "xfa/fwl/cfwl_notedriver.h"
#include "xfa/fwl/cfwl_themebackground.h"
#include "xfa/fwl/cfwl_themetext.h"
#include "xfa/fwl/ifwl_themeprovider.h"

#define MONTHCAL_HSEP_HEIGHT 1
#define MONTHCAL_VSEP_WIDTH 1
#define MONTHCAL_HMARGIN 3
#define MONTHCAL_VMARGIN 2
#define MONTHCAL_ROWS 9
#define MONTHCAL_COLUMNS 7
#define MONTHCAL_HEADER_BTN_VMARGIN 7
#define MONTHCAL_HEADER_BTN_HMARGIN 5

namespace {

CFX_WideString GetCapacityForDay(IFWL_ThemeProvider* pTheme,
                                 CFWL_ThemePart& params,
                                 uint32_t day) {
  ASSERT(day < 7);

  if (day == 0)
    return L"Sun";
  if (day == 1)
    return L"Mon";
  if (day == 2)
    return L"Tue";
  if (day == 3)
    return L"Wed";
  if (day == 4)
    return L"Thu";
  if (day == 5)
    return L"Fri";
  return L"Sat";
}

CFX_WideString GetCapacityForMonth(IFWL_ThemeProvider* pTheme,
                                   CFWL_ThemePart& params,
                                   uint32_t month) {
  ASSERT(month < 12);

  if (month == 0)
    return L"January";
  if (month == 1)
    return L"February";
  if (month == 2)
    return L"March";
  if (month == 3)
    return L"April";
  if (month == 4)
    return L"May";
  if (month == 5)
    return L"June";
  if (month == 6)
    return L"July";
  if (month == 7)
    return L"August";
  if (month == 8)
    return L"September";
  if (month == 9)
    return L"October";
  if (month == 10)
    return L"November";
  return L"December";
}

}  // namespace

CFWL_MonthCalendar::CFWL_MonthCalendar(
    const CFWL_App* app,
    std::unique_ptr<CFWL_WidgetProperties> properties,
    CFWL_Widget* pOuter)
    : CFWL_Widget(app, std::move(properties), pOuter),
      m_bInitialized(false),
      m_pDateTime(new CFX_DateTime),
      m_iCurYear(2011),
      m_iCurMonth(1),
      m_iYear(2011),
      m_iMonth(1),
      m_iDay(1),
      m_iHovered(-1),
      m_iLBtnPartStates(CFWL_PartState_Normal),
      m_iRBtnPartStates(CFWL_PartState_Normal),
      m_bFlag(false) {
  m_rtHead.Reset();
  m_rtWeek.Reset();
  m_rtLBtn.Reset();
  m_rtRBtn.Reset();
  m_rtDates.Reset();
  m_rtHSep.Reset();
  m_rtHeadText.Reset();
  m_rtToday.Reset();
  m_rtTodayFlag.Reset();
  m_rtClient.Reset();
  m_rtWeekNum.Reset();
  m_rtWeekNumSep.Reset();
}

CFWL_MonthCalendar::~CFWL_MonthCalendar() {
  ClearDateItem();
  m_arrSelDays.RemoveAll();
}

FWL_Type CFWL_MonthCalendar::GetClassID() const {
  return FWL_Type::MonthCalendar;
}

CFX_RectF CFWL_MonthCalendar::GetAutosizedWidgetRect() {
  CFX_SizeF fs = CalcSize();
  CFX_RectF rect;
  rect.Set(0, 0, fs.x, fs.y);
  InflateWidgetRect(rect);
  return rect;
}

void CFWL_MonthCalendar::Update() {
  if (IsLocked())
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  GetCapValue();
  if (!m_bInitialized) {
    InitDate();
    m_bInitialized = true;
  }

  ClearDateItem();
  ResetDateItem();
  Layout();
}

void CFWL_MonthCalendar::DrawWidget(CFX_Graphics* pGraphics,
                                    const CFX_Matrix* pMatrix) {
  if (!pGraphics)
    return;
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();

  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  if (HasBorder())
    DrawBorder(pGraphics, CFWL_Part::Border, pTheme, pMatrix);

  DrawBackground(pGraphics, pTheme, pMatrix);
  DrawHeadBK(pGraphics, pTheme, pMatrix);
  DrawLButton(pGraphics, pTheme, pMatrix);
  DrawRButton(pGraphics, pTheme, pMatrix);
  DrawSeperator(pGraphics, pTheme, pMatrix);
  DrawDatesInBK(pGraphics, pTheme, pMatrix);
  DrawDatesInCircle(pGraphics, pTheme, pMatrix);
  DrawCaption(pGraphics, pTheme, pMatrix);
  DrawWeek(pGraphics, pTheme, pMatrix);
  DrawDatesIn(pGraphics, pTheme, pMatrix);
  DrawDatesOut(pGraphics, pTheme, pMatrix);
  DrawToday(pGraphics, pTheme, pMatrix);
}

void CFWL_MonthCalendar::SetSelect(int32_t iYear,
                                   int32_t iMonth,
                                   int32_t iDay) {
  ChangeToMonth(iYear, iMonth);
  AddSelDay(iDay);
}

void CFWL_MonthCalendar::DrawBackground(CFX_Graphics* pGraphics,
                                        IFWL_ThemeProvider* pTheme,
                                        const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::Background;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_rtPart = m_rtClient;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

void CFWL_MonthCalendar::DrawHeadBK(CFX_Graphics* pGraphics,
                                    IFWL_ThemeProvider* pTheme,
                                    const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::Header;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_rtPart = m_rtHead;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

void CFWL_MonthCalendar::DrawLButton(CFX_Graphics* pGraphics,
                                     IFWL_ThemeProvider* pTheme,
                                     const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::LBtn;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = m_iLBtnPartStates;
  params.m_rtPart = m_rtLBtn;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

void CFWL_MonthCalendar::DrawRButton(CFX_Graphics* pGraphics,
                                     IFWL_ThemeProvider* pTheme,
                                     const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::RBtn;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = m_iRBtnPartStates;
  params.m_rtPart = m_rtRBtn;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

void CFWL_MonthCalendar::DrawCaption(CFX_Graphics* pGraphics,
                                     IFWL_ThemeProvider* pTheme,
                                     const CFX_Matrix* pMatrix) {
  CFWL_ThemeText textParam;
  textParam.m_pWidget = this;
  textParam.m_iPart = CFWL_Part::Caption;
  textParam.m_dwStates = CFWL_PartState_Normal;
  textParam.m_pGraphics = pGraphics;
  textParam.m_wsText = GetHeadText(m_iCurYear, m_iCurMonth);
  m_szHead =
      CalcTextSize(textParam.m_wsText, m_pProperties->m_pThemeProvider, false);
  CalcHeadSize();
  textParam.m_rtPart = m_rtHeadText;
  textParam.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
  textParam.m_iTTOAlign = FDE_TTOALIGNMENT_Center;
  if (pMatrix)
    textParam.m_matrix.Concat(*pMatrix);
  pTheme->DrawText(&textParam);
}

void CFWL_MonthCalendar::DrawSeperator(CFX_Graphics* pGraphics,
                                       IFWL_ThemeProvider* pTheme,
                                       const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::HSeparator;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_rtPart = m_rtHSep;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

void CFWL_MonthCalendar::DrawDatesInBK(CFX_Graphics* pGraphics,
                                       IFWL_ThemeProvider* pTheme,
                                       const CFX_Matrix* pMatrix) {
  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::DateInBK;
  params.m_pGraphics = pGraphics;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);

  int32_t iCount = m_arrDates.GetSize();
  for (int32_t j = 0; j < iCount; j++) {
    DATEINFO* pDataInfo = m_arrDates.GetAt(j);
    if (pDataInfo->dwStates & FWL_ITEMSTATE_MCD_Selected) {
      params.m_dwStates |= CFWL_PartState_Selected;
      if (pDataInfo->dwStates & FWL_ITEMSTATE_MCD_Flag) {
        params.m_dwStates |= CFWL_PartState_Flagged;
      }
    } else if (j == m_iHovered - 1) {
      params.m_dwStates |= CFWL_PartState_Hovered;
    } else if (pDataInfo->dwStates & FWL_ITEMSTATE_MCD_Flag) {
      params.m_dwStates = CFWL_PartState_Flagged;
      pTheme->DrawBackground(&params);
    }
    params.m_rtPart = pDataInfo->rect;
    pTheme->DrawBackground(&params);
    params.m_dwStates = 0;
  }
}

void CFWL_MonthCalendar::DrawWeek(CFX_Graphics* pGraphics,
                                  IFWL_ThemeProvider* pTheme,
                                  const CFX_Matrix* pMatrix) {
  CFWL_ThemeText params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::Week;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_iTTOAlign = FDE_TTOALIGNMENT_Center;
  CFX_RectF rtDayOfWeek;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);

  for (int32_t i = 0; i < 7; i++) {
    rtDayOfWeek.Set(m_rtWeek.left + i * (m_szCell.x + MONTHCAL_HMARGIN * 2),
                    m_rtWeek.top, m_szCell.x, m_szCell.y);
    params.m_rtPart = rtDayOfWeek;
    params.m_wsText = GetCapacityForDay(pTheme, params, i);
    params.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
    pTheme->DrawText(&params);
  }
}

void CFWL_MonthCalendar::DrawToday(CFX_Graphics* pGraphics,
                                   IFWL_ThemeProvider* pTheme,
                                   const CFX_Matrix* pMatrix) {
  CFWL_ThemeText params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::Today;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_iTTOAlign = FDE_TTOALIGNMENT_CenterLeft;
  params.m_wsText = L"Today" + GetTodayText(m_iYear, m_iMonth, m_iDay);

  m_szToday =
      CalcTextSize(params.m_wsText, m_pProperties->m_pThemeProvider, false);
  CalcTodaySize();
  params.m_rtPart = m_rtToday;
  params.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawText(&params);
}

void CFWL_MonthCalendar::DrawDatesIn(CFX_Graphics* pGraphics,
                                     IFWL_ThemeProvider* pTheme,
                                     const CFX_Matrix* pMatrix) {
  CFWL_ThemeText params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::DatesIn;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_iTTOAlign = FDE_TTOALIGNMENT_Center;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);

  int32_t iCount = m_arrDates.GetSize();
  for (int32_t j = 0; j < iCount; j++) {
    DATEINFO* pDataInfo = m_arrDates.GetAt(j);
    params.m_wsText = pDataInfo->wsDay;
    params.m_rtPart = pDataInfo->rect;
    params.m_dwStates = pDataInfo->dwStates;
    if (j + 1 == m_iHovered)
      params.m_dwStates |= CFWL_PartState_Hovered;
    params.m_dwTTOStyles = FDE_TTOSTYLE_SingleLine;
    pTheme->DrawText(&params);
  }
}

void CFWL_MonthCalendar::DrawDatesOut(CFX_Graphics* pGraphics,
                                      IFWL_ThemeProvider* pTheme,
                                      const CFX_Matrix* pMatrix) {
  CFWL_ThemeText params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::DatesOut;
  params.m_pGraphics = pGraphics;
  params.m_dwStates = CFWL_PartState_Normal;
  params.m_iTTOAlign = FDE_TTOALIGNMENT_Center;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawText(&params);
}

void CFWL_MonthCalendar::DrawDatesInCircle(CFX_Graphics* pGraphics,
                                           IFWL_ThemeProvider* pTheme,
                                           const CFX_Matrix* pMatrix) {
  if (m_iMonth != m_iCurMonth || m_iYear != m_iCurYear)
    return;
  if (m_iDay < 1 || m_iDay > m_arrDates.GetSize())
    return;

  DATEINFO* pDate = m_arrDates[m_iDay - 1];
  if (!pDate)
    return;

  CFWL_ThemeBackground params;
  params.m_pWidget = this;
  params.m_iPart = CFWL_Part::DateInCircle;
  params.m_pGraphics = pGraphics;
  params.m_rtPart = pDate->rect;
  params.m_dwStates = CFWL_PartState_Normal;
  if (pMatrix)
    params.m_matrix.Concat(*pMatrix);
  pTheme->DrawBackground(&params);
}

CFX_SizeF CFWL_MonthCalendar::CalcSize() {
  if (!m_pProperties->m_pThemeProvider)
    return CFX_SizeF();

  CFX_SizeF fs;
  CFWL_ThemePart params;
  params.m_pWidget = this;
  IFWL_ThemeProvider* pTheme = m_pProperties->m_pThemeProvider;
  FX_FLOAT fMaxWeekW = 0.0f;
  FX_FLOAT fMaxWeekH = 0.0f;

  for (uint32_t i = 0; i < 7; ++i) {
    CFX_SizeF sz = CalcTextSize(GetCapacityForDay(pTheme, params, i),
                                m_pProperties->m_pThemeProvider, false);
    fMaxWeekW = (fMaxWeekW >= sz.x) ? fMaxWeekW : sz.x;
    fMaxWeekH = (fMaxWeekH >= sz.y) ? fMaxWeekH : sz.y;
  }

  FX_FLOAT fDayMaxW = 0.0f;
  FX_FLOAT fDayMaxH = 0.0f;
  for (int day = 10; day <= 31; day++) {
    CFX_WideString wsDay;
    wsDay.Format(L"%d", day);
    CFX_SizeF sz = CalcTextSize(wsDay, m_pProperties->m_pThemeProvider, false);
    fDayMaxW = (fDayMaxW >= sz.x) ? fDayMaxW : sz.x;
    fDayMaxH = (fDayMaxH >= sz.y) ? fDayMaxH : sz.y;
  }
  m_szCell.x = FX_FLOAT((fMaxWeekW >= fDayMaxW) ? (int)(fMaxWeekW + 0.5)
                                                : (int)(fDayMaxW + 0.5));
  m_szCell.y = (fMaxWeekH >= fDayMaxH) ? fMaxWeekH : fDayMaxH;
  fs.x = m_szCell.x * MONTHCAL_COLUMNS +
         MONTHCAL_HMARGIN * MONTHCAL_COLUMNS * 2 +
         MONTHCAL_HEADER_BTN_HMARGIN * 2;
  FX_FLOAT fMonthMaxW = 0.0f;
  FX_FLOAT fMonthMaxH = 0.0f;

  for (uint32_t i = 0; i < 12; ++i) {
    CFX_SizeF sz = CalcTextSize(GetCapacityForMonth(pTheme, params, i),
                                m_pProperties->m_pThemeProvider, false);
    fMonthMaxW = (fMonthMaxW >= sz.x) ? fMonthMaxW : sz.x;
    fMonthMaxH = (fMonthMaxH >= sz.y) ? fMonthMaxH : sz.y;
  }

  CFX_SizeF szYear = CalcTextSize(GetHeadText(m_iYear, m_iMonth),
                                  m_pProperties->m_pThemeProvider, false);
  fMonthMaxH = std::max(fMonthMaxH, szYear.y);
  m_szHead = CFX_SizeF(fMonthMaxW + szYear.x, fMonthMaxH);
  fMonthMaxW = m_szHead.x + MONTHCAL_HEADER_BTN_HMARGIN * 2 + m_szCell.x * 2;
  fs.x = std::max(fs.x, fMonthMaxW);

  CFX_WideString wsToday = GetTodayText(m_iYear, m_iMonth, m_iDay);
  m_wsToday = L"Today" + wsToday;
  m_szToday = CalcTextSize(wsToday, m_pProperties->m_pThemeProvider, false);
  m_szToday.y = (m_szToday.y >= m_szCell.y) ? m_szToday.y : m_szCell.y;
  fs.y = m_szCell.x + m_szCell.y * (MONTHCAL_ROWS - 2) + m_szToday.y +
         MONTHCAL_VMARGIN * MONTHCAL_ROWS * 2 + MONTHCAL_HEADER_BTN_VMARGIN * 4;
  return fs;
}

void CFWL_MonthCalendar::CalcHeadSize() {
  FX_FLOAT fHeadHMargin = (m_rtClient.width - m_szHead.x) / 2;
  FX_FLOAT fHeadVMargin = (m_szCell.x - m_szHead.y) / 2;
  m_rtHeadText.Set(m_rtClient.left + fHeadHMargin,
                   m_rtClient.top + MONTHCAL_HEADER_BTN_VMARGIN +
                       MONTHCAL_VMARGIN + fHeadVMargin,
                   m_szHead.x, m_szHead.y);
}

void CFWL_MonthCalendar::CalcTodaySize() {
  m_rtTodayFlag.Set(
      m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN + MONTHCAL_HMARGIN,
      m_rtDates.bottom() + MONTHCAL_HEADER_BTN_VMARGIN + MONTHCAL_VMARGIN,
      m_szCell.x, m_szToday.y);
  m_rtToday.Set(
      m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN + m_szCell.x +
          MONTHCAL_HMARGIN * 2,
      m_rtDates.bottom() + MONTHCAL_HEADER_BTN_VMARGIN + MONTHCAL_VMARGIN,
      m_szToday.x, m_szToday.y);
}

void CFWL_MonthCalendar::Layout() {
  m_rtClient = GetClientRect();

  m_rtHead.Set(
      m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN, m_rtClient.top,
      m_rtClient.width - MONTHCAL_HEADER_BTN_HMARGIN * 2,
      m_szCell.x + (MONTHCAL_HEADER_BTN_VMARGIN + MONTHCAL_VMARGIN) * 2);
  m_rtWeek.Set(m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN, m_rtHead.bottom(),
               m_rtClient.width - MONTHCAL_HEADER_BTN_HMARGIN * 2,
               m_szCell.y + MONTHCAL_VMARGIN * 2);
  m_rtLBtn.Set(m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN,
               m_rtClient.top + MONTHCAL_HEADER_BTN_VMARGIN, m_szCell.x,
               m_szCell.x);
  m_rtRBtn.Set(m_rtClient.left + m_rtClient.width -
                   MONTHCAL_HEADER_BTN_HMARGIN - m_szCell.x,
               m_rtClient.top + MONTHCAL_HEADER_BTN_VMARGIN, m_szCell.x,
               m_szCell.x);
  m_rtHSep.Set(
      m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN + MONTHCAL_HMARGIN,
      m_rtWeek.bottom() - MONTHCAL_VMARGIN,
      m_rtClient.width - (MONTHCAL_HEADER_BTN_HMARGIN + MONTHCAL_HMARGIN) * 2,
      MONTHCAL_HSEP_HEIGHT);
  m_rtDates.Set(m_rtClient.left + MONTHCAL_HEADER_BTN_HMARGIN,
                m_rtWeek.bottom(),
                m_rtClient.width - MONTHCAL_HEADER_BTN_HMARGIN * 2,
                m_szCell.y * (MONTHCAL_ROWS - 3) +
                    MONTHCAL_VMARGIN * (MONTHCAL_ROWS - 3) * 2);

  CalDateItem();
}

void CFWL_MonthCalendar::CalDateItem() {
  bool bNewWeek = false;
  int32_t iWeekOfMonth = 0;
  FX_FLOAT fLeft = m_rtDates.left;
  FX_FLOAT fTop = m_rtDates.top;
  int32_t iCount = m_arrDates.GetSize();
  for (int32_t i = 0; i < iCount; i++) {
    DATEINFO* pDateInfo = m_arrDates.GetAt(i);
    if (bNewWeek) {
      iWeekOfMonth++;
      bNewWeek = false;
    }
    pDateInfo->rect.Set(
        fLeft + pDateInfo->iDayOfWeek * (m_szCell.x + (MONTHCAL_HMARGIN * 2)),
        fTop + iWeekOfMonth * (m_szCell.y + (MONTHCAL_VMARGIN * 2)),
        m_szCell.x + (MONTHCAL_HMARGIN * 2),
        m_szCell.y + (MONTHCAL_VMARGIN * 2));
    if (pDateInfo->iDayOfWeek >= 6)
      bNewWeek = true;
  }
}

void CFWL_MonthCalendar::GetCapValue() {
  if (!m_pProperties->m_pThemeProvider)
    m_pProperties->m_pThemeProvider = GetAvailableTheme();
}

void CFWL_MonthCalendar::InitDate() {
  // TODO(dsinclair): These should pull the real today values instead of
  // pretending it's 2011-01-01.
  m_iYear = 2011;
  m_iMonth = 1;
  m_iDay = 1;
  m_iCurYear = m_iYear;
  m_iCurMonth = m_iMonth;

  m_wsToday = GetTodayText(m_iYear, m_iMonth, m_iDay);
  m_wsHead = GetHeadText(m_iCurYear, m_iCurMonth);
  m_dtMin = DATE(1500, 12, 1);
  m_dtMax = DATE(2200, 1, 1);
}

void CFWL_MonthCalendar::ClearDateItem() {
  for (int32_t i = 0; i < m_arrDates.GetSize(); i++)
    delete m_arrDates.GetAt(i);
  m_arrDates.RemoveAll();
}

void CFWL_MonthCalendar::ResetDateItem() {
  m_pDateTime->Set(m_iCurYear, m_iCurMonth, 1);
  int32_t iDays = FX_DaysInMonth(m_iCurYear, m_iCurMonth);
  int32_t iDayOfWeek = m_pDateTime->GetDayOfWeek();
  for (int32_t i = 0; i < iDays; i++) {
    if (iDayOfWeek >= 7)
      iDayOfWeek = 0;

    CFX_WideString wsDay;
    wsDay.Format(L"%d", i + 1);
    uint32_t dwStates = 0;
    if (m_iYear == m_iCurYear && m_iMonth == m_iCurMonth && m_iDay == (i + 1))
      dwStates |= FWL_ITEMSTATE_MCD_Flag;
    if (m_arrSelDays.Find(i + 1) != -1)
      dwStates |= FWL_ITEMSTATE_MCD_Selected;

    CFX_RectF rtDate;
    rtDate.Set(0, 0, 0, 0);
    m_arrDates.Add(new DATEINFO(i + 1, iDayOfWeek, dwStates, rtDate, wsDay));
    iDayOfWeek++;
  }
}

void CFWL_MonthCalendar::NextMonth() {
  int32_t iYear = m_iCurYear;
  int32_t iMonth = m_iCurMonth;
  if (iMonth >= 12) {
    iMonth = 1;
    iYear++;
  } else {
    iMonth++;
  }
  DATE dt(m_iCurYear, m_iCurMonth, 1);
  if (!(dt < m_dtMax))
    return;

  m_iCurYear = iYear, m_iCurMonth = iMonth;
  ChangeToMonth(m_iCurYear, m_iCurMonth);
}

void CFWL_MonthCalendar::PrevMonth() {
  int32_t iYear = m_iCurYear;
  int32_t iMonth = m_iCurMonth;
  if (iMonth <= 1) {
    iMonth = 12;
    iYear--;
  } else {
    iMonth--;
  }

  DATE dt(m_iCurYear, m_iCurMonth, 1);
  if (!(dt > m_dtMin))
    return;

  m_iCurYear = iYear, m_iCurMonth = iMonth;
  ChangeToMonth(m_iCurYear, m_iCurMonth);
}

void CFWL_MonthCalendar::ChangeToMonth(int32_t iYear, int32_t iMonth) {
  m_iCurYear = iYear;
  m_iCurMonth = iMonth;
  m_iHovered = -1;

  ClearDateItem();
  ResetDateItem();
  CalDateItem();
  m_wsHead = GetHeadText(m_iCurYear, m_iCurMonth);
}

void CFWL_MonthCalendar::RemoveSelDay() {
  int32_t iCount = m_arrSelDays.GetSize();
  int32_t iDatesCount = m_arrDates.GetSize();
  for (int32_t i = 0; i < iCount; i++) {
    int32_t iSelDay = m_arrSelDays.GetAt(i);
    if (iSelDay <= iDatesCount) {
      DATEINFO* pDateInfo = m_arrDates.GetAt(iSelDay - 1);
      pDateInfo->dwStates &= ~FWL_ITEMSTATE_MCD_Selected;
    }
  }
  m_arrSelDays.RemoveAll();
  return;
}

void CFWL_MonthCalendar::AddSelDay(int32_t iDay) {
  ASSERT(iDay > 0);
  if (m_arrSelDays.Find(iDay) != -1)
    return;

  RemoveSelDay();
  if (iDay <= m_arrDates.GetSize()) {
    DATEINFO* pDateInfo = m_arrDates.GetAt(iDay - 1);
    pDateInfo->dwStates |= FWL_ITEMSTATE_MCD_Selected;
  }
  m_arrSelDays.Add(iDay);
}

void CFWL_MonthCalendar::JumpToToday() {
  if (m_iYear != m_iCurYear || m_iMonth != m_iCurMonth) {
    m_iCurYear = m_iYear;
    m_iCurMonth = m_iMonth;
    ChangeToMonth(m_iYear, m_iMonth);
    AddSelDay(m_iDay);
    return;
  }

  if (m_arrSelDays.Find(m_iDay) == -1)
    AddSelDay(m_iDay);
}

CFX_WideString CFWL_MonthCalendar::GetHeadText(int32_t iYear, int32_t iMonth) {
  ASSERT(iMonth > 0 && iMonth < 13);
  static const FX_WCHAR* const pMonth[] = {
      L"January",   L"February", L"March",    L"April",
      L"May",       L"June",     L"July",     L"August",
      L"September", L"October",  L"November", L"December"};
  CFX_WideString wsHead;
  wsHead.Format(L"%s, %d", pMonth[iMonth - 1], iYear);
  return wsHead;
}

CFX_WideString CFWL_MonthCalendar::GetTodayText(int32_t iYear,
                                                int32_t iMonth,
                                                int32_t iDay) {
  CFX_WideString wsToday;
  wsToday.Format(L", %d/%d/%d", iDay, iMonth, iYear);
  return wsToday;
}

int32_t CFWL_MonthCalendar::GetDayAtPoint(FX_FLOAT x, FX_FLOAT y) {
  int32_t iCount = m_arrDates.GetSize();
  for (int32_t i = 0; i < iCount; i++) {
    DATEINFO* pDateInfo = m_arrDates.GetAt(i);
    if (pDateInfo->rect.Contains(x, y))
      return ++i;
  }
  return -1;
}

CFX_RectF CFWL_MonthCalendar::GetDayRect(int32_t iDay) {
  if (iDay <= 0 || iDay > m_arrDates.GetSize())
    return CFX_RectF();

  DATEINFO* pDateInfo = m_arrDates[iDay - 1];
  if (!pDateInfo)
    return CFX_RectF();
  return pDateInfo->rect;
}

void CFWL_MonthCalendar::OnProcessMessage(CFWL_Message* pMessage) {
  if (!pMessage)
    return;

  switch (pMessage->GetType()) {
    case CFWL_Message::Type::SetFocus:
    case CFWL_Message::Type::KillFocus:
      GetOuter()->GetDelegate()->OnProcessMessage(pMessage);
      break;
    case CFWL_Message::Type::Key:
      break;
    case CFWL_Message::Type::Mouse: {
      CFWL_MessageMouse* pMouse = static_cast<CFWL_MessageMouse*>(pMessage);
      switch (pMouse->m_dwCmd) {
        case FWL_MouseCommand::LeftButtonDown:
          OnLButtonDown(pMouse);
          break;
        case FWL_MouseCommand::LeftButtonUp:
          OnLButtonUp(pMouse);
          break;
        case FWL_MouseCommand::Move:
          OnMouseMove(pMouse);
          break;
        case FWL_MouseCommand::Leave:
          OnMouseLeave(pMouse);
          break;
        default:
          break;
      }
      break;
    }
    default:
      break;
  }
  CFWL_Widget::OnProcessMessage(pMessage);
}

void CFWL_MonthCalendar::OnDrawWidget(CFX_Graphics* pGraphics,
                                      const CFX_Matrix* pMatrix) {
  DrawWidget(pGraphics, pMatrix);
}

void CFWL_MonthCalendar::OnLButtonDown(CFWL_MessageMouse* pMsg) {
  if (m_rtLBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iLBtnPartStates = CFWL_PartState_Pressed;
    PrevMonth();
    RepaintRect(m_rtClient);
  } else if (m_rtRBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iRBtnPartStates |= CFWL_PartState_Pressed;
    NextMonth();
    RepaintRect(m_rtClient);
  } else if (m_rtToday.Contains(pMsg->m_fx, pMsg->m_fy)) {
    JumpToToday();
    RepaintRect(m_rtClient);
  } else {
    CFWL_DateTimePicker* pIPicker = static_cast<CFWL_DateTimePicker*>(m_pOuter);
    if (pIPicker->IsMonthCalendarVisible())
      m_bFlag = true;
  }
}

void CFWL_MonthCalendar::OnLButtonUp(CFWL_MessageMouse* pMsg) {
  if (m_pWidgetMgr->IsFormDisabled())
    return DisForm_OnLButtonUp(pMsg);

  if (m_rtLBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iLBtnPartStates = 0;
    RepaintRect(m_rtLBtn);
    return;
  }
  if (m_rtRBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iRBtnPartStates = 0;
    RepaintRect(m_rtRBtn);
    return;
  }
  if (m_rtToday.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  int32_t iOldSel = 0;
  if (m_arrSelDays.GetSize() > 0)
    iOldSel = m_arrSelDays[0];

  int32_t iCurSel = GetDayAtPoint(pMsg->m_fx, pMsg->m_fy);
  CFWL_DateTimePicker* pIPicker = static_cast<CFWL_DateTimePicker*>(m_pOuter);
  CFX_RectF rt = pIPicker->GetFormProxy()->GetWidgetRect();
  rt.Set(0, 0, rt.width, rt.height);
  if (iCurSel > 0) {
    DATEINFO* lpDatesInfo = m_arrDates.GetAt(iCurSel - 1);
    CFX_RectF rtInvalidate(lpDatesInfo->rect);
    if (iOldSel > 0 && iOldSel <= m_arrDates.GetSize()) {
      lpDatesInfo = m_arrDates.GetAt(iOldSel - 1);
      rtInvalidate.Union(lpDatesInfo->rect);
    }
    AddSelDay(iCurSel);
    if (!m_pOuter)
      return;

    pIPicker->ProcessSelChanged(m_iCurYear, m_iCurMonth, iCurSel);
    pIPicker->ShowMonthCalendar(false);
  } else if (m_bFlag && (!rt.Contains(pMsg->m_fx, pMsg->m_fy))) {
    pIPicker->ShowMonthCalendar(false);
  }
  m_bFlag = false;
}

void CFWL_MonthCalendar::DisForm_OnLButtonUp(CFWL_MessageMouse* pMsg) {
  if (m_rtLBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iLBtnPartStates = 0;
    RepaintRect(m_rtLBtn);
    return;
  }
  if (m_rtRBtn.Contains(pMsg->m_fx, pMsg->m_fy)) {
    m_iRBtnPartStates = 0;
    RepaintRect(m_rtRBtn);
    return;
  }
  if (m_rtToday.Contains(pMsg->m_fx, pMsg->m_fy))
    return;

  int32_t iOldSel = 0;
  if (m_arrSelDays.GetSize() > 0)
    iOldSel = m_arrSelDays[0];

  int32_t iCurSel = GetDayAtPoint(pMsg->m_fx, pMsg->m_fy);
  if (iCurSel > 0) {
    DATEINFO* lpDatesInfo = m_arrDates.GetAt(iCurSel - 1);
    CFX_RectF rtInvalidate(lpDatesInfo->rect);
    if (iOldSel > 0 && iOldSel <= m_arrDates.GetSize()) {
      lpDatesInfo = m_arrDates.GetAt(iOldSel - 1);
      rtInvalidate.Union(lpDatesInfo->rect);
    }
    AddSelDay(iCurSel);
    CFWL_DateTimePicker* pDateTime =
        static_cast<CFWL_DateTimePicker*>(m_pOuter);
    pDateTime->ProcessSelChanged(m_iCurYear, m_iCurMonth, iCurSel);
    pDateTime->ShowMonthCalendar(false);
  }
}

void CFWL_MonthCalendar::OnMouseMove(CFWL_MessageMouse* pMsg) {
  bool bRepaint = false;
  CFX_RectF rtInvalidate;
  rtInvalidate.Set(0, 0, 0, 0);
  if (m_rtDates.Contains(pMsg->m_fx, pMsg->m_fy)) {
    int32_t iHover = GetDayAtPoint(pMsg->m_fx, pMsg->m_fy);
    bRepaint = m_iHovered != iHover;
    if (bRepaint) {
      if (m_iHovered > 0)
        rtInvalidate = GetDayRect(m_iHovered);
      if (iHover > 0) {
        CFX_RectF rtDay = GetDayRect(iHover);
        if (rtInvalidate.IsEmpty())
          rtInvalidate = rtDay;
        else
          rtInvalidate.Union(rtDay);
      }
    }
    m_iHovered = iHover;
  } else {
    bRepaint = m_iHovered > 0;
    if (bRepaint)
      rtInvalidate = GetDayRect(m_iHovered);

    m_iHovered = -1;
  }
  if (bRepaint && !rtInvalidate.IsEmpty())
    RepaintRect(rtInvalidate);
}

void CFWL_MonthCalendar::OnMouseLeave(CFWL_MessageMouse* pMsg) {
  if (m_iHovered <= 0)
    return;

  CFX_RectF rtInvalidate = GetDayRect(m_iHovered);
  m_iHovered = -1;
  if (!rtInvalidate.IsEmpty())
    RepaintRect(rtInvalidate);
}

CFWL_MonthCalendar::DATEINFO::DATEINFO(int32_t day,
                                       int32_t dayofweek,
                                       uint32_t dwSt,
                                       CFX_RectF rc,
                                       CFX_WideString& wsday)
    : iDay(day),
      iDayOfWeek(dayofweek),
      dwStates(dwSt),
      rect(rc),
      wsDay(wsday) {}

CFWL_MonthCalendar::DATEINFO::~DATEINFO() {}
