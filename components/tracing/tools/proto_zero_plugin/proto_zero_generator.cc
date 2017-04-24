// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_zero_generator.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/io/printer.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream.h"
#include "third_party/protobuf/src/google/protobuf/stubs/strutil.h"

namespace tracing {
namespace proto {

using google::protobuf::Descriptor;  // Message descriptor.
using google::protobuf::EnumDescriptor;
using google::protobuf::EnumValueDescriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

using google::protobuf::Split;
using google::protobuf::StripPrefixString;
using google::protobuf::StripString;
using google::protobuf::StripSuffixString;
using google::protobuf::UpperString;

namespace {

inline std::string ProtoStubName(const FileDescriptor* proto) {
  return StripSuffixString(proto->name(), ".proto") + ".pbzero";
}

class GeneratorJob {
 public:
  GeneratorJob(const FileDescriptor *file,
               Printer* stub_h_printer,
               Printer* stub_cc_printer)
      : source_(file), stub_h_(stub_h_printer), stub_cc_(stub_cc_printer) {
  }

  bool GenerateStubs() {
    Preprocess();
    GeneratePrologue();
    for (const EnumDescriptor* enumeration: enums_)
      GenerateEnumDescriptor(enumeration);
    for (const Descriptor* message : messages_)
      GenerateMessageDescriptor(message);
    GenerateEpilogue();
    return error_.empty();
  }

  void SetOption(const std::string& name, const std::string& value) {
    if (name == "wrapper_namespace") {
      wrapper_namespace_ = value;
    } else {
      Abort(std::string() + "Unknown plugin option '" + name + "'.");
    }
  }

  // If generator fails to produce stubs for a particular proto definitions
  // it finishes with undefined output and writes the first error occured.
  const std::string& GetFirstError() const {
    return error_;
  }

 private:
  // Only the first error will be recorded.
  void Abort(const std::string& reason) {
    if (error_.empty())
      error_ = reason;
  }

  // Get full name (including outer descriptors) of proto descriptor.
  template <class T>
  inline std::string GetDescriptorName(const T* descriptor) {
    if (!package_.empty()) {
      return StripPrefixString(descriptor->full_name(), package_ + ".");
    } else {
      return descriptor->full_name();
    }
  }

  // Get C++ class name corresponding to proto descriptor.
  // Nested names are splitted by underscores. Underscores in type names aren't
  // prohibited but not recommended in order to avoid name collisions.
  template <class T>
  inline std::string GetCppClassName(const T* descriptor, bool full = false) {
    std::string name = GetDescriptorName(descriptor);
    StripString(&name, ".", '_');
    if (full)
      name = full_namespace_prefix_ + name;
    return name;
  }

  inline std::string GetFieldNumberConstant(const FieldDescriptor* field) {
    std::string name = field->camelcase_name();
    if (!name.empty()) {
      name.at(0) = toupper(name.at(0));
      name = "k" + name + "FieldNumber";
    } else {
      // Protoc allows fields like 'bool _ = 1'.
      Abort("Empty field name in camel case notation.");
    }
    return name;
  }

  // Small enums can be written faster without involving VarInt encoder.
  inline bool IsTinyEnumField(const FieldDescriptor* field) {
    if (field->type() != FieldDescriptor::TYPE_ENUM)
      return false;
    const EnumDescriptor* enumeration = field->enum_type();

    for (int i = 0; i < enumeration->value_count(); ++i) {
      int32_t value = enumeration->value(i)->number();
      if (value < 0 || value > 0x7F)
        return false;
    }
    return true;
  }

  void CollectDescriptors() {
    // Collect message descriptors in DFS order.
    std::vector<const Descriptor*> stack;
    for (int i = 0; i < source_->message_type_count(); ++i)
      stack.push_back(source_->message_type(i));

    while (!stack.empty()) {
      const Descriptor* message = stack.back();
      stack.pop_back();
      messages_.push_back(message);
      for (int i = 0; i < message->nested_type_count(); ++i) {
        stack.push_back(message->nested_type(i));
      }
    }

    // Collect enums.
    for (int i = 0; i < source_->enum_type_count(); ++i)
      enums_.push_back(source_->enum_type(i));

    for (const Descriptor* message : messages_) {
      for (int i = 0; i < message->enum_type_count(); ++i) {
        enums_.push_back(message->enum_type(i));
      }
    }
  }

