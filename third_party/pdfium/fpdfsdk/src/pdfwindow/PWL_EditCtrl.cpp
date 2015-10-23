// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/pdfwindow/PDFWindow.h"
#include "../../include/pdfwindow/PWL_Wnd.h"
#include "../../include/pdfwindow/PWL_EditCtrl.h"
#include "../../include/pdfwindow/PWL_ScrollBar.h"
#include "../../include/pdfwindow/PWL_Utils.h"
#include "../../include/pdfwindow/PWL_Caret.h"
#include "../../include/pdfwindow/PWL_FontMap.h"

#define IsFloatZero(f) ((f) < 0.0001 && (f) > -0.0001)
#define IsFloatBigger(fa, fb) ((fa) > (fb) && !IsFloatZero((fa) - (fb)))
#define IsFloatSmaller(fa, fb) ((fa) < (fb) && !IsFloatZero((fa) - (fb)))
#define IsFloatEqual(fa, fb) IsFloatZero((fa) - (fb))

/* ---------------------------- CPWL_EditCtrl ------------------------------ */

CPWL_EditCtrl::CPWL_EditCtrl()
    : m_pEdit(NULL),
      m_pEditCaret(NULL),
      m_bMouseDown(FALSE),
      m_pEditNotify(NULL),
      m_nCharSet(DEFAULT_CHARSET),
      m_nCodePage(0) {
  m_pEdit = IFX_Edit::NewEdit();
  ASSERT(m_pEdit != NULL);
}

CPWL_EditCtrl::~CPWL_EditCtrl() {
  IFX_Edit::DelEdit(m_pEdit);
}

void CPWL_EditCtrl::OnCreate(PWL_CREATEPARAM& cp) {
  cp.eCursorType = FXCT_VBEAM;
}

void CPWL_EditCtrl::OnCreated() {
  SetFontSize(GetCreationParam().fFontSize);

  m_pEdit->SetFontMap(GetFontMap());
  m_pEdit->SetNotify(this);
  m_pEdit->Initialize();
}

FX_BOOL CPWL_EditCtrl::IsWndHorV() {
  CPDF_Matrix mt = GetWindowMatrix();
  CPDF_Point point1(0, 1);
  CPDF_Point point2(1, 1);

  mt.Transform(point1.x, point1.y);
  mt.Transform(point2.x, point2.y);

  return point2.y == point1.y;
}

void CPWL_EditCtrl::SetCursor() {
  if (IsValid()) {
    if (IFX_SystemHandler* pSH = GetSystemHandler()) {
      if (IsWndHorV())
        pSH->SetCursor(FXCT_VBEAM);
      else
        pSH->SetCursor(FXCT_HBEAM);
    }
  }
}

void CPWL_EditCtrl::RePosChildWnd() {
  m_pEdit->SetPlateRect(GetClientRect());
}

void CPWL_EditCtrl::OnNotify(CPWL_Wnd* pWnd,
                             FX_DWORD msg,
                             intptr_t wParam,
                             intptr_t lParam) {
  CPWL_Wnd::OnNotify(pWnd, msg, wParam, lParam);

  switch (msg) {
    case PNM_SETSCROLLINFO:
      switch (wParam) {
        case SBT_VSCROLL:
          if (CPWL_Wnd* pChild = GetVScrollBar()) {
            pChild->OnNotify(pWnd, PNM_SETSCROLLINFO, wParam, lParam);
          }
          break;
      }
      break;
    case PNM_SETSCROLLPOS:
      switch (wParam) {
        case SBT_VSCROLL:
          if (CPWL_Wnd* pChild = GetVScrollBar()) {
            pChild->OnNotify(pWnd, PNM_SETSCROLLPOS, wParam, lParam);
          }
          break;
      }
      break;
    case PNM_SCROLLWINDOW: {
      FX_FLOAT fPos = *(FX_FLOAT*)lParam;
      switch (wParam) {
        case SBT_VSCROLL:
          m_pEdit->SetScrollPos(CPDF_Point(m_pEdit->GetScrollPos().x, fPos));
          break;
      }
    } break;
    case PNM_SETCARETINFO: {
      if (PWL_CARET_INFO* pCaretInfo = (PWL_CARET_INFO*)wParam) {
        SetCaret(pCaretInfo->bVisible, pCaretInfo->ptHead, pCaretInfo->ptFoot);
      }
    } break;
  }
}

