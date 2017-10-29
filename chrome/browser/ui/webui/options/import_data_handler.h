// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/importer/importer_data_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ExternalProcessImporterHost;
class ImporterList;

namespace options {

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler,
                          public importer::ImporterProgressObserver,
                          public ui::SelectFileDialog::Listener {
 public:
  ImportDataHandler();
  ~ImportDataHandler() override;

  // OptionsPageUIHandler:
  void GetLocalizedValues(base::DictionaryValue* localized_strings) override;
  void InitializeHandler() override;
  void InitializePage() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t imported_items);

  void ImportData(const base::ListValue* args);

  // importer::ImporterProgressObserver:
  void ImportStarted() override;
  void ImportItemStarted(importer::ImportItem item) override;
  void ImportItemEnded(importer::ImportItem item) override;
  void ImportEnded() override;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;

  // Opens a file selection dialog to choose the bookmarks HTML file.
  void HandleChooseBookmarksFile(const base::ListValue* args);

  std::unique_ptr<ImporterList> importer_list_;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ExternalProcessImporterHost* importer_host_;  // weak

  bool import_did_succeed_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
