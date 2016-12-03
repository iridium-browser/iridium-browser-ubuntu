// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef CORE_FPDFDOC_INCLUDE_CPDF_FORMFIELD_H_
#define CORE_FPDFDOC_INCLUDE_CPDF_FORMFIELD_H_

#include "core/fpdfdoc/include/cpdf_aaction.h"
#include "core/fpdfdoc/include/cpdf_formfield.h"
#include "core/fxcrt/include/fx_basic.h"
#include "core/fxcrt/include/fx_string.h"
#include "core/fxcrt/include/fx_system.h"

#define FIELDTYPE_UNKNOWN 0
#define FIELDTYPE_PUSHBUTTON 1
#define FIELDTYPE_CHECKBOX 2
#define FIELDTYPE_RADIOBUTTON 3
#define FIELDTYPE_COMBOBOX 4
#define FIELDTYPE_LISTBOX 5
#define FIELDTYPE_TEXTFIELD 6
#define FIELDTYPE_SIGNATURE 7

class CPDF_Dictionary;
class CPDF_Font;
class CPDF_FormControl;
class CPDF_InterForm;
class CPDF_String;

CPDF_Object* FPDF_GetFieldAttr(CPDF_Dictionary* pFieldDict,
                               const FX_CHAR* name,
                               int nLevel = 0);
CFX_WideString FPDF_GetFullName(CPDF_Dictionary* pFieldDict);

class CPDF_FormField {
 public:
  enum Type {
    Unknown,
    PushButton,
    RadioButton,
    CheckBox,
    Text,
    RichText,
    File,
    ListBox,
    ComboBox,
    Sign
  };

  CFX_WideString GetFullName() const;

  Type GetType() const { return m_Type; }
  uint32_t GetFlags() const { return m_Flags; }

  CPDF_Dictionary* GetFieldDict() const { return m_pDict; }
  void SetFieldDict(CPDF_Dictionary* pDict) { m_pDict = pDict; }

  FX_BOOL ResetField(FX_BOOL bNotify = FALSE);

  int CountControls() const { return m_ControlList.GetSize(); }

  CPDF_FormControl* GetControl(int index) const {
    return m_ControlList.GetAt(index);
  }

  int GetControlIndex(const CPDF_FormControl* pControl) const;
  int GetFieldType() const;

  CPDF_AAction GetAdditionalAction() const;
  CFX_WideString GetAlternateName() const;
  CFX_WideString GetMappingName() const;

  uint32_t GetFieldFlags() const;
  CFX_ByteString GetDefaultStyle() const;
  CFX_WideString GetRichTextString() const;

  CFX_WideString GetValue() const;
  CFX_WideString GetDefaultValue() const;
  FX_BOOL SetValue(const CFX_WideString& value, FX_BOOL bNotify = FALSE);

  int GetMaxLen() const;
  int CountSelectedItems() const;
  int GetSelectedIndex(int index) const;

  FX_BOOL ClearSelection(FX_BOOL bNotify = FALSE);
  FX_BOOL IsItemSelected(int index) const;
  FX_BOOL SetItemSelection(int index,
                           FX_BOOL bSelected,
                           FX_BOOL bNotify = FALSE);

  FX_BOOL IsItemDefaultSelected(int index) const;

  int GetDefaultSelectedItem() const;
  int CountOptions() const;

  CFX_WideString GetOptionLabel(int index) const;
  CFX_WideString GetOptionValue(int index) const;

  int FindOption(CFX_WideString csOptLabel) const;
  int FindOptionValue(const CFX_WideString& csOptValue) const;

  FX_BOOL CheckControl(int iControlIndex, bool bChecked, bool bNotify = false);

  int GetTopVisibleIndex() const;
  int CountSelectedOptions() const;

  int GetSelectedOptionIndex(int index) const;
  FX_BOOL IsOptionSelected(int iOptIndex) const;

  FX_BOOL SelectOption(int iOptIndex,
                       FX_BOOL bSelected,
                       FX_BOOL bNotify = FALSE);

  FX_BOOL ClearSelectedOptions(FX_BOOL bNotify = FALSE);

#ifdef PDF_ENABLE_XFA
  FX_BOOL ClearOptions(FX_BOOL bNotify = FALSE);

  int InsertOption(CFX_WideString csOptLabel,
                   int index = -1,
                   FX_BOOL bNotify = FALSE);
#endif  // PDF_ENABLE_XFA

  FX_FLOAT GetFontSize() const { return m_FontSize; }
  CPDF_Font* GetFont() const { return m_pFont; }

 private:
  friend class CPDF_InterForm;
  friend class CPDF_FormControl;

  CPDF_FormField(CPDF_InterForm* pForm, CPDF_Dictionary* pDict);
  ~CPDF_FormField();

  CFX_WideString GetValue(FX_BOOL bDefault) const;
  FX_BOOL SetValue(const CFX_WideString& value,
                   FX_BOOL bDefault,
                   FX_BOOL bNotify);

  void SyncFieldFlags();
  int FindListSel(CPDF_String* str);
  CFX_WideString GetOptionText(int index, int sub_index) const;

  void LoadDA();
  CFX_WideString GetCheckValue(FX_BOOL bDefault) const;
  FX_BOOL SetCheckValue(const CFX_WideString& value,
                        FX_BOOL bDefault,
                        FX_BOOL bNotify);

  bool NotifyBeforeSelectionChange(const CFX_WideString& value);
  void NotifyAfterSelectionChange();

  bool NotifyBeforeValueChange(const CFX_WideString& value);
  void NotifyAfterValueChange();

  bool NotifyListOrComboBoxBeforeChange(const CFX_WideString& value);
  void NotifyListOrComboBoxAfterChange();

  CPDF_FormField::Type m_Type;
  uint32_t m_Flags;
  CPDF_InterForm* m_pForm;
  CPDF_Dictionary* m_pDict;
  CFX_ArrayTemplate<CPDF_FormControl*> m_ControlList;
  FX_FLOAT m_FontSize;
  CPDF_Font* m_pFont;
};

#endif  // CORE_FPDFDOC_INCLUDE_CPDF_FORMFIELD_H_