  void CollectDependencies() {
    // Public import basically means that callers only need to import this
    // proto in order to use the stuff publicly imported by this proto.
    for (int i = 0; i < source_->public_dependency_count(); ++i)
      public_imports_.insert(source_->public_dependency(i));

    if (source_->weak_dependency_count() > 0)
      Abort("Weak imports are not supported.");

    // Sanity check. Collect public imports (of collected imports) in DFS order.
    // Visibilty for current proto:
    // - all imports listed in current proto,
    // - public imports of everything imported (recursive).
    std::vector<const FileDescriptor*> stack;
    for (int i = 0; i < source_->dependency_count(); ++i) {
      const FileDescriptor* import = source_->dependency(i);
      stack.push_back(import);
      if (public_imports_.count(import) == 0) {
        private_imports_.insert(import);
      }
    }

    while (!stack.empty()) {
      const FileDescriptor* import = stack.back();
      stack.pop_back();
      // Having imports under different packages leads to unnecessary
      // complexity with namespaces.
      if (import->package() != package_)
        Abort("Imported proto must be in the same package.");

      for (int i = 0; i < import->public_dependency_count(); ++i) {
        stack.push_back(import->public_dependency(i));
      }
    }

    // Collect descriptors of messages and enums used in current proto.
    // It will be used to generate necessary forward declarations and performed
    // sanity check guarantees that everything lays in the same namespace.
    for (const Descriptor* message : messages_) {
      for (int i = 0; i < message->field_count(); ++i) {
        const FieldDescriptor* field = message->field(i);

        if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
          if (public_imports_.count(field->message_type()->file()) == 0) {
            // Avoid multiple forward declarations since
            // public imports have been already included.
            referenced_messages_.insert(field->message_type());
          }
        } else if (field->type() == FieldDescriptor::TYPE_ENUM) {
          if (public_imports_.count(field->enum_type()->file()) == 0) {
            referenced_enums_.insert(field->enum_type());
          }
        }
      }
    }
  }

  void Preprocess() {
    // Package name maps to a series of namespaces.
    package_ = source_->package();
    namespaces_ = Split(package_, ".");
    if (!wrapper_namespace_.empty())
      namespaces_.insert(namespaces_.begin(), wrapper_namespace_);

    full_namespace_prefix_ = "::";
    for (const std::string& ns : namespaces_)
      full_namespace_prefix_ += ns + "::";

    CollectDescriptors();
    CollectDependencies();
  }

  // Print top header, namespaces and forward declarations.
  void GeneratePrologue() {
    std::string greeting =
        "// Autogenerated. DO NOT EDIT.\n"
        "// Protobuf compiler (protoc) has generated these stubs with\n"
        "// //components/tracing/tools/proto_zero_plugin.\n";
    std::string guard = package_ + "_" + source_->name() + "_H_";
    UpperString(&guard);
    StripString(&guard, ".-/\\", '_');

    stub_h_->Print(
        "$greeting$\n"
        "#ifndef $guard$\n"
        "#define $guard$\n\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "#include \"components/tracing/core/proto_zero_message.h\"\n",
        "greeting", greeting,
        "guard", guard);
    stub_cc_->Print(
        "$greeting$\n"
        "#include \"$name$.h\"\n",
        "greeting", greeting,
        "name", ProtoStubName(source_));

    // Print includes for public imports.
    for (const FileDescriptor* dependency : public_imports_) {
      // Dependency name could contatin slashes but importing from upper-level
      // directories is not possible anyway since build system process each
      // proto file individually. Hence proto lookup path always equal to the
      // directory where particular proto file is located and protoc does not
      // allow reference to upper directory (aka ..) in import path.
      //
      // Laconically said:
      // - source_->name() may never have slashes,
      // - dependency->name() may have slashes but always reffers to inner path.
      stub_h_->Print(
          "#include \"$name$.h\"\n",
          "name", ProtoStubName(dependency));
    }
    stub_h_->Print("\n");

    // Print includes for private imports to .cc file.
    for (const FileDescriptor* dependency : private_imports_) {
      stub_cc_->Print(
         "#include \"$name$.h\"\n",
         "name", ProtoStubName(dependency));
    }
    stub_cc_->Print("\n");

    if (messages_.size() > 0) {
      stub_cc_->Print(
          "namespace {\n"
          "  static const ::tracing::v2::proto::ProtoFieldDescriptor "
          "kInvalidField = {\"\", "
          "::tracing::v2::proto::ProtoFieldDescriptor::Type::TYPE_INVALID, "
          "0, false};\n"
          "}\n\n");
    }

    // Print namespaces.
    for (const std::string& ns : namespaces_) {
      stub_h_->Print("namespace $ns$ {\n", "ns", ns);
      stub_cc_->Print("namespace $ns$ {\n", "ns", ns);
    }
    stub_h_->Print("\n");
    stub_cc_->Print("\n");

    // Print forward declarations.
    for (const Descriptor* message : referenced_messages_) {
      stub_h_->Print(
          "class $class$;\n",
          "class", GetCppClassName(message));
    }
    for (const EnumDescriptor* enumeration : referenced_enums_) {
      stub_h_->Print(
          "enum $class$ : int32_t;\n",
          "class", GetCppClassName(enumeration));
    }
    stub_h_->Print("\n");
  }

