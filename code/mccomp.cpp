#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <deque>
#include <string.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::sys;

FILE *pFile;

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

// The lexer returns one of these for known things.
enum TOKEN_TYPE {

  IDENT = -1,        // [a-zA-Z_][a-zA-Z_0-9]*
  ASSIGN = int('='), // '='

  // delimiters
  LBRA = int('{'),  // left brace
  RBRA = int('}'),  // right brace
  LPAR = int('('),  // left parenthesis
  LBOX = int('['),  // left bracket
  RBOX = int(']'),  // right bracket
  RPAR = int(')'),  // right parenthesis
  SC = int(';'),    // semicolon
  COMMA = int(','), // comma

  // types
  INT_TOK = -2,   // "int"
  VOID_TOK = -3,  // "void"
  FLOAT_TOK = -4, // "float"
  BOOL_TOK = -5,  // "bool"

  // keywords
  EXTERN = -6,  // "extern"
  IF = -7,      // "if"
  ELSE = -8,    // "else"
  WHILE = -9,   // "while"
  RETURN = -10, // "return"
  // TRUE   = -12,     // "true"
  // FALSE   = -13,     // "false"

  // literals
  INT_LIT = -14,   // [0-9]+
  FLOAT_LIT = -15, // [0-9]+.[0-9]+
  BOOL_LIT = -16,  // "true" or "false" key words

  // logical operators
  AND = -17, // "&&"
  OR = -18,  // "||"

  // operators
  PLUS = int('+'),    // addition or unary plus
  MINUS = int('-'),   // substraction or unary negative
  ASTERIX = int('*'), // multiplication
  DIV = int('/'),     // division
  MOD = int('%'),     // modular
  NOT = int('!'),     // unary negation

  // comparison operators
  EQ = -19,      // equal
  NE = -20,      // not equal
  LE = -21,      // less than or equal to
  LT = int('<'), // less than
  GE = -23,      // greater than or equal to
  GT = int('>'), // greater than

  // special tokens
  EOF_TOK = 0, // signal end of file

  // invalid
  INVALID = -100 // signal invalid token
};

// TOKEN class is used to keep track of information about a token
class TOKEN {
public:
  TOKEN() = default;
  int type = -100;
  std::string lexeme;
  int lineNo;
  int columnNo;
  const std::string getIdentifierStr() const;
  const int getIntVal() const;
  const float getFloatVal() const;
  const bool getBoolVal() const;
};

static std::string globalLexeme;
static int lineNo, columnNo;

const std::string TOKEN::getIdentifierStr() const {
  if (type != IDENT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIdentifierStr called on non-IDENT token");
    exit(2);
  }
  return lexeme;
}

const int TOKEN::getIntVal() const {
  if (type != INT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getIntVal called on non-INT_LIT token");
    exit(2);
  }
  return strtod(lexeme.c_str(), nullptr);
}

const float TOKEN::getFloatVal() const {
  if (type != FLOAT_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getFloatVal called on non-FLOAT_LIT token");
    exit(2);
  }
  return strtof(lexeme.c_str(), nullptr);
}

const bool TOKEN::getBoolVal() const {
  if (type != BOOL_LIT) {
    fprintf(stderr, "%d:%d Error: %s\n", lineNo, columnNo,
            "getBoolVal called on non-BOOL_LIT token");
    exit(2);
  }
  return (lexeme == "true");
}

static TOKEN returnTok(std::string lexVal, int tok_type) {
  TOKEN return_tok;
  return_tok.lexeme = lexVal;
  return_tok.type = tok_type;
  return_tok.lineNo = lineNo;
  return_tok.columnNo = columnNo - lexVal.length() - 1;
  return return_tok;
}

// Read file line by line -- or look for \n and if found add 1 to line number
// and reset column number to 0
/// gettok - Return the next token from standard input.
static TOKEN gettok() {

  static int LastChar = ' ';
  static int NextChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar)) {
    if (LastChar == '\n' || LastChar == '\r') {
      lineNo++;
      columnNo = 1;
    }
    LastChar = getc(pFile);
    columnNo++;
  }

  if (isalpha(LastChar) ||
      (LastChar == '_')) { // identifier: [a-zA-Z_][a-zA-Z_0-9]*
    globalLexeme = LastChar;
    columnNo++;

    while (isalnum((LastChar = getc(pFile))) || (LastChar == '_')) {
      globalLexeme += LastChar;
      columnNo++;
    }

    if (globalLexeme == "int")
      return returnTok("int", INT_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "float")
      return returnTok("float", FLOAT_TOK);
    if (globalLexeme == "void")
      return returnTok("void", VOID_TOK);
    if (globalLexeme == "bool")
      return returnTok("bool", BOOL_TOK);
    if (globalLexeme == "extern")
      return returnTok("extern", EXTERN);
    if (globalLexeme == "if")
      return returnTok("if", IF);
    if (globalLexeme == "else")
      return returnTok("else", ELSE);
    if (globalLexeme == "while")
      return returnTok("while", WHILE);
    if (globalLexeme == "return")
      return returnTok("return", RETURN);
    if (globalLexeme == "true") {
      //   BoolVal = true;
      return returnTok("true", BOOL_LIT);
    }
    if (globalLexeme == "false") {
      //   BoolVal = false;
      return returnTok("false", BOOL_LIT);
    }
    return returnTok(globalLexeme.c_str(), IDENT);
  }

  if (LastChar == '=') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // EQ: ==
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("==", EQ);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("=", ASSIGN);
    }
  }

  if (LastChar == '{') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("{", LBRA);
  }
  if (LastChar == '}') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("}", RBRA);
  }
  if (LastChar == '(') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("(", LPAR);
  }
  if (LastChar == ')') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(")", RPAR);
  }
  // NEW: square brackets
  if (LastChar == '[') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("[", LBOX);
  }
  if (LastChar == ']') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok("]", RBOX);
  }
  if (LastChar == ';') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(";", SC);
  }
  if (LastChar == ',') {
    LastChar = getc(pFile);
    columnNo++;
    return returnTok(",", COMMA);
  }

  if (isdigit(LastChar) || LastChar == '.') { // Number: [0-9]+.
    std::string NumStr;

    if (LastChar == '.') { // Floatingpoint Number: .[0-9]+
      do {
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      //   FloatVal = strtof(NumStr.c_str(), nullptr);
      return returnTok(NumStr, FLOAT_LIT);
    } else {
      do { // Start of Number: [0-9]+
        NumStr += LastChar;
        LastChar = getc(pFile);
        columnNo++;
      } while (isdigit(LastChar));

      if (LastChar == '.') { // Floatingpoint Number: [0-9]+.[0-9]+)
        do {
          NumStr += LastChar;
          LastChar = getc(pFile);
          columnNo++;
        } while (isdigit(LastChar));

        // FloatVal = strtof(NumStr.c_str(), nullptr);
        return returnTok(NumStr, FLOAT_LIT);
      } else { // Integer : [0-9]+
        // IntVal = strtod(NumStr.c_str(), nullptr);
        return returnTok(NumStr, INT_LIT);
      }
    }
  }

  if (LastChar == '&') {
    NextChar = getc(pFile);
    if (NextChar == '&') { // AND: &&
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("&&", AND);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("&", int('&'));
    }
  }

  if (LastChar == '|') {
    NextChar = getc(pFile);
    if (NextChar == '|') { // OR: ||
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("||", OR);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("|", int('|'));
    }
  }

  if (LastChar == '!') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // NE: !=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("!=", NE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("!", NOT);
      ;
    }
  }

  if (LastChar == '<') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // LE: <=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok("<=", LE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok("<", LT);
    }
  }

  if (LastChar == '>') {
    NextChar = getc(pFile);
    if (NextChar == '=') { // GE: >=
      LastChar = getc(pFile);
      columnNo += 2;
      return returnTok(">=", GE);
    } else {
      LastChar = NextChar;
      columnNo++;
      return returnTok(">", GT);
    }
  }

  if (LastChar == '/') { // could be division or could be the start of a comment
    LastChar = getc(pFile);
    columnNo++;
    if (LastChar == '/') { // definitely a comment
      do {
        LastChar = getc(pFile);
        columnNo++;
      } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

      if (LastChar != EOF)
        return gettok();
    } else
      return returnTok("/", DIV);
  }

  // Check for end of file.  Don't eat the EOF.
  if (LastChar == EOF) {
    columnNo++;
    return returnTok("0", EOF_TOK);
  }

  // Otherwise, just return the character as its ascii value.
  int ThisChar = LastChar;
  std::string s(1, ThisChar);
  LastChar = getc(pFile);
  columnNo++;
  return returnTok(s, int(ThisChar));
}

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
static TOKEN CurTok;
static std::deque<TOKEN> tok_buffer;




static TOKEN getNextToken() {

  if (tok_buffer.size() == 0)
    tok_buffer.push_back(gettok());

  TOKEN temp = tok_buffer.front();
  tok_buffer.pop_front();

  return CurTok = temp;
}

static void putBackToken(TOKEN tok) { tok_buffer.push_front(tok); }
// Simple indent helper used by dump() methods
static void indentOut(int n) { for (int i = 0; i < n; ++i) fprintf(stderr, "  "); }

// Helper to pretty-print token/operator names
static const char* opTokName(int t) {
  switch (t) {
    case PLUS: return "+";   case MINUS: return "-";
    case ASTERIX: return "*"; case DIV: return "/";
    case MOD: return "%";    case LT: return "<";
    case LE: return "<=";    case GT: return ">";
    case GE: return ">=";    case EQ: return "==";
    case NE: return "!=";    case AND: return "&&";
    case OR: return "||";
    default: return "?";
  }
}

// ---- pretty-print helpers for Task 1.3 (string-based AST printing) ----
static inline std::string indentStr(int n) {
  return std::string(n * 2, ' ');
}

static inline void appendln(std::string& out, int indent, const std::string& line) {
  out += indentStr(indent);
  out += line;
  out += "\n";
}

// Prefix each line in 's' with (indent * 2) spaces.
// Used to “nest” child to_string() blocks nicely.
static inline std::string indentLines(const std::string& s, int indent) {
  std::string out;
  out.reserve(s.size() + 64);
  size_t start = 0;
  while (start < s.size()) {
    size_t nl = s.find('\n', start);
    if (nl == std::string::npos) nl = s.size();
    out += indentStr(indent);
    out.append(s, start, nl - start);
    out += "\n";
    start = (nl == s.size()) ? nl : nl + 1;
  }
  return out;
}


/// ASTnode - Base class for all AST nodes.
class ASTnode {
public:
  virtual ~ASTnode() {}
  virtual Value *codegen() { return nullptr; }

  // Simple type tags so main can order codegen without RTTI
  virtual bool isGlobVarDecl() const { return false; }
  virtual bool isFunctionDecl() const { return false; }


  // Public forwarder so derived nodes can invoke a child's inner printer
  void to_string_into(std::string& out, int indent) const {
    to_string_inner(out, indent);
  }

  // Public facade used by llvm::outs() << ast and by your own code.
  // It collects the full multi-line string starting at indent 0.
  virtual std::string to_string() const {
    std::string out;
    to_string_inner(out, /*indent=*/0);
    return out;
  }

