// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_PARSE_TREE_H_
#define TOOLS_GN_PARSE_TREE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/err.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

class AccessorNode;
class BinaryOpNode;
class BlockCommentNode;
class BlockNode;
class ConditionNode;
class EndNode;
class FunctionCallNode;
class IdentifierNode;
class ListNode;
class LiteralNode;
class Scope;
class UnaryOpNode;

class Comments {
 public:
  Comments();
  virtual ~Comments();

  const std::vector<Token>& before() const { return before_; }
  void append_before(Token c) { before_.push_back(c); }
  void clear_before() { before_.clear(); }

  const std::vector<Token>& suffix() const { return suffix_; }
  void append_suffix(Token c) { suffix_.push_back(c); }
  // Reverse the order of the suffix comments. When walking the tree in
  // post-order we append suffix comments in reverse order, so this fixes them
  // up.
  void ReverseSuffix();

  const std::vector<Token>& after() const { return after_; }
  void append_after(Token c) { after_.push_back(c); }

 private:
  // Whole line comments before the expression.
  std::vector<Token> before_;

  // End-of-line comments after this expression.
  std::vector<Token> suffix_;

  // For top-level expressions only, after_ lists whole-line comments
  // following the expression.
  std::vector<Token> after_;

  DISALLOW_COPY_AND_ASSIGN(Comments);
};

// ParseNode -------------------------------------------------------------------

// A node in the AST.
class ParseNode {
 public:
  ParseNode();
  virtual ~ParseNode();

  virtual const AccessorNode* AsAccessor() const;
  virtual const BinaryOpNode* AsBinaryOp() const;
  virtual const BlockCommentNode* AsBlockComment() const;
  virtual const BlockNode* AsBlock() const;
  virtual const ConditionNode* AsConditionNode() const;
  virtual const EndNode* AsEnd() const;
  virtual const FunctionCallNode* AsFunctionCall() const;
  virtual const IdentifierNode* AsIdentifier() const;
  virtual const ListNode* AsList() const;
  virtual const LiteralNode* AsLiteral() const;
  virtual const UnaryOpNode* AsUnaryOp() const;

  virtual Value Execute(Scope* scope, Err* err) const = 0;

  virtual LocationRange GetRange() const = 0;

  // Returns an error with the given messages and the range set to something
  // that indicates this node.
  virtual Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const = 0;

  // Prints a representation of this node to the given string, indenting
  // by the given number of spaces.
  virtual void Print(std::ostream& out, int indent) const = 0;

  const Comments* comments() const { return comments_.get(); }
  Comments* comments_mutable();
  void PrintComments(std::ostream& out, int indent) const;

 private:
  scoped_ptr<Comments> comments_;

  DISALLOW_COPY_AND_ASSIGN(ParseNode);
};

// AccessorNode ----------------------------------------------------------------

// Access an array or scope element.
//
// Currently, such values are only read-only. In that you can do:
//   a = obj1.a
//   b = obj2[0]
// But not
//   obj1.a = 5
//   obj2[0] = 6
//
// In the current design where the dot operator is used only for templates, we
// explicitly don't want to allow you to do "invoker.foo = 5", so if we added
// support for accessors to be lvalues, we would also need to add some concept
// of a constant scope. Supporting this would also add a lot of complications
// to the operator= implementation, since some accessors might return values
// in the const root scope that shouldn't be modified. Without a strong
// use-case for this, it seems simpler to just disallow it.
//
// Additionally, the left-hand-side of the accessor must currently be an
// identifier. So you can't do things like:
//   function_call()[1]
//   a = b.c.d
// These are easier to implement if we needed them but given the very limited
// use cases for this, it hasn't seemed worth the bother.
class AccessorNode : public ParseNode {
 public:
  AccessorNode();
  ~AccessorNode() override;

  const AccessorNode* AsAccessor() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  // Base is the thing on the left of the [] or dot, currently always required
  // to be an identifier token.
  const Token& base() const { return base_; }
  void set_base(const Token& b) { base_ = b; }

  // Index is the expression inside the []. Will be null if member is set.
  const ParseNode* index() const { return index_.get(); }
  void set_index(scoped_ptr<ParseNode> i) { index_ = i.Pass(); }

  // The member is the identifier on the right hand side of the dot. Will be
  // null if the index is set.
  const IdentifierNode* member() const { return member_.get(); }
  void set_member(scoped_ptr<IdentifierNode> i) { member_ = i.Pass(); }

  void SetNewLocation(int line_number);