  void GenerateEnumDescriptor(const EnumDescriptor* enumeration) {
    stub_h_->Print(
        "enum $class$ : int32_t {\n",
        "class", GetCppClassName(enumeration));
    stub_h_->Indent();

    std::string value_name_prefix;
    if (enumeration->containing_type() != nullptr)
        value_name_prefix = GetCppClassName(enumeration) + "_";

    for (int i = 0; i < enumeration->value_count(); ++i) {
      const EnumValueDescriptor* value = enumeration->value(i);
      stub_h_->Print(
          "$name$ = $number$,\n",
          "name", value_name_prefix + value->name(),
          "number", std::to_string(value->number()));
    }

    stub_h_->Outdent();
    stub_h_->Print("};\n\n");
  }

  void GenerateSimpleFieldDescriptor(const FieldDescriptor* field) {
    std::map<std::string, std::string> setter;
    setter["id"] = std::to_string(field->number());
    setter["name"] = field->name();
    setter["action"] = field->is_repeated() ? "add" : "set";

    std::string appender;
    std::string cpp_type;

    switch (field->type()) {
      case FieldDescriptor::TYPE_BOOL: {
        appender = "AppendTinyVarInt";
        cpp_type = "bool";
        break;
      }
      case FieldDescriptor::TYPE_INT32: {
        appender = "AppendVarInt";
        cpp_type = "int32_t";
        break;
      }
      case FieldDescriptor::TYPE_INT64: {
        appender = "AppendVarInt";
        cpp_type = "int64_t";
        break;
      }
      case FieldDescriptor::TYPE_UINT32: {
        appender = "AppendVarInt";
        cpp_type = "uint32_t";
        break;
      }
      case FieldDescriptor::TYPE_UINT64: {
        appender = "AppendVarInt";
        cpp_type = "uint64_t";
        break;
      }
      case FieldDescriptor::TYPE_SINT32: {
        appender = "AppendSignedVarInt";
        cpp_type = "int32_t";
        break;
      }
      case FieldDescriptor::TYPE_SINT64: {
        appender = "AppendSignedVarInt";
        cpp_type = "int64_t";
        break;
      }
      case FieldDescriptor::TYPE_FIXED32: {
        appender = "AppendFixed";
        cpp_type = "uint32_t";
        break;
      }
      case FieldDescriptor::TYPE_FIXED64: {
        appender = "AppendFixed";
        cpp_type = "uint64_t";
        break;
      }
      case FieldDescriptor::TYPE_SFIXED32: {
        appender = "AppendFixed";
        cpp_type = "int32_t";
        break;
      }
      case FieldDescriptor::TYPE_SFIXED64: {
        appender = "AppendFixed";
        cpp_type = "int64_t";
        break;
      }
      case FieldDescriptor::TYPE_FLOAT: {
        appender = "AppendFixed";
        cpp_type = "float";
        break;
      }
      case FieldDescriptor::TYPE_DOUBLE: {
        appender = "AppendFixed";
        cpp_type = "double";
        break;
      }
      case FieldDescriptor::TYPE_ENUM: {
        appender = IsTinyEnumField(field) ? "AppendTinyVarInt" : "AppendVarInt";
        cpp_type = GetCppClassName(field->enum_type(), true);
        break;
      }
      case FieldDescriptor::TYPE_STRING: {
        appender = "AppendString";
        cpp_type = "const char*";
        break;
      }
      case FieldDescriptor::TYPE_BYTES: {
        stub_h_->Print(
            setter,
            "void $action$_$name$(const uint8_t* data, size_t size) {\n"
            "  AppendBytes($id$, data, size);\n"
            "}\n");
        return;
      }
      default: {
        Abort("Unsupported field type.");
        return;
      }
    }
    setter["appender"] = appender;
    setter["cpp_type"] = cpp_type;
    stub_h_->Print(
        setter,
        "void $action$_$name$($cpp_type$ value) {\n"
        "  $appender$($id$, value);\n"
        "}\n");
  }

