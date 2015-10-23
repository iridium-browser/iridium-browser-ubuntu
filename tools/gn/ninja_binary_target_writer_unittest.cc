// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

TEST(NinjaBinaryTargetWriter, SourceSet) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::WIN);

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::SOURCE_SET);
  target.visibility().SetPublic();
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));
  // Also test object files, which should be just passed through to the
  // dependents to link.
  target.sources().push_back(SourceFile("//foo/input3.o"));
  target.sources().push_back(SourceFile("//foo/input4.obj"));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  // Source set itself.
  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        "cflags_cc =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = bar\n"
        "\n"
        "build obj/foo/bar.input1.o: cxx ../../foo/input1.cc\n"
        "build obj/foo/bar.input2.o: cxx ../../foo/input2.cc\n"
        "\n"
        "build obj/foo/bar.stamp: stamp obj/foo/bar.input1.o "
            "obj/foo/bar.input2.o ../../foo/input3.o ../../foo/input4.obj\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }

  // A shared library that depends on the source set.
  Target shlib_target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  shlib_target.set_output_type(Target::SHARED_LIBRARY);
  shlib_target.public_deps().push_back(LabelTargetPair(&target));
  shlib_target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(shlib_target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&shlib_target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = libshlib\n"
        "\n"
        "\n"
        // Ordering of the obj files here should come out in the order
        // specified, with the target's first, followed by the source set's, in
        // order.
        "build ./libshlib.so: solink obj/foo/bar.input1.o "
            "obj/foo/bar.input2.o ../../foo/input3.o ../../foo/input4.obj "
            "|| obj/foo/bar.stamp\n"
        "  ldflags =\n"
        "  libs =\n"
        "  output_extension = .so\n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }

  // A static library that depends on the source set (should not link it).
  Target stlib_target(setup.settings(), Label(SourceDir("//foo/"), "stlib"));
  stlib_target.set_output_type(Target::STATIC_LIBRARY);
  stlib_target.public_deps().push_back(LabelTargetPair(&target));
  stlib_target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(stlib_target.OnResolved(&err));

  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&stlib_target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = libstlib\n"
        "\n"
        "\n"
        // There are no sources so there are no params to alink. (In practice
        // this will probably fail in the archive tool.)
        "build obj/foo/libstlib.a: alink || obj/foo/bar.stamp\n"
        "  ldflags =\n"
        "  libs =\n"
        "  output_extension = \n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }

  // Make the static library 'complete', which means it should be linked.
  stlib_target.set_complete_static_lib(true);
  {
    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&stlib_target, out);
    writer.Run();

    const char expected[] =
        "defines =\n"
        "include_dirs =\n"
        "root_out_dir = .\n"
        "target_out_dir = obj/foo\n"
        "target_output_name = libstlib\n"
        "\n"
        "\n"
        // Ordering of the obj files here should come out in the order
        // specified, with the target's first, followed by the source set's, in
        // order.
        "build obj/foo/libstlib.a: alink obj/foo/bar.input1.o "
            "obj/foo/bar.input2.o ../../foo/input3.o ../../foo/input4.obj "
            "|| obj/foo/bar.stamp\n"
        "  ldflags =\n"
        "  libs =\n"
        "  output_extension = \n";
    std::string out_str = out.str();
    EXPECT_EQ(expected, out_str);
  }
}

// This tests that output extension overrides apply, and input dependencies
// are applied.
TEST(NinjaBinaryTargetWriter, ProductExtensionAndInputDeps) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::LINUX);

  // An action for our library to depend on.
  Target action(setup.settings(), Label(SourceDir("//foo/"), "action"));
  action.set_output_type(Target::ACTION_FOREACH);
  action.visibility().SetPublic();
  action.SetToolchain(setup.toolchain());
  ASSERT_TRUE(action.OnResolved(&err));

  // A shared library w/ the product_extension set to a custom value.
  Target target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  target.set_output_type(Target::SHARED_LIBRARY);
  target.set_output_extension(std::string("so.6"));
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));
  target.public_deps().push_back(LabelTargetPair(&action));
  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_cc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = libshlib\n"
      "\n"
      "build obj/foo/shlib.inputdeps.stamp: stamp obj/foo/action.stamp\n"
      "build obj/foo/libshlib.input1.o: cxx ../../foo/input1.cc"
        " || obj/foo/shlib.inputdeps.stamp\n"
      "build obj/foo/libshlib.input2.o: cxx ../../foo/input2.cc"
        " || obj/foo/shlib.inputdeps.stamp\n"
      "\n"
      "build ./libshlib.so.6: solink obj/foo/libshlib.input1.o "
      // The order-only dependency here is stricly unnecessary since the
      // sources list this as an order-only dep. See discussion in the code
      // that writes this.
          "obj/foo/libshlib.input2.o || obj/foo/action.stamp\n"
      "  ldflags =\n"
      "  libs =\n"
      "  output_extension = .so.6\n";

  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

