// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_PARSER_CXFA_SIMPLE_PARSER_H_
#define XFA_FXFA_PARSER_CXFA_SIMPLE_PARSER_H_

#include <memory>

#include "xfa/fde/xml/fde_xml_imp.h"
#include "xfa/fxfa/fxfa_basic.h"

class CXFA_Document;
class CXFA_Node;
class CXFA_XMLParser;
class IFX_SeekableReadStream;
class IFX_Pause;
class IFGAS_Stream;

class CXFA_SimpleParser {
 public:
  CXFA_SimpleParser(CXFA_Document* pFactory, bool bDocumentParser);
  ~CXFA_SimpleParser();

  int32_t StartParse(const CFX_RetainPtr<IFX_SeekableReadStream>& pStream,
                     XFA_XDPPACKET ePacketID);
  int32_t DoParse(IFX_Pause* pPause);
  int32_t ParseXMLData(const CFX_WideString& wsXML,
                       CFDE_XMLNode*& pXMLNode,
                       IFX_Pause* pPause);
  void ConstructXFANode(CXFA_Node* pXFANode, CFDE_XMLNode* pXMLNode);
  CXFA_Node* GetRootNode() const;
  CFDE_XMLDoc* GetXMLDoc() const;
  void CloseParser();

  void SetFactory(CXFA_Document* pFactory);

 private:
  CXFA_Node* ParseAsXDPPacket(CFDE_XMLNode* pXMLDocumentNode,
                              XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_XDP(CFDE_XMLNode* pXMLDocumentNode,
                                  XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Config(CFDE_XMLNode* pXMLDocumentNode,
                                     XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_TemplateForm(CFDE_XMLNode* pXMLDocumentNode,
                                           XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Data(CFDE_XMLNode* pXMLDocumentNode,
                                   XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_LocaleConnectionSourceSet(
      CFDE_XMLNode* pXMLDocumentNode,
      XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_Xdc(CFDE_XMLNode* pXMLDocumentNode,
                                  XFA_XDPPACKET ePacketID);
  CXFA_Node* ParseAsXDPPacket_User(CFDE_XMLNode* pXMLDocumentNode,
                                   XFA_XDPPACKET ePacketID);
  CXFA_Node* NormalLoader(CXFA_Node* pXFANode,
                          CFDE_XMLNode* pXMLDoc,
                          XFA_XDPPACKET ePacketID,
                          bool bUseAttribute);
  CXFA_Node* DataLoader(CXFA_Node* pXFANode,
                        CFDE_XMLNode* pXMLDoc,
                        bool bDoTransform);
  CXFA_Node* UserPacketLoader(CXFA_Node* pXFANode, CFDE_XMLNode* pXMLDoc);
  void ParseContentNode(CXFA_Node* pXFANode,
                        CFDE_XMLNode* pXMLNode,
                        XFA_XDPPACKET ePacketID);
  void ParseDataValue(CXFA_Node* pXFANode,
                      CFDE_XMLNode* pXMLNode,
                      XFA_XDPPACKET ePacketID);
  void ParseDataGroup(CXFA_Node* pXFANode,
                      CFDE_XMLNode* pXMLNode,
                      XFA_XDPPACKET ePacketID);
  void ParseInstruction(CXFA_Node* pXFANode,
                        CFDE_XMLInstruction* pXMLInstruction,
                        XFA_XDPPACKET ePacketID);

  CXFA_XMLParser* m_pXMLParser;
  std::unique_ptr<CFDE_XMLDoc> m_pXMLDoc;
  CFX_RetainPtr<IFGAS_Stream> m_pStream;
  CFX_RetainPtr<IFX_SeekableReadStream> m_pFileRead;
  CXFA_Document* m_pFactory;
  CXFA_Node* m_pRootNode;
  XFA_XDPPACKET m_ePacketID;
  bool m_bDocumentParser;
};

#endif  // XFA_FXFA_PARSER_CXFA_SIMPLE_PARSER_H_