  void GenerateNestedMessageFieldDescriptor(const FieldDescriptor* field) {
    std::string action = field->is_repeated() ? "add" : "set";
    std::string inner_class = GetCppClassName(field->message_type());
    std::string outer_class = GetCppClassName(field->containing_type());

    stub_h_->Print(
        "$inner_class$* $action$_$name$();\n",
        "name", field->name(),
        "action", action,
        "inner_class", inner_class);
    stub_cc_->Print(
        "$inner_class$* $outer_class$::$action$_$name$() {\n"
        "  return BeginNestedMessage<$inner_class$>($id$);\n"
        "}\n\n",
        "id", std::to_string(field->number()),
        "name", field->name(),
        "action", action,
        "inner_class", inner_class,
        "outer_class", outer_class);
  }

  void GenerateReflectionForMessageFields(const Descriptor* message) {
    const bool has_fields = (message->field_count() > 0);

    // Field number constants.
    if (has_fields) {
      stub_h_->Print("enum : int32_t {\n");
      stub_h_->Indent();

      for (int i = 0; i < message->field_count(); ++i) {
        const FieldDescriptor* field = message->field(i);
        stub_h_->Print(
            "$name$ = $id$,\n",
            "name", GetFieldNumberConstant(field),
            "id", std::to_string(field->number()));
      }
      stub_h_->Outdent();
      stub_h_->Print("};\n");
    }

    // Fields reflection table.
    stub_h_->Print(
        "static const ::tracing::v2::proto::ProtoFieldDescriptor* "
        "GetFieldDescriptor(uint32_t field_id);\n");

    std::string class_name = GetCppClassName(message);
    if (has_fields) {
      stub_cc_->Print(
          "static const ::tracing::v2::proto::ProtoFieldDescriptor "
          "kFields_$class$[] = {\n",
          "class", class_name);
      stub_cc_->Indent();
      for (int i = 0; i < message->field_count(); ++i) {
        const FieldDescriptor* field = message->field(i);
        std::string type_const =
            std::string("TYPE_") + FieldDescriptor::TypeName(field->type());
        UpperString(&type_const);
        stub_cc_->Print(
            "{\"$name$\", "
            "::tracing::v2::proto::ProtoFieldDescriptor::Type::$type$, "
            "$number$, $is_repeated$},\n",
            "name", field->name(),
            "type", type_const,
            "number", std::to_string(field->number()),
            "is_repeated", std::to_string(field->is_repeated()));
      }
      stub_cc_->Outdent();
      stub_cc_->Print("};\n\n");
    }

    // Fields reflection getter.
    stub_cc_->Print(
        "const ::tracing::v2::proto::ProtoFieldDescriptor* "
        "$class$::GetFieldDescriptor(uint32_t field_id) {\n",
        "class", class_name);
    stub_cc_->Indent();
    if (has_fields) {
      stub_cc_->Print("switch (field_id) {\n");
      stub_cc_->Indent();
      for (int i = 0; i < message->field_count(); ++i) {
        stub_cc_->Print(
            "case $field$:\n"
            "  return &kFields_$class$[$id$];\n",
            "class", class_name,
            "field", GetFieldNumberConstant(message->field(i)),
            "id", std::to_string(i));
      }
      stub_cc_->Print(
          "default:\n"
          "  return &kInvalidField;\n");
      stub_cc_->Outdent();
      stub_cc_->Print("}\n");
    } else {
      stub_cc_->Print("return &kInvalidField;\n");
    }
    stub_cc_->Outdent();
    stub_cc_->Print("}\n\n");
  }