TEST(NinjaBinaryTargetWriter, EmptyProductExtension) {
  TestWithScope setup;
  Err err;

  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::LINUX);

  // This test is the same as ProductExtension, except that
  // we call set_output_extension("") and ensure that we still get the default.
  Target target(setup.settings(), Label(SourceDir("//foo/"), "shlib"));
  target.set_output_type(Target::SHARED_LIBRARY);
  target.set_output_extension(std::string());
  target.sources().push_back(SourceFile("//foo/input1.cc"));
  target.sources().push_back(SourceFile("//foo/input2.cc"));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err));

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_cc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = libshlib\n"
      "\n"
      "build obj/foo/libshlib.input1.o: cxx ../../foo/input1.cc\n"
      "build obj/foo/libshlib.input2.o: cxx ../../foo/input2.cc\n"
      "\n"
      "build ./libshlib.so: solink obj/foo/libshlib.input1.o "
          "obj/foo/libshlib.input2.o\n"
      "  ldflags =\n"
      "  libs =\n"
      "  output_extension = .so\n";

  std::string out_str = out.str();
  EXPECT_EQ(expected, out_str);
}

TEST(NinjaBinaryTargetWriter, SourceSetDataDeps) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::LINUX);

  Err err;

  // This target is a data (runtime) dependency of the intermediate target.
  Target data(setup.settings(), Label(SourceDir("//foo/"), "data_target"));
  data.set_output_type(Target::EXECUTABLE);
  data.visibility().SetPublic();
  data.SetToolchain(setup.toolchain());
  ASSERT_TRUE(data.OnResolved(&err));

  // Intermediate source set target.
  Target inter(setup.settings(), Label(SourceDir("//foo/"), "inter"));
  inter.set_output_type(Target::SOURCE_SET);
  inter.visibility().SetPublic();
  inter.data_deps().push_back(LabelTargetPair(&data));
  inter.SetToolchain(setup.toolchain());
  inter.sources().push_back(SourceFile("//foo/inter.cc"));
  ASSERT_TRUE(inter.OnResolved(&err)) << err.message();

  // Write out the intermediate target.
  std::ostringstream inter_out;
  NinjaBinaryTargetWriter inter_writer(&inter, inter_out);
  inter_writer.Run();

  // The intermediate source set will be a stamp file that depends on the
  // object files, and will have an order-only dependency on its data dep and
  // data file.
  const char inter_expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_cc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = inter\n"
      "\n"
      "build obj/foo/inter.inter.o: cxx ../../foo/inter.cc\n"
      "\n"
      "build obj/foo/inter.stamp: stamp obj/foo/inter.inter.o || "
          "./data_target\n";
  EXPECT_EQ(inter_expected, inter_out.str());

  // Final target.
  Target exe(setup.settings(), Label(SourceDir("//foo/"), "exe"));
  exe.set_output_type(Target::EXECUTABLE);
  exe.public_deps().push_back(LabelTargetPair(&inter));
  exe.SetToolchain(setup.toolchain());
  exe.sources().push_back(SourceFile("//foo/final.cc"));
  ASSERT_TRUE(exe.OnResolved(&err));

  std::ostringstream final_out;
  NinjaBinaryTargetWriter final_writer(&exe, final_out);
  final_writer.Run();

  // The final output depends on both object files (one from the final target,
  // one from the source set) and has an order-only dependency on the source
  // set's stamp file and the final target's data file. The source set stamp
  // dependency will create an implicit order-only dependency on the data
  // target.
  const char final_expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_cc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = exe\n"
      "\n"
      "build obj/foo/exe.final.o: cxx ../../foo/final.cc\n"
      "\n"
      "build ./exe: link obj/foo/exe.final.o obj/foo/inter.inter.o || "
          "obj/foo/inter.stamp\n"
      "  ldflags =\n"
      "  libs =\n"
      "  output_extension = \n";
  EXPECT_EQ(final_expected, final_out.str());
}

TEST(NinjaBinaryTargetWriter, SharedLibraryModuleDefinitionFile) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.settings()->set_target_os(Settings::WIN);

  Target shared_lib(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  shared_lib.set_output_type(Target::SHARED_LIBRARY);
  shared_lib.SetToolchain(setup.toolchain());
  shared_lib.sources().push_back(SourceFile("//foo/sources.cc"));
  shared_lib.sources().push_back(SourceFile("//foo/bar.def"));

  Err err;
  ASSERT_TRUE(shared_lib.OnResolved(&err));

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&shared_lib, out);
  writer.Run();

  const char expected[] =
      "defines =\n"
      "include_dirs =\n"
      "cflags =\n"
      "cflags_cc =\n"
      "root_out_dir = .\n"
      "target_out_dir = obj/foo\n"
      "target_output_name = libbar\n"
      "\n"
      "build obj/foo/libbar.sources.o: cxx ../../foo/sources.cc\n"
      "\n"
      "build ./libbar.so: solink obj/foo/libbar.sources.o | ../../foo/bar.def\n"
      "  ldflags = /DEF:../../foo/bar.def\n"
      "  libs =\n"
      "  output_extension = .so\n";
  EXPECT_EQ(expected, out.str());
}