  // Existing dump stays and now uses to_string() so both paths match.
  virtual void dump(int indent = 0) const {
    // We want dump() to respect the requested indent. We take the fully
    // generated string (which starts at indent 0) and re-indent it in one go.
    const std::string s = to_string();
    const std::string shifted = indentLines(s, indent);
    // print
    fwrite(shifted.data(), 1, shifted.size(), stderr);
  }

protected:
  // New hook: subclasses will override THIS to build the indented string.
  // Use appendln(out, indent, "Line...") and indentLines(childStr, indent+N).
  virtual void to_string_inner(std::string& out, int indent) const {
    appendln(out, indent, "<ASTnode>");
  }
};



/// IntASTnode - Class for integer literals like 1, 2, 10,
class IntASTnode : public ASTnode {
  int Val;
  TOKEN Tok;
  std::string Name;

public:
  IntASTnode(TOKEN tok, int val) : Val(val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// BoolASTnode - Class for boolean literals true and false,
class BoolASTnode : public ASTnode {
  bool Bool;
  TOKEN Tok;

public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// FloatASTnode - Node class for floating point literals like "1.0".
class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;

public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// VariableASTnode - Class for referencing a variable (i.e. identifier), like
/// "a".
enum IDENT_TYPE { IDENTIFIER = 0 };
class VariableASTnode : public ASTnode {
protected:
  TOKEN Tok;
  std::string Name;
  IDENT_TYPE VarType;

public:
  VariableASTnode(TOKEN tok, const std::string &Name)
      : Tok(tok), Name(Name), VarType(IDENT_TYPE::IDENTIFIER) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Tok.lexeme; }
  const IDENT_TYPE getVarType() const { return VarType; }
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// ParamAST - Class for a parameter declaration
class ParamAST {
  std::string Name;
  std::string Type;

public:
  ParamAST(const std::string &name, const std::string &type)
      : Name(name), Type(type) {}
  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
};

/// DeclAST - Base class for declarations, variables and functions
class DeclAST : public ASTnode {

public:
  virtual ~DeclAST() {}
};

/// VarDeclAST - Class for a variable declaration
class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }
};

class ArrayDeclAST : public VarDeclAST { 
  std::vector<int> Dimensions;
  
public:
  // Constructor
  ArrayDeclAST(std::unique_ptr<VariableASTnode> var, 
               const std::string& type, 
               std::vector<int> dimensions_list)
      : VarDeclAST(std::move(var), type), 
        Dimensions(std::move(dimensions_list)) {}
  
  // Getter - return Dimensions
  const std::vector<int>& getDims() const { 
    return Dimensions; 
  }
  
  // dump method
  void dump(int indent = 0) const override {
    indentOut(indent);
    fprintf(stderr, "ArrayDecl %s : %s [", 
            getName().c_str(), getType().c_str());
    
    for (size_t i = 0; i < Dimensions.size(); ++i) {
      fprintf(stderr, "%d", Dimensions[i]);
      if (i + 1 < Dimensions.size()) 
        fprintf(stderr, "][");
    }
    fprintf(stderr, "]\n");
  }
  
  // to_string_inner method
  void to_string_inner(std::string &out, int indent) const override {
    std::string line = "ArrayDecl " + getName() + " : " + getType() + " [";
    
    for (size_t i = 0; i < Dimensions.size(); ++i) {
      line += std::to_string(Dimensions[i]);
      if (i + 1 < Dimensions.size()) 
        line += "][";
    }
    line += "]";
    appendln(out, indent, line);
  }
};

/// GlobVarDeclAST - Class for a Global variable declaration
class GlobVarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string Type;

public:
  GlobVarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
      : Var(std::move(var)), Type(type) {}
  const std::string &getType() const { return Type; }
  const std::string &getName() const { return Var->getName(); }

  bool isGlobVarDecl() const override { return true; }

  Value *codegen() override;
};

/// FunctionPrototypeAST - Class for a function declaration's signature
class FunctionPrototypeAST {
  std::string Name;
  std::string Type;
  std::vector<std::unique_ptr<ParamAST>> Params; // vector of parameters

public:
  FunctionPrototypeAST(const std::string &name, const std::string &type,
                       std::vector<std::unique_ptr<ParamAST>> params)
      : Name(name), Type(type), Params(std::move(params)) {}

  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  int getSize() const { return Params.size(); }
  std::vector<std::unique_ptr<ParamAST>> &getParams() { return Params; }
  const std::vector<std::unique_ptr<ParamAST>>& getParams() const { return Params; }

};

class ExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Node1;
  char Op;
  std::unique_ptr<ASTnode> Node2;

public:
  ExprAST(std::unique_ptr<ASTnode> node1, char op,
          std::unique_ptr<ASTnode> node2)
      : Node1(std::move(node1)), Op(op), Node2(std::move(node2)) {}
  const std::string &getType();
};

/// UnaryExprAST - Expression class for unary operations (-, !)
class UnaryExprAST : public ASTnode {
  // variables to store the single character and operator
  char Op;
  std::unique_ptr<ASTnode> Operand;

public:
// constructor which runs when I create an object 
  UnaryExprAST(char op, std::unique_ptr<ASTnode> operand)
      : Op(op), Operand(std::move(operand)) {} // this is the intialiser list using param values
  
  // Getter methods
  char getOp() const { return Op; }
  ASTnode* getOperand() const { return Operand.get(); }
  
  // Virtual Methods
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// BinaryExprAST - Expression class for binary operators
class BinaryExprAST : public ASTnode {
  int OpTok;                            // TOKEN_TYPE value (PLUS, EQ, LE, etc.)
  std::unique_ptr<ASTnode> LHS, RHS;

public:
  BinaryExprAST(int opTok, std::unique_ptr<ASTnode> lhs,
                std::unique_ptr<ASTnode> rhs)
      : OpTok(opTok), LHS(std::move(lhs)), RHS(std::move(rhs)) {}

  int getOpTok() const { return OpTok; }
  ASTnode* getLHS() const { return LHS.get(); }
  ASTnode* getRHS() const { return RHS.get(); }

  Value *codegen() override;
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
};

/// AssignAST - Expression class for assignment: IDENT = expr
class AssignAST : public ASTnode {
  std::unique_ptr<VariableASTnode> LHS;
  std::unique_ptr<ASTnode> RHS;

public:
  AssignAST(std::unique_ptr<VariableASTnode> lhs,
            std::unique_ptr<ASTnode> rhs)
      : LHS(std::move(lhs)), RHS(std::move(rhs)) {}

  VariableASTnode* getLHS() const { return LHS.get(); }
  ASTnode* getRHS() const { return RHS.get(); }

  Value* codegen() override;           // declaration only
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
};

class ArrayAccessAST : public ASTnode {
  std::string name;                                    
  std::vector<std::unique_ptr<ASTnode>> indices;      
  
public:
  // Constructor 
  ArrayAccessAST(const std::string& name, 
                 std::vector<std::unique_ptr<ASTnode>> indices_param)
      : name(name), indices(std::move(indices_param)) {}
  
  // Getters 
  const std::string &getName() const {
    return name;
  }
  
  const std::vector<std::unique_ptr<ASTnode>> &getIndices() const {
    return indices;
  }
  
  // codegen 
  Value *codegen() override {
    fprintf(stderr,
            "ArrayAccessAST::codegen not implemented yet for '%s'\n",
            name.c_str());  //: lowercase 'name'
    exit(2);
  }
  
  // dump 
  void dump(int indent = 0) const override {
    indentOut(indent);
    fprintf(stderr, "ArrayAccess %s\n", name.c_str());  
    
    for (size_t i = 0; i < indices.size(); ++i) {        
      indentOut(indent + 1);
      fprintf(stderr, "index[%zu]:\n", i);
      if (indices[i]) indices[i]->dump(indent + 2);      
    }
  }
  
  // to_string_inner 
  void to_string_inner(std::string &out, int indent) const override {
    appendln(out, indent, "ArrayAccess " + name);        
    
    for (size_t i = 0; i < indices.size(); ++i) {        
      appendln(out, indent + 1,
               "index[" + std::to_string(i) + "]:");
      if (indices[i]) indices[i]->to_string_into(out, indent + 2);  
    }
  }
};


/// CallExprAST - Expression class for function calls
class CallExprAST : public ASTnode {
  std::string Callee;                         // Function name
  std::vector<std::unique_ptr<ASTnode>> Args; // Arguments

public:
  CallExprAST(const std::string &callee, 
              std::vector<std::unique_ptr<ASTnode>> args)
      : Callee(callee), Args(std::move(args)) {}
  
  const std::string &getCallee() const { return Callee; }
  const std::vector<std::unique_ptr<ASTnode>>& getArgs() const { return Args; }
  
  Value *codegen() override;
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
};

/// BlockAST - Class for a block with declarations followed by statements
class BlockAST : public ASTnode {
  std::vector<std::unique_ptr<VarDeclAST>> LocalDecls;
  std::vector<std::unique_ptr<ASTnode>> Stmts;

public:
  BlockAST(std::vector<std::unique_ptr<VarDeclAST>> localDecls,
           std::vector<std::unique_ptr<ASTnode>> stmts)
      : LocalDecls(std::move(localDecls)), Stmts(std::move(stmts)) {}

  // NEW getters:
  const std::vector<std::unique_ptr<VarDeclAST>>& getLocalDecls() const { return LocalDecls; }
  const std::vector<std::unique_ptr<ASTnode>>&    getStmts() const      { return Stmts; }

  // NEW dump:
  void dump(int indent = 0) const override {
    indentOut(indent); fprintf(stderr, "Block\n");
    indentOut(indent+1); fprintf(stderr, "Locals:\n");
    for (auto &d : LocalDecls) {
      indentOut(indent+2); fprintf(stderr, "VarDecl %s : %s\n",
        d->getName().c_str(), d->getType().c_str());
    }
    indentOut(indent+1); fprintf(stderr, "Stmts:\n");
    for (auto &s : Stmts) {
      if (s) s->dump(indent + 2);
    }
  }
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};



/// FunctionDeclAST - This class represents a function definition itself.
class FunctionDeclAST : public DeclAST {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;

public:
  FunctionDeclAST(std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)) {}

  bool isFunctionDecl() const override { return true; }

  // NEW getters:
  const FunctionPrototypeAST* getProto() const { return Proto.get(); }
  const ASTnode*              getBody()  const { return Block.get(); }

  // NEW dump:
  void dump(int indent = 0) const override {
    indentOut(indent);
    fprintf(stderr, "Function %s : %s\n",
            Proto ? Proto->getName().c_str() : "<anon>",
            Proto ? Proto->getType().c_str() : "<unknown>");
    if (Proto) {
      indentOut(indent+1); fprintf(stderr, "Params (%d):\n", Proto->getSize());
      int i = 0;
      for (auto &p : Proto->getParams()) {
        indentOut(indent+2);
        fprintf(stderr, "%d: %s : %s\n", i++, p->getName().c_str(), p->getType().c_str());
      }
    }
    if (Block) {
      indentOut(indent+1); fprintf(stderr, "Body:\n");
      Block->dump(indent + 2);
    }
  }
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

void FunctionDeclAST::to_string_inner(std::string& out, int indent) const {
  const auto* P = Proto.get();
  appendln(out, indent, std::string("Function ") +
                       (P ? P->getName() : "<anon>") + " : " +
                       (P ? P->getType() : "<unknown>"));
  if (P) {
    appendln(out, indent + 1, "Params (" + std::to_string(P->getSize()) + "):");
    int i = 0;
    for (const auto& param : P->getParams()) {  // uses the const getter
      appendln(out, indent + 2,
               std::to_string(i++) + ": " + param->getName() + " : " + param->getType());
    }
  }
  if (Block) {
    appendln(out, indent + 1, "Body:");
    Block->to_string_into(out, indent + 2);
  }
}




static std::vector<std::unique_ptr<FunctionPrototypeAST>> gExterns;
static std::vector<std::unique_ptr<ASTnode>>              gTopDecls;


/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ASTnode> Cond, std::unique_ptr<ASTnode> Then,
            std::unique_ptr<ASTnode> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// WhileExprAST - Expression class for while.
class WhileExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Body;

public:
  WhileExprAST(std::unique_ptr<ASTnode> cond, std::unique_ptr<ASTnode> body)
      : Cond(std::move(cond)), Body(std::move(body)) {}
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};