  void GenerateMessageDescriptor(const Descriptor* message) {
    stub_h_->Print(
        "class $name$ : public ::tracing::v2::ProtoZeroMessage {\n"
        " public:\n",
        "name", GetCppClassName(message));
    stub_h_->Indent();

    GenerateReflectionForMessageFields(message);

    // Using statements for nested messages.
    for (int i = 0; i < message->nested_type_count(); ++i) {
      const Descriptor* nested_message = message->nested_type(i);
      stub_h_->Print(
          "using $local_name$ = $global_name$;\n",
          "local_name", nested_message->name(),
          "global_name", GetCppClassName(nested_message, true));
    }

    // Using statements for nested enums.
    for (int i = 0; i < message->enum_type_count(); ++i) {
      const EnumDescriptor* nested_enum = message->enum_type(i);
      stub_h_->Print(
          "using $local_name$ = $global_name$;\n",
          "local_name", nested_enum->name(),
          "global_name", GetCppClassName(nested_enum, true));
    }

    // Values of nested enums.
    for (int i = 0; i < message->enum_type_count(); ++i) {
      const EnumDescriptor* nested_enum = message->enum_type(i);
      std::string value_name_prefix = GetCppClassName(nested_enum) + "_";

      for (int j = 0; j < nested_enum->value_count(); ++j) {
        const EnumValueDescriptor* value = nested_enum->value(j);
        stub_h_->Print(
            "static const $class$ $name$ = $full_name$;\n",
            "class", nested_enum->name(),
            "name", value->name(),
            "full_name", value_name_prefix + value->name());
      }
    }

    // Field descriptors.
    for (int i = 0; i < message->field_count(); ++i) {
      const FieldDescriptor* field = message->field(i);
      if (field->is_packed()) {
        Abort("Packed repeated fields are not supported.");
        return;
      }
      if (field->type() != FieldDescriptor::TYPE_MESSAGE) {
        GenerateSimpleFieldDescriptor(field);
      } else {
        GenerateNestedMessageFieldDescriptor(field);
      }
    }

    stub_h_->Outdent();
    stub_h_->Print("};\n\n");
  }

  void GenerateEpilogue() {
    for (unsigned i = 0; i < namespaces_.size(); ++i) {
      stub_h_->Print("} // Namespace.\n");
      stub_cc_->Print("} // Namespace.\n");
    }
    stub_h_->Print("#endif  // Include guard.\n");
  }

  const FileDescriptor* const source_;
  Printer* const stub_h_;
  Printer* const stub_cc_;
  std::string error_;

  std::string package_;
  std::string wrapper_namespace_;
  std::vector<std::string> namespaces_;
  std::string full_namespace_prefix_;
  std::vector<const Descriptor*> messages_;
  std::vector<const EnumDescriptor*> enums_;

  std::set<const FileDescriptor*> public_imports_;
  std::set<const FileDescriptor*> private_imports_;
  std::set<const Descriptor*> referenced_messages_;
  std::set<const EnumDescriptor*> referenced_enums_;
};

}  // namespace

ProtoZeroGenerator::ProtoZeroGenerator() {
}

ProtoZeroGenerator::~ProtoZeroGenerator() {
}

bool ProtoZeroGenerator::Generate(const FileDescriptor* file,
                                  const std::string& options,
                                  GeneratorContext* context,
                                  std::string* error) const {

  const std::unique_ptr<ZeroCopyOutputStream> stub_h_file_stream(
      context->Open(ProtoStubName(file) + ".h"));
  const std::unique_ptr<ZeroCopyOutputStream> stub_cc_file_stream(
      context->Open(ProtoStubName(file) + ".cc"));

  // Variables are delimited by $.
  Printer stub_h_printer(stub_h_file_stream.get(), '$');
  Printer stub_cc_printer(stub_cc_file_stream.get(), '$');
  GeneratorJob job(file, &stub_h_printer, &stub_cc_printer);

  // Parse additional options.
  for (const std::string& option : Split(options, ",")) {
    std::vector<std::string> option_pair = Split(option, "=");
    job.SetOption(option_pair[0], option_pair[1]);
  }

  if (!job.GenerateStubs()) {
    *error = job.GetFirstError();
    return false;
  }
  return true;
}

}  // namespace proto
}  // namespace tracing
