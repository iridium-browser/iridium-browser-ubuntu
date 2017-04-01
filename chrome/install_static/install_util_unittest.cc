// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include <tuple>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome_elf/nt_registry/nt_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace install_static {

// Tests the MatchPattern function in the install_static library.
TEST(InstallStaticTest, MatchPattern) {
  EXPECT_TRUE(MatchPattern(L"", L""));
  EXPECT_TRUE(MatchPattern(L"", L"*"));
  EXPECT_FALSE(MatchPattern(L"", L"*a"));
  EXPECT_FALSE(MatchPattern(L"", L"abc"));
  EXPECT_TRUE(MatchPattern(L"Hello1234", L"He??o*1*"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*?"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*"));
  EXPECT_FALSE(MatchPattern(L"Foo", L"F*b"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*c*d"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*?c*d"));
  EXPECT_FALSE(MatchPattern(L"abcd", L"abcd*efgh"));
  EXPECT_TRUE(MatchPattern(L"foobarabc", L"*bar*"));
}

// Tests the install_static::GetSwitchValueFromCommandLine function.
TEST(InstallStaticTest, GetSwitchValueFromCommandLineTest) {
  // Simple case with one switch.
  std::wstring value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=bar", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar  --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar \t\t\t --abc=def bleh", L"abc");
  EXPECT_EQ(L"def", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --foo=bar  --abc=def bleh", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe", L"type");
  EXPECT_EQ(L"", value);

  // Non existent switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe type=bar", L"type");
  EXPECT_EQ(L"", value);

  // Trailing spaces after the switch.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t", L"type");
  EXPECT_EQ(L"bar", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      L"c:\\temp\\bleh.exe --type=bar      \t\t --foo=bleh", L"foo");
  EXPECT_EQ(L"bleh", value);

  // Nothing after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=", L"type");
  EXPECT_TRUE(value.empty());

  // Whitespace after a switch.
  value =
      GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type= ", L"type");
  EXPECT_TRUE(value.empty());

  // Just tabs after a switch.
  value = GetSwitchValueFromCommandLine(L"c:\\temp\\bleh.exe --type=\t\t\t",
                                        L"type");
  EXPECT_TRUE(value.empty());
}

TEST(InstallStaticTest, SpacesAndQuotesInCommandLineArguments) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\"");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"x.exe");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"x.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"\"c:\\with space\\something.exe\"");
  ASSERT_EQ(1u, tokenized.size());
  EXPECT_EQ(L"c:\\with space\\something.exe", tokenized[0]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" arg");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"arg", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\with space\\b.exe\" \"arg\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"arg", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" c:\\tmp\\");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\tmp\\", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"\"C:\\a\\b.exe\" \"c:\\some file path\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\a\\b.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\some file path\"", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \\\\x\\\\ \\\\y\\\\");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\\x\\\\", tokenized[1]);
  EXPECT_EQ(L"\\\\y\\\\", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \"\\\\space quoted\\\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\\space quoted\\", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" --stuff    -x -Y   \"c:\\some thing\\\"    "
      L"weewaa    ");
  ASSERT_EQ(5u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"--stuff", tokenized[1]);
  EXPECT_EQ(L"-x", tokenized[2]);
  EXPECT_EQ(L"-Y", tokenized[3]);
  EXPECT_EQ(L"c:\\some thing\"    weewaa    ", tokenized[4]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" --stuff=\"d:\\stuff and things\"");
  EXPECT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"--stuff=d:\\stuff and things", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"\"C:\\with space\\b.exe\" \\\\\\\"\"");
  EXPECT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"C:\\with space\\b.exe", tokenized[0]);
  EXPECT_EQ(L"\\\"", tokenized[1]);
}

// Test cases from
// https://blogs.msdn.microsoft.com/oldnewthing/20100917-00/?p=12833.
TEST(InstallStaticTest, SpacesAndQuotesOldNewThing) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(L"program.exe \"hello there.txt\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello there.txt", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"C:\\Hello there.txt\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"C:\\Hello there.txt", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"hello\\\"there\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello\"there", tokenized[1]);

  tokenized =
      TokenizeCommandLineToArray(L"program.exe \"hello\\\\\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"program.exe", tokenized[0]);
  EXPECT_EQ(L"hello\\", tokenized[1]);
}