 private:
  Value ExecuteArrayAccess(Scope* scope, Err* err) const;
  Value ExecuteScopeAccess(Scope* scope, Err* err) const;

  Token base_;

  // Either index or member will be set according to what type of access this
  // is.
  scoped_ptr<ParseNode> index_;
  scoped_ptr<IdentifierNode> member_;

  DISALLOW_COPY_AND_ASSIGN(AccessorNode);
};

// BinaryOpNode ----------------------------------------------------------------

class BinaryOpNode : public ParseNode {
 public:
  BinaryOpNode();
  ~BinaryOpNode() override;

  const BinaryOpNode* AsBinaryOp() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& op() const { return op_; }
  void set_op(const Token& t) { op_ = t; }

  const ParseNode* left() const { return left_.get(); }
  void set_left(scoped_ptr<ParseNode> left) {
    left_ = left.Pass();
  }

  const ParseNode* right() const { return right_.get(); }
  void set_right(scoped_ptr<ParseNode> right) {
    right_ = right.Pass();
  }

 private:
  scoped_ptr<ParseNode> left_;
  Token op_;
  scoped_ptr<ParseNode> right_;

  DISALLOW_COPY_AND_ASSIGN(BinaryOpNode);
};

// BlockNode -------------------------------------------------------------------

class BlockNode : public ParseNode {
 public:
  BlockNode();
  ~BlockNode() override;

  const BlockNode* AsBlock() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  void set_begin_token(const Token& t) { begin_token_ = t; }
  void set_end(scoped_ptr<EndNode> e) { end_ = e.Pass(); }
  const EndNode* End() const { return end_.get(); }

  const std::vector<ParseNode*>& statements() const { return statements_; }
  void append_statement(scoped_ptr<ParseNode> s) {
    statements_.push_back(s.release());
  }

 private:
  // Tokens corresponding to { and }, if any (may be NULL). The end is stored
  // in a custom parse node so that it can have comments hung off of it.
  Token begin_token_;
  scoped_ptr<EndNode> end_;

  // Owning pointers, use unique_ptr when we can use C++11.
  std::vector<ParseNode*> statements_;

  DISALLOW_COPY_AND_ASSIGN(BlockNode);
};

// ConditionNode ---------------------------------------------------------------

class ConditionNode : public ParseNode {
 public:
  ConditionNode();
  ~ConditionNode() override;

  const ConditionNode* AsConditionNode() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  void set_if_token(const Token& token) { if_token_ = token; }

  const ParseNode* condition() const { return condition_.get(); }
  void set_condition(scoped_ptr<ParseNode> c) {
    condition_ = c.Pass();
  }

  const BlockNode* if_true() const { return if_true_.get(); }
  void set_if_true(scoped_ptr<BlockNode> t) {
    if_true_ = t.Pass();
  }

  // This is either empty, a block (for the else clause), or another
  // condition.
  const ParseNode* if_false() const { return if_false_.get(); }
  void set_if_false(scoped_ptr<ParseNode> f) {
    if_false_ = f.Pass();
  }

 private:
  // Token corresponding to the "if" string.
  Token if_token_;

  scoped_ptr<ParseNode> condition_;  // Always non-null.
  scoped_ptr<BlockNode> if_true_;  // Always non-null.
  scoped_ptr<ParseNode> if_false_;  // May be null.

  DISALLOW_COPY_AND_ASSIGN(ConditionNode);
};

// FunctionCallNode ------------------------------------------------------------

class FunctionCallNode : public ParseNode {
 public:
  FunctionCallNode();
  ~FunctionCallNode() override;

  const FunctionCallNode* AsFunctionCall() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& function() const { return function_; }
  void set_function(Token t) { function_ = t; }

  const ListNode* args() const { return args_.get(); }
  void set_args(scoped_ptr<ListNode> a) { args_ = a.Pass(); }

  const BlockNode* block() const { return block_.get(); }
  void set_block(scoped_ptr<BlockNode> b) { block_ = b.Pass(); }

 private:
  Token function_;
  scoped_ptr<ListNode> args_;
  scoped_ptr<BlockNode> block_;  // May be null.

  DISALLOW_COPY_AND_ASSIGN(FunctionCallNode);
};

// IdentifierNode --------------------------------------------------------------

class IdentifierNode : public ParseNode {
 public:
  IdentifierNode();
  explicit IdentifierNode(const Token& token);
  ~IdentifierNode() override;

  const IdentifierNode* AsIdentifier() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& value() const { return value_; }
  void set_value(const Token& t) { value_ = t; }

