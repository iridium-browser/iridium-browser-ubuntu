// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXCRT_CFX_UNOWNED_PTR_H_
#define CORE_FXCRT_CFX_UNOWNED_PTR_H_

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

// CFX_UnownedPtr is a smart pointer class that behaves very much like a
// standard C-style pointer. The advantages of using it over raw
// pointers are:
//
// 1. It documents the nature of the pointer with no need to add a comment
//    explaining that is it // Not owned. Additionally, an attempt to delete
//    an unowned ptr will fail to compile rather than silently succeeding,
//    since it is a class and not a raw pointer.
//
// 2. When built for a memory tool like ASAN, the class provides a destructor
//    which checks that the object being pointed to is still alive.
//
// Hence, when using UnownedPtr, no dangling pointers are ever permitted,
// even if they are not de-referenced after becoming dangling. The style of
// programming required is that the lifetime an object containing an
// UnownedPtr must be strictly less than the object to which it points.
//
// The same checks are also performed at assignment time to prove that the
// old value was not a dangling pointer, either.
//
// The array indexing operation [] is not supported on an unowned ptr,
// because an unowned ptr expresses a one to one relationship with some
// other heap object.

template <class T>
class CFX_UnownedPtr {
 public:
  CFX_UnownedPtr() {}
  CFX_UnownedPtr(const CFX_UnownedPtr& that) : CFX_UnownedPtr(that.Get()) {}

  template <typename U>
  explicit CFX_UnownedPtr(U* pObj) : m_pObj(pObj) {}

  // Deliberately implicit to allow returning nullptrs.
  // NOLINTNEXTLINE(runtime/explicit)
  CFX_UnownedPtr(std::nullptr_t ptr) {}

  ~CFX_UnownedPtr() { ProbeForLowSeverityLifetimeIssue(); }

  CFX_UnownedPtr& operator=(T* that) {
    ProbeForLowSeverityLifetimeIssue();
    m_pObj = that;
    return *this;
  }

  CFX_UnownedPtr& operator=(const CFX_UnownedPtr& that) {
    ProbeForLowSeverityLifetimeIssue();
    if (*this != that)
      m_pObj = that.Get();
    return *this;
  }

  bool operator==(const CFX_UnownedPtr& that) const {
    return Get() == that.Get();
  }
  bool operator!=(const CFX_UnownedPtr& that) const { return !(*this == that); }
  bool operator<(const CFX_UnownedPtr& that) const {
    return std::less<T*>()(Get(), that.Get());
  }

  template <typename U>
  bool operator==(const U* that) const {
    return Get() == that;
  }

  template <typename U>
  bool operator!=(const U* that) const {
    return !(*this == that);
  }

  T* Get() const { return m_pObj; }

  T* Release() {
    ProbeForLowSeverityLifetimeIssue();
    T* pTemp = nullptr;
    std::swap(pTemp, m_pObj);
    return pTemp;
  }

  explicit operator bool() const { return !!m_pObj; }
  T& operator*() const { return *m_pObj; }
  T* operator->() const { return m_pObj; }

 private:
  inline void ProbeForLowSeverityLifetimeIssue() {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
    if (m_pObj)
      reinterpret_cast<const volatile uint8_t*>(m_pObj)[0];
#endif
  }

  T* m_pObj = nullptr;
};

template <typename T, typename U>
inline bool operator==(const U* lhs, const CFX_UnownedPtr<T>& rhs) {
  return rhs == lhs;
}

template <typename T, typename U>
inline bool operator!=(const U* lhs, const CFX_UnownedPtr<T>& rhs) {
  return rhs != lhs;
}

#endif  // CORE_FXCRT_CFX_UNOWNED_PTR_H_