/// ReturnAST - Class for a return value
class ReturnAST : public ASTnode {
  std::unique_ptr<ASTnode> Val;

public:
  ReturnAST(std::unique_ptr<ASTnode> value) : Val(std::move(value)) {}
  void dump(int indent = 0) const override;
  void to_string_inner(std::string& out, int indent) const override;
  Value *codegen() override;
};


/// ArgsAST - Class for a function argumetn in a function call
class ArgsAST : public ASTnode {
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> ArgsList;

public:
  ArgsAST(const std::string &Callee, std::vector<std::unique_ptr<ASTnode>> list)
      : Callee(Callee), ArgsList(std::move(list)) {}

};

/// LogError* - These are little helper function for error handling.
std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  fprintf(stderr, "%d:%d Error: %s\n", tok.lineNo, tok.columnNo, Str);
  exit(2);
  return nullptr;
}

std::unique_ptr<FunctionPrototypeAST> LogErrorP(TOKEN tok, const char *Str) {
  LogError(tok, Str);
  exit(2);
  return nullptr;
}

std::unique_ptr<ASTnode> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  exit(2);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Recursive Descent - Function call for each production
//===----------------------------------------------------------------------===//

static std::unique_ptr<ASTnode> ParseDecl();
static std::unique_ptr<ASTnode> ParseStmt();
static std::unique_ptr<ASTnode> ParseBlock();
static std::unique_ptr<ASTnode> ParseExper();
static std::unique_ptr<ParamAST> ParseParam();
static std::unique_ptr<VarDeclAST> ParseLocalDecl();
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime();

// ---- Expressions (forward declarations) ----
static std::unique_ptr<ASTnode> ParseRval();
static std::unique_ptr<ASTnode> ParseOrExpr();
static std::unique_ptr<ASTnode> ParseAndExpr();
static std::unique_ptr<ASTnode> ParseEqExpr();
static std::unique_ptr<ASTnode> ParseRelExpr();
static std::unique_ptr<ASTnode> ParseAddExpr();
static std::unique_ptr<ASTnode> ParseMulExpr();
static std::unique_ptr<ASTnode> ParseUnary();
static std::unique_ptr<ASTnode> ParsePrimary();
static void ParseArgs(std::vector<std::unique_ptr<ASTnode>>& out);


// ---- operator helpers for precedence layers ----
static inline bool isMulOp(int t){ return t==ASTERIX || t==DIV || t==MOD; }
static inline bool isAddOp(int t){ return t==PLUS    || t==MINUS; }
static inline bool isRelOp(int t){ return t==LT || t==LE || t==GT || t==GE; }
static inline bool isEqOp (int t){ return t==EQ || t==NE; }



