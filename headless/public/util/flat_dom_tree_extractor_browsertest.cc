// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/flat_dom_tree_extractor.h"

#include <memory>
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace headless {

namespace {

std::string NormaliseJSON(const std::string& json) {
  std::unique_ptr<base::Value> parsed_json = base::JSONReader::Read(json);
  DCHECK(parsed_json);
  std::string normalized_json;
  base::JSONWriter::WriteWithOptions(
      *parsed_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &normalized_json);
  return normalized_json;
}

}  // namespace

class FlatDomTreeExtractorBrowserTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/dom_tree_test.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->RemoveObserver(this);

    extractor_.reset(new FlatDomTreeExtractor(devtools_client_.get()));

    std::vector<std::string> css_whitelist = {
        "color",       "display",      "font-style", "font-family",
        "margin-left", "margin-right", "margin-top", "margin-bottom"};
    extractor_->ExtractDomTree(
        css_whitelist,
        base::Bind(&FlatDomTreeExtractorBrowserTest::OnDomTreeExtracted,
                   base::Unretained(this)));
  }

  void OnDomTreeExtracted(FlatDomTreeExtractor::DomTree dom_tree) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("");

    std::vector<std::unique_ptr<base::DictionaryValue>> dom_nodes(
        dom_tree.dom_nodes_.size());

    std::map<int, std::unique_ptr<base::ListValue>> child_lists;

    // For convenience flatten the dom tree into an array.
    for (size_t i = 0; i < dom_tree.dom_nodes_.size(); i++) {
      dom::Node* node = const_cast<dom::Node*>(dom_tree.dom_nodes_[i]);

      dom_nodes[i].reset(
          static_cast<base::DictionaryValue*>(node->Serialize().release()));

      if (node->HasParentId()) {
        if (child_lists.find(node->GetParentId()) == child_lists.end()) {
          child_lists.insert(std::make_pair(
              node->GetParentId(), base::MakeUnique<base::ListValue>()));
        }
        child_lists[node->GetParentId()]->AppendInteger(i);
      }
      dom_nodes[i]->Remove("children", nullptr);

      // Convert content document pointers into indexes.
      if (node->HasContentDocument()) {
        dom_nodes[i]->SetInteger(
            "contentDocumentIndex",
            dom_tree
                .node_id_to_index_[node->GetContentDocument()->GetNodeId()]);
        dom_nodes[i]->Remove("contentDocument", nullptr);
      }

      dom_nodes[i]->Remove("childNodeCount", nullptr);

      // Frame IDs are random.
      if (dom_nodes[i]->HasKey("frameId"))
        dom_nodes[i]->SetString("frameId", "?");

      // Ports are random.
      std::string url;
      if (dom_nodes[i]->GetString("baseURL", &url)) {
        dom_nodes[i]->SetString(
            "baseURL", GURL(url).ReplaceComponents(replace_port).spec());
      }

      if (dom_nodes[i]->GetString("documentURL", &url)) {
        dom_nodes[i]->SetString(
            "documentURL", GURL(url).ReplaceComponents(replace_port).spec());
      }
    }

    for (auto& pair : child_lists) {
      dom_nodes[dom_tree.node_id_to_index_[pair.first]]->Set(
          "childIndices", std::move(pair.second));
    }

    // Merge LayoutTreeNode data into the dictionaries.
    for (const css::LayoutTreeNode* layout_node : dom_tree.layout_tree_nodes_) {
      auto it = dom_tree.node_id_to_index_.find(layout_node->GetNodeId());
      ASSERT_TRUE(it != dom_tree.node_id_to_index_.end());

      base::DictionaryValue* node_dict = dom_nodes[it->second].get();
      node_dict->Set("boundingBox", layout_node->GetBoundingBox()->Serialize());

      if (layout_node->HasLayoutText())
        node_dict->SetString("layoutText", layout_node->GetLayoutText());

      if (layout_node->HasStyleIndex())
        node_dict->SetInteger("styleIndex", layout_node->GetStyleIndex());

      if (layout_node->HasInlineTextNodes()) {
        std::unique_ptr<base::ListValue> inline_text_nodes(
            new base::ListValue());
        for (const std::unique_ptr<css::InlineTextBox>& inline_text_box :
             *layout_node->GetInlineTextNodes()) {
          size_t index = inline_text_nodes->GetSize();
          inline_text_nodes->Set(index, inline_text_box->Serialize());
        }
        node_dict->Set("inlineTextNodes", std::move(inline_text_nodes));
      }
    }

    std::vector<std::unique_ptr<base::DictionaryValue>> computed_styles(
        dom_tree.computed_styles_.size());

    for (size_t i = 0; i < dom_tree.computed_styles_.size(); i++) {
      std::unique_ptr<base::DictionaryValue> style(new base::DictionaryValue());
      for (const auto& style_property :
           *dom_tree.computed_styles_[i]->GetProperties()) {
        style->SetString(style_property->GetName(), style_property->GetValue());
      }
      computed_styles[i] = std::move(style);
    }

    const std::vector<std::string> expected_dom_nodes = {
        R"raw_string({
           "backendNodeId": 7,
           "localName": "",
           "nodeId": 5,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello world!",
           "parentId": 4
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 6,
           "childIndices": [ 0 ],
           "localName": "title",
           "nodeId": 4,
           "nodeName": "TITLE",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 3
        })raw_string",

        R"raw_string({
           "attributes": [ "href", "dom_tree_test.css", "rel", "stylesheet",
                           "type", "text/css" ],
           "backendNodeId": 8,
           "localName": "link",
           "nodeId": 6,
           "nodeName": "LINK",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 3
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 5,
           "childIndices": [ 1, 2 ],
           "localName": "head",
           "nodeId": 3,
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 2
        })raw_string",

        R"raw_string({
           "backendNodeId": 12,
           "boundingBox": {
              "height": 32.0,
              "width": 320.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 32.0,
                 "width": 320.0,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 10,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Some text.",
           "localName": "",
           "nodeId": 10,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some text.",
           "parentId": 9,
           "styleIndex": 2
        })raw_string",

        R"raw_string({
           "attributes": [ "class", "red" ],
           "backendNodeId": 11,
           "boundingBox": {
              "height": 32.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 4 ],
           "localName": "h1",
           "nodeId": 9,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 8,
           "styleIndex": 2
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 16,
           "localName": "head",
           "nodeId": 14,
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 13
        })raw_string",

        R"raw_string({
           "backendNodeId": 19,
           "boundingBox": {
              "height": 36.0,
              "width": 308.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 36.0,
                 "width": 307.734375,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 22,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Hello from the iframe!",
           "localName": "",
           "nodeId": 17,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello from the iframe!",
           "parentId": 16,
           "styleIndex": 5
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 18,
           "boundingBox": {
              "height": 37.0,
              "width": 384.0,
              "x": 18.0,
              "y": 71.0
           },
           "childIndices": [ 7 ],
           "localName": "h1",
           "nodeId": 16,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 15,
           "styleIndex": 5
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 17,
           "boundingBox": {
              "height": 171.0,
              "width": 384.0,
              "x": 18.0,
              "y": 71.0
           },
           "childIndices": [ 8 ],
           "localName": "body",
           "nodeId": 15,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 13,
           "styleIndex": 4
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 15,
           "boundingBox": {
              "height": 200.0,
              "width": 400.0,
              "x": 10.0,
              "y": 63.0
           },
           "childIndices": [ 6, 9 ],
           "frameId": "?",
           "localName": "html",
           "nodeId": 13,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 12,
           "styleIndex": 3
        })raw_string",

        R"raw_string({
           "attributes": [ "src", "/iframe.html", "width", "400", "height",
                           "200" ],
           "backendNodeId": 13,
           "boundingBox": {
              "height": 205.0,
              "width": 404.0,
              "x": 8.0,
              "y": 61.0
           },
           "contentDocumentIndex": 12,
           "frameId": "?",
           "localName": "iframe",
           "nodeId": 11,
           "nodeName": "IFRAME",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 8,
           "styleIndex": 6
        })raw_string",

        R"raw_string({
           "backendNodeId": 14,
           "baseURL": "http://127.0.0.1/iframe.html",
           "childIndices": [ 10 ],
           "documentURL": "http://127.0.0.1/iframe.html",
           "localName": "",
           "nodeId": 12,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": "",
           "xmlVersion": ""
        })raw_string",

        R"raw_string({
           "backendNodeId": 24,
           "boundingBox": {
              "height": 17.0,
              "width": 112.0,
              "x": 8.0,
              "y": 265.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 112.0,
                 "x": 8.0,
                 "y": 265.4375
              },
              "numCharacters": 7,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Google!",
           "localName": "",
           "nodeId": 22,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Google!",
           "parentId": 21,
           "styleIndex": 7
        })raw_string",

        R"raw_string({
           "attributes": [ "href", "https://www.google.com" ],
           "backendNodeId": 23,
           "boundingBox": {
              "height": 17.0,
              "width": 112.0,
              "x": 8.0,
              "y": 265.0
           },
           "childIndices": [ 13 ],
           "localName": "a",
           "nodeId": 21,
           "nodeName": "A",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 20,
           "styleIndex": 7
        })raw_string",

        R"raw_string({
           "backendNodeId": 26,
           "boundingBox": {
              "height": 17.0,
              "width": 192.0,
              "x": 8.0,
              "y": 297.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 192.0,
                 "x": 8.0,
                 "y": 297.4375
              },
              "numCharacters": 12,
              "startCharacterIndex": 0
           } ],
           "layoutText": "A paragraph!",
           "localName": "",
           "nodeId": 24,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "A paragraph!",
           "parentId": 23,
           "styleIndex": 8
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 25,
           "boundingBox": {
              "height": 17.0,
              "width": 784.0,
              "x": 8.0,
              "y": 297.0
           },
           "childIndices": [ 15 ],
           "localName": "p",
           "nodeId": 23,
           "nodeName": "P",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 20,
           "styleIndex": 8
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 27,
           "boundingBox": {
              "height": 0.0,
              "width": 0.0,
              "x": 0.0,
              "y": 0.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 0.0,
                 "x": 8.0,
                 "y": 329.4375
              },
              "numCharacters": 1,
              "startCharacterIndex": 0
           } ],
           "layoutText": "\n",
           "localName": "br",
           "nodeId": 25,
           "nodeName": "BR",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 20,
           "styleIndex": 6
        })raw_string",

        R"raw_string({
           "backendNodeId": 29,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 8.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 80.0,
                 "x": 8.0,
                 "y": 345.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutText": "Some ",
           "localName": "",
           "nodeId": 27,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some ",
           "parentId": 26,
           "styleIndex": 9
        })raw_string",

        R"raw_string({
           "backendNodeId": 31,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 88.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 80.0,
                 "x": 88.0,
                 "y": 345.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutText": "green",
           "localName": "",
           "nodeId": 29,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "green",
           "parentId": 28,
           "styleIndex": 10
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 30,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 88.0,
              "y": 345.0
           },
           "childIndices": [ 19 ],
           "localName": "em",
           "nodeId": 28,
           "nodeName": "EM",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 26,
           "styleIndex": 10
        })raw_string",

        R"raw_string({
           "backendNodeId": 32,
           "boundingBox": {
              "height": 17.0,
              "width": 128.0,
              "x": 168.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 128.0,
                 "x": 168.0,
                 "y": 345.4375
              },
              "numCharacters": 8,
              "startCharacterIndex": 0
           } ],
           "layoutText": " text...",
           "localName": "",
           "nodeId": 30,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": " text...",
           "parentId": 26,
           "styleIndex": 9
        })raw_string",

        R"raw_string({
           "attributes": [ "class", "green" ],
           "backendNodeId": 28,
           "boundingBox": {
              "height": 17.0,
              "width": 784.0,
              "x": 8.0,
              "y": 345.0
           },
           "childIndices": [ 18, 20, 21 ],
           "localName": "div",
           "nodeId": 26,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 20,
           "styleIndex": 9
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id4" ],
           "backendNodeId": 22,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childIndices": [ 14, 16, 17, 22 ],
           "localName": "div",
           "nodeId": 20,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 19,
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id3" ],
           "backendNodeId": 21,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childIndices": [ 23 ],
           "localName": "div",
           "nodeId": 19,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 18,
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id2" ],
           "backendNodeId": 20,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childIndices": [ 24 ],
           "localName": "div",
           "nodeId": 18,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 8,
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [ "id", "id1" ],
           "backendNodeId": 10,
           "boundingBox": {
              "height": 354.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 5, 11, 25 ],
           "localName": "div",
           "nodeId": 8,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 7,
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 9,
           "boundingBox": {
              "height": 584.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childIndices": [ 26 ],
           "localName": "body",
           "nodeId": 7,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 2,
           "styleIndex": 1
        })raw_string",

        R"raw_string({
           "attributes": [  ],
           "backendNodeId": 4,
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childIndices": [ 3, 27 ],
           "frameId": "?",
           "localName": "html",
           "nodeId": 2,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "parentId": 1,
           "styleIndex": 0
        })raw_string",

        R"raw_string({
           "backendNodeId": 3,
           "baseURL": "http://127.0.0.1/dom_tree_test.html",
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childIndices": [ 28 ],
           "documentURL": "http://127.0.0.1/dom_tree_test.html",
           "localName": "",
           "nodeId": 1,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": "",
           "xmlVersion": ""
        })raw_string"};

    EXPECT_EQ(expected_dom_nodes.size(), dom_nodes.size());

    for (size_t i = 0; i < dom_nodes.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(
          *dom_nodes[i], base::JSONWriter::OPTIONS_PRETTY_PRINT, &result_json);

      ASSERT_LT(i, expected_dom_nodes.size());
      EXPECT_EQ(NormaliseJSON(expected_dom_nodes[i]), result_json) << " Node # "
                                                                   << i;
    }

    const std::vector<std::string> expected_styles = {
        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "8px",
           "margin-left": "8px",
           "margin-right": "8px",
           "margin-top": "8px"
        })raw_string",

        R"raw_string({
           "color": "rgb(255, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "8px",
           "margin-left": "8px",
           "margin-right": "8px",
           "margin-top": "8px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 238)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "16px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "16px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "italic",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        }
        )raw_string"};

    for (size_t i = 0; i < computed_styles.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(*computed_styles[i],
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &result_json);

      ASSERT_LT(i, expected_styles.size());
      EXPECT_EQ(NormaliseJSON(expected_styles[i]), result_json) << " Style # "
                                                                << i;
    }

    FinishAsynchronousTest();
  }

  std::unique_ptr<FlatDomTreeExtractor> extractor_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(FlatDomTreeExtractorBrowserTest);

}  // namespace headless
