// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_printer.h"

#include <cups/cups.h>

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend.h"

namespace {

const char kDriverInfoTagName[] = "system_driverinfo";

const char kCUPSPrinterInfoOpt[] = "printer-info";
const char kCUPSPrinterStateOpt[] = "printer-state";
const char kCUPSPrinterMakeModelOpt[] = "printer-make-and-model";

}  // namespace

namespace printing {

CupsPrinter::CupsPrinter(http_t* http,
                         std::unique_ptr<cups_dest_t, DestinationDeleter> dest,
                         std::unique_ptr<cups_dinfo_t, DestInfoDeleter> info)
    : cups_http_(http),
      destination_(std::move(dest)),
      dest_info_(std::move(info)) {
  DCHECK(cups_http_);
  DCHECK(destination_);
}

CupsPrinter::CupsPrinter(CupsPrinter&& printer)
    : cups_http_(printer.cups_http_),
      destination_(std::move(printer.destination_)),
      dest_info_(std::move(printer.dest_info_)) {
  DCHECK(cups_http_);
  DCHECK(destination_);
}

CupsPrinter::~CupsPrinter() {}

bool CupsPrinter::is_default() const {
  return destination_->is_default;
}

ipp_attribute_t* CupsPrinter::GetSupportedOptionValues(
    base::StringPiece option_name) const {
  if (!InitializeDestInfo())
    return nullptr;

  return cupsFindDestSupported(cups_http_, destination_.get(), dest_info_.get(),
                               option_name.as_string().c_str());
}

std::vector<base::StringPiece> CupsPrinter::GetSupportedOptionValueStrings(
    base::StringPiece option_name) const {
  ipp_attribute_t* attr = GetSupportedOptionValues(option_name);
  std::vector<base::StringPiece> values;
  if (!attr) {
    return values;
  }

  base::StringPiece value;
  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; ++i) {
    value.set(ippGetString(attr, i, nullptr));
    values.push_back(value);
  }

  return values;
}

ipp_attribute_t* CupsPrinter::GetDefaultOptionValue(
    base::StringPiece option_name) const {
  if (!InitializeDestInfo())
    return nullptr;

  return cupsFindDestDefault(cups_http_, destination_.get(), dest_info_.get(),
                             option_name.as_string().c_str());
}

bool CupsPrinter::CheckOptionSupported(base::StringPiece name,
                                       base::StringPiece value) const {
  if (!InitializeDestInfo())
    return false;

  int supported = cupsCheckDestSupported(
      cups_http_, destination_.get(), dest_info_.get(),
      name.as_string().c_str(), value.as_string().c_str());
  return supported == 1;
}

bool CupsPrinter::ToPrinterInfo(PrinterBasicInfo* printer_info) const {
  const cups_dest_t* printer = destination_.get();

  printer_info->printer_name = printer->name;
  printer_info->is_default = printer->is_default;

  const char* info = cupsGetOption(kCUPSPrinterInfoOpt, printer->num_options,
                                   printer->options);
  if (info)
    printer_info->printer_description = info;

  const char* state = cupsGetOption(kCUPSPrinterStateOpt, printer->num_options,
                                    printer->options);
  if (state)
    base::StringToInt(state, &printer_info->printer_status);

  const char* drv_info = cupsGetOption(kCUPSPrinterMakeModelOpt,
                                       printer->num_options, printer->options);
  if (drv_info)
    printer_info->options[kDriverInfoTagName] = *drv_info;

  // Store printer options.
  for (int opt_index = 0; opt_index < printer->num_options; ++opt_index) {
    printer_info->options[printer->options[opt_index].name] =
        printer->options[opt_index].value;
  }

  return true;
}

base::FilePath CupsPrinter::GetPPD() const {
  base::StringPiece printer_name = destination_->name;
  const char* ppd_path =
      cupsGetPPD2(cups_http_, printer_name.as_string().c_str());
  base::FilePath path(ppd_path);

  if (ppd_path) {
    // There is no reliable way right now to detect full and complete PPD
    // get downloaded. If we reach http timeout, it may simply return
    // downloaded part as a full response. It might be good enough to check
    // http->data_remaining or http->_data_remaining, unfortunately http_t
    // is an internal structure and fields are not exposed in CUPS headers.
    // httpGetLength or httpGetLength2 returning the full content size.
    // Comparing file size against that content length might be unreliable
    // since some http reponses are encoded and content_length > file size.
    // Let's just check for the obvious CUPS and http errors here.
    ipp_status_t error_code = cupsLastError();
    int http_error = httpError(cups_http_);
    if (error_code > IPP_OK_EVENTS_COMPLETE || http_error != 0) {
      LOG(ERROR) << "Error downloading PPD file, name: " << destination_->name
                 << ", CUPS error: " << static_cast<int>(error_code)
                 << ", HTTP error: " << http_error;
      base::DeleteFile(path, false);
      path.clear();
    }
  }

  return path;
}

std::string CupsPrinter::GetName() const {
  return std::string(destination_->name);
}

std::string CupsPrinter::GetMakeAndModel() const {
  const char* make_and_model =
      cupsGetOption(kCUPSPrinterMakeModelOpt, destination_->num_options,
                    destination_->options);

  return make_and_model ? std::string(make_and_model) : std::string();
}

bool CupsPrinter::IsAvailable() const {
  return InitializeDestInfo();
}

bool CupsPrinter::InitializeDestInfo() const {
  if (dest_info_)
    return true;

  dest_info_.reset(cupsCopyDestInfo(cups_http_, destination_.get()));
  return !!dest_info_;
}

ipp_status_t CupsPrinter::CreateJob(int* job_id,
                                    base::StringPiece title,
                                    const std::vector<cups_option_t>& options) {
  DCHECK(dest_info_) << "Verify availability before starting a print job";

  cups_option_t* data = const_cast<cups_option_t*>(
      options.data());  // createDestJob will not modify the data
  ipp_status_t create_status = cupsCreateDestJob(
      cups_http_, destination_.get(), dest_info_.get(), job_id,
      title.as_string().c_str(), options.size(), data);

  return create_status;
}

bool CupsPrinter::StartDocument(int job_id,
                                base::StringPiece document_name,
                                bool last_document,
                                const std::vector<cups_option_t>& options) {
  DCHECK(dest_info_);
  DCHECK(job_id);

  cups_option_t* data = const_cast<cups_option_t*>(
      options.data());  // createStartDestDocument will not modify the data
  http_status_t start_doc_status = cupsStartDestDocument(
      cups_http_, destination_.get(), dest_info_.get(), job_id,
      document_name.as_string().c_str(), CUPS_FORMAT_PDF, options.size(), data,
      last_document ? 0 : 1);

  return start_doc_status == HTTP_CONTINUE;
}

bool CupsPrinter::StreamData(const std::vector<char>& buffer) {
  http_status_t status =
      cupsWriteRequestData(cups_http_, buffer.data(), buffer.size());
  return status == HTTP_STATUS_CONTINUE;
}

bool CupsPrinter::FinishDocument() {
  DCHECK(dest_info_);

  ipp_status_t status =
      cupsFinishDestDocument(cups_http_, destination_.get(), dest_info_.get());

  return status == IPP_STATUS_OK;
}

ipp_status_t CupsPrinter::CloseJob(int job_id) {
  DCHECK(dest_info_);
  DCHECK(job_id);

  return cupsCloseDestJob(cups_http_, destination_.get(), dest_info_.get(),
                          job_id);
}

}  // namespace printing
