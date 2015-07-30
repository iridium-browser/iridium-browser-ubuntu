// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_H_
#define COMPONENTS_USER_MANAGER_USER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "components/user_manager/user_id.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager_export.h"
#include "components/user_manager/user_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
class ChromeUserManagerImpl;
class FakeChromeUserManager;
class MockUserManager;
class SupervisedUserManagerImpl;
class UserAddingScreenTest;
class UserImageManagerImpl;
class UserSessionManager;
}

namespace user_manager {

class UserManagerBase;
class FakeUserManager;

// A class representing information about a previously logged in user.
// Each user has a canonical email (username), returned by |email()| and
// may have a different displayed email (in the raw form as entered by user),
// returned by |displayed_email()|.
// Displayed emails are for use in UI only, anywhere else users must be referred
// to by |email()|.
class USER_MANAGER_EXPORT User : public UserInfo {
 public:
  // User OAuth token status according to the last check.
  // Please note that enum values 1 and 2 were used for OAuth1 status and are
  // deprecated now.
  typedef enum {
    OAUTH_TOKEN_STATUS_UNKNOWN = 0,
    OAUTH2_TOKEN_STATUS_INVALID = 3,
    OAUTH2_TOKEN_STATUS_VALID = 4,
  } OAuthTokenStatus;

  // These special values are used instead of actual default image indices.
  typedef enum {
    USER_IMAGE_INVALID = -3,

    // Returned as |image_index| when user profile image is used as user image.
    USER_IMAGE_PROFILE = -2,

    // Returned as |image_index| when user-selected file or photo is used as
    // user image.
    USER_IMAGE_EXTERNAL = -1,
  } UserImageType;

  // This enum is used to define the buckets for an enumerated UMA histogram.
  // Hence,
  //   (a) existing enumerated constants should never be deleted or reordered,
  //   (b) new constants should only be appended at the end of the enumeration.
  enum WallpaperType {
    /* DAILY = 0 */    // Removed.
    CUSTOMIZED = 1,    // Selected by user.
    DEFAULT = 2,       // Default.
    /* UNKNOWN = 3 */  // Removed.
    ONLINE = 4,        // WallpaperInfo.location denotes an URL.
    POLICY = 5,        // Controlled by policy, can't be changed by the user.
    WALLPAPER_TYPE_COUNT = 6
  };

  // Returns true if user type has gaia account.
  static bool TypeHasGaiaAccount(UserType user_type);

  // Returns the user type.
  virtual UserType GetType() const = 0;

  // The email the user used to log in.
  const std::string& email() const { return email_; }

  // The displayed user name.
  base::string16 display_name() const { return display_name_; }

  // If the user has to use SAML to log in.
  bool using_saml() const { return using_saml_; }

  // UserInfo
  std::string GetEmail() const override;
  base::string16 GetDisplayName() const override;
  base::string16 GetGivenName() const override;
  const gfx::ImageSkia& GetImage() const override;
  UserID GetUserID() const override;

  // Allows managing child status of the user. Used for RegularUser.
  virtual void SetIsChild(bool is_child);

  // Returns true if user has gaia account. True for users of types
  // USER_TYPE_REGULAR and USER_TYPE_CHILD.
  virtual bool HasGaiaAccount() const;

  // Returns true if user is supervised.
  virtual bool IsSupervised() const;

  // Returns the account name part of the email. Use the display form of the
  // email if available and use_display_name == true. Otherwise use canonical.
  std::string GetAccountName(bool use_display_email) const;

  // Whether the user has a default image.
  bool HasDefaultImage() const;

  // True if user image can be synced.
  virtual bool CanSyncImage() const;

  int image_index() const { return image_index_; }
  bool has_raw_image() const { return user_image_.has_raw_image(); }
  // Returns raw representation of static user image.
  const UserImage::RawImage& raw_image() const {
    return user_image_.raw_image();
  }

  // Whether |raw_image| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool image_is_safe_format() const { return user_image_.is_safe_format(); }

  // Returns the URL of user image, if there is any. Currently only the profile
  // image has a URL, for other images empty URL is returned.
  GURL image_url() const { return user_image_.url(); }

  // True if user image is a stub (while real image is being loaded from file).
  bool image_is_stub() const { return image_is_stub_; }

  // True if image is being loaded from file.
  bool image_is_loading() const { return image_is_loading_; }

  // The displayed (non-canonical) user email.
  virtual std::string display_email() const;

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }

  // Whether online authentication against GAIA should be enforced during the
  // user's next sign-in.
  bool force_online_signin() const { return force_online_signin_; }

  // True if the user's session can be locked (i.e. the user has a password with
  // which to unlock the session).
  bool can_lock() const;

  // Returns empty string when home dir hasn't been mounted yet.
  std::string username_hash() const;

  // True if current user is logged in.
  bool is_logged_in() const;

  // True if current user is active within the current session.
  bool is_active() const;

  // True if the user Profile is created.
  bool is_profile_created() const { return profile_is_created_; }

 protected:
  friend class UserManagerBase;
  friend class chromeos::ChromeUserManagerImpl;
  friend class chromeos::SupervisedUserManagerImpl;
  friend class chromeos::UserImageManagerImpl;
  friend class chromeos::UserSessionManager;

  // For testing:
  friend class FakeUserManager;
  friend class chromeos::FakeChromeUserManager;
  friend class chromeos::MockUserManager;
  friend class chromeos::UserAddingScreenTest;

  // Do not allow anyone else to create new User instances.
  static User* CreateRegularUser(const UserID& email);
  static User* CreateGuestUser();
  static User* CreateKioskAppUser(const UserID& kiosk_app_username);
  static User* CreateSupervisedUser(const UserID& username);
  static User* CreatePublicAccountUser(const UserID& email);

  explicit User(const std::string& email);
  ~User() override;

  const std::string* GetAccountLocale() const { return account_locale_.get(); }

  // Setters are private so only UserManager can call them.
  void SetAccountLocale(const std::string& resolved_account_locale);

  void SetImage(const UserImage& user_image, int image_index);

  void SetImageURL(const GURL& image_url);

  // Sets a stub image until the next |SetImage| call. |image_index| may be
  // one of |USER_IMAGE_EXTERNAL| or |USER_IMAGE_PROFILE|.
  // If |is_loading| is |true|, that means user image is being loaded from file.
  void SetStubImage(const UserImage& stub_user_image,
                    int image_index,
                    bool is_loading);

  void set_display_name(const base::string16& display_name) {
    display_name_ = display_name;
  }

  void set_given_name(const base::string16& given_name) {
    given_name_ = given_name;
  }

  void set_display_email(const std::string& display_email) {
    display_email_ = display_email;
  }

  void set_using_saml(const bool using_saml) { using_saml_ = using_saml; }

  const UserImage& user_image() const { return user_image_; }

  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

  void set_force_online_signin(bool force_online_signin) {
    force_online_signin_ = force_online_signin;
  }

  void set_username_hash(const std::string& username_hash) {
    username_hash_ = username_hash;
  }

  void set_is_logged_in(bool is_logged_in) { is_logged_in_ = is_logged_in; }

  void set_can_lock(bool can_lock) { can_lock_ = can_lock; }

  void set_is_active(bool is_active) { is_active_ = is_active; }

  void set_profile_is_created() { profile_is_created_ = true; }

  // True if user has google account (not a guest or managed user).
  bool has_gaia_account() const;

 private:
  std::string email_;
  base::string16 display_name_;
  base::string16 given_name_;
  // The displayed user email, defaults to |email_|.
  std::string display_email_;
  bool using_saml_;
  UserImage user_image_;
  OAuthTokenStatus oauth_token_status_;
  bool force_online_signin_;

  // This is set to chromeos locale if account data has been downloaded.
  // (Or failed to download, but at least one download attempt finished).
  // An empty string indicates error in data load, or in
  // translation of Account locale to chromeos locale.
  scoped_ptr<std::string> account_locale_;

  // Used to identify homedir mount point.
  std::string username_hash_;

  // Either index of a default image for the user, |USER_IMAGE_EXTERNAL| or
  // |USER_IMAGE_PROFILE|.
  int image_index_;

  // True if current user image is a stub set by a |SetStubImage| call.
  bool image_is_stub_;

  // True if current user image is being loaded from file.
  bool image_is_loading_;

  // True if user is able to lock screen.
  bool can_lock_;

  // True if user is currently logged in in current session.
  bool is_logged_in_;

  // True if user is currently logged in and active in current session.
  bool is_active_;

  // True if user Profile is created
  bool profile_is_created_;

  DISALLOW_COPY_AND_ASSIGN(User);
};

// List of known users.
typedef std::vector<User*> UserList;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_H_
