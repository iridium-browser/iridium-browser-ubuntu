// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/runtime_deps.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scope.h"

namespace {

void InitTargetWithType(TestWithScope& setup,
                        Target* target,
                        Target::OutputType type) {
  target->set_output_type(type);
  target->visibility().SetPublic();
  target->SetToolchain(setup.toolchain());
}

// Convenience function to make the correct kind of pair.
std::pair<OutputFile, const Target*> MakePair(const char* str,
                                              const Target* t) {
  return std::pair<OutputFile, const Target*>(OutputFile(str), t);
}

std::string GetVectorDescription(
    const std::vector<std::pair<OutputFile, const Target*>>& v) {
  std::string result;
  for (size_t i = 0; i < v.size(); i++) {
    if (i != 0)
      result.append(", ");
    result.append("\"" + v[i].first.value() + "\"");
  }
  return result;
}

}  // namespace

// Tests an exe depending on different types of libraries.
TEST(RuntimeDeps, Libs) {
  TestWithScope setup;
  Err err;

  // Dependency hierarchy: main(exe) -> stat
  //                                 -> shared
  //                                 -> set

  Target stat(setup.settings(), Label(SourceDir("//"), "stat"));
  InitTargetWithType(setup, &stat, Target::STATIC_LIBRARY);
  stat.data().push_back("//stat.dat");
  ASSERT_TRUE(stat.OnResolved(&err));

  Target shared(setup.settings(), Label(SourceDir("//"), "shared"));
  InitTargetWithType(setup, &shared, Target::SHARED_LIBRARY);
  shared.data().push_back("//shared.dat");
  ASSERT_TRUE(shared.OnResolved(&err));

  Target set(setup.settings(), Label(SourceDir("//"), "set"));
  InitTargetWithType(setup, &set, Target::SOURCE_SET);
  set.data().push_back("//set.dat");
  ASSERT_TRUE(set.OnResolved(&err));

  Target main(setup.settings(), Label(SourceDir("//"), "main"));
  InitTargetWithType(setup, &main, Target::EXECUTABLE);
  main.private_deps().push_back(LabelTargetPair(&stat));
  main.private_deps().push_back(LabelTargetPair(&shared));
  main.private_deps().push_back(LabelTargetPair(&set));
  main.data().push_back("//main.dat");
  ASSERT_TRUE(main.OnResolved(&err));

  std::vector<std::pair<OutputFile, const Target*>> result =
      ComputeRuntimeDeps(&main);

  // The result should have deps of main, all 4 dat files, and libshared.so
  ASSERT_EQ(6u, result.size()) << GetVectorDescription(result);

  // The first one should always be the main exe.
  EXPECT_TRUE(MakePair("./main", &main) == result[0]);

  // The rest of the ordering is undefined. First the data files.
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../stat.dat", &stat)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../shared.dat", &shared)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../set.dat", &set)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../main.dat", &main)) !=
              result.end()) << GetVectorDescription(result);

  // Check the static library
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("./libshared.so", &shared)) !=
              result.end()) << GetVectorDescription(result);
}

// Tests that executables that aren't listed as data deps aren't included in
// the output, but executables that are data deps are included.
TEST(RuntimeDeps, ExeDataDep) {
  TestWithScope setup;
  Err err;

  // Dependency hierarchy: main(exe) -> datadep(exe) -> final_in(source set)
  //                                 -> dep(exe) -> final_out(source set)
  // The final_in/out targets each have data files. final_in's should be
  // included, final_out's should not be.

  Target final_in(setup.settings(), Label(SourceDir("//"), "final_in"));
  InitTargetWithType(setup, &final_in, Target::SOURCE_SET);
  final_in.data().push_back("//final_in.dat");
  ASSERT_TRUE(final_in.OnResolved(&err));

  Target datadep(setup.settings(), Label(SourceDir("//"), "datadep"));
  InitTargetWithType(setup, &datadep, Target::EXECUTABLE);
  datadep.private_deps().push_back(LabelTargetPair(&final_in));
  ASSERT_TRUE(datadep.OnResolved(&err));

  Target final_out(setup.settings(), Label(SourceDir("//"), "final_out"));
  InitTargetWithType(setup, &final_out, Target::SOURCE_SET);
  final_out.data().push_back("//final_out.dat");
  ASSERT_TRUE(final_out.OnResolved(&err));

  Target dep(setup.settings(), Label(SourceDir("//"), "dep"));
  InitTargetWithType(setup, &dep, Target::EXECUTABLE);
  dep.private_deps().push_back(LabelTargetPair(&final_out));
  ASSERT_TRUE(dep.OnResolved(&err));

  Target main(setup.settings(), Label(SourceDir("//"), "main"));
  InitTargetWithType(setup, &main, Target::EXECUTABLE);
  main.private_deps().push_back(LabelTargetPair(&dep));
  main.data_deps().push_back(LabelTargetPair(&datadep));
  ASSERT_TRUE(main.OnResolved(&err));

  std::vector<std::pair<OutputFile, const Target*>> result =
      ComputeRuntimeDeps(&main);

  // The result should have deps of main, datadep, final_in.dat
  ASSERT_EQ(3u, result.size()) << GetVectorDescription(result);

  // The first one should always be the main exe.
  EXPECT_TRUE(MakePair("./main", &main) == result[0]);

  // The rest of the ordering is undefined.
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("./datadep", &datadep)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../final_in.dat", &final_in)) !=
              result.end()) << GetVectorDescription(result);
}