// element ::= FLOAT_LIT
static std::unique_ptr<ASTnode> ParseFloatNumberExpr() {
  auto Result = std::make_unique<FloatASTnode>(CurTok, CurTok.getFloatVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= INT_LIT
static std::unique_ptr<ASTnode> ParseIntNumberExpr() {
  auto Result = std::make_unique<IntASTnode>(CurTok, CurTok.getIntVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// element ::= BOOL_LIT
static std::unique_ptr<ASTnode> ParseBoolExpr() {
  auto Result = std::make_unique<BoolASTnode>(CurTok, CurTok.getBoolVal());
  getNextToken(); // consume the number
  return std::move(Result);
}

// param_list_prime ::= "," param param_list_prime
//                   |  ε
static std::vector<std::unique_ptr<ParamAST>> ParseParamListPrime() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  if (CurTok.type == COMMA) { // more parameters in list
    getNextToken();           // eat ","

    auto param = ParseParam();
    if (param) {
      printf("found param in param_list_prime: %s\n", param->getName().c_str());
      param_list.push_back(std::move(param));
      auto param_list_prime = ParseParamListPrime();
      for (unsigned i = 0; i < param_list_prime.size(); i++) {
        param_list.push_back(std::move(param_list_prime.at(i)));
      }
    }
  } else if (CurTok.type == RPAR) { // FOLLOW(param_list_prime)
    // expand by param_list_prime ::= ε
    // do nothing
  } else {
    LogError(CurTok, "expected ',' or ')' in list of parameter declarations");
  }

  return param_list;
}

// param ::= var_type IDENT
static std::unique_ptr<ParamAST> ParseParam() {
  std::string Type = CurTok.lexeme;  // Save the type (int/float/bool)
  getNextToken();  // eat type token

  if (CurTok.type == IDENT) {
    std::string Name = CurTok.getIdentifierStr();  // Save parameter name
    getNextToken();  // eat IDENT
    
    // FIX: Actually create and return the ParamAST
    return std::make_unique<ParamAST>(Name, Type);
  }

  return nullptr;  // Error case: no IDENT after type
}

// param_list ::= param param_list_prime
static std::vector<std::unique_ptr<ParamAST>> ParseParamList() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  auto param = ParseParam();
  if (param) {
    param_list.push_back(std::move(param));
    auto param_list_prime = ParseParamListPrime();
    for (unsigned i = 0; i < param_list_prime.size(); i++) {
      param_list.push_back(std::move(param_list_prime.at(i)));
    }
  }

  return param_list;
}

// params ::= param_list
//         |  ε
static std::vector<std::unique_ptr<ParamAST>> ParseParams() {
  std::vector<std::unique_ptr<ParamAST>> param_list;

  std::string Type;
  std::string Name = "";

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(param_list)

    auto list = ParseParamList();
    for (unsigned i = 0; i < list.size(); i++) {
      param_list.push_back(std::move(list.at(i)));
    }

  } else if (CurTok.type == VOID_TOK) { // FIRST("void")
    // void
    // check that the next token is a )
    getNextToken(); // eat 'void'
    if (CurTok.type != RPAR) {
      LogError(CurTok, "expected ')', after 'void' in \
       end of function declaration");
    }
  } else if (CurTok.type == RPAR) { // FOLLOW(params)
    // expand by params ::= ε
    // do nothing
  } else {
    LogError(
        CurTok,
        "expected 'int', 'bool' or 'float' in function declaration or ') in \
       end of function declaration");
  }

  return param_list;
}

/*** TODO : Task 2 - Parser ***

// args ::= arg_list
//      |  ε
// arg_list ::= arg_list "," expr
//      | expr

// rval ::= rval "||" rval
//      | rval "&&" rval
//      | rval "==" rval | rval "!=" rval
//      | rval "<=" rval | rval "<" rval | rval ">=" rval | rval ">" rval
//      | rval "+" rval | rval "-" rval
//      | rval "*" rval | rval "/" rval | rval "%" rval
//      | "-" rval | "!" rval
//      | "(" expr ")"
//      | IDENT | IDENT "(" args ")"
//      | INT_LIT | FLOAT_LIT | BOOL_LIT
**/


/*** Expression Parsing - TODO: Implement these functions ***/



// -------- args --------
// args ::= arg_list | ε
// arg_list ::= expr ("," expr)*
static void ParseArgs(std::vector<std::unique_ptr<ASTnode>>& out) {
  if (CurTok.type == RPAR) return; // ε
  // first expr
  if (auto e = ParseExper()) out.push_back(std::move(e));
  else LogError(CurTok, "expected expression in argument list");
  // ("," expr)*
  while (CurTok.type == COMMA) {
    getNextToken(); // eat ','
    if (auto e = ParseExper()) out.push_back(std::move(e));
    else LogError(CurTok, "expected expression after ',' in argument list");
  }
}

// NEW: parse one or more [expr] suffixes on an identifier in expressions
// array_index_suffix ::= "[" expr "]" ( "[" expr "]" )*
static std::vector<std::unique_ptr<ASTnode>> ParseArrayIndices() {
  std::vector<std::unique_ptr<ASTnode>> indices;

  // We are called when CurTok.type == LBOX
  while (CurTok.type == LBOX) {
    getNextToken(); // eat '['
    auto idx = ParseExper();
    if (!idx) LogError(CurTok, "expected expression inside array index");

    if (CurTok.type != RBOX) {
      LogError(CurTok, "expected ']' after array index expression");
    }
    getNextToken(); // eat ']'

    indices.push_back(std::move(idx));

    if (indices.size() == 3) break; // only 1D..3D allowed
  }

  return indices;
}


// -------- primary --------
// primary ::= '(' expr ')' | IDENT | IDENT '(' args ')' | INT_LIT | FLOAT_LIT | BOOL_LIT
static std::unique_ptr<ASTnode> ParsePrimary() {
  switch (CurTok.type) {
    case LPAR: {
      getNextToken(); // eat '('
      auto inner = ParseExper();
      if (!inner) return nullptr;
      if (CurTok.type != RPAR) return LogError(CurTok, "expected ')'");
      getNextToken(); // eat ')'
      return inner;
    }
    case IDENT: {
      TOKEN identTok = CurTok;
      std::string name = identTok.getIdentifierStr();
      getNextToken(); // eat IDENT

      if (CurTok.type == LPAR) {
        // function call
        getNextToken(); // eat '('
        std::vector<std::unique_ptr<ASTnode>> args;
        ParseArgs(args);
        if (CurTok.type != RPAR) return LogError(CurTok, "expected ')' after arguments");
        getNextToken(); // eat ')'
        return std::make_unique<CallExprAST>(name, std::move(args));
      }
      // variable reference
      return std::make_unique<VariableASTnode>(identTok, name);
    }
    case INT_LIT:
      return ParseIntNumberExpr();
    case FLOAT_LIT:
      return ParseFloatNumberExpr();
    case BOOL_LIT:
      return ParseBoolExpr();
    default:
      return LogError(CurTok, "expected primary expression");
  }
}

// -------- unary --------
// unary ::= '-' unary | '!' unary | primary
static std::unique_ptr<ASTnode> ParseUnary() {
  if (CurTok.type == MINUS || CurTok.type == NOT) {
    char opch = (CurTok.type == MINUS) ? '-' : '!';
    getNextToken(); // eat op
    auto operand = ParseUnary();
    if (!operand) return nullptr;
    return std::make_unique<UnaryExprAST>(opch, std::move(operand));
  }
  return ParsePrimary();
}

// -------- multiplicative --------
// mul_expr ::= unary (('*' | '/' | '%') unary)*
static std::unique_ptr<ASTnode> ParseMulExpr() {
  auto lhs = ParseUnary();
  if (!lhs) return nullptr;
  while (isMulOp(CurTok.type)) {
    int opTok = CurTok.type;
    getNextToken(); // eat op
    auto rhs = ParseUnary();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- additive --------
// add_expr ::= mul_expr (('+' | '-') mul_expr)*
static std::unique_ptr<ASTnode> ParseAddExpr() {
  auto lhs = ParseMulExpr();
  if (!lhs) return nullptr;
  while (isAddOp(CurTok.type)) {
    int opTok = CurTok.type;
    getNextToken(); // eat op
    auto rhs = ParseMulExpr();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- relational --------
// rel_expr ::= add_expr (('<' | '<=' | '>' | '>=') add_expr)*
static std::unique_ptr<ASTnode> ParseRelExpr() {
  auto lhs = ParseAddExpr();
  if (!lhs) return nullptr;
  while (isRelOp(CurTok.type)) {
    int opTok = CurTok.type;
    getNextToken(); // eat op
    auto rhs = ParseAddExpr();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- equality --------
// eq_expr ::= rel_expr (('==' | '!=') rel_expr)*
static std::unique_ptr<ASTnode> ParseEqExpr() {
  auto lhs = ParseRelExpr();
  if (!lhs) return nullptr;
  while (isEqOp(CurTok.type)) {
    int opTok = CurTok.type;
    getNextToken(); // eat op
    auto rhs = ParseRelExpr();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- logical and --------
// and_expr ::= eq_expr ( '&&' eq_expr )*
static std::unique_ptr<ASTnode> ParseAndExpr() {
  auto lhs = ParseEqExpr();
  if (!lhs) return nullptr;
  while (CurTok.type == AND) {
    int opTok = CurTok.type;
    getNextToken(); // eat '&&'
    auto rhs = ParseEqExpr();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- logical or --------
// or_expr ::= and_expr ( '||' and_expr )*
static std::unique_ptr<ASTnode> ParseOrExpr() {
  auto lhs = ParseAndExpr();
  if (!lhs) return nullptr;
  while (CurTok.type == OR) {
    int opTok = CurTok.type;
    getNextToken(); // eat '||'
    auto rhs = ParseAndExpr();
    if (!rhs) return nullptr;
    lhs = std::make_unique<BinaryExprAST>(opTok, std::move(lhs), std::move(rhs));
  }
  return lhs;
}

// -------- rval --------
static std::unique_ptr<ASTnode> ParseRval() {
  return ParseOrExpr();
}





// expr ::= IDENT "=" expr
//      |  rval
static std::unique_ptr<ASTnode> ParseExper() {
  //
  // TO BE COMPLETED

  if (CurTok.type == IDENT) {
    TOKEN identTok = CurTok;                       // current IDENT
    std::string name = identTok.getIdentifierStr();
    TOKEN next = getNextToken();                   // peek 1 token ahead (now CurTok == next)

    if (next.type == ASSIGN) {
      // IDENT '=' expr  (right-assoc)
      getNextToken();                              // move to first token of RHS
      auto rhs = ParseExper();
      if (!rhs) return nullptr;
      auto lhsVar = std::make_unique<VariableASTnode>(identTok, name);
      return std::make_unique<AssignAST>(std::move(lhsVar), std::move(rhs));
    }

    // NOT an assignment:
    // Restore to the state BEFORE peeking:
    //  - Put back ONLY the peeked token.
    //  - Set CurTok back to the IDENT (do NOT push IDENT into the buffer).
    putBackToken(next);
    CurTok = identTok;

    // Now parse as a normal rval.
  }

  return ParseRval();
}

// expr_stmt ::= expr ";"
//            |  ";"
static std::unique_ptr<ASTnode> ParseExperStmt() {

  if (CurTok.type == SC) { // empty statement
    getNextToken();        // eat ;
    return nullptr;
  } else {
    auto expr = ParseExper();
    if (expr) {
      if (CurTok.type == SC) {
        getNextToken(); // eat ;
        return expr;
      } else {
        LogError(CurTok, "expected ';' to end expression statement");
      }
    } else
      return nullptr;
  }
  return nullptr;
}

// else_stmt  ::= "else" block
//             |  ε
static std::unique_ptr<ASTnode> ParseElseStmt() {

  if (CurTok.type == ELSE) { // FIRST(else_stmt)
    // expand by else_stmt  ::= "else" "{" stmt "}"
    getNextToken(); // eat "else"

    if (!(CurTok.type == LBRA)) {
      return LogError(
          CurTok, "expected { to start else block of if-then-else statment");
    }
    auto Else = ParseBlock();
    if (!Else)
      return nullptr;
    return Else;
  } else if (CurTok.type == NOT || CurTok.type == MINUS ||
             CurTok.type == PLUS || CurTok.type == LPAR ||
             CurTok.type == IDENT || CurTok.type == INT_LIT ||
             CurTok.type == BOOL_LIT || CurTok.type == FLOAT_LIT ||
             CurTok.type == SC || CurTok.type == LBRA || CurTok.type == WHILE ||
             CurTok.type == IF || CurTok.type == ELSE ||
             CurTok.type == RETURN ||
             CurTok.type == RBRA) { // FOLLOW(else_stmt)
    // expand by else_stmt  ::= ε
    // return an empty statement
    return nullptr;
  } else
    LogError(CurTok, "expected 'else' or one of \
    '!', '-', '+', '(' , IDENT , INT_LIT, BOOL_LIT, FLOAT_LIT, ';', \
    '{', 'while', 'if', 'else', ε, 'return', '}' ");

  return nullptr;
}

// if_stmt ::= "if" "(" expr ")" block else_stmt
static std::unique_ptr<ASTnode> ParseIfStmt() {
  getNextToken(); // eat the if.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    // condition.
    auto Cond = ParseExper();
    if (!Cond)
      return nullptr;
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected )");
    getNextToken(); // eat )

    if (!(CurTok.type == LBRA)) {
      return LogError(CurTok, "expected { to start then block of if statment");
    }

    auto Then = ParseBlock();
    if (!Then)
      return nullptr;
    auto Else = ParseElseStmt();

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                       std::move(Else));

  } else
    return LogError(CurTok, "expected (");

  return nullptr;
}

// return_stmt ::= "return" ";"
//             |  "return" expr ";"
static std::unique_ptr<ASTnode> ParseReturnStmt() {
  getNextToken(); // eat the return
  if (CurTok.type == SC) {
    getNextToken(); // eat the ;
    // return a null value
    return std::make_unique<ReturnAST>(std::move(nullptr));
  } else if (CurTok.type == NOT || CurTok.type == MINUS ||
             CurTok.type == PLUS || CurTok.type == LPAR ||
             CurTok.type == IDENT || CurTok.type == BOOL_LIT ||
             CurTok.type == INT_LIT ||
             CurTok.type == FLOAT_LIT) { // FIRST(expr)
    auto val = ParseExper();
    if (!val)
      return nullptr;

    if (CurTok.type == SC) {
      getNextToken(); // eat the ;
      return std::make_unique<ReturnAST>(std::move(val));
    } else
      return LogError(CurTok, "expected ';'");
  } else
    return LogError(CurTok, "expected ';' or expression");

  return nullptr;
}

// while_stmt ::= "while" "(" expr ")" stmt
static std::unique_ptr<ASTnode> ParseWhileStmt() {

  getNextToken(); // eat the while.
  if (CurTok.type == LPAR) {
    getNextToken(); // eat (
    // condition.
    auto Cond = ParseExper();
    if (!Cond)
      return nullptr;
    if (CurTok.type != RPAR)
      return LogError(CurTok, "expected )");
    getNextToken(); // eat )

    auto Body = ParseStmt();
    if (!Body)
      return nullptr;

    return std::make_unique<WhileExprAST>(std::move(Cond), std::move(Body));
  } else
    return LogError(CurTok, "expected (");
}

// stmt ::= expr_stmt
//      |  block
//      |  if_stmt
//      |  while_stmt
//      |  return_stmt
static std::unique_ptr<ASTnode> ParseStmt() {

  if (CurTok.type == NOT || CurTok.type == MINUS || CurTok.type == PLUS ||
      CurTok.type == LPAR || CurTok.type == IDENT || CurTok.type == BOOL_LIT ||
      CurTok.type == INT_LIT || CurTok.type == FLOAT_LIT ||
      CurTok.type == SC) { // FIRST(expr_stmt)
    // expand by stmt ::= expr_stmt
    auto expr_stmt = ParseExperStmt();
    fprintf(stderr, "Parsed an expression statement\n");
    return expr_stmt;
  } else if (CurTok.type == LBRA) { // FIRST(block)
    auto block_stmt = ParseBlock();
    if (block_stmt) {
      fprintf(stderr, "Parsed a block\n");
      return block_stmt;
    }
  } else if (CurTok.type == IF) { // FIRST(if_stmt)
    auto if_stmt = ParseIfStmt();
    if (if_stmt) {
      fprintf(stderr, "Parsed an if statment\n");
      return if_stmt;
    }
  } else if (CurTok.type == WHILE) { // FIRST(while_stmt)
    auto while_stmt = ParseWhileStmt();
    if (while_stmt) {
      fprintf(stderr, "Parsed a while statment\n");
      return while_stmt;
    }
  } else if (CurTok.type == RETURN) { // FIRST(return_stmt)
    auto return_stmt = ParseReturnStmt();
    if (return_stmt) {
      fprintf(stderr, "Parsed a return statment\n");
      return return_stmt;
    }
  }
  // else if(CurTok.type == RBRA) { // FOLLOW(stmt_list_prime)
  //  expand by stmt_list_prime ::= ε
  //  do nothing
  //}
  else { // syntax error
    return LogError(CurTok, "expected BLA BLA\n");
  }
  return nullptr;
}

// stmt_list ::= stmt stmt_list_prime
static std::vector<std::unique_ptr<ASTnode>> ParseStmtList() {
  std::vector<std::unique_ptr<ASTnode>> stmt_list; // vector of statements
  auto stmt = ParseStmt();
  if (stmt) {
    stmt_list.push_back(std::move(stmt));
  }
  auto stmt_list_prime = ParseStmtListPrime();
  for (unsigned i = 0; i < stmt_list_prime.size(); i++) {
    stmt_list.push_back(std::move(stmt_list_prime.at(i)));
  }
  return stmt_list;
}

// stmt_list_prime ::= stmt stmt_list_prime
//                  |  ε
static std::vector<std::unique_ptr<ASTnode>> ParseStmtListPrime() {
  std::vector<std::unique_ptr<ASTnode>> stmt_list; // vector of statements
  if (CurTok.type == NOT || CurTok.type == MINUS || CurTok.type == PLUS ||
      CurTok.type == LPAR || CurTok.type == IDENT || CurTok.type == BOOL_LIT ||
      CurTok.type == INT_LIT || CurTok.type == FLOAT_LIT || CurTok.type == SC ||
      CurTok.type == LBRA || CurTok.type == WHILE || CurTok.type == IF ||
      CurTok.type == ELSE || CurTok.type == RETURN) { // FIRST(stmt)
    // expand by stmt_list ::= stmt stmt_list_prime
    auto stmt = ParseStmt();
    if (stmt) {
      stmt_list.push_back(std::move(stmt));
    }
    auto stmt_prime = ParseStmtListPrime();
    for (unsigned i = 0; i < stmt_prime.size(); i++) {
      stmt_list.push_back(std::move(stmt_prime.at(i)));
    }

  } else if (CurTok.type == RBRA) { // FOLLOW(stmt_list_prime)
    // expand by stmt_list_prime ::= ε
    // do nothing
  }
  return stmt_list; // note stmt_list can be empty as we can have empty blocks,
                    // etc.
}

// local_decls_prime ::= local_decl local_decls_prime
//                    |  ε
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDeclsPrime() {
  std::vector<std::unique_ptr<VarDeclAST>>
      local_decls_prime; // vector of local decls

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(local_decl)
    auto local_decl = ParseLocalDecl();
    if (local_decl) {
      local_decls_prime.push_back(std::move(local_decl));
    }
    auto prime = ParseLocalDeclsPrime();
    for (unsigned i = 0; i < prime.size(); i++) {
      local_decls_prime.push_back(std::move(prime.at(i)));
    }
  } else if (CurTok.type == MINUS || CurTok.type == NOT ||
             CurTok.type == LPAR || CurTok.type == IDENT ||
             CurTok.type == INT_LIT || CurTok.type == FLOAT_LIT ||
             CurTok.type == BOOL_LIT || CurTok.type == SC ||
             CurTok.type == LBRA || CurTok.type == IF || CurTok.type == WHILE ||
             CurTok.type == RETURN) { // FOLLOW(local_decls_prime)
    // expand by local_decls_prime ::=  ε
    // do nothing;
  } else {
    LogError(
        CurTok,
        "expected '-', '!', ('' , IDENT , STRING_LIT , INT_LIT , FLOAT_LIT, \
      BOOL_LIT, ';', '{', 'if', 'while', 'return' after local variable declaration\n");
  }

  return local_decls_prime;
}


// NEW: parse 1–3 constant dimensions for a declaration
// array_suffix ::= "[" INT_LIT "]"
//                | "[" INT_LIT "]" "[" INT_LIT "]"
//                | "[" INT_LIT "]" "[" INT_LIT "]" "[" INT_LIT "]"
static std::vector<int> ParseArrayDimsDecl() {
  std::vector<int> dims;

  // We are called when CurTok.type is LBOX
  while (CurTok.type == LBOX) {
    getNextToken(); // eat '['

    if (CurTok.type != INT_LIT) {
      LogError(CurTok, "expected integer literal inside array dimension");
    }
    int size = CurTok.getIntVal();
    dims.push_back(size);
    getNextToken(); // eat INT_LIT

    if (CurTok.type != RBOX) {
      LogError(CurTok, "expected ']' after array dimension");
    }
    getNextToken(); // eat ']'

    if (dims.size() == 3) break; // only 1D, 2D, 3D
  }

  if (dims.empty()) {
    LogError(CurTok, "array declaration must have at least one dimension");
  }

  return dims;
}



// local_decl ::= var_type IDENT ";"
// var_type ::= "int"
//           |  "float"
//           |  "bool"
static std::unique_ptr<VarDeclAST> ParseLocalDecl() {
  TOKEN PrevTok;
  std::string Type;
  std::string Name = "";
  std::unique_ptr<VarDeclAST> local_decl;

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(var_type)
    PrevTok = CurTok;
    getNextToken(); // eat 'int' or 'float or 'bool'
    if (CurTok.type == IDENT) {
      Type = PrevTok.lexeme;
      Name = CurTok.getIdentifierStr(); // save the identifier name
      auto ident = std::make_unique<VariableASTnode>(CurTok, Name);
      local_decl = std::make_unique<VarDeclAST>(std::move(ident), Type);

      getNextToken(); // eat 'IDENT'
      if (CurTok.type != SC) {
        LogError(CurTok, "Expected ';' to end local variable declaration");
        return nullptr;
      }
      getNextToken(); // eat ';'
      fprintf(stderr, "Parsed a local variable declaration\n");
    } else {
      LogError(CurTok, "expected identifier' in local variable declaration");
      return nullptr;
    }
  }
  return local_decl;
}

// local_decls ::= local_decl local_decls_prime
static std::vector<std::unique_ptr<VarDeclAST>> ParseLocalDecls() {
  std::vector<std::unique_ptr<VarDeclAST>> local_decls; // vector of local decls

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK ||
      CurTok.type == BOOL_TOK) { // FIRST(local_decl)

    auto local_decl = ParseLocalDecl();
    if (local_decl) {
      local_decls.push_back(std::move(local_decl));
    }
    auto local_decls_prime = ParseLocalDeclsPrime();
    for (unsigned i = 0; i < local_decls_prime.size(); i++) {
      local_decls.push_back(std::move(local_decls_prime.at(i)));
    }

  } else if (CurTok.type == MINUS || CurTok.type == NOT ||
             CurTok.type == LPAR || CurTok.type == IDENT ||
             CurTok.type == INT_LIT || CurTok.type == RETURN ||
             CurTok.type == FLOAT_LIT || CurTok.type == BOOL_LIT ||
             CurTok.type == COMMA || CurTok.type == LBRA || CurTok.type == IF ||
             CurTok.type == WHILE) { // FOLLOW(local_decls)
                                     // do nothing
  } else {
    LogError(
        CurTok,
        "expected '-', '!', '(' , IDENT , STRING_LIT , INT_LIT , FLOAT_LIT, \
        BOOL_LIT, ';', '{', 'if', 'while', 'return'");
  }

  return local_decls;
}

// parse block
// block ::= "{" local_decls stmt_list "}"
static std::unique_ptr<ASTnode> ParseBlock() {
  std::vector<std::unique_ptr<VarDeclAST>> local_decls; // vector of local decls
  std::vector<std::unique_ptr<ASTnode>> stmt_list;      // vector of statements

  getNextToken(); // eat '{'

  local_decls = ParseLocalDecls();
  fprintf(stderr, "Parsed a set of local variable declaration\n");
  stmt_list = ParseStmtList();
  fprintf(stderr, "Parsed a list of statements\n");
  if (CurTok.type == RBRA)
    getNextToken(); // eat '}'
  else {            // syntax error
    LogError(CurTok, "expected '}' , close body of block");
    return nullptr;
  }

  return std::make_unique<BlockAST>(std::move(local_decls),
                                    std::move(stmt_list));
}

// decl ::= var_decl
//       |  fun_decl
static std::unique_ptr<ASTnode> ParseDecl() {
  std::string IdName;
  std::vector<std::unique_ptr<ParamAST>> param_list;

  TOKEN PrevTok = CurTok; // to keep track of the type token

  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {
    getNextToken(); // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

    IdName = CurTok.getIdentifierStr(); // save the identifier name

    if (CurTok.type == IDENT) {
      auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
      getNextToken(); // eat the IDENT
      if (CurTok.type ==
          SC) {         // found ';' then this is a global variable declaration.
        getNextToken(); // eat ;
        fprintf(stderr, "Parsed a variable declaration\n");

        if (PrevTok.type != VOID_TOK)
          return std::make_unique<GlobVarDeclAST>(std::move(ident),
                                                  PrevTok.lexeme);
        else
          return LogError(PrevTok,
                          "Cannot have variable declaration with type 'void'");
      } else if (CurTok.type ==
                 LPAR) { // found '(' then this is a function declaration.
        getNextToken();  // eat (

        auto P =
            ParseParams(); // parse the parameters, returns a vector of params
        // if (P.size() == 0) return nullptr;
        fprintf(stderr, "Parsed parameter list for function\n");

        if (CurTok.type != RPAR) // syntax error
          return LogError(CurTok, "expected ')' in function declaration");

        getNextToken();          // eat )
        if (CurTok.type != LBRA) // syntax error
          return LogError(
              CurTok, "expected '{' in function declaration, function body");

        auto B = ParseBlock(); // parse the function body
        if (!B)
          return nullptr;
        else
          fprintf(stderr, "Parsed block of statements in function\n");

        // now create a Function prototype
        // create a Function body
        // put these to together
        // and return a std::unique_ptr<FunctionDeclAST>
        fprintf(stderr, "Parsed a function declaration\n");

        auto Proto = std::make_unique<FunctionPrototypeAST>(
            IdName, PrevTok.lexeme, std::move(P));
        return std::make_unique<FunctionDeclAST>(std::move(Proto),
                                                 std::move(B));
      } else
        return LogError(CurTok, "expected ';' or ('");
    } else
      return LogError(CurTok, "expected an identifier");

  } else
    LogError(CurTok,
             "expected 'void', 'int' or 'float' or EOF token"); // syntax error

  return nullptr;
}

// decl_list_prime ::= decl decl_list_prime
//                  |  ε
static void ParseDeclListPrime() {
  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) { // FIRST(decl)

    if (auto decl = ParseDecl()) {
      fprintf(stderr, "Parsed a top-level variable or function declaration\n");
      gTopDecls.push_back(std::move(decl));  // store it
    }
    ParseDeclListPrime();
  } else if (CurTok.type == EOF_TOK) { // FOLLOW(decl_list_prime)
    // expand by decl_list_prime ::= ε
    // do nothing
  } else { // syntax error
    LogError(CurTok, "expected 'void', 'int', 'bool' or 'float' or EOF token");
  }
}

// decl_list ::= decl decl_list_prime
static void ParseDeclList() {
  auto decl = ParseDecl();
  if (decl) {
    fprintf(stderr, "Parsed a top-level variable or function declaration\n");
    gTopDecls.push_back(std::move(decl));  // store it
    ParseDeclListPrime();
  }
}

// extern ::= "extern" type_spec IDENT "(" params ")" ";"
static std::unique_ptr<FunctionPrototypeAST> ParseExtern() {
  std::string IdName;
  TOKEN PrevTok;

  if (CurTok.type == EXTERN) {
    getNextToken(); // eat the EXTERN

    if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
        CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {

      PrevTok = CurTok; // to keep track of the type token
      getNextToken();   // eat the VOID_TOK, INT_TOK, BOOL_TOK or FLOAT_TOK

      if (CurTok.type == IDENT) {
        IdName = CurTok.getIdentifierStr(); // save the identifier name
        auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
        getNextToken(); // eat the IDENT

        if (CurTok.type ==
            LPAR) {       // found '(' - this is an extern function declaration.
          getNextToken(); // eat (

          auto P =
              ParseParams(); // parse the parameters, returns a vector of params
          if (P.size() == 0)
            return nullptr;
          else
            fprintf(stderr, "Parsed parameter list for external function\n");

          if (CurTok.type != RPAR) // syntax error
            return LogErrorP(
                CurTok, "expected ')' in closing extern function declaration");

          getNextToken(); // eat )

          if (CurTok.type == SC) {
            getNextToken(); // eat ";"
            auto Proto = std::make_unique<FunctionPrototypeAST>(
                IdName, PrevTok.lexeme, std::move(P));
            return Proto;
          } else
            return LogErrorP(
                CurTok,
                "expected ;' in ending extern function declaration statement");
        } else
          return LogErrorP(CurTok,
                           "expected (' in extern function declaration");
      }

    } else
      LogErrorP(CurTok, "expected 'void', 'int' or 'float' in extern function "
                        "declaration\n"); // syntax error
  }

  return nullptr;
}

// extern_list_prime ::= extern extern_list_prime
//                   |  ε
static void ParseExternListPrime() {

  if (CurTok.type == EXTERN) { // FIRST(extern)
    if (auto Extern = ParseExtern()) {
      fprintf(stderr,
              "Parsed a top-level external function declaration -- 2\n");
      gExterns.push_back(std::move(Extern));
    }
    ParseExternListPrime();
  } else if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
             CurTok.type == FLOAT_TOK ||
             CurTok.type == BOOL_TOK) { // FOLLOW(extern_list_prime)
    // expand by decl_list_prime ::= ε
    // do nothing
  } else { // syntax error
    LogError(CurTok, "expected 'extern' or 'void',  'int' ,  'float',  'bool'");
  }
}

// extern_list ::= extern extern_list_prime
static void ParseExternList() {
  auto Extern = ParseExtern();
  if (Extern) {
    fprintf(stderr, "Parsed a top-level external function declaration -- 1\n");
    // fprintf(stderr, "Current token: %s \n", CurTok.lexeme.c_str());
    gExterns.push_back(std::move(Extern));  // store it
    if (CurTok.type == EXTERN)
      ParseExternListPrime();
  }
}

// program ::= extern_list decl_list
static void parser() {
  if (CurTok.type == EOF_TOK)
    return;
  ParseExternList();
  if (CurTok.type == EOF_TOK)
    return;
  ParseDeclList();
  if (CurTok.type == EOF_TOK)
    return;
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

// ---- Codegen globals and helpers ----

// Current frame locals (name -> alloca)
static std::map<std::string, AllocaInst*> NamedValues;

// Stack of outer frames
static std::vector<std::map<std::string, AllocaInst*>> ScopeStack;

// Global symbols for file scope (name -> GlobalVariable)
static std::map<std::string, GlobalVariable*> GlobalNamedValues;

// Canonical MiniC types
static Type* miniCIntTy()   { return Type::getInt32Ty(TheContext); }
static Type* miniCBoolTy()  { return Type::getInt1Ty(TheContext);  }
static Type* miniCFloatTy() { return Type::getFloatTy(TheContext); }  // MiniC float

// Map parsed type strings to LLVM types
static Type* typeFromString(const std::string& t) {
  if (t == "int")   return miniCIntTy();
  if (t == "bool")  return miniCBoolTy();
  if (t == "float") return miniCFloatTy();
  if (t == "void")  return Type::getVoidTy(TheContext);
  fprintf(stderr, "Unknown type '%s'\n", t.c_str());
  exit(2);
}

// Build an LLVM FunctionType from a MiniC prototype
static FunctionType* functionTypeFromProto(const FunctionPrototypeAST* P) {
  std::vector<Type*> paramTypes;
  paramTypes.reserve(P->getSize());

  for (const auto& param : P->getParams()) {
    paramTypes.push_back(typeFromString(param->getType()));
  }

  Type* retTy = typeFromString(P->getType());
  return FunctionType::get(retTy, paramTypes, /*isVarArg=*/false);
}

// Declare a function in the module from a prototype, or check a previous one
static Function* declareFunctionFromProto(const FunctionPrototypeAST* P) {
  const std::string& name = P->getName();
  FunctionType* FT = functionTypeFromProto(P);

  // Look for an existing function with this name
  Function* F = TheModule->getFunction(name);

  if (F) {
    // Check the type matches the prototype
    if (F->getFunctionType() != FT) {
      fprintf(stderr,
              "Conflicting declarations for function '%s'\n",
              name.c_str());
      exit(2);
    }
    return F;
  }

  // Create a fresh declaration
  F = Function::Create(FT,
                       Function::ExternalLinkage,
                       name,
                       TheModule.get());

  // Give argument names from the prototype
  unsigned idx = 0;
  for (auto& arg : F->args()) {
    arg.setName(P->getParams()[idx++]->getName());
  }

  return F;
}


// ---- low level type predicates ----
static inline bool isInt(Type* T)   { return T->isIntegerTy(32); }
static inline bool isBool(Type* T)  { return T->isIntegerTy(1);  }
static inline bool isFloat(Type* T) { return T->isFloatTy();     }

// Thin wrappers used by expression codegen
static inline bool isIntTy(Type* T)   { return isInt(T); }
static inline bool isBoolTy(Type* T)  { return isBool(T); }
static inline bool isFloatTy(Type* T) { return isFloat(T); }

static inline Type* getIntTy()   { return miniCIntTy(); }
static inline Type* getBoolTy()  { return miniCBoolTy(); }
static inline Type* getFloatTy() { return miniCFloatTy(); }

// MINIC_RULE_BOOL_WIDEN_ARITH
// Decide which type wins when mixing types
// int + float  → float
// bool + int   → int
// bool + bool  → bool
static Type* getCommonType(Type* T1, Type* T2) {
  if (T1 == T2)
    return T1;

  if (isFloatTy(T1) || isFloatTy(T2))
    return getFloatTy();

  if (isIntTy(T1) || isIntTy(T2))
    return getIntTy();

  return getBoolTy();
}


// MINIC_RULE_WIDENING_HELPER
static bool isWideningType(Type* from, Type* to) {
  if (from == to) return true;

  if (isBoolTy(from) && isIntTy(to))   return true; // bool -> int
  if (isBoolTy(from) && isFloatTy(to)) return true; // bool -> float
  if (isIntTy(from)  && isFloatTy(to)) return true; // int  -> float

  return false; // everything else is narrowing
}


// Create a stack slot in a function's entry block
static AllocaInst* CreateEntryAlloca(Function* F, StringRef VarName, Type* Ty) {
  IRBuilder<> tmp(&F->getEntryBlock(), F->getEntryBlock().begin()); // place at start
  return tmp.CreateAlloca(Ty, nullptr, VarName);                     // %VarName = alloca Ty
}

// ---- Scopes ----

// Start a new scope with a fresh frame
static void pushScope() {
  ScopeStack.push_back(std::move(NamedValues));
  NamedValues.clear();
}

// Restore the previous scope
static void popScope() {
  if (ScopeStack.empty()) {
    fprintf(stderr, "Scope stack underflow\n");
    exit(2);
  }
  NamedValues = std::move(ScopeStack.back());
  ScopeStack.pop_back();
}

// Lookup helpers
static AllocaInst* lookupLocal(const std::string& name) {
  // first look in current scope
  auto it = NamedValues.find(name);
  if (it != NamedValues.end())
    return it->second;
    
  // then walk outer scopes from innermost to outermost
  for (auto scope = ScopeStack.rbegin(); scope != ScopeStack.rend(); ++scope) {
    auto curr_scope = scope->find(name);
    if (curr_scope != scope->end()) {
      return curr_scope->second;
    }
  }
  
  return nullptr;
}


// Only check in current frame for redeclarations
static AllocaInst* lookupLocalCurrent(const std::string& name) {
  auto it = NamedValues.find(name);
  return it == NamedValues.end() ? nullptr : it->second;
}

static GlobalVariable* lookupGlobal(const std::string& name) {
  return TheModule->getGlobalVariable(name, /*AllowInternal*/ true);
}


// Zero initialiser for a type
static Constant* zeroOf(Type* T) {
  if (isInt(T))         return ConstantInt::get(T, 0, true);
  if (isBool(T))        return ConstantInt::get(T, 0, false);
  if (isFloat(T))       return ConstantFP::get(T, 0.0);
  if (T->isPointerTy()) return ConstantPointerNull::get(cast<PointerType>(T));
  return UndefValue::get(T);
}

// Safe casts with basic MiniC rules, error on illegal casts
static Value* castTo(Type* dstTy, Value* v, const char* context) {
  Type* srcTy = v->getType();
  if (srcTy == dstTy) return v;

  // int <-> double
  if (isInt(srcTy)   && isFloat(dstTy)) return Builder.CreateSIToFP(v, dstTy, "sitofp");
  if (isFloat(srcTy) && isInt(dstTy))   return Builder.CreateFPToSI(v, dstTy, "fptosi");

  // bool -> int/double
  if (isBool(srcTy) && isInt(dstTy))    return Builder.CreateZExt(v, dstTy, "b2i");
  if (isBool(srcTy) && isFloat(dstTy))  return Builder.CreateUIToFP(v, dstTy, "b2f");

  // int/double -> bool (compare with zero)
  if (isInt(srcTy)   && isBool(dstTy))  return Builder.CreateICmpNE(v, ConstantInt::get(srcTy, 0), "i2b");
  if (isFloat(srcTy) && isBool(dstTy))  return Builder.CreateFCmpONE(v, ConstantFP::get(srcTy, 0.0), "f2b");

  fprintf(stderr, "Type error in %s: cannot cast from ", context);
  srcTy->print(errs()); errs() << " to "; dstTy->print(errs()); errs() << "\n";
  exit(2);
}

// ---- Block codegen ----
// Blocks create a new scope, allocate locals in the function entry, then emit statements.
Value* BlockAST::codegen() {
  pushScope();

  BasicBlock* curBB = Builder.GetInsertBlock();
  if (!curBB) {
    fprintf(stderr, "Block codegen used with no active insertion block\n");
    exit(2);
  }
  Function* F = curBB->getParent();
  if (!F) {
    fprintf(stderr, "Block codegen used outside of a function\n");
    exit(2);
  }

  // Allocate and zero-initialise each local in the function entry
  for (auto& d : LocalDecls) {
    const std::string& name = d->getName();
    Type* ty = typeFromString(d->getType());

    // MINIC_RULE_LOCAL_SHADOW
    if (lookupLocalCurrent(name)) {
      fprintf(stderr, "Redeclaration of local '%s'\n", name.c_str());
      exit(2);
    }


    AllocaInst* slot = CreateEntryAlloca(F, name, ty);
    Builder.CreateStore(zeroOf(ty), slot);
    NamedValues[name] = slot;
  }

  // Emit statements in order
  for (auto& s : Stmts) {
    if (s) s->codegen();
  }

  popScope();
  return nullptr;
}

// ---- Literals codegen ----

// int literal -> i32 constant
Value* IntASTnode::codegen() {
  return ConstantInt::get(miniCIntTy(), Val, /*isSigned=*/true);
}

// float literal -> double constant
Value* FloatASTnode::codegen() {
  return ConstantFP::get(miniCFloatTy(), Val);
}

// bool literal -> i1 constant
Value* BoolASTnode::codegen() {
  return ConstantInt::get(miniCBoolTy(), Bool ? 1 : 0);
}

// --- Variables codegen ---
Value* VariableASTnode::codegen() {  
    AllocaInst* local = lookupLocal(Name);  
    if (local) {
        auto* val = Builder.CreateLoad(local->getAllocatedType(), local, Name.c_str()); 
        return val;
    }
    
    GlobalVariable* global = lookupGlobal(Name);  
    if (global) {
        auto* val = Builder.CreateLoad(global->getValueType(), global, Name.c_str());
        return val;  
    }
    
    // Not found - error!
    fprintf(stderr, "Unknown variable name: %s\n", Name.c_str());
    exit(2);
}

Value* UnaryExprAST::codegen(){
    Value *v = Operand->codegen();
    if (!v) return nullptr;  
    Type* T = v->getType();
    
    if (Op == '-') {
        if (isInt(T)) {
            return Builder.CreateNeg(v, "ineg");
        }
        if (isFloat(T)) {
            return Builder.CreateFNeg(v, "fneg");
        }
        Value* bool_int = castTo(miniCIntTy(), v, "u-");
        return Builder.CreateNeg(bool_int, "ineg");
    }
    
    if (Op == '!'){  
        Value* bool_val = castTo(miniCBoolTy(), v , "u!");
        return Builder.CreateNot(bool_val, "not"); 
    }
    
    fprintf(stderr , "unkown unary operator, '%c'\n", Op); 
    exit(2);
}

Value* AssignAST::codegen() {
    auto* lhsVar = getLHS();
    const std::string& name = lhsVar->getName();

    // local variable target
    if (AllocaInst* local_var_memory = lookupLocal(name)) {
        Type* dstTy = local_var_memory->getAllocatedType();
        Value* rhs  = getRHS()->codegen();
        if (!rhs) return nullptr;

        Type* srcTy = rhs->getType();

        // MINIC_RULE_ASSIGN_WIDENING
        if (srcTy != dstTy) {
            if (!isWideningType(srcTy, dstTy)) {
                fprintf(stderr, "Illegal narrowing assignment to '%s'\n",
                        name.c_str());
                errs() << "  target type: "; dstTy->print(errs());
                errs() << "\n  value type:  "; srcTy->print(errs());
                errs() << "\n";
                exit(2);
            }
            rhs = castTo(dstTy, rhs, "assign");
        }

        Builder.CreateStore(rhs, local_var_memory);
        return rhs;
    }

    // global variable target
    if (GlobalVariable* g = lookupGlobal(name)) {
        Type* dstTy = g->getValueType();
        Value* rhs  = getRHS()->codegen();
        if (!rhs) return nullptr;

        Type* srcTy = rhs->getType();

        // MINIC_RULE_ASSIGN_WIDENING
        if (srcTy != dstTy) {
            if (!isWideningType(srcTy, dstTy)) {
                fprintf(stderr, "Illegal narrowing assignment to global '%s'\n",
                        name.c_str());
                errs() << "  target type: "; dstTy->print(errs());
                errs() << "\n  value type:  "; srcTy->print(errs());
                errs() << "\n";
                exit(2);
            }
            rhs = castTo(dstTy, rhs, "assign");
        }

        Builder.CreateStore(rhs, g);
        return rhs;
    }

    fprintf(stderr, "Assignment to unknown variable '%s'\n", name.c_str());
    exit(2);
}



Value* BinaryExprAST::codegen() {
  Value* lhs = getLHS()->codegen();
  Value* rhs = getRHS()->codegen();
  if (!lhs || !rhs) return nullptr;

  Type* lhs_type = lhs->getType();
  Type* rhs_type = rhs->getType();
  Type* common_type = getCommonType(lhs_type, rhs_type);

  int op = getOpTok();

  // Logical operators: force both sides to bool first
  if (op == AND || op == OR) {
    lhs = castTo(miniCBoolTy(), lhs, "logical lhs");
    rhs = castTo(miniCBoolTy(), rhs, "logical rhs");

    if (op == AND) {
      return Builder.CreateAnd(lhs, rhs, "andtmp");
    } else { // OR
      return Builder.CreateOr(lhs, rhs, "ortmp");
    }
  }

  // For arithmetic and comparisons, promote both sides to the common type
  lhs = castTo(common_type, lhs, "binary lhs");
  rhs = castTo(common_type, rhs, "binary rhs");

  bool useFloat = isFloat(common_type);
  bool useIntLike = isInt(common_type) || isBool(common_type);

  switch (op) {
    // ── arithmetic ─────────────────────
    case PLUS:
      if (useFloat)   return Builder.CreateFAdd(lhs, rhs, "addtmp");
      if (useIntLike) return Builder.CreateAdd(lhs, rhs, "addtmp");
      fprintf(stderr, "Error: invalid types for '+'\n");
      exit(2);

    case MINUS:
      if (useFloat)   return Builder.CreateFSub(lhs, rhs, "subtmp");
      if (useIntLike) return Builder.CreateSub(lhs, rhs, "subtmp");
      fprintf(stderr, "Error: invalid types for '-'\n");
      exit(2);

    case ASTERIX:
      if (useFloat)   return Builder.CreateFMul(lhs, rhs, "multmp");
      if (useIntLike) return Builder.CreateMul(lhs, rhs, "multmp");
      fprintf(stderr, "Error: invalid types for '*'\n");
      exit(2);

    case DIV:
      if (useFloat)   return Builder.CreateFDiv(lhs, rhs, "divtmp");
      if (useIntLike) return Builder.CreateSDiv(lhs, rhs, "divtmp");
      fprintf(stderr, "Error: invalid types for '/'\n");
      exit(2);

    case MOD:
      if (useFloat) {
        fprintf(stderr, "Error: '%%' not defined for float\n");
        exit(2);
      }
      if (useIntLike) {
        return Builder.CreateSRem(lhs, rhs, "modtmp");
      }
      fprintf(stderr, "Error: invalid types for '%%'\n");
      exit(2);


    // ── comparisons (return i1) ────────
    case LT:
      if (useFloat) return Builder.CreateFCmpOLT(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSLT(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '<'\n");
      exit(2);

    case LE:
      if (useFloat) return Builder.CreateFCmpOLE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSLE(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '<='\n");
      exit(2);

    case GT:
      if (useFloat) return Builder.CreateFCmpOGT(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSGT(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '>'\n");
      exit(2);

    case GE:
      if (useFloat) return Builder.CreateFCmpOGE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSGE(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '>='\n");
      exit(2);

    case EQ:
      if (useFloat) return Builder.CreateFCmpOEQ(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpEQ(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '=='\n");
      exit(2);

    case NE:
      if (useFloat) return Builder.CreateFCmpONE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpNE(lhs, rhs, "cmptmp");
      fprintf(stderr, "Error: invalid types for '!='\n");
      exit(2);

    default:
      fprintf(stderr, "Error: Unknown binary operator token %d\n", op);
      exit(2);
  }

  // unreachable, but keeps compiler happy
  return nullptr;
}


Value *GlobVarDeclAST::codegen() {
  // MINIC_RULE_GLOBAL_ONCE
  const std::string &name = getName();
  llvm::Type *ty = typeFromString(getType());

  // check for redeclaration
  if (lookupGlobal(name) || GlobalNamedValues.count(name)) {
    fprintf(stderr, "Redeclaration of global '%s'\n", name.c_str());
    exit(2);
  }

  // Disallow a global with the same name as a function
  if (TheModule->getFunction(name)) {
    fprintf(stderr,
            "Global variable '%s' conflicts with a function of the same name\n",
            name.c_str());
    exit(2);
  }


  llvm::Constant *init = zeroOf(ty);

  auto *G = new llvm::GlobalVariable(
      *TheModule,
      ty,
      /*isConstant=*/false,
      llvm::GlobalValue::ExternalLinkage,
      init,
      name);

  GlobalNamedValues[name] = G;
  return G;
}


Value* FunctionDeclAST::codegen() {
    const std::string &function_name = Proto->getName();

    // Disallow a function with the same name as a global variable
    if (lookupGlobal(function_name)) {
        fprintf(stderr,
                "Function '%s' conflicts with a global variable of the same name\n",
                function_name.c_str());
        exit(2);
    }


    // Build the expected LLVM type from the prototype
    FunctionType* function_type = functionTypeFromProto(Proto.get());

    // Look up any existing declaration or definition
    Function* F = TheModule->getFunction(function_name);

    if (F) {
        // Type must match the prototype
        if (F->getFunctionType() != function_type) {
            fprintf(stderr,
                    "Definition of function '%s' does not match a previous declaration\n",
                    function_name.c_str());
            exit(2);
        }

        // Do not allow more than one definition
        if (!F->empty()) {
            fprintf(stderr, "Redefinition of function '%s'\n",
                    function_name.c_str());
            exit(2);
        }
    } else {
        // No previous declaration, create a new one
        F = Function::Create(function_type,
                             Function::ExternalLinkage,
                             function_name,
                             TheModule.get());
    }

    // Name the arguments to match the prototype
    unsigned idx = 0;
    for (auto &arg : F->args()) {
        arg.setName(Proto->getParams()[idx++]->getName());
    }

    // Create entry block and set insertion point
    BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
    Builder.SetInsertPoint(BB);

    // Reset symbol tables for this function
    NamedValues.clear();
    ScopeStack.clear();

    // Allocate each argument in the entry block and store the value
    idx = 0;
    for (auto &arg : F->args()) {
        std::string argName = std::string(arg.getName());
        Type *argType = arg.getType();

        AllocaInst *slot = CreateEntryAlloca(F, argName, argType);
        Builder.CreateStore(&arg, slot);
        NamedValues[argName] = slot;
    }

    // Generate code for the body block
    if (Block) {
        Block->codegen();
    }

    // If body did not end with a return, insert a default one
    if (!BB->getTerminator()) {
        Type* return_type = function_type->getReturnType();
        if (return_type->isVoidTy()) {
            Builder.CreateRetVoid();
        } else {
            Builder.CreateRet(zeroOf(return_type));
        }
    }

    // Verify the function
    if (verifyFunction(*F, &errs())) {
        fprintf(stderr, "Function verification failed for '%s'\n",
                function_name.c_str());
        F->print(errs());
        exit(2);
    }

    return F;
}


Value* CallExprAST::codegen() {
  // Look up the function in the module
  Function* calleeF = TheModule->getFunction(Callee);
  if (!calleeF) {
    fprintf(stderr, "Unknown function '%s' in call\n", Callee.c_str());
    exit(2);
  }

  // Check argument count
  // MINIC_RULE_NO_VARARGS
  if (calleeF->arg_size() != Args.size()) {
    fprintf(stderr, "Function '%s' expects %u args, got %zu\n",
            Callee.c_str(),
            static_cast<unsigned>(calleeF->arg_size()),
            Args.size());
    exit(2);
  }

  // Generate and type-check each argument
  std::vector<Value*> argValues;
  argValues.reserve(Args.size());

  unsigned idx = 0;
  for (auto& argExpr : Args) {
    Value* argVal = argExpr->codegen();
    if (!argVal) return nullptr;

    Type* paramTy = calleeF->getFunctionType()->getParamType(idx);

    // MINIC_RULE_CALL_WIDENING
    Type* srcTy = argVal->getType();
    if (srcTy != paramTy) {
      if (!isWideningType(srcTy, paramTy)) {
        fprintf(stderr, "Illegal narrowing in call to '%s' for argument %u\n",
                Callee.c_str(), idx);
        errs() << "  parameter type: "; paramTy->print(errs());
        errs() << "\n  argument type:  "; srcTy->print(errs());
        errs() << "\n";
        exit(2);
      }
      argVal = castTo(paramTy, argVal, "callarg");
    }

    argValues.push_back(argVal);
    ++idx;
  }

  // Emit the call
  CallInst* callInst = Builder.CreateCall(calleeF, argValues,
                                          calleeF->getReturnType()->isVoidTy()
                                            ? ""
                                            : "calltmp");

  // For void functions, you still return the CallInst as the Value*
  return callInst;
}


Value* IfExprAST::codegen() {
    Value* condV = Cond->codegen();
    if (!condV) return nullptr;
    condV = castTo(miniCBoolTy(), condV, "ifcond"); // MINIC_RULE_COND_BOOL_CAST

    BasicBlock* curBB = Builder.GetInsertBlock();
    if (!curBB) {
        fprintf(stderr, "IfExprAST used with no insertion block\n");
        exit(2);
    }

    Function* F = curBB->getParent();
    if (!F) {
        fprintf(stderr, "IfExprAST used outside of a function\n");
        exit(2);
    }

    // Attach all blocks to the function when you create them
    BasicBlock* thenBB  = BasicBlock::Create(TheContext, "if.then", F);
    BasicBlock* elseBB  = Else ? BasicBlock::Create(TheContext, "if.else", F) : nullptr;
    BasicBlock* mergeBB = BasicBlock::Create(TheContext, "if.end",  F);

    if (Else)
        Builder.CreateCondBr(condV, thenBB, elseBB);
    else
        Builder.CreateCondBr(condV, thenBB, mergeBB);

    // Then block
    Builder.SetInsertPoint(thenBB);
    if (Then) Then->codegen();
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(mergeBB);

    // Else block, if present
    if (Else) {
        Builder.SetInsertPoint(elseBB);
        Else->codegen();
        if (!Builder.GetInsertBlock()->getTerminator())
            Builder.CreateBr(mergeBB);
    }

    // Merge block
    Builder.SetInsertPoint(mergeBB);
    return nullptr;
}


Value* WhileExprAST::codegen() {
    BasicBlock* curBB = Builder.GetInsertBlock();
    if (!curBB) {
      fprintf(stderr, "WhileExprAST used with no insertion block\n");
      exit(2);
    }

    Function* F = curBB->getParent();
    if (!F) {
      fprintf(stderr, "WhileExprAST used outside of a function\n");
      exit(2);
    }

    // Attach all blocks to F when created
    BasicBlock* condBB = BasicBlock::Create(TheContext, "while.cond", F);
    BasicBlock* bodyBB = BasicBlock::Create(TheContext, "while.body", F);
    BasicBlock* endBB  = BasicBlock::Create(TheContext, "while.end",  F);

    // Jump from current block to cond block
    Builder.CreateBr(condBB);

    // Condition block
    Builder.SetInsertPoint(condBB);
    Value* condV = Cond->codegen();
    if (!condV) return nullptr;
    condV = castTo(miniCBoolTy(), condV, "whilecond"); // MINIC_RULE_COND_BOOL_CAST

    Builder.CreateCondBr(condV, bodyBB, endBB);

    // Body block
    Builder.SetInsertPoint(bodyBB);
    if (Body) Body->codegen();
    if (!Builder.GetInsertBlock()->getTerminator())
      Builder.CreateBr(condBB);

    // End block
    Builder.SetInsertPoint(endBB);

    return nullptr;
}



Value* ReturnAST::codegen() {
    BasicBlock* currBB = Builder.GetInsertBlock();
    if (!currBB) {
        fprintf(stderr, "ReturnAST used without any insertion block\n");
        exit(2);
    }

    Function* F = currBB->getParent();
    if (!F) {
        fprintf(stderr, "ReturnAST used outside of a function\n");
        exit(2);
    }

    Type* func_return_type = F->getReturnType();

    // "return;" with no value
    if (!Val) {
        if (!func_return_type->isVoidTy()) {
            fprintf(stderr, "Non-void function missing return value\n");
            exit(2);
        }
        return Builder.CreateRetVoid();
    }

    // "return expr;"
    Value* return_val = Val->codegen();
    if (!return_val) return nullptr;

    Type* srcTy = return_val->getType();

    // MINIC_RULE_RETURN_WIDENING
    if (srcTy != func_return_type) {
        if (!isWideningType(srcTy, func_return_type)) {
            fprintf(stderr, "Illegal narrowing return type\n");
            errs() << "  function return type: "; func_return_type->print(errs());
            errs() << "\n  expression type:      "; srcTy->print(errs());
            errs() << "\n";
            exit(2);
        }
        return_val = castTo(func_return_type, return_val, "ret");
    }

    return Builder.CreateRet(return_val);
}




//===----------------------------------------------------------------------===//
// AST Printer
//===----------------------------------------------------------------------===//

// Stream an AST node using its to_string()
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ASTnode& ast) {
  os << ast.to_string();
  return os;
}

// void IntASTnode::display(int tabs) {
//   printf("%s\n",getType().c_str());
// }


// Literals & variables
void IntASTnode::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Int(%d)\n", Val);
}
void FloatASTnode::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Float(%g)\n", Val);
}
void BoolASTnode::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Bool(%s)\n", Bool ? "true" : "false");
}
void VariableASTnode::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Var(%s)\n", Name.c_str());
}

// Expressions
void UnaryExprAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Unary('%c')\n", Op);
  if (Operand) Operand->dump(indent+1);
}

void UnaryExprAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, std::string("Unary('") + Op + "')");
  if (Operand) {
    // indent child by +1
    Operand->to_string_into(out, indent + 1);
  }
}

void BinaryExprAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Binary(%s)\n", opTokName(OpTok));
  if (LHS) { indentOut(indent+1); fprintf(stderr, "LHS:\n"); LHS->dump(indent+2); }
  if (RHS) { indentOut(indent+1); fprintf(stderr, "RHS:\n"); RHS->dump(indent+2); }
}

void BinaryExprAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, std::string("Binary(") + opTokName(OpTok) + ")");
  if (LHS) {
    appendln(out, indent + 1, "LHS:");
    LHS->to_string_into(out, indent + 2);
  }
  if (RHS) {
    appendln(out, indent + 1, "RHS:");
    RHS->to_string_into(out, indent + 2);
  }
}


void AssignAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Assign\n");
  indentOut(indent+1); fprintf(stderr, "LHS:\n");
  if (LHS) LHS->dump(indent+2);
  indentOut(indent+1); fprintf(stderr, "RHS:\n");
  if (RHS) RHS->dump(indent+2);
}

void AssignAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Assign");
  appendln(out, indent + 1, "LHS:");
  if (LHS) LHS->to_string_into(out, indent + 2);
  appendln(out, indent + 1, "RHS:");
  if (RHS) RHS->to_string_into(out, indent + 2);
}


void CallExprAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Call %s\n", Callee.c_str());
  int i = 0;
  for (auto &a : Args) {
    indentOut(indent+1); fprintf(stderr, "arg[%d]:\n", i++);
    if (a) a->dump(indent+2);
  }
}

void CallExprAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Call " + Callee);
  int i = 0;
  for (auto& a : Args) {
    appendln(out, indent + 1, "arg[" + std::to_string(i++) + "]:");
    if (a) a->to_string_into(out, indent + 2);
  }
}


// Statements
void IfExprAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "If\n");
  indentOut(indent+1); fprintf(stderr, "Cond:\n");
  if (Cond) Cond->dump(indent+2);
  indentOut(indent+1); fprintf(stderr, "Then:\n");
  if (Then) Then->dump(indent+2);
  if (Else) { indentOut(indent+1); fprintf(stderr, "Else:\n"); Else->dump(indent+2); }
}
void WhileExprAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "While\n");
  indentOut(indent+1); fprintf(stderr, "Cond:\n");
  if (Cond) Cond->dump(indent+2);
  indentOut(indent+1); fprintf(stderr, "Body:\n");
  if (Body) Body->dump(indent+2);
}
void ReturnAST::dump(int indent) const {
  indentOut(indent); fprintf(stderr, "Return\n");
  if (Val) { indentOut(indent+1); fprintf(stderr, "Val:\n"); Val->dump(indent+2); }
}

void BlockAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Block");
  appendln(out, indent + 1, "Locals:");
  for (auto& d : LocalDecls) {
    appendln(out, indent + 2, "VarDecl " + d->getName() + " : " + d->getType());
  }
  appendln(out, indent + 1, "Stmts:");
  for (auto& s : Stmts) {
    if (s) s->to_string_into(out, indent + 2);
  }
}

void IfExprAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "If");
  appendln(out, indent + 1, "Cond:");
  if (Cond) Cond->to_string_into(out, indent + 2);
  appendln(out, indent + 1, "Then:");
  if (Then) Then->to_string_into(out, indent + 2);
  if (Else) {
    appendln(out, indent + 1, "Else:");
    Else->to_string_into(out, indent + 2);
  }
}

void WhileExprAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "While");
  appendln(out, indent + 1, "Cond:");
  if (Cond) Cond->to_string_into(out, indent + 2);
  appendln(out, indent + 1, "Body:");
  if (Body) Body->to_string_into(out, indent + 2);
}

void ReturnAST::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Return");
  if (Val) {
    appendln(out, indent + 1, "Val:");
    Val->to_string_into(out, indent + 2);
  }
}


// ----- to_string_inner implementations for leaf nodes -----

void IntASTnode::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Int(" + std::to_string(Val) + ")");
}