// Test cases from
// http://www.windowsinspired.com/how-a-windows-programs-splits-its-command-line-into-individual-arguments/.
// These are mostly about the special handling of argv[0], which uses different
// quoting than the rest of the arguments.
TEST(InstallStaticTest, SpacesAndQuotesWindowsInspired) {
  std::vector<std::wstring> tokenized;

  tokenized = TokenizeCommandLineToArray(
      L"\"They said \"you can't do this!\", didn't they?\"");
  ASSERT_EQ(5u, tokenized.size());
  EXPECT_EQ(L"They said ", tokenized[0]);
  EXPECT_EQ(L"you", tokenized[1]);
  EXPECT_EQ(L"can't", tokenized[2]);
  EXPECT_EQ(L"do", tokenized[3]);
  EXPECT_EQ(L"this!, didn't they?", tokenized[4]);

  tokenized = TokenizeCommandLineToArray(
      L"test.exe \"c:\\Path With Spaces\\Ending In Backslash\\\" Arg2 Arg3");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"test.exe", tokenized[0]);
  EXPECT_EQ(L"c:\\Path With Spaces\\Ending In Backslash\" Arg2 Arg3",
            tokenized[1]);

  tokenized = TokenizeCommandLineToArray(
      L"FinalProgram.exe \"first second \"\"embedded quote\"\" third\"");
  ASSERT_EQ(4u, tokenized.size());
  EXPECT_EQ(L"FinalProgram.exe", tokenized[0]);
  EXPECT_EQ(L"first second \"embedded", tokenized[1]);
  EXPECT_EQ(L"quote", tokenized[2]);
  EXPECT_EQ(L"third", tokenized[3]);

  tokenized = TokenizeCommandLineToArray(
      L"\"F\"i\"r\"s\"t S\"e\"c\"o\"n\"d\" T\"h\"i\"r\"d\"");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"F", tokenized[0]);
  EXPECT_EQ(L"irst Second Third", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"F\"\"ir\"s\"\"t \\\"Second Third\"");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"F\"\"ir\"s\"\"t", tokenized[0]);
  EXPECT_EQ(L"\"Second", tokenized[1]);
  EXPECT_EQ(L"Third", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L"  Something Else");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"", tokenized[0]);
  EXPECT_EQ(L"Something", tokenized[1]);
  EXPECT_EQ(L"Else", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L" Something Else");
  ASSERT_EQ(3u, tokenized.size());
  EXPECT_EQ(L"", tokenized[0]);
  EXPECT_EQ(L"Something", tokenized[1]);
  EXPECT_EQ(L"Else", tokenized[2]);

  tokenized = TokenizeCommandLineToArray(L"\"123 456\tabc\\def\"ghi");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"123 456\tabc\\def", tokenized[0]);
  EXPECT_EQ(L"ghi", tokenized[1]);

  tokenized = TokenizeCommandLineToArray(L"123\"456\"\tabc");
  ASSERT_EQ(2u, tokenized.size());
  EXPECT_EQ(L"123\"456\"", tokenized[0]);
  EXPECT_EQ(L"abc", tokenized[1]);
}

TEST(InstallStaticTest, BrowserProcessTest) {
  EXPECT_EQ(ProcessType::UNINITIALIZED, g_process_type);
  InitializeProcessType();
  EXPECT_FALSE(IsNonBrowserProcess());
}