// Tests that action and copy outputs are considered if they're data deps, but
// not if they're regular deps. Action and copy "data" files are always
// included.
TEST(RuntimeDeps, ActionOutputs) {
  TestWithScope setup;
  Err err;

  // Dependency hierarchy: main(exe) -> datadep (action)
  //                                 -> datadep_copy (copy)
  //                                 -> dep (action)
  //                                 -> dep_copy (copy)

  Target datadep(setup.settings(), Label(SourceDir("//"), "datadep"));
  InitTargetWithType(setup, &datadep, Target::ACTION);
  datadep.data().push_back("//datadep.data");
  datadep.action_values().outputs() =
      SubstitutionList::MakeForTest("//datadep.output");
  ASSERT_TRUE(datadep.OnResolved(&err));

  Target datadep_copy(setup.settings(), Label(SourceDir("//"), "datadep_copy"));
  InitTargetWithType(setup, &datadep_copy, Target::COPY_FILES);
  datadep_copy.sources().push_back(SourceFile("//input"));
  datadep_copy.data().push_back("//datadep_copy.data");
  datadep_copy.action_values().outputs() =
      SubstitutionList::MakeForTest("//datadep_copy.output");
  ASSERT_TRUE(datadep_copy.OnResolved(&err));

  Target dep(setup.settings(), Label(SourceDir("//"), "dep"));
  InitTargetWithType(setup, &dep, Target::ACTION);
  dep.data().push_back("//dep.data");
  dep.action_values().outputs() =
      SubstitutionList::MakeForTest("//dep.output");
  ASSERT_TRUE(dep.OnResolved(&err));

  Target dep_copy(setup.settings(), Label(SourceDir("//"), "dep_copy"));
  InitTargetWithType(setup, &dep_copy, Target::COPY_FILES);
  dep_copy.sources().push_back(SourceFile("//input"));
  dep_copy.data().push_back("//dep_copy/data/");  // Tests a directory.
  dep_copy.action_values().outputs() =
      SubstitutionList::MakeForTest("//dep_copy.output");
  ASSERT_TRUE(dep_copy.OnResolved(&err));

  Target main(setup.settings(), Label(SourceDir("//"), "main"));
  InitTargetWithType(setup, &main, Target::EXECUTABLE);
  main.private_deps().push_back(LabelTargetPair(&dep));
  main.private_deps().push_back(LabelTargetPair(&dep_copy));
  main.data_deps().push_back(LabelTargetPair(&datadep));
  main.data_deps().push_back(LabelTargetPair(&datadep_copy));
  ASSERT_TRUE(main.OnResolved(&err));

  std::vector<std::pair<OutputFile, const Target*>> result =
      ComputeRuntimeDeps(&main);

  // The result should have deps of main, both datadeps files, but only
  // the data file from dep.
  ASSERT_EQ(7u, result.size()) << GetVectorDescription(result);

  // The first one should always be the main exe.
  EXPECT_TRUE(MakePair("./main", &main) == result[0]);

  // The rest of the ordering is undefined.
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../datadep.data", &datadep)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../datadep_copy.data", &datadep_copy)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../datadep.output", &datadep)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../datadep_copy.output", &datadep_copy)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../dep.data", &dep)) !=
              result.end()) << GetVectorDescription(result);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../dep_copy/data/", &dep_copy)) !=
              result.end()) << GetVectorDescription(result);

  // Explicitly asking for the runtime deps of an action target only includes
  // the data and not all outputs.
  result = ComputeRuntimeDeps(&dep);
  ASSERT_EQ(1u, result.size());
  EXPECT_TRUE(MakePair("../../dep.data", &dep) == result[0]);
}

// Tests that a dependency duplicated in regular and data deps is processed
// as a data dep.
TEST(RuntimeDeps, Dupe) {
  TestWithScope setup;
  Err err;

  Target action(setup.settings(), Label(SourceDir("//"), "action"));
  InitTargetWithType(setup, &action, Target::ACTION);
  action.action_values().outputs() =
      SubstitutionList::MakeForTest("//action.output");
  ASSERT_TRUE(action.OnResolved(&err));

  Target target(setup.settings(), Label(SourceDir("//"), "foo"));
  InitTargetWithType(setup, &target, Target::EXECUTABLE);
  target.private_deps().push_back(LabelTargetPair(&action));
  target.data_deps().push_back(LabelTargetPair(&action));
  ASSERT_TRUE(target.OnResolved(&err));

  // The results should be the executable and the copy output.
  std::vector<std::pair<OutputFile, const Target*>> result =
      ComputeRuntimeDeps(&target);
  EXPECT_TRUE(std::find(result.begin(), result.end(),
                        MakePair("../../action.output", &action)) !=
              result.end()) << GetVectorDescription(result);
}