TEST(NinjaBinaryTargetWriter, WinPrecompiledHeaders) {
  Err err;

  // This setup's toolchain does not have precompiled headers defined.
  TestWithScope setup;

  // A precompiled header toolchain.
  Settings pch_settings(setup.build_settings(), "withpch/");
  Toolchain pch_toolchain(&pch_settings,
                          Label(SourceDir("//toolchain/"), "withpch"));

  // Declare a C++ compiler that supports PCH.
  scoped_ptr<Tool> cxx_tool(new Tool);
  TestWithScope::SetCommandForTool(
      "c++ {{source}} {{cflags}} {{cflags_cc}} {{defines}} {{include_dirs}} "
      "-o {{output}}",
      cxx_tool.get());
  cxx_tool->set_outputs(SubstitutionList::MakeForTest(
      "{{source_out_dir}}/{{target_output_name}}.{{source_name_part}}.o"));
  cxx_tool->set_precompiled_header_type(Tool::PCH_MSVC);
  pch_toolchain.SetTool(Toolchain::TYPE_CXX, cxx_tool.Pass());
  pch_toolchain.ToolchainSetupComplete();

  // This target doesn't specify precompiled headers.
  {
    Target no_pch_target(&pch_settings,
                         Label(SourceDir("//foo/"), "no_pch_target"));
    no_pch_target.set_output_type(Target::SOURCE_SET);
    no_pch_target.visibility().SetPublic();
    no_pch_target.sources().push_back(SourceFile("//foo/input1.cc"));
    no_pch_target.SetToolchain(&pch_toolchain);
    ASSERT_TRUE(no_pch_target.OnResolved(&err));

    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&no_pch_target, out);
    writer.Run();

    const char no_pch_expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        "cflags_cc =\n"
        "target_output_name = no_pch_target\n"
        "\n"
        "build withpch/obj/foo/no_pch_target.input1.o: "
               "cxx ../../foo/input1.cc\n"
        "\n"
        "build withpch/obj/foo/no_pch_target.stamp: "
               "stamp withpch/obj/foo/no_pch_target.input1.o\n";
    EXPECT_EQ(no_pch_expected, out.str());
  }

  // This target specifies PCH.
  {
    Target pch_target(&pch_settings,
                      Label(SourceDir("//foo/"), "pch_target"));
    pch_target.config_values().set_precompiled_header("build/precompile.h");
    pch_target.config_values().set_precompiled_source(
        SourceFile("//build/precompile.cc"));
    pch_target.set_output_type(Target::SOURCE_SET);
    pch_target.visibility().SetPublic();
    pch_target.sources().push_back(SourceFile("//foo/input1.cc"));
    pch_target.SetToolchain(&pch_toolchain);
    ASSERT_TRUE(pch_target.OnResolved(&err));

    std::ostringstream out;
    NinjaBinaryTargetWriter writer(&pch_target, out);
    writer.Run();

    const char pch_win_expected[] =
        "defines =\n"
        "include_dirs =\n"
        "cflags =\n"
        // There should only be one .pch file created, for C++ files.
        "cflags_cc = /Fpwithpch/obj/foo/pch_target_cc.pch "
                     "/Yubuild/precompile.h\n"
        "target_output_name = pch_target\n"
        "\n"
        // Compile the precompiled source file with /Yc.
        "build withpch/obj/build/pch_target.precompile.cc.o: "
               "cxx ../../build/precompile.cc\n"
        "  cflags_cc = ${cflags_cc} /Ycbuild/precompile.h\n"
        "\n"
        "build withpch/obj/foo/pch_target.input1.o: "
               "cxx ../../foo/input1.cc | "
               // Explicit dependency on the PCH build step.
               "withpch/obj/build/pch_target.precompile.cc.o\n"
        "\n"
        "build withpch/obj/foo/pch_target.stamp: "
               "stamp withpch/obj/foo/pch_target.input1.o "
               // The precompiled object file was added to the outputs.
               "withpch/obj/build/pch_target.precompile.cc.o\n";
    EXPECT_EQ(pch_win_expected, out.str());
  }
}

// Should throw an error with the scheduler if a duplicate object file exists.
// This is dependent on the toolchain's object file mapping.
TEST(NinjaBinaryTargetWriter, DupeObjFileError) {
  Scheduler scheduler;

  TestWithScope setup;
  TestTarget target(setup, "//foo:bar", Target::EXECUTABLE);
  target.sources().push_back(SourceFile("//a.cc"));
  target.sources().push_back(SourceFile("//a.cc"));

  EXPECT_FALSE(scheduler.is_failed());

  std::ostringstream out;
  NinjaBinaryTargetWriter writer(&target, out);
  writer.Run();

  // Should have issued an error.
  EXPECT_TRUE(scheduler.is_failed());
}
