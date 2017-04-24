/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/xml/XSLImportRule.h"

#include "core/dom/Document.h"
#include "core/loader/resource/XSLStyleSheetResource.h"
#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

XSLImportRule::XSLImportRule(XSLStyleSheet* parent, const String& href)
    : m_parentStyleSheet(parent), m_strHref(href), m_loading(false) {}

XSLImportRule::~XSLImportRule() {}

void XSLImportRule::setXSLStyleSheet(const String& href,
                                     const KURL& baseURL,
                                     const String& sheet) {
  if (m_styleSheet)
    m_styleSheet->setParentStyleSheet(0);

  m_styleSheet = XSLStyleSheet::create(this, href, baseURL);

  XSLStyleSheet* parent = parentStyleSheet();
  if (parent)
    m_styleSheet->setParentStyleSheet(parent);

  m_styleSheet->parseString(sheet);
  m_loading = false;

  if (parent)
    parent->checkLoaded();
}

bool XSLImportRule::isLoading() {
  return m_loading || (m_styleSheet && m_styleSheet->isLoading());
}

void XSLImportRule::loadSheet() {
  Document* ownerDocument = nullptr;
  XSLStyleSheet* rootSheet = parentStyleSheet();

  if (rootSheet) {
    while (XSLStyleSheet* parentSheet = rootSheet->parentStyleSheet())
      rootSheet = parentSheet;
  }

  if (rootSheet)
    ownerDocument = rootSheet->ownerDocument();

  String absHref = m_strHref;
  XSLStyleSheet* parentSheet = parentStyleSheet();
  if (!parentSheet->baseURL().isNull()) {
    // Use parent styleheet's URL as the base URL
    absHref = KURL(parentSheet->baseURL(), m_strHref).getString();
  }

  // Check for a cycle in our import chain. If we encounter a stylesheet in
  // our parent chain with the same URL, then just bail.
  for (XSLStyleSheet* parentSheet = parentStyleSheet(); parentSheet;
       parentSheet = parentSheet->parentStyleSheet()) {
    if (absHref == parentSheet->baseURL().getString())
      return;
  }

  ResourceLoaderOptions fetchOptions(ResourceFetcher::defaultResourceOptions());
  FetchRequest request(ResourceRequest(ownerDocument->completeURL(absHref)),
                       FetchInitiatorTypeNames::xml, fetchOptions);
  request.setOriginRestriction(FetchRequest::RestrictToSameOrigin);
  XSLStyleSheetResource* resource = XSLStyleSheetResource::fetchSynchronously(
      request, ownerDocument->fetcher());
  if (!resource || !resource->sheet())
    return;

  DCHECK(!m_styleSheet);
  setXSLStyleSheet(absHref, resource->response().url(), resource->sheet());
}

DEFINE_TRACE(XSLImportRule) {
  visitor->trace(m_parentStyleSheet);
  visitor->trace(m_styleSheet);
}

}  // namespace blink
