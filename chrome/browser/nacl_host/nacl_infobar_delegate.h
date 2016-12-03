// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class NaClInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a NaCl infobar and delegate and adds the infobar to the infobar
  // service corresponding to the given render process and view IDs.
  static void Create(int render_process_id, int render_view_id);

 private:
  NaClInfoBarDelegate();
  ~NaClInfoBarDelegate() override;

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;

  DISALLOW_COPY_AND_ASSIGN(NaClInfoBarDelegate);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_INFOBAR_DELEGATE_H_
