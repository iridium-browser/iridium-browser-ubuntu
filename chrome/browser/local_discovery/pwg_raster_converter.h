// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PWG_RASTER_CONVERTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PWG_RASTER_CONVERTER_H_

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"

namespace base {
class FilePath;
}

namespace cloud_devices {
class CloudDeviceDescription;
}

namespace gfx {
class Size;
}

namespace printing {
class PdfRenderSettings;
struct PwgRasterSettings;
}

namespace local_discovery {

class PWGRasterConverter {
 public:
  // Callback for when the PDF is converted to a PWG raster.
  // |success| denotes whether the conversion succeeded.
  // |temp_file| is the path to the temp file (owned by the converter) that
  //     contains the PWG raster data.
  typedef base::Callback<void(bool /*success*/,
                              const base::FilePath& /*temp_file*/)>
          ResultCallback;
  virtual ~PWGRasterConverter() {}

  static scoped_ptr<PWGRasterConverter> CreateDefault();

  // Generates conversion settings to be used with converter from printer
  // capabilities and page size.
  // TODO(vitalybuka): Extract page size from pdf document data.
  static printing::PdfRenderSettings GetConversionSettings(
      const cloud_devices::CloudDeviceDescription& printer_capabilities,
      const gfx::Size& page_size);

  // Generates pwg bitmap settings to be used with the converter from
  // device capabilites and printing ticket.
  static printing::PwgRasterSettings GetBitmapSettings(
      const cloud_devices::CloudDeviceDescription& printer_capabilities,
      const cloud_devices::CloudDeviceDescription& ticket);

  virtual void Start(base::RefCountedMemory* data,
                     const printing::PdfRenderSettings& conversion_settings,
                     const printing::PwgRasterSettings& bitmap_settings,
                     const ResultCallback& callback) = 0;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PWG_RASTER_CONVERTER_H_