class InstallStaticUtilTest
    : public ::testing::TestWithParam<
          std::tuple<InstallConstantIndex, const char*>> {
 protected:
  InstallStaticUtilTest() {
    InstallConstantIndex mode_index;
    const char* level;

    std::tie(mode_index, level) = GetParam();

    mode_ = &kInstallModes[mode_index];
    system_level_ = std::string(level) != "user";
    EXPECT_TRUE(!system_level_ || mode_->supports_system_level);
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    nt_root_key_ = system_level_ ? nt::HKLM : nt::HKCU;

    std::unique_ptr<PrimaryInstallDetails> details =
        base::MakeUnique<PrimaryInstallDetails>();
    details->set_mode(mode_);
    details->set_channel(mode_->default_channel_name);
    details->set_system_level(system_level_);
    InstallDetails::SetForProcess(std::move(details));

    base::string16 path;
    override_manager_.OverrideRegistry(root_key_, &path);
    nt::SetTestingOverride(nt_root_key_, path);
  }

  ~InstallStaticUtilTest() {
    InstallDetails::SetForProcess(nullptr);
    nt::SetTestingOverride(nt_root_key_, base::string16());
  }

  bool system_level() const { return system_level_; }

  const wchar_t* default_channel() const { return mode_->default_channel_name; }

  void SetUsageStat(DWORD value, bool medium) {
    ASSERT_TRUE(!medium || system_level_);
    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, GetUsageStatsKeyPath(medium).c_str(),
                                KEY_SET_VALUE | KEY_WOW64_32KEY)
                  .WriteValue(L"usagestats", value));
  }

  void SetMetricsReportingPolicy(DWORD value) {
#if defined(GOOGLE_CHROME_BUILD)
    static constexpr wchar_t kPolicyKey[] =
        L"Software\\Policies\\Google\\Chrome";
#else
    static constexpr wchar_t kPolicyKey[] = L"Software\\Policies\\Chromium";
#endif

    ASSERT_EQ(ERROR_SUCCESS,
              base::win::RegKey(root_key_, kPolicyKey, KEY_SET_VALUE)
                  .WriteValue(L"MetricsReportingEnabled", value));
  }

 private:
  // Returns the registry path for the key holding the product's usagestats
  // value. |medium| = true returns the path for ClientStateMedium.
  std::wstring GetUsageStatsKeyPath(bool medium) {
    EXPECT_TRUE(!medium || system_level_);

    std::wstring result(L"Software\\");
    if (kUseGoogleUpdateIntegration) {
      result.append(L"Google\\Update\\ClientState");
      if (medium)
        result.append(L"Medium");
      result.push_back(L'\\');
      result.append(mode_->app_guid);
    } else {
      result.append(kProductPathName);
    }
    return result;
  }

  registry_util::RegistryOverrideManager override_manager_;
  HKEY root_key_ = nullptr;
  nt::ROOT_KEY nt_root_key_ = nt::AUTO;
  const InstallConstants* mode_ = nullptr;
  bool system_level_ = false;

  DISALLOW_COPY_AND_ASSIGN(InstallStaticUtilTest);
};

TEST_P(InstallStaticUtilTest, UsageStatsAbsent) {
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZero) {
  SetUsageStat(0, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsZeroMedium) {
  if (!system_level())
    return;
  SetUsageStat(0, true);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOne) {
  SetUsageStat(1, false);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, UsageStatsOneMedium) {
  if (!system_level())
    return;
  SetUsageStat(1, true);
  EXPECT_TRUE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, ReportingIsEnforcedByPolicy) {
  bool reporting_enabled = false;
  EXPECT_FALSE(ReportingIsEnforcedByPolicy(&reporting_enabled));

  SetMetricsReportingPolicy(0);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_FALSE(reporting_enabled);

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(ReportingIsEnforcedByPolicy(&reporting_enabled));
  EXPECT_TRUE(reporting_enabled);
}

TEST_P(InstallStaticUtilTest, UsageStatsPolicy) {
  // Policy alone.
  SetMetricsReportingPolicy(0);
  EXPECT_FALSE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(1);
  EXPECT_TRUE(GetCollectStatsConsent());

  // Policy trumps usagestats.
  SetMetricsReportingPolicy(1);
  SetUsageStat(0, false);
  EXPECT_TRUE(GetCollectStatsConsent());

  SetMetricsReportingPolicy(0);
  SetUsageStat(1, false);
  EXPECT_FALSE(GetCollectStatsConsent());
}

TEST_P(InstallStaticUtilTest, GetChromeChannelName) {
  EXPECT_EQ(default_channel(), GetChromeChannelName());
}

#if defined(GOOGLE_CHROME_BUILD)
// Stable supports user and system levels.
INSTANTIATE_TEST_CASE_P(Stable,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(STABLE_INDEX),
                                         testing::Values("user", "system")));
// Canary is only at user level.
INSTANTIATE_TEST_CASE_P(Canary,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(CANARY_INDEX),
                                         testing::Values("user")));
#else   // GOOGLE_CHROME_BUILD
// Chromium supports user and system levels.
INSTANTIATE_TEST_CASE_P(Chromium,
                        InstallStaticUtilTest,
                        testing::Combine(testing::Values(CHROMIUM_INDEX),
                                         testing::Values("user", "system")));
#endif  // !GOOGLE_CHROME_BUILD

}  // namespace install_static
