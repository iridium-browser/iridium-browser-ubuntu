// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Apps/extensions explicitly whitelisted for use in public sessions.
const char* const kPublicSessionWhitelist[] = {
    // Public sessions in general:
    "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
    "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer
    "haiffjcadagjlijoggckpgfnoeiflnem",  // Citrix Receiver
    "mfaihdlpglflfgpfjcifdjdjcckigekc",  // ARC Runtime

    // Libraries:
    "aclofikceldphonlfmghmimkodjdmhck",  // Ancoris login component
    "eilbnahdgoddoedakcmfkcgfoegeloil",  // Ancoris proxy component
    "ceehlgckkmkaoggdnjhibffkphfnphmg",  // Libdata login
    "fnhgfoccpcjdnjcobejogdnlnidceemb",  // OverDrive

    // Retail mode:
    "bjfeaefhaooblkndnoabbkkkenknkemb",  // 500 px demo
    "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds demo
    "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled demo
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar demo
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "cdjikkcakjcdjemakobkmijmikhkegcj",  // Chrome Remote Desktop demo
    "jkoildpomkimndcphjpffmephmcmkfhn",  // Chromebook Demo App
    "lbhdhapagjhalobandnbdnmblnmocojh",  // Crackle demo
    "ielkookhdphmgbipcfmafkaiagademfp",  // Custom bookmarks
    "kogjlbfgggambihdjcpijgcbmenblimd",  // Custom bookmarks
    "ogbkmlkceflgpilgbmbcfbifckpkfacf",  // Custom bookmarks
    "pbbbjjecobhljkkcenlakfnkmkfkfamd",  // Custom bookmarks
    "jkbfjmnjcdmhlfpephomoiipbhcoiffb",  // Custom bookmarks
    "dgmblbpgafgcgpkoiilhjifindhinmai",  // Custom bookmarks
    "iggnealjakkgfofealilhkkclnbnfnmo",  // Custom bookmarks
    "lplkobnahgbopmpkdapaihnnojkphahc",  // Custom bookmarks
    "lejnflfhjpcannpaghnahbedlabpmhoh",  // Custom bookmarks
    "dhjmfhojkfjmfbnbnpichdmcdghdpccg",  // Cut the Rope demo
    "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer demo
    "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app demo
    "ekgadegabdkcbkodfbgidncffijbghhl",  // Duolingo demo
    "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote demo
    "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail demo
    "nldmakcnfaflagmohifhcihkfgcbmhph",  // Gmail offline demo
    "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive demo
    "dondgdlndnpianbklfnehgdhkickdjck",  // Google Keep demo
    "amfoiggnkefambnaaphodjdmdooiinna",  // Google Play Movie and TV demo
    "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+ demo
    "ifpkhncdnjfipfjlhfidljjffdgklanh",  // Google+ Photos demo
    "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app demo
    "ndlgnmfmgpdecjgehbcejboifbbmlkhp",  // Hash demo
    "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension demo
    "jckncghadoodfbbbmbpldacojkooophh",  // Journal demo
    "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle demo
    "idneggepppginmaklfbaniklagjghpio",  // Kingsroad demo
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app demo
    "kcjbmmhccecjokfmckhddpmghepcnidb",  // Mint demo
    "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app demo
    "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix demo
    "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times demo
    "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora demo
    "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr demo
    "ifnadhpngkodeccijnalokiabanejfgm",  // Pixsta demo
    "klcojgagjmpgmffcildkgbfmfffncpcd",  // Plex demo
    "nnikmgjhdlphciaonjmoppfckbpoinnb",  // Pocket demo
    "khldngaiohpnnoikfmnmfnebecgeobep",  // Polarr Photo demo
    "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice demo
    "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets demo
    "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides demo
    "ikmidginfdcbojdbmejkeakncgdbmonc",  // Soundtrap demo
    "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify demo
    "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app demo
    "onklhlmbpfnmgmelakhgehkfdmkpmekd",  // Todoist demo
    "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck demo
    "gnckahkflocidcgjbeheneogeflpjien",  // Vine demo
    "pdckcbpciaaicoomipamcabpdadhofgh",  // Weatherbug demo
    "biliocemfcghhioihldfdmkkhnofcgmb",  // Webcam Toy demo
    "bhfoghflalnnjfcfkaelngenjgjjhapk",  // Wevideo demo
    "pjckdjlmdcofkkkocnmhcbehkiapalho",  // Wunderlist demo
    "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube demo

    // Testing extensions:
    "ongnjlefhnoajpbodoldndkbkdgfomlp",  // Show Managed Storage
};

}  // namespace

DeviceLocalAccountManagementPolicyProvider::
    DeviceLocalAccountManagementPolicyProvider(
        policy::DeviceLocalAccount::Type account_type)
    : account_type_(account_type) {
}

DeviceLocalAccountManagementPolicyProvider::
    ~DeviceLocalAccountManagementPolicyProvider() {
}

std::string DeviceLocalAccountManagementPolicyProvider::
    GetDebugPolicyProviderName() const {
#if defined(NDEBUG)
  NOTREACHED();
  return std::string();
#else
  return "whitelist for device-local accounts";
#endif
}

bool DeviceLocalAccountManagementPolicyProvider::UserMayLoad(
    const extensions::Extension* extension,
    base::string16* error) const {
  if (account_type_ == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION) {
    // Allow extension if it is an externally hosted component of Chrome.
    if (extension->location() ==
        extensions::Manifest::EXTERNAL_COMPONENT) {
      return true;
    }

    // Allow extension if its type is whitelisted for use in public sessions.
    if (extension->GetType() == extensions::Manifest::TYPE_HOSTED_APP)
      return true;

    // Allow extension if its specific ID is whitelisted for use in public
    // sessions.
    for (size_t i = 0; i < arraysize(kPublicSessionWhitelist); ++i) {
      if (extension->id() == kPublicSessionWhitelist[i])
        return true;
    }
  } else if (account_type_ == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    // For single-app kiosk sessions, allow platform apps and shared modules.
    if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP ||
        extension->GetType() == extensions::Manifest::TYPE_SHARED_MODULE)
      return true;
  }

  // Disallow all other extensions.
  if (error) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_IN_DEVICE_LOCAL_ACCOUNT,
          base::UTF8ToUTF16(extension->name()),
          base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace chromeos