void FloatASTnode::to_string_inner(std::string& out, int indent) const {
  // use default float formatting (matches your dump pretty well)
  appendln(out, indent, "Float(" + std::to_string(Val) + ")");
}

void BoolASTnode::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, std::string("Bool(") + (Bool ? "true" : "false") + ")");
}

void VariableASTnode::to_string_inner(std::string& out, int indent) const {
  appendln(out, indent, "Var(" + Name + ")");
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  if (argc == 2) {
    pFile = fopen(argv[1], "r");
    if (pFile == NULL)
      perror("Error opening file");
  } else {
    std::cout << "Usage: ./code InputFile\n";
    return 1;
  }

  // initialize line number and column numbers to zero
  lineNo = 1;
  columnNo = 1;

  // get the first token
  getNextToken();
  // while (CurTok.type != EOF_TOK) {
  //   fprintf(stderr, "Token: %s with type %d\n", CurTok.lexeme.c_str(),
  //           CurTok.type);
  //   getNextToken();
  // }
  // fprintf(stderr, "Lexer Finished\n");

  // RUN THE PARSER (Task 2)
  parser();
  fprintf(stderr, "Parsing Finished\n");

    // ---- Dump AST ----
  fprintf(stderr, "=== AST (externs) ===\n");
  for (auto& ex : gExterns) {
    fprintf(stderr, "Extern %s : %s (params=%d)\n",
      ex->getName().c_str(), ex->getType().c_str(), ex->getSize());
    for (auto& p : ex->getParams()) {
      fprintf(stderr, "  - %s %s\n", p->getType().c_str(), p->getName().c_str());
    }
  }

  for (auto& d : gTopDecls) {
    if (d) {
      llvm::outs() << *d << "\n";
    }
  }

  // Make the module, which holds all the code.
  TheModule = std::make_unique<Module>("mini-c", TheContext);


  // ================== Code generation phase ==================

  // 1) Declare all extern functions in the module
  for (auto &ex : gExterns) {
    declareFunctionFromProto(ex.get());
  }

  // 2) Forward declare all non-extern functions (prototypes first)
  for (auto &d : gTopDecls) {
    if (d && d->isFunctionDecl()) {
      auto *FDecl = static_cast<FunctionDeclAST *>(d.get());
      declareFunctionFromProto(FDecl->getProto());
    }
  }

  // 3) First pass: generate all global variables
  for (auto &d : gTopDecls) {
    if (d && d->isGlobVarDecl()) {
      d->codegen();
    }
  }

  // 4) Second pass: generate everything else (functions etc.)
  for (auto &d : gTopDecls) {
    if (d && !d->isGlobVarDecl()) {
      d->codegen();
    }
  }


  // Run the parser now.

  /* UNCOMMENT : Task 2 - Parser */
  //  parser();
  //  fprintf(stderr, "Parsing Finished\n");  

  printf(
      "********************* FINAL IR (begin) ****************************\n");
  // Print out all of the generated code into a file called output.ll
  // printf("%s\n", argv[1]);
  auto Filename = "output.ll";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message();
    return 1;
  }
  // TheModule->print(errs(), nullptr); // print IR to terminal
  TheModule->print(dest, nullptr);
  printf(
      "********************* FINAL IR (end) ******************************\n");

  fclose(pFile); // close the file that contains the code that was parsed
  return 0;
}
