// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CENC_UTILS_H_
#define MEDIA_CDM_CENC_UTILS_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"
#include "media/cdm/json_web_key.h"

namespace media {

// Validate that |input| is a set of concatenated 'pssh' boxes and the sizes
// match. Returns true if |input| looks valid, false otherwise.
MEDIA_EXPORT bool ValidatePsshInput(const std::vector<uint8_t>& input);

// Gets the Key Ids from a 'pssh' box for the Common SystemID among one or
// more concatenated 'pssh' boxes. If |input| looks valid, then true is
// returned and |key_ids| is updated to contain the values found. Otherwise
// return false.
// TODO(jrummell): This returns true if no Common SystemID 'pssh' boxes are
// found, or are included but don't contain any key IDs. This should be
// fixed once the test files are updated to include correct 'pssh' boxes.
// http://crbug.com/460308
MEDIA_EXPORT bool GetKeyIdsForCommonSystemId(const std::vector<uint8_t>& input,
                                             KeyIdList* key_ids);

}  // namespace media

#endif  // MEDIA_CDM_CENC_UTILS_H_
