// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_
#define CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_

#include <stddef.h>

#include "net/cert/cert_type.h"
#include "net/cert/x509_certificate.h"

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates. It decouples the UI from the underlying crypto library
// (currently NSS or OpenSSL - in development).
// This is currently only used by linux, as mac / windows use their own native
// certificate viewers and crypto libraries.
namespace x509_certificate_model {

std::string GetCertNameOrNickname(
    net::X509Certificate::OSCertHandle cert_handle);

std::string GetTokenName(net::X509Certificate::OSCertHandle cert_handle);

std::string GetVersion(net::X509Certificate::OSCertHandle cert_handle);

net::CertType GetType(net::X509Certificate::OSCertHandle cert_handle);

void GetUsageStrings(
    net::X509Certificate::OSCertHandle cert_handle,
    std::vector<std::string>* usages);

std::string GetSerialNumberHexified(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerCommonName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerOrgName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetIssuerOrgUnitName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectOrgName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectOrgUnitName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

std::string GetSubjectCommonName(
    net::X509Certificate::OSCertHandle cert_handle,
    const std::string& alternative_text);

bool GetTimes(net::X509Certificate::OSCertHandle cert_handle,
              base::Time* issued, base::Time* expires);

std::string GetTitle(net::X509Certificate::OSCertHandle cert_handle);
std::string GetIssuerName(net::X509Certificate::OSCertHandle cert_handle);
std::string GetSubjectName(net::X509Certificate::OSCertHandle cert_handle);

struct Extension {
  std::string name;
  std::string value;
};

typedef std::vector<Extension> Extensions;

void GetExtensions(
    const std::string& critical_label,
    const std::string& non_critical_label,
    net::X509Certificate::OSCertHandle cert_handle,
    Extensions* extensions);

// Hash a certificate using the given algorithm, return the result as a
// colon-seperated hex string.
std::string HashCertSHA256(net::X509Certificate::OSCertHandle cert_handle);
std::string HashCertSHA1(net::X509Certificate::OSCertHandle cert_handle);

// For host values, if they contain IDN Punycode-encoded A-labels, this will
// return a string suitable for display that contains both the original and the
// decoded U-label form.  Otherwise, the string will be returned as is.
std::string ProcessIDN(const std::string& input);

std::string GetCMSString(const net::X509Certificate::OSCertHandles& cert_chain,
                         size_t start, size_t end);

std::string ProcessSecAlgorithmSignature(
    net::X509Certificate::OSCertHandle cert_handle);
std::string ProcessSecAlgorithmSubjectPublicKey(
    net::X509Certificate::OSCertHandle cert_handle);
std::string ProcessSecAlgorithmSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle);

std::string ProcessSubjectPublicKeyInfo(
    net::X509Certificate::OSCertHandle cert_handle);

std::string ProcessRawBitsSignatureWrap(
    net::X509Certificate::OSCertHandle cert_handle);

// Format a buffer as |hex_separator| separated string, with 16 bytes on each
// line separated using |line_separator|.
std::string ProcessRawBytesWithSeparators(const unsigned char* data,
                                          size_t data_length,
                                          char hex_separator,
                                          char line_separator);

// Format a buffer as a space separated string, with 16 bytes on each line.
std::string ProcessRawBytes(const unsigned char* data,
                            size_t data_length);

#if defined(USE_NSS_CERTS)
// Format a buffer as a space separated string, with 16 bytes on each line.
// |data_length| is the length in bits.
std::string ProcessRawBits(const unsigned char* data,
                           size_t data_length);
#endif  // USE_NSS_CERTS

}  // namespace x509_certificate_model

#endif  // CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_H_
