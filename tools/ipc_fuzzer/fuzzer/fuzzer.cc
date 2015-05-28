// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_switches.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "tools/ipc_fuzzer/fuzzer/fuzzer.h"
#include "tools/ipc_fuzzer/fuzzer/rand_util.h"
#include "tools/ipc_fuzzer/message_lib/message_cracker.h"
#include "tools/ipc_fuzzer/message_lib/message_file.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

// First include of all message files to provide basic types.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

#if defined(COMPILER_GCC)
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(COMPILER_MSVC)
#define PRETTY_FUNCTION __FUNCSIG__
#else
#define PRETTY_FUNCTION __FUNCTION__
#endif

namespace IPC {
class Message;
}  // namespace IPC

namespace {
// For breaking deep recursion.
int g_depth = 0;
}  // namespace

namespace ipc_fuzzer {

FuzzerFunctionVector g_function_vector;

bool Fuzzer::ShouldGenerate() {
  return false;
}

// Partially-specialized class that knows how to handle a given type.
template <class P>
struct FuzzTraits {
  static bool Fuzz(P* p, Fuzzer *fuzzer) {
    // This is the catch-all for types we don't have enough information
    // to generate.
    std::cerr << "Can't handle " << PRETTY_FUNCTION << "\n";
    return false;
  }
};

// Template function to invoke partially-specialized class method.
template <class P>
static bool FuzzParam(P* p, Fuzzer* fuzzer) {
  return FuzzTraits<P>::Fuzz(p, fuzzer);
}

template <class P>
static bool FuzzParamArray(P* p, size_t length, Fuzzer* fuzzer) {
  for (size_t i = 0; i < length; i++, p++) {
    if (!FuzzTraits<P>::Fuzz(p, fuzzer))
      return false;
  }
  return true;
}

// Specializations to generate primitive types.
template <>
struct FuzzTraits<bool> {
  static bool Fuzz(bool* p, Fuzzer* fuzzer) {
    fuzzer->FuzzBool(p);
    return true;
  }
};

template <>
struct FuzzTraits<int> {
  static bool Fuzz(int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(p);
    return true;
  }
};

template <>
struct FuzzTraits<unsigned int> {
  static bool Fuzz(unsigned int* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt(reinterpret_cast<int*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<long> {
  static bool Fuzz(long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(p);
    return true;
  }
};

template <>
struct FuzzTraits<unsigned long> {
  static bool Fuzz(unsigned long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzLong(reinterpret_cast<long*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<long long> {
  static bool Fuzz(long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned long long> {
  static bool Fuzz(unsigned long long* p, Fuzzer* fuzzer) {
    fuzzer->FuzzInt64(reinterpret_cast<int64*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<short> {
  static bool Fuzz(short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned short> {
  static bool Fuzz(unsigned short* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUInt16(reinterpret_cast<uint16*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<char> {
  static bool Fuzz(char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(reinterpret_cast<unsigned char*>(p));
    return true;
  }
};

template <>
struct FuzzTraits<unsigned char> {
  static bool Fuzz(unsigned char* p, Fuzzer* fuzzer) {
    fuzzer->FuzzUChar(p);
    return true;
  }
};

template <>
struct FuzzTraits<wchar_t> {
  static bool Fuzz(wchar_t* p, Fuzzer* fuzzer) {
    fuzzer->FuzzWChar(p);
    return true;
  }
};

template <>
struct FuzzTraits<float> {
  static bool Fuzz(float* p, Fuzzer* fuzzer) {
    fuzzer->FuzzFloat(p);
    return true;
  }
};

template <>
struct FuzzTraits<double> {
  static bool Fuzz(double* p, Fuzzer* fuzzer) {
    fuzzer->FuzzDouble(p);
    return true;
  }
};

template <>
struct FuzzTraits<std::string> {
  static bool Fuzz(std::string* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString(p);
    return true;
  }
};

template <>
struct FuzzTraits<base::string16> {
  static bool Fuzz(base::string16* p, Fuzzer* fuzzer) {
    fuzzer->FuzzString16(p);
    return true;
  }
};

// Specializations for tuples.
template <>
struct FuzzTraits<Tuple<>> {
  static bool Fuzz(Tuple<>* p, Fuzzer* fuzzer) {
    return true;
  }
};

template <class A>
struct FuzzTraits<Tuple<A>> {
  static bool Fuzz(Tuple<A>* p, Fuzzer* fuzzer) {
    return FuzzParam(&get<0>(*p), fuzzer);
  }
};

template <class A, class B>
struct FuzzTraits<Tuple<A, B>> {
  static bool Fuzz(Tuple<A, B>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&get<0>(*p), fuzzer) &&
        FuzzParam(&get<1>(*p), fuzzer);
  }
};

template <class A, class B, class C>
struct FuzzTraits<Tuple<A, B, C>> {
  static bool Fuzz(Tuple<A, B, C>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&get<0>(*p), fuzzer) &&
        FuzzParam(&get<1>(*p), fuzzer) &&
        FuzzParam(&get<2>(*p), fuzzer);
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<Tuple<A, B, C, D>> {
  static bool Fuzz(Tuple<A, B, C, D>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&get<0>(*p), fuzzer) &&
        FuzzParam(&get<1>(*p), fuzzer) &&
        FuzzParam(&get<2>(*p), fuzzer) &&
        FuzzParam(&get<3>(*p), fuzzer);
  }
};

template <class A, class B, class C, class D, class E>
struct FuzzTraits<Tuple<A, B, C, D, E>> {
  static bool Fuzz(Tuple<A, B, C, D, E>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&get<0>(*p), fuzzer) &&
        FuzzParam(&get<1>(*p), fuzzer) &&
        FuzzParam(&get<2>(*p), fuzzer) &&
        FuzzParam(&get<3>(*p), fuzzer) &&
        FuzzParam(&get<4>(*p), fuzzer);
  }
};

// Specializations for containers.
template <class A>
struct FuzzTraits<std::vector<A> > {
  static bool Fuzz(std::vector<A>* p, Fuzzer* fuzzer) {
    ++g_depth;
    size_t count = p->size();
    if (fuzzer->ShouldGenerate()) {
      count = g_depth > 3 ? 0 : RandElementCount();
      p->resize(count);
    }
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&p->at(i), fuzzer)) {
        --g_depth;
        return false;
      }
    }
    --g_depth;
    return true;
  }
};

template <class A>
struct FuzzTraits<std::set<A> > {
  static bool Fuzz(std::set<A>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      std::set<A> result;
      typename std::set<A>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        A item = *it;
        if (!FuzzParam(&item, fuzzer))
          return false;
        result.insert(item);
      }
      *p = result;
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    A a;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&a, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(a);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B>
struct FuzzTraits<std::map<A, B> > {
  static bool Fuzz(std::map<A, B>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      typename std::map<A, B>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        if (!FuzzParam(&it->second, fuzzer))
          return false;
      }
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&place_holder, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B, class C, class D>
struct FuzzTraits<std::map<A, B, C, D>> {
  static bool Fuzz(std::map<A, B, C, D>* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      typename std::map<A, B, C, D>::iterator it;
      for (it = p->begin(); it != p->end(); ++it) {
        if (!FuzzParam(&it->second, fuzzer))
          return false;
      }
      return true;
    }

    static int g_depth = 0;
    size_t count = ++g_depth > 3 ? 0 : RandElementCount();
    std::pair<A, B> place_holder;
    for (size_t i = 0; i < count; ++i) {
      if (!FuzzParam(&place_holder, fuzzer)) {
        --g_depth;
        return false;
      }
      p->insert(place_holder);
    }
    --g_depth;
    return true;
  }
};

template <class A, class B>
struct FuzzTraits<std::pair<A, B> > {
  static bool Fuzz(std::pair<A, B>* p, Fuzzer* fuzzer) {
    return
        FuzzParam(&p->first, fuzzer) &&
        FuzzParam(&p->second, fuzzer);
  }
};

// Specializations for hand-coded types.

template <>
struct FuzzTraits<base::FilePath> {
  static bool Fuzz(base::FilePath* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      base::FilePath::StringType path = p->value();
      if(!FuzzParam(&path, fuzzer))
        return false;
      *p = base::FilePath(path);
      return true;
    }

    const char path_chars[] = "ACz0/.~:";
    size_t count = RandInRange(60);
    base::FilePath::StringType random_path;
    for (size_t i = 0; i < count; ++i)
      random_path += path_chars[RandInRange(sizeof(path_chars) - 1)];
    *p = base::FilePath(random_path);
    return true;
  }
};

template <>
struct FuzzTraits<base::File::Error> {
  static bool Fuzz(base::File::Error* p, Fuzzer* fuzzer) {
    int value = static_cast<int>(*p);
    if (!FuzzParam(&value, fuzzer))
      return false;
    *p = static_cast<base::File::Error>(value);
    return true;
  }
};

template <>
struct FuzzTraits<base::File::Info> {
  static bool Fuzz(base::File::Info* p, Fuzzer* fuzzer) {
    double last_modified = p->last_modified.ToDoubleT();
    double last_accessed = p->last_accessed.ToDoubleT();
    double creation_time = p->creation_time.ToDoubleT();
    if (!FuzzParam(&p->size, fuzzer))
      return false;
    if (!FuzzParam(&p->is_directory, fuzzer))
      return false;
    if (!FuzzParam(&last_modified, fuzzer))
      return false;
    if (!FuzzParam(&last_accessed, fuzzer))
      return false;
    if (!FuzzParam(&creation_time, fuzzer))
      return false;
    p->last_modified = base::Time::FromDoubleT(last_modified);
    p->last_accessed = base::Time::FromDoubleT(last_accessed);
    p->creation_time = base::Time::FromDoubleT(creation_time);
    return true;
  }
};

template <>
struct FuzzTraits<base::NullableString16> {
  static bool Fuzz(base::NullableString16* p, Fuzzer* fuzzer) {
    base::string16 string = p->string();
    bool is_null = p->is_null();
    if (!FuzzParam(&string, fuzzer))
      return false;
    if (!FuzzParam(&is_null, fuzzer))
      return false;
    *p = base::NullableString16(string, is_null);
    return true;
  }
};

template <>
struct FuzzTraits<base::Time> {
  static bool Fuzz(base::Time* p, Fuzzer* fuzzer) {
    int64 internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::Time::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::TimeDelta> {
  static bool Fuzz(base::TimeDelta* p, Fuzzer* fuzzer) {
    int64 internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::TimeDelta::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::TimeTicks> {
  static bool Fuzz(base::TimeTicks* p, Fuzzer* fuzzer) {
    int64 internal_value = p->ToInternalValue();
    if (!FuzzParam(&internal_value, fuzzer))
      return false;
    *p = base::TimeTicks::FromInternalValue(internal_value);
    return true;
  }
};

template <>
struct FuzzTraits<base::ListValue> {
  static bool Fuzz(base::ListValue* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    ++g_depth;
    size_t list_length = p->GetSize();
    if (fuzzer->ShouldGenerate())
      list_length = g_depth > 3 ? 0 : RandInRange(8);
    for (size_t index = 0; index < list_length; ++index) {
      switch (RandInRange(8)) {
        case base::Value::TYPE_BOOLEAN: {
          bool tmp;
          p->GetBoolean(index, &tmp);
          fuzzer->FuzzBool(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_INTEGER: {
          int tmp;
          p->GetInteger(index, &tmp);
          fuzzer->FuzzInt(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_DOUBLE: {
          double tmp;
          p->GetDouble(index, &tmp);
          fuzzer->FuzzDouble(&tmp);
          p->Set(index, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_STRING: {
          std::string tmp;
          p->GetString(index, &tmp);
          fuzzer->FuzzString(&tmp);
          p->Set(index, new base::StringValue(tmp));
          break;
        }
        case base::Value::TYPE_BINARY: {
          char tmp[200];
          size_t bin_length = RandInRange(sizeof(tmp));
          fuzzer->FuzzData(tmp, bin_length);
          p->Set(index,
                 base::BinaryValue::CreateWithCopiedBuffer(tmp, bin_length));
          break;
        }
        case base::Value::TYPE_DICTIONARY: {
          base::DictionaryValue* tmp = new base::DictionaryValue();
          p->GetDictionary(index, &tmp);
          FuzzParam(tmp, fuzzer);
          p->Set(index, tmp);
          break;
        }
        case base::Value::TYPE_LIST: {
          base::ListValue* tmp = new base::ListValue();
          p->GetList(index, &tmp);
          FuzzParam(tmp, fuzzer);
          p->Set(index, tmp);
          break;
        }
        case base::Value::TYPE_NULL:
        default:
          break;
      }
    }
    --g_depth;
    return true;
  }
};

template <>
struct FuzzTraits<base::DictionaryValue> {
  static bool Fuzz(base::DictionaryValue* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    ++g_depth;
    size_t dict_length = g_depth > 3 ? 0 : RandInRange(8);
    for (size_t index = 0; index < dict_length; ++index) {
      std::string property;
      fuzzer->FuzzString(&property);
      switch (RandInRange(8)) {
        case base::Value::TYPE_BOOLEAN: {
          bool tmp;
          fuzzer->FuzzBool(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_INTEGER: {
          int tmp;
          fuzzer->FuzzInt(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_DOUBLE: {
          double tmp;
          fuzzer->FuzzDouble(&tmp);
          p->SetWithoutPathExpansion(property, new base::FundamentalValue(tmp));
          break;
        }
        case base::Value::TYPE_STRING: {
          std::string tmp;
          fuzzer->FuzzString(&tmp);
          p->SetWithoutPathExpansion(property, new base::StringValue(tmp));
          break;
        }
        case base::Value::TYPE_BINARY: {
          char tmp[200];
          size_t bin_length = RandInRange(sizeof(tmp));
          fuzzer->FuzzData(tmp, bin_length);
          p->SetWithoutPathExpansion(
              property,
              base::BinaryValue::CreateWithCopiedBuffer(tmp, bin_length));
          break;
        }
        case base::Value::TYPE_DICTIONARY: {
          base::DictionaryValue* tmp = new base::DictionaryValue();
          FuzzParam(tmp, fuzzer);
          p->SetWithoutPathExpansion(property, tmp);
          break;
        }
        case base::Value::TYPE_LIST: {
          base::ListValue* tmp = new base::ListValue();
          FuzzParam(tmp, fuzzer);
          p->SetWithoutPathExpansion(property, tmp);
          break;
        }
        case base::Value::TYPE_NULL:
        default:
          break;
      }
    }
    --g_depth;
    return true;
  }
};

template <>
struct FuzzTraits<blink::WebGamepad> {
  static bool Fuzz(blink::WebGamepad* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->connected, fuzzer))
      return false;
    if (!FuzzParam(&p->timestamp, fuzzer))
      return false;
    unsigned idLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::idLengthCap + 1));
    if (!FuzzParamArray(&p->id[0], idLength, fuzzer))
      return false;
    p->axesLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::axesLengthCap + 1));
    if (!FuzzParamArray(&p->axes[0], p->axesLength, fuzzer))
      return false;
    p->buttonsLength = static_cast<unsigned>(
        RandInRange(blink::WebGamepad::buttonsLengthCap + 1));
    if (!FuzzParamArray(&p->buttons[0], p->buttonsLength, fuzzer))
      return false;
    unsigned mappingsLength = static_cast<unsigned>(
      RandInRange(blink::WebGamepad::mappingLengthCap + 1));
    if (!FuzzParamArray(&p->mapping[0], mappingsLength, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<blink::WebGamepadButton> {
  static bool Fuzz(blink::WebGamepadButton* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->pressed, fuzzer))
      return false;
    if (!FuzzParam(&p->value, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<cc::CompositorFrame> {
  static bool Fuzz(cc::CompositorFrame* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    if (!FuzzParam(&p->metadata, fuzzer))
      return false;

    switch (RandInRange(4)) {
      case 0: {
        p->delegated_frame_data.reset(new cc::DelegatedFrameData());
        if (!FuzzParam(p->delegated_frame_data.get(), fuzzer))
          return false;
        return true;
      }
      case 1: {
        p->gl_frame_data.reset(new cc::GLFrameData());
        if (!FuzzParam(p->gl_frame_data.get(), fuzzer))
          return false;
        return true;
      }
      case 2: {
        p->software_frame_data.reset(new cc::SoftwareFrameData());
        if (!FuzzParam(p->software_frame_data.get(), fuzzer))
          return false;
        return true;
      }
      default:
        // Fuzz nothing to handle the no frame case.
        return true;
    }
  }
};

template <>
struct FuzzTraits<cc::CompositorFrameAck> {
  static bool Fuzz(cc::CompositorFrameAck* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->resources, fuzzer))
      return false;
    if (!FuzzParam(&p->last_software_frame_id, fuzzer))
      return false;

    if (!p->gl_frame_data)
      p->gl_frame_data.reset(new cc::GLFrameData);
    if (!FuzzParam(p->gl_frame_data.get(), fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<cc::DelegatedFrameData> {
  static bool Fuzz(cc::DelegatedFrameData* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->device_scale_factor, fuzzer))
      return false;
    if (!FuzzParam(&p->resource_list, fuzzer))
      return false;
    if (!FuzzParam(&p->render_pass_list, fuzzer))
      return false;
    return true;
  }
};

template <class A>
struct FuzzTraits<cc::ListContainer<A>> {
  static bool Fuzz(cc::ListContainer<A>* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<cc::QuadList> {
  static bool Fuzz(cc::QuadList* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<cc::RenderPass> {
  static bool Fuzz(cc::RenderPass* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->id, fuzzer))
      return false;
    if (!FuzzParam(&p->output_rect, fuzzer))
      return false;
    if (!FuzzParam(&p->damage_rect, fuzzer))
      return false;
    if (!FuzzParam(&p->transform_to_root_target, fuzzer))
      return false;
    if (!FuzzParam(&p->has_transparent_background, fuzzer))
      return false;
    if (!FuzzParam(&p->quad_list, fuzzer))
      return false;
    if (!FuzzParam(&p->shared_quad_state_list, fuzzer))
      return false;
    // Omitting |copy_requests| as it is not sent over IPC.
    return true;
  }
};

template <>
struct FuzzTraits<cc::RenderPassList> {
  static bool Fuzz(cc::RenderPassList* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      for (size_t i = 0; i < p->size(); ++i) {
        if (!FuzzParam(p->at(i), fuzzer))
          return false;
      }
      return true;
    }

    size_t count = RandElementCount();
    for (size_t i = 0; i < count; ++i) {
      scoped_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
      if (!FuzzParam(render_pass.get(), fuzzer))
        return false;
      p->push_back(render_pass.Pass());
    }
    return true;
  }
};

template <>
struct FuzzTraits<cc::SoftwareFrameData> {
  static bool Fuzz(cc::SoftwareFrameData* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->id, fuzzer))
      return false;
    if (!FuzzParam(&p->size, fuzzer))
      return false;
    if (!FuzzParam(&p->damage_rect, fuzzer))
      return false;
    if (!FuzzParam(&p->bitmap_id, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<content::IndexedDBKey> {
  static bool Fuzz(content::IndexedDBKey* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    ++g_depth;
    blink::WebIDBKeyType web_type =
        static_cast<blink::WebIDBKeyType>(RandInRange(7));
    switch (web_type) {
      case blink::WebIDBKeyTypeArray: {
        size_t length = g_depth > 3 ? 0 : RandInRange(4);
        std::vector<content::IndexedDBKey> array;
        array.resize(length);
        for (size_t i = 0; i < length; ++i) {
            if (!FuzzParam(&array[i], fuzzer)) {
              --g_depth;
              return false;
            }
        }
        *p = content::IndexedDBKey(array);
        return true;
      }
      case blink::WebIDBKeyTypeBinary: {
        std::string binary;
        if (!FuzzParam(&binary, fuzzer)) {
            --g_depth;
            return false;
        }
        *p = content::IndexedDBKey(binary);
        return true;
      }
      case blink::WebIDBKeyTypeString: {
        base::string16 string;
        if (!FuzzParam(&string, fuzzer))
          return false;
        *p = content::IndexedDBKey(string);
        return true;
      }
      case blink::WebIDBKeyTypeDate:
      case blink::WebIDBKeyTypeNumber: {
        double number;
        if (!FuzzParam(&number, fuzzer)) {
            --g_depth;
            return false;
        }
        *p = content::IndexedDBKey(number, web_type);
        return true;
      }
      case blink::WebIDBKeyTypeInvalid:
      case blink::WebIDBKeyTypeNull: {
        *p = content::IndexedDBKey(web_type);
        return true;
      }
      default: {
          NOTREACHED();
          --g_depth;
          return false;
      }
    }
  }
};

template <>
struct FuzzTraits<content::IndexedDBKeyRange> {
  static bool Fuzz(content::IndexedDBKeyRange* p, Fuzzer* fuzzer) {
    content::IndexedDBKey lower = p->lower();
    content::IndexedDBKey upper = p->upper();
    bool lower_open = p->lower_open();
    bool upper_open = p->upper_open();
    if (!FuzzParam(&lower, fuzzer))
      return false;
    if (!FuzzParam(&upper, fuzzer))
      return false;
    if (!FuzzParam(&lower_open, fuzzer))
      return false;
    if (!FuzzParam(&upper_open, fuzzer))
      return false;
    *p = content::IndexedDBKeyRange(lower, upper, lower_open, upper_open);
    return true;
  }
};

template <>
struct FuzzTraits<content::IndexedDBKeyPath> {
  static bool Fuzz(content::IndexedDBKeyPath* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    switch (RandInRange(3)) {
      case 0: {
        std::vector<base::string16> array;
        if (!FuzzParam(&array, fuzzer))
          return false;
        *p = content::IndexedDBKeyPath(array);
        break;
      }
      case 1: {
        base::string16 string;
        if (!FuzzParam(&string, fuzzer))
          return false;
        *p = content::IndexedDBKeyPath(string);
        break;
      }
      case 2: {
        *p = content::IndexedDBKeyPath();
        break;
      }
    }
    return true;
  }
};

template <>
struct FuzzTraits<content::NPIdentifier_Param> {
  static bool Fuzz(content::NPIdentifier_Param* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<content::NPVariant_Param> {
  static bool Fuzz(content::NPVariant_Param* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<content::PageState> {
  static bool Fuzz(content::PageState* p, Fuzzer* fuzzer) {
    std::string data = p->ToEncodedData();
    if (!FuzzParam(&data, fuzzer))
      return false;
    *p = content::PageState::CreateFromEncodedData(data);
    return true;
  }
};

template <>
struct FuzzTraits<content::SyntheticGesturePacket> {
  static bool Fuzz(content::SyntheticGesturePacket* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    scoped_ptr<content::SyntheticGestureParams> gesture_params;
    switch (RandInRange(
        content::SyntheticGestureParams::SYNTHETIC_GESTURE_TYPE_MAX + 1)) {
      case content::SyntheticGestureParams::GestureType::
          SMOOTH_SCROLL_GESTURE: {
        content::SyntheticSmoothScrollGestureParams* params =
            new content::SyntheticSmoothScrollGestureParams();
        if (!FuzzParam(&params->anchor, fuzzer))
          return false;
        if (!FuzzParam(&params->distances, fuzzer))
          return false;
        if (!FuzzParam(&params->prevent_fling, fuzzer))
          return false;
        if (!FuzzParam(&params->speed_in_pixels_s, fuzzer))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::SMOOTH_DRAG_GESTURE: {
        content::SyntheticSmoothDragGestureParams* params =
            new content::SyntheticSmoothDragGestureParams();
        if (!FuzzParam(&params->start_point, fuzzer))
          return false;
        if (!FuzzParam(&params->distances, fuzzer))
          return false;
        if (!FuzzParam(&params->speed_in_pixels_s, fuzzer))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::PINCH_GESTURE: {
        content::SyntheticPinchGestureParams* params =
            new content::SyntheticPinchGestureParams();
        if (!FuzzParam(&params->scale_factor, fuzzer))
          return false;
        if (!FuzzParam(&params->anchor, fuzzer))
          return false;
        if (!FuzzParam(&params->relative_pointer_speed_in_pixels_s,
                           fuzzer))
          return false;
        gesture_params.reset(params);
        break;
      }
      case content::SyntheticGestureParams::GestureType::TAP_GESTURE: {
        content::SyntheticTapGestureParams* params =
            new content::SyntheticTapGestureParams();
        if (!FuzzParam(&params->position, fuzzer))
          return false;
        if (!FuzzParam(&params->duration_ms, fuzzer))
          return false;
        gesture_params.reset(params);
        break;
      }
    }
    p->set_gesture_params(gesture_params.Pass());
    return true;
  }
};

template <>
struct FuzzTraits<content::WebCursor> {
  static bool Fuzz(content::WebCursor* p, Fuzzer* fuzzer) {
    content::WebCursor::CursorInfo info;
    p->GetCursorInfo(&info);

    // |type| enum is not validated on de-serialization, so pick random value.
    if (!FuzzParam(reinterpret_cast<int*>(&info.type), fuzzer))
      return false;
    if (!FuzzParam(&info.hotspot, fuzzer))
      return false;
    if (!FuzzParam(&info.image_scale_factor, fuzzer))
      return false;
    if (!FuzzParam(&info.custom_image, fuzzer))
      return false;
    // Omitting |externalHandle| since it is not serialized.

    // Scale factor is expected to be greater than 0, otherwise we hit
    // a check failure.
    info.image_scale_factor = fabs(info.image_scale_factor) + 0.001;

    *p = content::WebCursor(info);
    return true;
  }
};

template <>
struct FuzzTraits<ContentSettingsPattern> {
  static bool Fuzz(ContentSettingsPattern* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This can crash if a pattern is generated from a random
    // string. We could carefully generate a pattern or fix pattern generation.
    return true;
  }
};

template <>
struct FuzzTraits<ExtensionMsg_PermissionSetStruct> {
  static bool Fuzz(ExtensionMsg_PermissionSetStruct* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<extensions::URLPatternSet> {
  static bool Fuzz(extensions::URLPatternSet* p, Fuzzer* fuzzer) {
    std::set<URLPattern> patterns = p->patterns();
    if (!FuzzParam(&patterns, fuzzer))
      return false;
    *p = extensions::URLPatternSet(patterns);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Point> {
  static bool Fuzz(gfx::Point* p, Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::PointF> {
  static bool Fuzz(gfx::PointF* p, Fuzzer* fuzzer) {
    float x = p->x();
    float y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    p->SetPoint(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Rect> {
  static bool Fuzz(gfx::Rect* p, Fuzzer* fuzzer) {
    gfx::Point origin = p->origin();
    gfx::Size size = p->size();
    if (!FuzzParam(&origin, fuzzer))
      return false;
    if (!FuzzParam(&size, fuzzer))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::RectF> {
  static bool Fuzz(gfx::RectF* p, Fuzzer* fuzzer) {
    gfx::PointF origin = p->origin();
    gfx::SizeF size = p->size();
    if (!FuzzParam(&origin, fuzzer))
      return false;
    if (!FuzzParam(&size, fuzzer))
      return false;
    p->set_origin(origin);
    p->set_size(size);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Range> {
  static bool Fuzz(gfx::Range* p, Fuzzer* fuzzer) {
    size_t start = p->start();
    size_t end = p->end();
    if (!FuzzParam(&start, fuzzer))
      return false;
    if (!FuzzParam(&end, fuzzer))
      return false;
    *p = gfx::Range(start, end);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Size> {
  static bool Fuzz(gfx::Size* p, Fuzzer* fuzzer) {
    int width = p->width();
    int height = p->height();
    if (!FuzzParam(&width, fuzzer))
      return false;
    if (!FuzzParam(&height, fuzzer))
      return false;
    p->SetSize(width, height);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::SizeF> {
  static bool Fuzz(gfx::SizeF* p, Fuzzer* fuzzer) {
    float w;
    float h;
    if (!FuzzParam(&w, fuzzer))
      return false;
    if (!FuzzParam(&h, fuzzer))
      return false;
    p->SetSize(w, h);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Transform> {
  static bool Fuzz(gfx::Transform* p, Fuzzer* fuzzer) {
    SkMScalar matrix[16];
    for (size_t i = 0; i < arraysize(matrix); i++) {
      matrix[i] = p->matrix().get(i / 4, i % 4);
    }
    if (!FuzzParamArray(&matrix[0], arraysize(matrix), fuzzer))
      return false;
    *p = gfx::Transform(matrix[0], matrix[1], matrix[2], matrix[3], matrix[4],
                        matrix[5], matrix[6], matrix[7], matrix[8], matrix[9],
                        matrix[10], matrix[11], matrix[12], matrix[13],
                        matrix[14], matrix[15]);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Vector2d> {
  static bool Fuzz(gfx::Vector2d* p, Fuzzer* fuzzer) {
    int x = p->x();
    int y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    *p = gfx::Vector2d(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gfx::Vector2dF> {
  static bool Fuzz(gfx::Vector2dF* p, Fuzzer* fuzzer) {
    float x = p->x();
    float y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    *p = gfx::Vector2dF(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<gpu::Mailbox> {
  static bool Fuzz(gpu::Mailbox* p, Fuzzer* fuzzer) {
    fuzzer->FuzzBytes(p->name, sizeof(p->name));
    return true;
  }
};

template <>
struct FuzzTraits<gpu::MailboxHolder> {
  static bool Fuzz(gpu::MailboxHolder* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->mailbox, fuzzer))
      return false;
    if (!FuzzParam(&p->texture_target, fuzzer))
      return false;
    if (!FuzzParam(&p->sync_point, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<gpu::ValueState> {
  static bool Fuzz(gpu::ValueState* p, Fuzzer* fuzzer) {
    if (!FuzzParamArray(&p->float_value[0], 4, fuzzer))
      return false;
    if (!FuzzParamArray(&p->int_value[0], 4, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<GURL> {
  static bool Fuzz(GURL* p, Fuzzer* fuzzer) {
    if (!fuzzer->ShouldGenerate()) {
      std::string spec = p->possibly_invalid_spec();
      if (!FuzzParam(&spec, fuzzer))
        return false;
      if (spec != p->possibly_invalid_spec())
        *p = GURL(spec);
      return true;
    }

    const char url_chars[] = "Ahtp0:/.?+\\%&#";
    size_t count = RandInRange(100);
    std::string random_url;
    for (size_t i = 0; i < count; ++i)
      random_url += url_chars[RandInRange(sizeof(url_chars) - 1)];
    int selector = RandInRange(10);
    if (selector == 0)
      random_url = std::string("http://") + random_url;
    else if (selector == 1)
      random_url = std::string("file://") + random_url;
    else if (selector == 2)
      random_url = std::string("javascript:") + random_url;
    else if (selector == 2)
      random_url = std::string("data:") + random_url;
    *p = GURL(random_url);
    return true;
  }
};

#if defined(OS_WIN)
template <>
struct FuzzTraits<HWND> {
  static bool Fuzz(HWND* p, Fuzzer* fuzzer) {
    // TODO(aarya): This should actually do something.
    return true;
  }
};
#endif

template <>
struct FuzzTraits<IPC::Message> {
  static bool Fuzz(IPC::Message* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    if (g_function_vector.empty())
      return false;
    size_t index = RandInRange(g_function_vector.size());
    IPC::Message* ipc_message = (*g_function_vector[index])(NULL, fuzzer);
    if (!ipc_message)
      return false;
    p = ipc_message;
    return true;
  }
};

template <>
struct FuzzTraits<IPC::PlatformFileForTransit> {
  static bool Fuzz(IPC::PlatformFileForTransit* p, Fuzzer* fuzzer) {
    // TODO(inferno): I don't think we can generate real ones due to check on
    // construct.
    return true;
  }
};

template <>
struct FuzzTraits<IPC::ChannelHandle> {
  static bool Fuzz(IPC::ChannelHandle* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    // TODO(inferno): Add way to generate real channel handles.
#if defined(OS_WIN)
    HANDLE fake_handle = (HANDLE)(RandU64());
    p->pipe = IPC::ChannelHandle::PipeHandle(fake_handle);
    return true;
#elif defined(OS_POSIX)
    return
      FuzzParam(&p->name, fuzzer) &&
      FuzzParam(&p->socket, fuzzer);
#endif
  }
};

#if defined(OS_WIN)
template <>
struct FuzzTraits<LOGFONT> {
  static bool Fuzz(LOGFONT* p, Fuzzer* fuzzer) {
    // TODO(aarya): This should actually do something.
    return true;
  }
};
#endif

template <>
struct FuzzTraits<media::AudioParameters> {
  static bool Fuzz(media::AudioParameters* p, Fuzzer* fuzzer) {
    int format = p->format();
    int channel_layout = p->channel_layout();
    int sample_rate = p->sample_rate();
    int bits_per_sample = p->bits_per_sample();
    int frames_per_buffer = p->frames_per_buffer();
    int channels = p->channels();
    int effects = p->effects();
    if (!FuzzParam(&format, fuzzer))
      return false;
    if (!FuzzParam(&channel_layout, fuzzer))
      return false;
    if (!FuzzParam(&sample_rate, fuzzer))
      return false;
    if (!FuzzParam(&bits_per_sample, fuzzer))
      return false;
    if (!FuzzParam(&frames_per_buffer, fuzzer))
      return false;
    if (!FuzzParam(&channels, fuzzer))
      return false;
    if (!FuzzParam(&effects, fuzzer))
      return false;
    media::AudioParameters params(
        static_cast<media::AudioParameters::Format>(format),
        static_cast<media::ChannelLayout>(channel_layout), channels,
        sample_rate, bits_per_sample, frames_per_buffer, effects);
    *p = params;
    return true;
  }
};

template <>
struct FuzzTraits<media::VideoCaptureFormat> {
  static bool Fuzz(media::VideoCaptureFormat* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->frame_size, fuzzer))
      return false;
    if (!FuzzParam(&p->frame_rate, fuzzer))
      return false;
    if (!FuzzParam(reinterpret_cast<int*>(&p->pixel_format), fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<net::LoadTimingInfo> {
  static bool Fuzz(net::LoadTimingInfo* p, Fuzzer* fuzzer) {
    return FuzzParam(&p->socket_log_id, fuzzer) &&
           FuzzParam(&p->socket_reused, fuzzer) &&
           FuzzParam(&p->request_start_time, fuzzer) &&
           FuzzParam(&p->request_start, fuzzer) &&
           FuzzParam(&p->proxy_resolve_start, fuzzer) &&
           FuzzParam(&p->proxy_resolve_end, fuzzer) &&
           FuzzParam(&p->connect_timing.dns_start, fuzzer) &&
           FuzzParam(&p->connect_timing.dns_end, fuzzer) &&
           FuzzParam(&p->connect_timing.connect_start, fuzzer) &&
           FuzzParam(&p->connect_timing.connect_end, fuzzer) &&
           FuzzParam(&p->connect_timing.ssl_start, fuzzer) &&
           FuzzParam(&p->connect_timing.ssl_end, fuzzer) &&
           FuzzParam(&p->send_start, fuzzer) &&
           FuzzParam(&p->send_end, fuzzer) &&
           FuzzParam(&p->receive_headers_end, fuzzer);
  }
};

template <>
struct FuzzTraits<net::HostPortPair> {
  static bool Fuzz(net::HostPortPair* p, Fuzzer* fuzzer) {
    std::string host = p->host();
    uint16 port = p->port();
    if (!FuzzParam(&host, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;
    p->set_host(host);
    p->set_port(port);
    return true;
  }
};

template <>
struct FuzzTraits<net::IPEndPoint> {
  static bool Fuzz(net::IPEndPoint* p, Fuzzer* fuzzer) {
    net::IPAddressNumber address = p->address();
    int port = p->port();
    if (!FuzzParam(&address, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;
    net::IPEndPoint ip_endpoint(address, port);
    *p = ip_endpoint;
    return true;
  }
};

template <>
struct FuzzTraits<network_hints::LookupRequest> {
  static bool Fuzz(network_hints::LookupRequest* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->hostname_list, fuzzer))
      return false;
    return true;
  }
};

// PP_ traits.
template <>
struct FuzzTraits<PP_Bool> {
  static bool Fuzz(PP_Bool* p, Fuzzer* fuzzer) {
    bool tmp = PP_ToBool(*p);
    if (!FuzzParam(&tmp, fuzzer))
      return false;
    *p = PP_FromBool(tmp);
    return true;
  }
};

template <>
struct FuzzTraits<PP_KeyInformation> {
  static bool Fuzz(PP_KeyInformation* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<PP_NetAddress_Private> {
  static bool Fuzz(PP_NetAddress_Private* p, Fuzzer* fuzzer) {
    p->size = RandInRange(sizeof(p->data) + 1);
    fuzzer->FuzzBytes(&p->data, p->size);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PPB_X509Certificate_Fields> {
  static bool Fuzz(ppapi::PPB_X509Certificate_Fields* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::PPBFlash_DrawGlyphs_Params> {
  static bool Fuzz(ppapi::proxy::PPBFlash_DrawGlyphs_Params* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::ResourceMessageCallParams> {
  static bool Fuzz(
      ppapi::proxy::ResourceMessageCallParams* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Resource resource;
    int32_t sequence;
    bool has_callback;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    if (!FuzzParam(&sequence, fuzzer))
      return false;
    if (!FuzzParam(&has_callback, fuzzer))
      return false;
    *p = ppapi::proxy::ResourceMessageCallParams(resource, sequence);
    if (has_callback)
      p->set_has_callback();
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::ResourceMessageReplyParams> {
  static bool Fuzz(
      ppapi::proxy::ResourceMessageReplyParams* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Resource resource;
    int32_t sequence;
    int32_t result;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    if (!FuzzParam(&sequence, fuzzer))
      return false;
    if (!FuzzParam(&result, fuzzer))
      return false;
    *p = ppapi::proxy::ResourceMessageReplyParams(resource, sequence);
    p->set_result(result);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedHandle> {
  static bool Fuzz(ppapi::proxy::SerializedHandle* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedFontDescription> {
  static bool Fuzz(ppapi::proxy::SerializedFontDescription* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedTrueTypeFontDesc> {
  static bool Fuzz(ppapi::proxy::SerializedTrueTypeFontDesc* p,
                       Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::proxy::SerializedVar> {
  static bool Fuzz(ppapi::proxy::SerializedVar* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::HostResource> {
  static bool Fuzz(ppapi::HostResource* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    PP_Instance instance;
    PP_Resource resource;
    if (!FuzzParam(&instance, fuzzer))
      return false;
    if (!FuzzParam(&resource, fuzzer))
      return false;
    p->SetHostResource(instance, resource);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PepperFilePath> {
  static bool Fuzz(ppapi::PepperFilePath *p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    unsigned domain = RandInRange(ppapi::PepperFilePath::DOMAIN_MAX_VALID+1);
    base::FilePath path;
    if (!FuzzParam(&path, fuzzer))
      return false;
    *p = ppapi::PepperFilePath(
        static_cast<ppapi::PepperFilePath::Domain>(domain), path);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::PpapiPermissions> {
  static bool Fuzz(ppapi::PpapiPermissions* p, Fuzzer* fuzzer) {
    uint32_t bits = p->GetBits();
    if (!FuzzParam(&bits, fuzzer))
      return false;
    *p = ppapi::PpapiPermissions(bits);
    return true;
  }
};

template <>
struct FuzzTraits<ppapi::SocketOptionData> {
  static bool Fuzz(ppapi::SocketOptionData* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This can be improved.
    int32 tmp;
    p->GetInt32(&tmp);
    if (!FuzzParam(&tmp, fuzzer))
      return false;
    p->SetInt32(tmp);
    return true;
  }
};

template <>
struct FuzzTraits<printing::PdfRenderSettings> {
  static bool Fuzz(printing::PdfRenderSettings* p, Fuzzer* fuzzer) {
    gfx::Rect area = p->area();
    int dpi = p->dpi();
    bool autorotate = p->autorotate();
    if (!FuzzParam(&area, fuzzer))
      return false;
    if (!FuzzParam(&dpi, fuzzer))
      return false;
    if (!FuzzParam(&autorotate, fuzzer))
      return false;
    *p = printing::PdfRenderSettings(area, dpi, autorotate);
    return true;
  }
};

template <>
struct FuzzTraits<remoting::ScreenResolution> {
  static bool Fuzz(remoting::ScreenResolution* p, Fuzzer* fuzzer) {
    webrtc::DesktopSize dimensions = p->dimensions();
    webrtc::DesktopVector dpi = p->dpi();
    if (!FuzzParam(&dimensions, fuzzer))
      return false;
    if (!FuzzParam(&dpi, fuzzer))
      return false;
    *p = remoting::ScreenResolution(dimensions, dpi);
    return true;
  }
};

template <>
struct FuzzTraits<SkBitmap> {
  static bool Fuzz(SkBitmap* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): This should actually do something.
    return true;
  }
};

template <>
struct FuzzTraits<storage::DataElement> {
  static bool Fuzz(storage::DataElement* p, Fuzzer* fuzzer) {
    // TODO(mbarbella): Support mutation.
    if (!fuzzer->ShouldGenerate())
      return true;

    switch (RandInRange(4)) {
      case storage::DataElement::Type::TYPE_BYTES: {
        if (RandEvent(2)) {
          p->SetToEmptyBytes();
        } else {
          char data[256];
          int data_len = RandInRange(sizeof(data));
          fuzzer->FuzzBytes(&data[0], data_len);
          p->SetToBytes(&data[0], data_len);
        }
        return true;
      }
      case storage::DataElement::Type::TYPE_FILE: {
        base::FilePath path;
        uint64 offset;
        uint64 length;
        base::Time modification_time;
        if (!FuzzParam(&path, fuzzer))
          return false;
        if (!FuzzParam(&offset, fuzzer))
          return false;
        if (!FuzzParam(&length, fuzzer))
          return false;
        if (!FuzzParam(&modification_time, fuzzer))
          return false;
        p->SetToFilePathRange(path, offset, length, modification_time);
        return true;
      }
      case storage::DataElement::Type::TYPE_BLOB: {
        std::string uuid;
        uint64 offset;
        uint64 length;
        if (!FuzzParam(&uuid, fuzzer))
          return false;
        if (!FuzzParam(&offset, fuzzer))
          return false;
        if (!FuzzParam(&length, fuzzer))
          return false;
        p->SetToBlobRange(uuid, offset, length);
        return true;
      }
      case storage::DataElement::Type::TYPE_FILE_FILESYSTEM: {
        GURL url;
        uint64 offset;
        uint64 length;
        base::Time modification_time;
        if (!FuzzParam(&url, fuzzer))
          return false;
        if (!FuzzParam(&offset, fuzzer))
          return false;
        if (!FuzzParam(&length, fuzzer))
          return false;
        if (!FuzzParam(&modification_time, fuzzer))
          return false;
        p->SetToFileSystemUrlRange(url, offset, length, modification_time);
        return true;
      }
      default: {
        NOTREACHED();
        return false;
      }
    }
  }
};

template <>
struct FuzzTraits<ui::LatencyInfo> {
  static bool Fuzz(ui::LatencyInfo* p, Fuzzer* fuzzer) {
    // TODO(inferno): Add param traits for |latency_components|.
    p->input_coordinates_size = static_cast<uint32>(
        RandInRange(ui::LatencyInfo::kMaxInputCoordinates + 1));
    if (!FuzzParamArray(
        &p->input_coordinates[0], p->input_coordinates_size, fuzzer))
      return false;
    if (!FuzzParam(&p->trace_id, fuzzer))
      return false;
    if (!FuzzParam(&p->terminated, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<ui::LatencyInfo::InputCoordinate> {
  static bool Fuzz(
      ui::LatencyInfo::InputCoordinate* p, Fuzzer* fuzzer) {
    if (!FuzzParam(&p->x, fuzzer))
      return false;
    if (!FuzzParam(&p->y, fuzzer))
      return false;
    return true;
  }
};

template <>
struct FuzzTraits<url::Origin> {
  static bool Fuzz(url::Origin* p, Fuzzer* fuzzer) {
    std::string origin = p->string();
    if (!FuzzParam(&origin, fuzzer))
        return false;
    *p = url::Origin(origin);
    return true;
  }
};

template <>
struct FuzzTraits<URLPattern> {
  static bool Fuzz(URLPattern* p, Fuzzer* fuzzer) {
    int valid_schemes = p->valid_schemes();
    std::string host = p->host();
    std::string port = p->port();
    std::string path = p->path();
    if (!FuzzParam(&valid_schemes, fuzzer))
      return false;
    if (!FuzzParam(&host, fuzzer))
      return false;
    if (!FuzzParam(&port, fuzzer))
      return false;
    if (!FuzzParam(&path, fuzzer))
      return false;
    *p = URLPattern(valid_schemes);
    p->SetHost(host);
    p->SetPort(port);
    p->SetPath(path);
    return true;
  }
};

template <>
struct FuzzTraits<webrtc::DesktopSize> {
  static bool Fuzz(webrtc::DesktopSize* p, Fuzzer* fuzzer) {
    int32_t width = p->width();
    int32_t height = p->height();
    if (!FuzzParam(&width, fuzzer))
      return false;
    if (!FuzzParam(&height, fuzzer))
      return false;
    *p = webrtc::DesktopSize(width, height);
    return true;
  }
};

template <>
struct FuzzTraits<webrtc::DesktopVector> {
  static bool Fuzz(webrtc::DesktopVector* p, Fuzzer* fuzzer) {
    int32_t x = p->x();
    int32_t y = p->y();
    if (!FuzzParam(&x, fuzzer))
      return false;
    if (!FuzzParam(&y, fuzzer))
      return false;
    p->set(x, y);
    return true;
  }
};

template <>
struct FuzzTraits<webrtc::DesktopRect> {
  static bool Fuzz(webrtc::DesktopRect* p, Fuzzer* fuzzer) {
    int32_t left = p->left();
    int32_t top = p->top();
    int32_t right = p->right();
    int32_t bottom = p->bottom();
    if (!FuzzParam(&left, fuzzer))
      return false;
    if (!FuzzParam(&top, fuzzer))
      return false;
    if (!FuzzParam(&right, fuzzer))
      return false;
    if (!FuzzParam(&bottom, fuzzer))
      return false;
    *p = webrtc::DesktopRect::MakeLTRB(left, top, right, bottom);
    return true;
  }
};

template <>
struct FuzzTraits<webrtc::MouseCursor> {
  static bool Fuzz(webrtc::MouseCursor* p, Fuzzer* fuzzer) {
    webrtc::DesktopVector hotspot = p->hotspot();
    if (!FuzzParam(&hotspot, fuzzer))
      return false;
    p->set_hotspot(hotspot);

    // TODO(mbarbella): Find a way to handle the size mutation properly.
    if (!fuzzer->ShouldGenerate())
      return false;

    // Using a small size here to avoid OOM or overflow on image allocation.
    webrtc::DesktopSize size(RandInRange(100), RandInRange(100));
    p->set_image(new webrtc::BasicDesktopFrame(size));
    return true;
  }
};

// Redefine macros to generate generating from traits declarations.
// STRUCT declarations cause corresponding STRUCT_TRAITS declarations to occur.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_BEGIN_WITH_PARENT
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN_WITH_PARENT(struct_name, parent) \
  IPC_STRUCT_BEGIN(struct_name)
#define IPC_STRUCT_BEGIN(struct_name) IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name, ...) IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_END() IPC_STRUCT_TRAITS_END()

// Set up so next include will generate generate trait classes.
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#define IPC_STRUCT_TRAITS_BEGIN(struct_name) \
  template <> \
  struct FuzzTraits<struct_name> { \
    static bool Fuzz(struct_name *p, Fuzzer* fuzzer) {

#define IPC_STRUCT_TRAITS_MEMBER(name) \
      if (!FuzzParam(&p->name, fuzzer)) \
        return false;

#define IPC_STRUCT_TRAITS_PARENT(type) \
      if (!FuzzParam(static_cast<type*>(p), fuzzer)) \
        return false;

#define IPC_STRUCT_TRAITS_END() \
      return true; \
    } \
  };

// If |condition| isn't met, the messsge will fail to serialize. Try
// increasingly smaller ranges until we find one that happens to meet
// the condition, or fail trying.
// TODO(mbarbella): Attempt to validate even in the mutation case.
#undef IPC_ENUM_TRAITS_VALIDATE
#define IPC_ENUM_TRAITS_VALIDATE(enum_name, condition)             \
  template <>                                                      \
  struct FuzzTraits<enum_name> {                                   \
    static bool Fuzz(enum_name* p, Fuzzer* fuzzer) {               \
      if (!fuzzer->ShouldGenerate()) {                             \
        return FuzzParam(reinterpret_cast<int*>(p), fuzzer);       \
      }                                                            \
      for (int shift = 30; shift; --shift) {                       \
        for (int tries = 0; tries < 2; ++tries) {                  \
          int value = RandInRange(1 << shift);                     \
          if (condition) {                                         \
            *reinterpret_cast<int*>(p) = value;                    \
            return true;                                           \
          }                                                        \
        }                                                          \
      }                                                            \
      std::cerr << "failed to satisfy " << #condition << "\n";     \
      return false;                                                \
    }                                                              \
  };

// Bring them into existence.
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

// Redefine macros to generate generating funtions
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist)       \
  IPC_##kind##_##type##_FUZZ(name, in, out, ilist, olist)

#define IPC_EMPTY_CONTROL_FUZZ(name, in, out, ilist, olist)             \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    if (msg) {                                                          \
      return NULL;                                                      \
    }                                                                   \
    return new name();                                                  \
  }

#define IPC_EMPTY_ROUTED_FUZZ(name, in, out, ilist, olist)              \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    if (msg) {                                                          \
      return NULL;                                                      \
    }                                                                   \
    return new name(RandInRange(MAX_FAKE_ROUTING_ID));                  \
  }

#define IPC_ASYNC_CONTROL_FUZZ(name, in, out, ilist, olist)             \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (msg) {                                                          \
      name::Read(static_cast<name*>(msg), &p);                          \
    }                                                                   \
    if (FuzzParam(&p, fuzzer)) {                                        \
      return new name(IPC_MEMBERS_IN_##in(p));                          \
    }                                                                   \
    std::cerr << "Don't know how to handle " << #name << "\n";          \
    return 0;                                                           \
  }

#define IPC_ASYNC_ROUTED_FUZZ(name, in, out, ilist, olist)              \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    IPC_TUPLE_IN_##in ilist p;                                          \
    if (msg) {                                                          \
      name::Read(static_cast<name*>(msg), &p);                          \
    }                                                                   \
    if (FuzzParam(&p, fuzzer)) {                                        \
      return new name(RandInRange(MAX_FAKE_ROUTING_ID)                  \
                      IPC_COMMA_##in                                    \
                      IPC_MEMBERS_IN_##in(p));                          \
    }                                                                   \
    std::cerr << "Don't know how to handle " << #name << "\n";          \
    return 0;                                                           \
  }

#define IPC_SYNC_CONTROL_FUZZ(name, in, out, ilist, olist)              \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    IPC_TUPLE_IN_##in ilist p;                                          \
    name* real_msg = static_cast<name*>(msg);                           \
    name* new_msg = NULL;                                               \
    if (real_msg) {                                                     \
      name::ReadSendParam(real_msg, &p);                                \
    }                                                                   \
    if (FuzzParam(&p, fuzzer)) {                                        \
      new_msg = new name(IPC_MEMBERS_IN_##in(p)                         \
                         IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                         IPC_MEMBERS_OUT_##out());                      \
    }                                                                   \
    if (real_msg && new_msg) {                                          \
      MessageCracker::CopyMessageID(new_msg, real_msg);                 \
    }                                                                   \
    else if (!new_msg) {                                                     \
      std::cerr << "Don't know how to handle " << #name << "\n";        \
    }                                                                   \
    return new_msg;                                                     \
  }

#define IPC_SYNC_ROUTED_FUZZ(name, in, out, ilist, olist)               \
  IPC::Message* fuzzer_for_##name(IPC::Message* msg, Fuzzer* fuzzer) {  \
    IPC_TUPLE_IN_##in ilist p;                                          \
    name* real_msg = static_cast<name*>(msg);                           \
    name* new_msg = NULL;                                               \
    if (real_msg) {                                                     \
      name::ReadSendParam(real_msg, &p);                                \
    }                                                                   \
    if (FuzzParam(&p, fuzzer)) {                                        \
      new_msg = new name(RandInRange(MAX_FAKE_ROUTING_ID)               \
                         IPC_COMMA_OR_##out(IPC_COMMA_##in)             \
                         IPC_MEMBERS_IN_##in(p)                         \
                         IPC_COMMA_AND_##out(IPC_COMMA_##in)            \
                         IPC_MEMBERS_OUT_##out());                      \
    }                                                                   \
    if (real_msg && new_msg) {                                          \
      MessageCracker::CopyMessageID(new_msg, real_msg);                 \
    }                                                                   \
    else if (!new_msg) {                                                     \
      std::cerr << "Don't know how to handle " << #name << "\n";        \
    }                                                                   \
    return new_msg;                                                     \
  }

#define MAX_FAKE_ROUTING_ID 15

#define IPC_MEMBERS_IN_0(p)
#define IPC_MEMBERS_IN_1(p) get<0>(p)
#define IPC_MEMBERS_IN_2(p) get<0>(p), get<1>(p)
#define IPC_MEMBERS_IN_3(p) get<0>(p), get<1>(p), get<2>(p)
#define IPC_MEMBERS_IN_4(p) get<0>(p), get<1>(p), get<2>(p), get<3>(p)
#define IPC_MEMBERS_IN_5(p) get<0>(p), get<1>(p), get<2>(p), get<3>(p), \
                            get<4>(p)

#define IPC_MEMBERS_OUT_0()
#define IPC_MEMBERS_OUT_1() NULL
#define IPC_MEMBERS_OUT_2() NULL, NULL
#define IPC_MEMBERS_OUT_3() NULL, NULL, NULL
#define IPC_MEMBERS_OUT_4() NULL, NULL, NULL, NULL
#define IPC_MEMBERS_OUT_5() NULL, NULL, NULL, NULL, NULL

#include "tools/ipc_fuzzer/message_lib/all_messages.h"
#include "ipc/ipc_message_null_macros.h"

void PopulateFuzzerFunctionVector(
    FuzzerFunctionVector* function_vector) {
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  function_vector->push_back(fuzzer_for_##name);
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

// Redefine macros to register fuzzing functions into map.
#include "ipc/ipc_message_null_macros.h"
#undef IPC_MESSAGE_DECL
#define IPC_MESSAGE_DECL(kind, type, name, in, out, ilist, olist) \
  (*map)[static_cast<uint32>(name::ID)] = fuzzer_for_##name;

void PopulateFuzzerFunctionMap(FuzzerFunctionMap* map) {
#include "tools/ipc_fuzzer/message_lib/all_messages.h"
}

}  // namespace ipc_fuzzer