void CPWL_EditCtrl::CreateChildWnd(const PWL_CREATEPARAM& cp) {
  if (!IsReadOnly())
    CreateEditCaret(cp);
}

void CPWL_EditCtrl::CreateEditCaret(const PWL_CREATEPARAM& cp) {
  if (!m_pEditCaret) {
    m_pEditCaret = new CPWL_Caret;
    m_pEditCaret->SetInvalidRect(GetClientRect());

    PWL_CREATEPARAM ecp = cp;
    ecp.pParentWnd = this;
    ecp.dwFlags = PWS_CHILD | PWS_NOREFRESHCLIP;
    ecp.dwBorderWidth = 0;
    ecp.nBorderStyle = PBS_SOLID;
    ecp.rcRectWnd = CPDF_Rect(0, 0, 0, 0);

    m_pEditCaret->Create(ecp);
  }
}

void CPWL_EditCtrl::SetFontSize(FX_FLOAT fFontSize) {
  m_pEdit->SetFontSize(fFontSize);
}

FX_FLOAT CPWL_EditCtrl::GetFontSize() const {
  return m_pEdit->GetFontSize();
}

FX_BOOL CPWL_EditCtrl::OnKeyDown(FX_WORD nChar, FX_DWORD nFlag) {
  if (m_bMouseDown)
    return TRUE;

  FX_BOOL bRet = CPWL_Wnd::OnKeyDown(nChar, nFlag);

  // FILTER
  switch (nChar) {
    default:
      return FALSE;
    case FWL_VKEY_Delete:
    case FWL_VKEY_Up:
    case FWL_VKEY_Down:
    case FWL_VKEY_Left:
    case FWL_VKEY_Right:
    case FWL_VKEY_Home:
    case FWL_VKEY_End:
    case FWL_VKEY_Insert:
    case 'C':
    case 'V':
    case 'X':
    case 'A':
    case 'Z':
    case 'c':
    case 'v':
    case 'x':
    case 'a':
    case 'z':
      break;
  }

  if (nChar == FWL_VKEY_Delete) {
    if (m_pEdit->IsSelected())
      nChar = FWL_VKEY_Unknown;
  }

  switch (nChar) {
    case FWL_VKEY_Delete:
      Delete();
      return TRUE;
    case FWL_VKEY_Insert:
      if (IsSHIFTpressed(nFlag))
        PasteText();
      return TRUE;
    case FWL_VKEY_Up:
      m_pEdit->OnVK_UP(IsSHIFTpressed(nFlag), FALSE);
      return TRUE;
    case FWL_VKEY_Down:
      m_pEdit->OnVK_DOWN(IsSHIFTpressed(nFlag), FALSE);
      return TRUE;
    case FWL_VKEY_Left:
      m_pEdit->OnVK_LEFT(IsSHIFTpressed(nFlag), FALSE);
      return TRUE;
    case FWL_VKEY_Right:
      m_pEdit->OnVK_RIGHT(IsSHIFTpressed(nFlag), FALSE);
      return TRUE;
    case FWL_VKEY_Home:
      m_pEdit->OnVK_HOME(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      return TRUE;
    case FWL_VKEY_End:
      m_pEdit->OnVK_END(IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
      return TRUE;
    case FWL_VKEY_Unknown:
      if (!IsSHIFTpressed(nFlag))
        Clear();
      else
        CutText();
      return TRUE;
    default:
      break;
  }

  return bRet;
}

FX_BOOL CPWL_EditCtrl::OnChar(FX_WORD nChar, FX_DWORD nFlag) {
  if (m_bMouseDown)
    return TRUE;

  CPWL_Wnd::OnChar(nChar, nFlag);

  // FILTER
  switch (nChar) {
    case 0x0A:
    case 0x1B:
      return FALSE;
    default:
      break;
  }

  FX_BOOL bCtrl = IsCTRLpressed(nFlag);
  FX_BOOL bAlt = IsALTpressed(nFlag);
  FX_BOOL bShift = IsSHIFTpressed(nFlag);

  FX_WORD word = nChar;

  if (bCtrl && !bAlt) {
    switch (nChar) {
      case 'C' - 'A' + 1:
        CopyText();
        return TRUE;
      case 'V' - 'A' + 1:
        PasteText();
        return TRUE;
      case 'X' - 'A' + 1:
        CutText();
        return TRUE;
      case 'A' - 'A' + 1:
        SelectAll();
        return TRUE;
      case 'Z' - 'A' + 1:
        if (bShift)
          Redo();
        else
          Undo();
        return TRUE;
      default:
        if (nChar < 32)
          return FALSE;
    }
  }

  if (IsReadOnly())
    return TRUE;

  if (m_pEdit->IsSelected() && word == FWL_VKEY_Back)
    word = FWL_VKEY_Unknown;

  Clear();

  switch (word) {
    case FWL_VKEY_Back:
      Backspace();
      break;
    case FWL_VKEY_Return:
      InsertReturn();
      break;
    case FWL_VKEY_Unknown:
      break;
    default:
      if (IsINSERTpressed(nFlag))
        Delete();
      InsertWord(word, GetCharSet());
      break;
  }

  return TRUE;
}

FX_BOOL CPWL_EditCtrl::OnLButtonDown(const CPDF_Point& point, FX_DWORD nFlag) {
  CPWL_Wnd::OnLButtonDown(point, nFlag);

  if (ClientHitTest(point)) {
    if (m_bMouseDown)
      InvalidateRect();

    m_bMouseDown = TRUE;
    SetCapture();

    m_pEdit->OnMouseDown(point, IsSHIFTpressed(nFlag), IsCTRLpressed(nFlag));
  }

  return TRUE;
}

FX_BOOL CPWL_EditCtrl::OnLButtonUp(const CPDF_Point& point, FX_DWORD nFlag) {
  CPWL_Wnd::OnLButtonUp(point, nFlag);

  if (m_bMouseDown) {
    // can receive keybord message
    if (ClientHitTest(point) && !IsFocused())
      SetFocus();

    ReleaseCapture();
    m_bMouseDown = FALSE;
  }

  return TRUE;
}

FX_BOOL CPWL_EditCtrl::OnMouseMove(const CPDF_Point& point, FX_DWORD nFlag) {
  CPWL_Wnd::OnMouseMove(point, nFlag);

  if (m_bMouseDown)
    m_pEdit->OnMouseMove(point, FALSE, FALSE);

  return TRUE;
}

CPDF_Rect CPWL_EditCtrl::GetContentRect() const {
  return m_pEdit->GetContentRect();
}

void CPWL_EditCtrl::SetEditCaret(FX_BOOL bVisible) {
  CPDF_Point ptHead(0, 0), ptFoot(0, 0);

  if (bVisible) {
    GetCaretInfo(ptHead, ptFoot);
  }

  CPVT_WordPlace wpTemp = m_pEdit->GetCaretWordPlace();
  IOnSetCaret(bVisible, ptHead, ptFoot, wpTemp);
}

void CPWL_EditCtrl::GetCaretInfo(CPDF_Point& ptHead, CPDF_Point& ptFoot) const {
  if (IFX_Edit_Iterator* pIterator = m_pEdit->GetIterator()) {
    pIterator->SetAt(m_pEdit->GetCaret());
    CPVT_Word word;
    CPVT_Line line;
    if (pIterator->GetWord(word)) {
      ptHead.x = word.ptWord.x + word.fWidth;
      ptHead.y = word.ptWord.y + word.fAscent;
      ptFoot.x = word.ptWord.x + word.fWidth;
      ptFoot.y = word.ptWord.y + word.fDescent;
    } else if (pIterator->GetLine(line)) {
      ptHead.x = line.ptLine.x;
      ptHead.y = line.ptLine.y + line.fLineAscent;
      ptFoot.x = line.ptLine.x;
      ptFoot.y = line.ptLine.y + line.fLineDescent;
    }
  }
}

void CPWL_EditCtrl::GetCaretPos(int32_t& x, int32_t& y) const {
  CPDF_Point ptHead(0, 0), ptFoot(0, 0);

  GetCaretInfo(ptHead, ptFoot);

  PWLtoWnd(ptHead, x, y);
}

void CPWL_EditCtrl::SetCaret(FX_BOOL bVisible,
                             const CPDF_Point& ptHead,
                             const CPDF_Point& ptFoot) {
  if (m_pEditCaret) {
    if (!IsFocused() || m_pEdit->IsSelected())
      bVisible = FALSE;

    m_pEditCaret->SetCaret(bVisible, ptHead, ptFoot);
  }
}

FX_BOOL CPWL_EditCtrl::IsModified() const {
  return m_pEdit->IsModified();
}

CFX_WideString CPWL_EditCtrl::GetText() const {
  return m_pEdit->GetText();
}

void CPWL_EditCtrl::SetSel(int32_t nStartChar, int32_t nEndChar) {
  m_pEdit->SetSel(nStartChar, nEndChar);
}

void CPWL_EditCtrl::GetSel(int32_t& nStartChar, int32_t& nEndChar) const {
  m_pEdit->GetSel(nStartChar, nEndChar);
}

void CPWL_EditCtrl::Clear() {
  if (!IsReadOnly())
    m_pEdit->Clear();
}

void CPWL_EditCtrl::SelectAll() {
  m_pEdit->SelectAll();
}

void CPWL_EditCtrl::Paint() {
  if (m_pEdit)
    m_pEdit->Paint();
}

void CPWL_EditCtrl::EnableRefresh(FX_BOOL bRefresh) {
  if (m_pEdit)
    m_pEdit->EnableRefresh(bRefresh);
}

int32_t CPWL_EditCtrl::GetCaret() const {
  if (m_pEdit)
    return m_pEdit->GetCaret();

  return -1;
}

void CPWL_EditCtrl::SetCaret(int32_t nPos) {
  if (m_pEdit)
    m_pEdit->SetCaret(nPos);
}

int32_t CPWL_EditCtrl::GetTotalWords() const {
  if (m_pEdit)
    return m_pEdit->GetTotalWords();

  return 0;
}

void CPWL_EditCtrl::SetScrollPos(const CPDF_Point& point) {
  if (m_pEdit)
    m_pEdit->SetScrollPos(point);
}

CPDF_Point CPWL_EditCtrl::GetScrollPos() const {
  if (m_pEdit)
    return m_pEdit->GetScrollPos();

  return CPDF_Point(0.0f, 0.0f);
}

CPDF_Font* CPWL_EditCtrl::GetCaretFont() const {
  int32_t nFontIndex = 0;

  if (IFX_Edit_Iterator* pIterator = m_pEdit->GetIterator()) {
    pIterator->SetAt(m_pEdit->GetCaret());
    CPVT_Word word;
    CPVT_Section section;
    if (pIterator->GetWord(word)) {
      nFontIndex = word.nFontIndex;
    } else if (HasFlag(PES_RICH)) {
      if (pIterator->GetSection(section)) {
        nFontIndex = section.WordProps.nFontIndex;
      }
    }
  }

  if (IFX_Edit_FontMap* pFontMap = GetFontMap())
    return pFontMap->GetPDFFont(nFontIndex);

  return NULL;
}

FX_FLOAT CPWL_EditCtrl::GetCaretFontSize() const {
  FX_FLOAT fFontSize = GetFontSize();

  if (IFX_Edit_Iterator* pIterator = m_pEdit->GetIterator()) {
    pIterator->SetAt(m_pEdit->GetCaret());
    CPVT_Word word;
    CPVT_Section section;
    if (pIterator->GetWord(word)) {
      fFontSize = word.fFontSize;
    } else if (HasFlag(PES_RICH)) {
      if (pIterator->GetSection(section)) {
        fFontSize = section.WordProps.fFontSize;
      }
    }
  }

  return fFontSize;
}

void CPWL_EditCtrl::SetText(const FX_WCHAR* csText) {
  m_pEdit->SetText(csText);
}

void CPWL_EditCtrl::CopyText() {}

void CPWL_EditCtrl::PasteText() {}

void CPWL_EditCtrl::CutText() {}

void CPWL_EditCtrl::ShowVScrollBar(FX_BOOL bShow) {}

void CPWL_EditCtrl::InsertText(const FX_WCHAR* csText) {
  if (!IsReadOnly())
    m_pEdit->InsertText(csText);
}

void CPWL_EditCtrl::InsertWord(FX_WORD word, int32_t nCharset) {
  if (!IsReadOnly())
    m_pEdit->InsertWord(word, nCharset);
}

void CPWL_EditCtrl::InsertReturn() {
  if (!IsReadOnly())
    m_pEdit->InsertReturn();
}

void CPWL_EditCtrl::Delete() {
  if (!IsReadOnly())
    m_pEdit->Delete();
}

void CPWL_EditCtrl::Backspace() {
  if (!IsReadOnly())
    m_pEdit->Backspace();
}

FX_BOOL CPWL_EditCtrl::CanUndo() const {
  return !IsReadOnly() && m_pEdit->CanUndo();
}

FX_BOOL CPWL_EditCtrl::CanRedo() const {
  return !IsReadOnly() && m_pEdit->CanRedo();
}

void CPWL_EditCtrl::Redo() {
  if (CanRedo())
    m_pEdit->Redo();
}

void CPWL_EditCtrl::Undo() {
  if (CanUndo())
    m_pEdit->Undo();
}

void CPWL_EditCtrl::IOnSetScrollInfoY(FX_FLOAT fPlateMin,
                                      FX_FLOAT fPlateMax,
                                      FX_FLOAT fContentMin,
                                      FX_FLOAT fContentMax,
                                      FX_FLOAT fSmallStep,
                                      FX_FLOAT fBigStep) {
  PWL_SCROLL_INFO Info;

  Info.fPlateWidth = fPlateMax - fPlateMin;
  Info.fContentMin = fContentMin;
  Info.fContentMax = fContentMax;
  Info.fSmallStep = fSmallStep;
  Info.fBigStep = fBigStep;

  OnNotify(this, PNM_SETSCROLLINFO, SBT_VSCROLL, (intptr_t)&Info);

  if (IsFloatBigger(Info.fPlateWidth, Info.fContentMax - Info.fContentMin) ||
      IsFloatEqual(Info.fPlateWidth, Info.fContentMax - Info.fContentMin)) {
    ShowVScrollBar(FALSE);
  } else {
    ShowVScrollBar(TRUE);
  }
}

void CPWL_EditCtrl::IOnSetScrollPosY(FX_FLOAT fy) {
  OnNotify(this, PNM_SETSCROLLPOS, SBT_VSCROLL, (intptr_t)&fy);
}

void CPWL_EditCtrl::IOnSetCaret(FX_BOOL bVisible,
                                const CPDF_Point& ptHead,
                                const CPDF_Point& ptFoot,
                                const CPVT_WordPlace& place) {
  PWL_CARET_INFO cInfo;
  cInfo.bVisible = bVisible;
  cInfo.ptHead = ptHead;
  cInfo.ptFoot = ptFoot;

  OnNotify(this, PNM_SETCARETINFO, (intptr_t)&cInfo, (intptr_t)NULL);
}

void CPWL_EditCtrl::IOnCaretChange(const CPVT_SecProps& secProps,
                                   const CPVT_WordProps& wordProps) {}

void CPWL_EditCtrl::IOnContentChange(const CPDF_Rect& rcContent) {
  if (IsValid()) {
    if (m_pEditNotify) {
      m_pEditNotify->OnContentChange(rcContent);
    }
  }
}

void CPWL_EditCtrl::IOnInvalidateRect(CPDF_Rect* pRect) {
  InvalidateRect(pRect);
}

int32_t CPWL_EditCtrl::GetCharSet() const {
  return m_nCharSet < 0 ? DEFAULT_CHARSET : m_nCharSet;
}

void CPWL_EditCtrl::GetTextRange(const CPDF_Rect& rect,
                                 int32_t& nStartChar,
                                 int32_t& nEndChar) const {
  nStartChar = m_pEdit->WordPlaceToWordIndex(
      m_pEdit->SearchWordPlace(CPDF_Point(rect.left, rect.top)));
  nEndChar = m_pEdit->WordPlaceToWordIndex(
      m_pEdit->SearchWordPlace(CPDF_Point(rect.right, rect.bottom)));
}

CFX_WideString CPWL_EditCtrl::GetText(int32_t& nStartChar,
                                      int32_t& nEndChar) const {
  CPVT_WordPlace wpStart = m_pEdit->WordIndexToWordPlace(nStartChar);
  CPVT_WordPlace wpEnd = m_pEdit->WordIndexToWordPlace(nEndChar);
  return m_pEdit->GetRangeText(CPVT_WordRange(wpStart, wpEnd));
}

void CPWL_EditCtrl::SetReadyToInput() {
  if (m_bMouseDown) {
    ReleaseCapture();
    m_bMouseDown = FALSE;
  }
}