  void SetNewLocation(int line_number);

 private:
  Token value_;

  DISALLOW_COPY_AND_ASSIGN(IdentifierNode);
};

// ListNode --------------------------------------------------------------------

class ListNode : public ParseNode {
 public:
  ListNode();
  ~ListNode() override;

  const ListNode* AsList() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  void set_begin_token(const Token& t) { begin_token_ = t; }
  void set_end(scoped_ptr<EndNode> e) { end_ = e.Pass(); }
  const EndNode* End() const { return end_.get(); }

  void append_item(scoped_ptr<ParseNode> s) {
    contents_.push_back(s.release());
  }
  const std::vector<const ParseNode*>& contents() const { return contents_; }

  void SortAsStringsList();

  // During formatting, do we want this list to always be multliline? This is
  // used to make assignments to deps, sources, etc. always be multiline lists,
  // rather than collapsed to a single line when they're one element.
  bool prefer_multiline() const { return prefer_multiline_; }
  void set_prefer_multiline(bool prefer_multiline) {
    prefer_multiline_ = prefer_multiline;
  }

  struct SortRange {
    size_t begin;
    size_t end;
    SortRange(size_t begin, size_t end) : begin(begin), end(end) {}
  };
  // Only public for testing.
  std::vector<SortRange> GetSortRanges() const;

 private:
  // Tokens corresponding to the [ and ]. The end token is stored in inside an
  // custom parse node so that it can have comments hung off of it.
  Token begin_token_;
  scoped_ptr<EndNode> end_;
  bool prefer_multiline_;

  // Owning pointers, use unique_ptr when we can use C++11.
  std::vector<const ParseNode*> contents_;

  DISALLOW_COPY_AND_ASSIGN(ListNode);
};

// LiteralNode -----------------------------------------------------------------

class LiteralNode : public ParseNode {
 public:
  LiteralNode();
  explicit LiteralNode(const Token& token);
  ~LiteralNode() override;

  const LiteralNode* AsLiteral() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& value() const { return value_; }
  void set_value(const Token& t) { value_ = t; }

  void SetNewLocation(int line_number);

 private:
  Token value_;

  DISALLOW_COPY_AND_ASSIGN(LiteralNode);
};

// UnaryOpNode -----------------------------------------------------------------

class UnaryOpNode : public ParseNode {
 public:
  UnaryOpNode();
  ~UnaryOpNode() override;

  const UnaryOpNode* AsUnaryOp() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& op() const { return op_; }
  void set_op(const Token& t) { op_ = t; }

  const ParseNode* operand() const { return operand_.get(); }
  void set_operand(scoped_ptr<ParseNode> operand) {
    operand_ = operand.Pass();
  }

 private:
  Token op_;
  scoped_ptr<ParseNode> operand_;

  DISALLOW_COPY_AND_ASSIGN(UnaryOpNode);
};

// BlockCommentNode ------------------------------------------------------------

// This node type is only used for standalone comments (that is, those not
// specifically attached to another syntax element. The most common of these
// is a standard header block. This node contains only the last line of such
// a comment block as the anchor, and other lines of the block comment are
// hung off of it as Before comments, similar to other syntax elements.
class BlockCommentNode : public ParseNode {
 public:
  BlockCommentNode();
  ~BlockCommentNode() override;

  const BlockCommentNode* AsBlockComment() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& comment() const { return comment_; }
  void set_comment(const Token& t) { comment_ = t; }

 private:
  Token comment_;

  DISALLOW_COPY_AND_ASSIGN(BlockCommentNode);
};

// EndNode ---------------------------------------------------------------------

// This node type is used as the end_ object for lists and blocks (rather than
// just the end ']', '}', or ')' token). This is so that during formatting
// traversal there is a node that appears at the end of the block to which
// comments can be attached.
class EndNode : public ParseNode {
 public:
  explicit EndNode(const Token& token);
  ~EndNode() override;

  const EndNode* AsEnd() const override;
  Value Execute(Scope* scope, Err* err) const override;
  LocationRange GetRange() const override;
  Err MakeErrorDescribing(
      const std::string& msg,
      const std::string& help = std::string()) const override;
  void Print(std::ostream& out, int indent) const override;

  const Token& value() const { return value_; }
  void set_value(const Token& t) { value_ = t; }

 private:
  Token value_;

  DISALLOW_COPY_AND_ASSIGN(EndNode);
};

#endif  // TOOLS_GN_PARSE_TREE_H_
