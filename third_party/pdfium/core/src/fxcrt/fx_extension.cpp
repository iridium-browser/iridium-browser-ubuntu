// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/fxcrt/fx_basic.h"
#include "../../include/fxcrt/fx_ext.h"
#include "extension.h"
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
#include <wincrypt.h>
#else
#include <ctime>
#endif

IFX_FileStream* FX_CreateFileStream(const FX_CHAR* filename, FX_DWORD dwModes) {
  IFXCRT_FileAccess* pFA = FXCRT_FileAccess_Create();
  if (!pFA) {
    return NULL;
  }
  if (!pFA->Open(filename, dwModes)) {
    pFA->Release();
    return NULL;
  }
  return new CFX_CRTFileStream(pFA);
}
IFX_FileStream* FX_CreateFileStream(const FX_WCHAR* filename,
                                    FX_DWORD dwModes) {
  IFXCRT_FileAccess* pFA = FXCRT_FileAccess_Create();
  if (!pFA) {
    return NULL;
  }
  if (!pFA->Open(filename, dwModes)) {
    pFA->Release();
    return NULL;
  }
  return new CFX_CRTFileStream(pFA);
}
IFX_FileRead* FX_CreateFileRead(const FX_CHAR* filename) {
  return FX_CreateFileStream(filename, FX_FILEMODE_ReadOnly);
}
IFX_FileRead* FX_CreateFileRead(const FX_WCHAR* filename) {
  return FX_CreateFileStream(filename, FX_FILEMODE_ReadOnly);
}
IFX_MemoryStream* FX_CreateMemoryStream(uint8_t* pBuffer,
                                        size_t dwSize,
                                        FX_BOOL bTakeOver) {
  return new CFX_MemoryStream(pBuffer, dwSize, bTakeOver);
}
IFX_MemoryStream* FX_CreateMemoryStream(FX_BOOL bConsecutive) {
  return new CFX_MemoryStream(bConsecutive);
}
#ifdef __cplusplus
extern "C" {
#endif
FX_FLOAT FXSYS_tan(FX_FLOAT a) {
  return (FX_FLOAT)tan(a);
}
FX_FLOAT FXSYS_logb(FX_FLOAT b, FX_FLOAT x) {
  return FXSYS_log(x) / FXSYS_log(b);
}
FX_FLOAT FXSYS_strtof(const FX_CHAR* pcsStr,
                      int32_t iLength,
                      int32_t* pUsedLen) {
  FXSYS_assert(pcsStr != NULL);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_strlen(pcsStr);
  }
  CFX_WideString ws = CFX_WideString::FromLocal(pcsStr, iLength);
  return FXSYS_wcstof(ws.c_str(), iLength, pUsedLen);
}
FX_FLOAT FXSYS_wcstof(const FX_WCHAR* pwsStr,
                      int32_t iLength,
                      int32_t* pUsedLen) {
  FXSYS_assert(pwsStr != NULL);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_wcslen(pwsStr);
  }
  if (iLength == 0) {
    return 0.0f;
  }
  int32_t iUsedLen = 0;
  FX_BOOL bNegtive = FALSE;
  switch (pwsStr[iUsedLen]) {
    case '-':
      bNegtive = TRUE;
    case '+':
      iUsedLen++;
      break;
  }
  FX_FLOAT fValue = 0.0f;
  while (iUsedLen < iLength) {
    FX_WCHAR wch = pwsStr[iUsedLen];
    if (wch >= L'0' && wch <= L'9') {
      fValue = fValue * 10.0f + (wch - L'0');
    } else {
      break;
    }
    iUsedLen++;
  }
  if (iUsedLen < iLength && pwsStr[iUsedLen] == L'.') {
    FX_FLOAT fPrecise = 0.1f;
    while (++iUsedLen < iLength) {
      FX_WCHAR wch = pwsStr[iUsedLen];
      if (wch >= L'0' && wch <= L'9') {
        fValue += (wch - L'0') * fPrecise;
        fPrecise *= 0.1f;
      } else {
        break;
      }
    }
  }
  if (pUsedLen) {
    *pUsedLen = iUsedLen;
  }
  return bNegtive ? -fValue : fValue;
}
FX_WCHAR* FXSYS_wcsncpy(FX_WCHAR* dstStr,
                        const FX_WCHAR* srcStr,
                        size_t count) {
  FXSYS_assert(dstStr != NULL && srcStr != NULL && count > 0);
  for (size_t i = 0; i < count; ++i)
    if ((dstStr[i] = srcStr[i]) == L'\0') {
      break;
    }
  return dstStr;
}
int32_t FXSYS_wcsnicmp(const FX_WCHAR* s1, const FX_WCHAR* s2, size_t count) {
  FXSYS_assert(s1 != NULL && s2 != NULL && count > 0);
  FX_WCHAR wch1 = 0, wch2 = 0;
  while (count-- > 0) {
    wch1 = (FX_WCHAR)FXSYS_tolower(*s1++);
    wch2 = (FX_WCHAR)FXSYS_tolower(*s2++);
    if (wch1 != wch2) {
      break;
    }
  }
  return wch1 - wch2;
}
int32_t FXSYS_strnicmp(const FX_CHAR* s1, const FX_CHAR* s2, size_t count) {
  FXSYS_assert(s1 != NULL && s2 != NULL && count > 0);
  FX_CHAR ch1 = 0, ch2 = 0;
  while (count-- > 0) {
    ch1 = (FX_CHAR)FXSYS_tolower(*s1++);
    ch2 = (FX_CHAR)FXSYS_tolower(*s2++);
    if (ch1 != ch2) {
      break;
    }
  }
  return ch1 - ch2;
}
FX_DWORD FX_HashCode_String_GetA(const FX_CHAR* pStr,
                                 int32_t iLength,
                                 FX_BOOL bIgnoreCase) {
  FXSYS_assert(pStr != NULL);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_strlen(pStr);
  }
  const FX_CHAR* pStrEnd = pStr + iLength;
  FX_DWORD dwHashCode = 0;
  if (bIgnoreCase) {
    while (pStr < pStrEnd) {
      dwHashCode = 31 * dwHashCode + FXSYS_tolower(*pStr++);
    }
  } else {
    while (pStr < pStrEnd) {
      dwHashCode = 31 * dwHashCode + *pStr++;
    }
  }
  return dwHashCode;
}
FX_DWORD FX_HashCode_String_GetW(const FX_WCHAR* pStr,
                                 int32_t iLength,
                                 FX_BOOL bIgnoreCase) {
  FXSYS_assert(pStr != NULL);
  if (iLength < 0) {
    iLength = (int32_t)FXSYS_wcslen(pStr);
  }
  const FX_WCHAR* pStrEnd = pStr + iLength;
  FX_DWORD dwHashCode = 0;
  if (bIgnoreCase) {
    while (pStr < pStrEnd) {
      dwHashCode = 1313 * dwHashCode + FXSYS_tolower(*pStr++);
    }
  } else {
    while (pStr < pStrEnd) {
      dwHashCode = 1313 * dwHashCode + *pStr++;
    }
  }
  return dwHashCode;
}
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void* FX_Random_MT_Start(FX_DWORD dwSeed) {
  FX_LPMTRANDOMCONTEXT pContext = FX_Alloc(FX_MTRANDOMCONTEXT, 1);
  pContext->mt[0] = dwSeed;
  FX_DWORD& i = pContext->mti;
  FX_DWORD* pBuf = pContext->mt;
  for (i = 1; i < MT_N; i++) {
    pBuf[i] = (1812433253UL * (pBuf[i - 1] ^ (pBuf[i - 1] >> 30)) + i);
  }
  pContext->bHaveSeed = TRUE;
  return pContext;
}
FX_DWORD FX_Random_MT_Generate(void* pContext) {
  FXSYS_assert(pContext != NULL);
  FX_LPMTRANDOMCONTEXT pMTC = (FX_LPMTRANDOMCONTEXT)pContext;
  FX_DWORD v;
  static FX_DWORD mag[2] = {0, MT_Matrix_A};
  FX_DWORD& mti = pMTC->mti;
  FX_DWORD* pBuf = pMTC->mt;
  if ((int)mti < 0 || mti >= MT_N) {
    if (mti > MT_N && !pMTC->bHaveSeed) {
      return 0;
    }
    FX_DWORD kk;
    for (kk = 0; kk < MT_N - MT_M; kk++) {
      v = (pBuf[kk] & MT_Upper_Mask) | (pBuf[kk + 1] & MT_Lower_Mask);
      pBuf[kk] = pBuf[kk + MT_M] ^ (v >> 1) ^ mag[v & 1];
    }
    for (; kk < MT_N - 1; kk++) {
      v = (pBuf[kk] & MT_Upper_Mask) | (pBuf[kk + 1] & MT_Lower_Mask);
      pBuf[kk] = pBuf[kk + (MT_M - MT_N)] ^ (v >> 1) ^ mag[v & 1];
    }
    v = (pBuf[MT_N - 1] & MT_Upper_Mask) | (pBuf[0] & MT_Lower_Mask);
    pBuf[MT_N - 1] = pBuf[MT_M - 1] ^ (v >> 1) ^ mag[v & 1];
    mti = 0;
  }
  v = pBuf[mti++];
  v ^= (v >> 11);
  v ^= (v << 7) & 0x9d2c5680UL;
  v ^= (v << 15) & 0xefc60000UL;
  v ^= (v >> 18);
  return v;
}
void FX_Random_MT_Close(void* pContext) {
  FXSYS_assert(pContext != NULL);
  FX_Free(pContext);
}
void FX_Random_GenerateMT(FX_DWORD* pBuffer, int32_t iCount) {
  FX_DWORD dwSeed;
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  if (!FX_GenerateCryptoRandom(&dwSeed, 1)) {
    FX_Random_GenerateBase(&dwSeed, 1);
  }
#else
  FX_Random_GenerateBase(&dwSeed, 1);
#endif
  void* pContext = FX_Random_MT_Start(dwSeed);
  while (iCount-- > 0) {
    *pBuffer++ = FX_Random_MT_Generate(pContext);
  }
  FX_Random_MT_Close(pContext);
}
void FX_Random_GenerateBase(FX_DWORD* pBuffer, int32_t iCount) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  SYSTEMTIME st1, st2;
  ::GetSystemTime(&st1);
  do {
    ::GetSystemTime(&st2);
  } while (FXSYS_memcmp(&st1, &st2, sizeof(SYSTEMTIME)) == 0);
  FX_DWORD dwHash1 =
      FX_HashCode_String_GetA((const FX_CHAR*)&st1, sizeof(st1), TRUE);
  FX_DWORD dwHash2 =
      FX_HashCode_String_GetA((const FX_CHAR*)&st2, sizeof(st2), TRUE);
  ::srand((dwHash1 << 16) | (FX_DWORD)dwHash2);
#else
  time_t tmLast = time(NULL), tmCur;
  while ((tmCur = time(NULL)) == tmLast)
    ;
  ::srand((tmCur << 16) | (tmLast & 0xFFFF));
#endif
  while (iCount-- > 0) {
    *pBuffer++ = (FX_DWORD)((::rand() << 16) | (::rand() & 0xFFFF));
  }
}
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
FX_BOOL FX_GenerateCryptoRandom(FX_DWORD* pBuffer, int32_t iCount) {
  HCRYPTPROV hCP = NULL;
  if (!::CryptAcquireContext(&hCP, NULL, NULL, PROV_RSA_FULL, 0) ||
      hCP == NULL) {
    return FALSE;
  }
  ::CryptGenRandom(hCP, iCount * sizeof(FX_DWORD), (uint8_t*)pBuffer);
  ::CryptReleaseContext(hCP, 0);
  return TRUE;
}
#endif
void FX_Random_GenerateCrypto(FX_DWORD* pBuffer, int32_t iCount) {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
  FX_GenerateCryptoRandom(pBuffer, iCount);
#else
  FX_Random_GenerateBase(pBuffer, iCount);
#endif
}
#ifdef __cplusplus
}
#endif
