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
#include <fstream>


using namespace llvm;
using namespace llvm::sys;

FILE *pFile;

static const char *InputFileName = nullptr;


static std::vector<std::string> SourceLines;

static const int MaxErrors = 50;
static int ErrorCount = 0;

static void noteError() {
  ++ErrorCount;
  if (ErrorCount >= MaxErrors) {
    fprintf(stderr,
            "fatal error: too many errors emitted, stopping now\n");
    exit(2);
  }
}

static bool TraceParser = false;  // set true if you want verbose parser trace
static bool PrintAST = true;  // set true if you want to print the AST
static bool PrintIR = true; // Global toggle for IR printing to stderr


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

static void printErrorAt(const TOKEN &tok, const char *msg);
static void printErrorAtCurrent(const char *msg);
static void printErrorAtLocation(int line, int col, const char *msg);  


static std::string globalLexeme;
static int lineNo, columnNo;

const std::string TOKEN::getIdentifierStr() const {
  if (type != IDENT) {
    printErrorAt(*this, "getIdentifierStr called on non-IDENT token");
    exit(2);
  }
  return this->lexeme;
}


const int TOKEN::getIntVal() const {
  if (type != INT_LIT) {
    printErrorAt(*this, "getIntVal called on non-INT_LIT token");
    exit(2);
  }
  return static_cast<int>(strtod(this->lexeme.c_str(), nullptr));
}

const float TOKEN::getFloatVal() const {
  if (type != FLOAT_LIT) {
    printErrorAt(*this, "getFloatVal called on non-FLOAT_LIT token");
    exit(2);
  }
  return strtof(this->lexeme.c_str(), nullptr);
}

const bool TOKEN::getBoolVal() const {
  if (type != BOOL_LIT) {
    printErrorAt(*this, "getBoolVal called on non-BOOL_LIT token");
    exit(2);
  }
  return (this->lexeme == "true");
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


class ASTnode {
public:
  virtual ~ASTnode() {}
  virtual Value *codegen() { return nullptr; }

  // New: default source location for nodes without tokens
  virtual int getLine() const { return -1; }
  virtual int getCol()  const { return -1; }

  // New: default flags used in codegen driver
  virtual bool isGlobVarDecl() const { return false; }
  virtual bool isFunctionDecl() const { return false; }

  // One-line expression view
  virtual std::string toExprString() const {
    std::string tmp;
    to_string_inner(tmp, 0);
    if (!tmp.empty() && tmp.back() == '\n') tmp.pop_back();
    return tmp;
  }

  // Full multi-line dump used by `dump` and operator<<
  virtual std::string to_string() const {
    std::string out;
    to_string_inner(out, 0);
    return out;
  }

  virtual void dump(int indent = 0) const {
    const std::string s = to_string();
    const std::string shifted = indentLines(s, indent);
    fwrite(shifted.data(), 1, shifted.size(), stderr);
  }

  // Helper used all over the printers
  void to_string_into(std::string &out, int indent) const {
    to_string_inner(out, indent);
  }

protected:
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
  std::string toExprString() const override;   
  Value *codegen() override;
  int getLine() const override { return Tok.lineNo; }
  int getCol()  const override { return Tok.columnNo; }
  void to_string_inner(std::string& out, int indent) const override;
};

/// BoolASTnode - Class for boolean literals true and false,
class BoolASTnode : public ASTnode {
  bool Bool;
  TOKEN Tok;

public:
  BoolASTnode(TOKEN tok, bool B) : Bool(B), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  std::string toExprString() const override;   
  Value *codegen() override;
  int getLine() const override { return Tok.lineNo; }
  int getCol()  const override { return Tok.columnNo; }
  void to_string_inner(std::string& out, int indent) const override;
};

/// FloatASTnode - Node class for floating point literals like "1.0".
class FloatASTnode : public ASTnode {
  double Val;
  TOKEN Tok;

public:
  FloatASTnode(TOKEN tok, double Val) : Val(Val), Tok(tok) {}
  const std::string &getType() const { return Tok.lexeme; }
  std::string toExprString() const override;   
  Value *codegen() override;
  int getLine() const override { return Tok.lineNo; }
  int getCol()  const override { return Tok.columnNo; }
  void to_string_inner(std::string& out, int indent) const override;
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
  std::string toExprString() const override;   
  Value *codegen() override;
  int getLine() const override { return Tok.lineNo; }
  int getCol()  const override { return Tok.columnNo; }
  void to_string_inner(std::string& out, int indent) const override;
};

/// ParamAST - Class for a parameter declaration
class ParamAST {
  std::string Name;
  std::string Type;
  std::vector<int> Dimensions;   // new

public:
  ParamAST(const std::string &name,
           const std::string &type,
           std::vector<int> dims = {})
      : Name(name), Type(type), Dimensions(std::move(dims)) {}

  const std::string &getName() const { return Name; }
  const std::string &getType() const { return Type; }
  const std::vector<int> &getDims() const { return Dimensions; }

  bool isArrayParam() const { return !Dimensions.empty(); }
};


/// DeclAST - Base class for declarations, variables and functions
class DeclAST : public ASTnode {

public:
  virtual ~DeclAST() {}
};

class VarDeclAST : public DeclAST {
  std::unique_ptr<VariableASTnode> Var;
  std::string TypeName;

  public:
    VarDeclAST(std::unique_ptr<VariableASTnode> var, const std::string &type)
    : Var(std::move(var)), TypeName(type) {}
    const std::string &getType() const { return TypeName; }
    const std::string &getName() const { return Var->getName(); }
    void to_string_inner(std::string& out, int indent) const override;
    Value *codegen() override;
};

/// VarDeclAST - Class for a variable declaration
class ArrayDeclAST : public VarDeclAST {
  std::vector<int> Dimensions;

public:
  ArrayDeclAST(std::unique_ptr<VariableASTnode> var,
               const std::string &type,
               std::vector<int> dimensions_list)
      : VarDeclAST(std::move(var), type),
        Dimensions(std::move(dimensions_list)) {}

  // Important: explicit virtual destructor so the vtable has a key function
  ~ArrayDeclAST() override;

  const std::vector<int> &getDims() const {
    return Dimensions;
  }

  // Treat as a global decl at top level when used in gTopDecls
  bool isGlobVarDecl() const override { return true; }

  Value *codegen() override;

  std::string toExprString() const override;   

  void to_string_inner(std::string& out, int indent) const override;
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

  void to_string_inner(std::string& out, int indent) const override;
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

  void to_string_inner(std::string& out, int indent) const override;
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
  std::string toExprString() const override;   
  Value *codegen() override;

  void to_string_inner(std::string& out, int indent) const override;
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

  int getLine() const override {
    return LHS ? LHS->getLine() : -1;
  }

  int getCol() const override {
    return LHS ? LHS->getCol() : -1;
  }

  Value *codegen() override;
  std::string toExprString() const override;   

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

  int getLine() const override { return LHS ? LHS->getLine() : -1; }
  int getCol()  const override { return LHS ? LHS->getCol()  : -1; }

  Value* codegen() override;           // declaration only
  std::string toExprString() const override;   

  void to_string_inner(std::string& out, int indent) const override;
};

class ArrayAccessAST : public ASTnode {
  TOKEN NameTok;                                       // new
  std::string name;                                    
  std::vector<std::unique_ptr<ASTnode>> indices;      
  
public:
  ArrayAccessAST(TOKEN nameTok,
                 const std::string& name, 
                 std::vector<std::unique_ptr<ASTnode>> indices_param)
      : NameTok(nameTok),
        name(name),
        indices(std::move(indices_param)) {}

  const std::string &getName() const { return name; }

  int getLine() const override { return NameTok.lineNo; }
  int getCol()  const override { return NameTok.columnNo; }

  const std::vector<std::unique_ptr<ASTnode>> &getIndices() const {
    return indices;
  }

  Value *codegen() override;

  std::string toExprString() const override;   

  void to_string_inner(std::string& out, int indent) const override;
};



class ArrayAssignAST : public ASTnode {
  std::unique_ptr<ArrayAccessAST> LHS;
  std::unique_ptr<ASTnode>        RHS;

public:
  ArrayAssignAST(std::unique_ptr<ArrayAccessAST> lhs,
                 std::unique_ptr<ASTnode> rhs)
      : LHS(std::move(lhs)), RHS(std::move(rhs)) {}

  ArrayAccessAST* getLHS() const { return LHS.get(); }
  ASTnode*        getRHS() const { return RHS.get(); }

  Value *codegen() override;

  std::string toExprString() const override;   
  void to_string_inner(std::string& out, int indent) const override;
};


/// CallExprAST - Expression class for function calls
class CallExprAST : public ASTnode {
  TOKEN CalleeTok;                              // token for function name
  std::string Callee;                          // Function name
  std::vector<std::unique_ptr<ASTnode>> Args;  // Arguments

public:
  CallExprAST(TOKEN calleeTok,
              const std::string &callee,
              std::vector<std::unique_ptr<ASTnode>> args)
      : CalleeTok(calleeTok),
        Callee(callee),
        Args(std::move(args)) {}

  const std::string &getCallee() const { return Callee; }
  const std::vector<std::unique_ptr<ASTnode>> &getArgs() const { return Args; }

  int getLine() const override { return CalleeTok.lineNo; }
  int getCol()  const override { return CalleeTok.columnNo; }

  Value *codegen() override;
  std::string toExprString() const override;
  void to_string_inner(std::string &out, int indent) const override;
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

  std::string toExprString() const override;   
  Value *codegen() override;
  void to_string_inner(std::string& out, int indent) const override;
};



/// FunctionDeclAST - This class represents a function definition itself.
class FunctionDeclAST : public DeclAST {
  std::unique_ptr<FunctionPrototypeAST> Proto;
  std::unique_ptr<ASTnode> Block;
  TOKEN LocTok;   // new: location for diagnostics

public:
  FunctionDeclAST(TOKEN locTok,
                  std::unique_ptr<FunctionPrototypeAST> Proto,
                  std::unique_ptr<ASTnode> Block)
      : Proto(std::move(Proto)), Block(std::move(Block)), LocTok(locTok) {}

  bool isFunctionDecl() const override { return true; }

  // NEW: location for error printing
  int getLine() const override { return LocTok.lineNo; }
  int getCol()  const override { return LocTok.columnNo; }

  const FunctionPrototypeAST* getProto() const { return Proto.get(); }
  const ASTnode*              getBody()  const { return Block.get(); }

  std::string toExprString() const override;   
  Value *codegen() override;
  void to_string_inner(std::string& out, int indent) const override;
};




static std::vector<std::unique_ptr<FunctionPrototypeAST>> gExterns;
static std::vector<std::unique_ptr<ASTnode>>              gTopDecls;


/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ASTnode> Cond, std::unique_ptr<ASTnode> Then,
            std::unique_ptr<ASTnode> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  std::string toExprString() const override;   
  Value *codegen() override;
  void to_string_inner(std::string& out, int indent) const override;
};

/// WhileExprAST - Expression class for while.
class WhileExprAST : public ASTnode {
  std::unique_ptr<ASTnode> Cond, Body;

public:
  WhileExprAST(std::unique_ptr<ASTnode> cond, std::unique_ptr<ASTnode> body)
      : Cond(std::move(cond)), Body(std::move(body)) {}
  std::string toExprString() const override;   
  Value *codegen() override;
  void to_string_inner(std::string& out, int indent) const override;
};

/// ReturnAST - Class for a return value
class ReturnAST : public ASTnode {
  std::unique_ptr<ASTnode> Val;

public:
  ReturnAST(std::unique_ptr<ASTnode> value) : Val(std::move(value)) {}
  std::string toExprString() const override;   
  Value *codegen() override;
  void to_string_inner(std::string& out, int indent) const override;
};


/// ArgsAST - Class for a function argumetn in a function call
class ArgsAST : public ASTnode {
  std::string Callee;
  std::vector<std::unique_ptr<ASTnode>> ArgsList;

public:
  ArgsAST(const std::string &Callee, std::vector<std::unique_ptr<ASTnode>> list)
      : Callee(Callee), ArgsList(std::move(list)) {}

};

// Forward declarations for location-based error printing
static void printErrorAt(const TOKEN &tok, const char *msg);
static void printErrorAtCurrent(const char *msg);


/// LogError* - These are little helper function for error handling.
std::unique_ptr<ASTnode> LogError(TOKEN tok, const char *Str) {
  printErrorAt(tok, Str);
  noteError();
  return nullptr;
}


std::unique_ptr<FunctionPrototypeAST> LogErrorP(TOKEN tok, const char *Str) {
  printErrorAt(tok, Str);
  noteError();
  return nullptr;
}


// For callers which only supply a message, use CurTok as location
std::unique_ptr<ASTnode> LogError(const char *Str) {
  printErrorAtCurrent(Str);
  noteError();
  return nullptr;
}

// Simple ANSI colour codes
static const char *COL_RESET   = "\033[0m";
static const char *COL_KW      = "\033[1;34m"; // keywords: if, else, return
static const char *COL_TYPE    = "\033[1;36m"; // types: int, float, bool
static const char *COL_STRING  = "\033[32m";   // string / char literals
static const char *COL_COMMENT = "\033[90m";   // // comments
static const char *COL_FILE    = "\033[1;34m"; // bright blue for filenames


// Very small C-ish highlighter for one line
static std::string colouriseSourceLine(const std::string &src) {
  enum State { NORMAL, IN_STRING, IN_CHAR, IN_COMMENT };
  State st = NORMAL;

  std::string out;
  out.reserve(src.size() * 2);

  auto isIdentChar = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };

  for (size_t i = 0; i < src.size();) {
    char c = src[i];

    if (st == IN_COMMENT) {
      out += c;
      ++i;
      continue;
    }

    // line comment
    if (st == NORMAL && c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
      out += COL_COMMENT;
      out.append(src.begin() + i, src.end());
      out += COL_RESET;
      break;
    }

    // string literal
    if (st == NORMAL && c == '"') {
      st = IN_STRING;
      out += COL_STRING;
      out += c;
      ++i;
      continue;
    }
    if (st == IN_STRING) {
      out += c;
      ++i;
      if (c == '"' && (i == 1 || src[i - 2] != '\\')) {
        out += COL_RESET;
        st = NORMAL;
      }
      continue;
    }

    // char literal
    if (st == NORMAL && c == '\'') {
      st = IN_CHAR;
      out += COL_STRING;
      out += c;
      ++i;
      continue;
    }
    if (st == IN_CHAR) {
      out += c;
      ++i;
      if (c == '\'' && (i == 1 || src[i - 2] != '\\')) {
        out += COL_RESET;
        st = NORMAL;
      }
      continue;
    }

    // identifiers + keywords + types
    if (st == NORMAL && (std::isalpha(static_cast<unsigned char>(c)) || c == '_')) {
      size_t j = i + 1;
      while (j < src.size() && isIdentChar(src[j])) ++j;
      std::string word = src.substr(i, j - i);

      bool isType =
          (word == "int"   ||
           word == "float" ||
           word == "bool"  ||
           word == "void");

      bool isKeyword =
          (word == "if"    ||
           word == "else"  ||
           word == "while" ||
           word == "for"   ||
           word == "return"||
           word == "true"  ||
           word == "false");

      if (isType) {
        out += COL_TYPE;
        out += word;
        out += COL_RESET;
      } else if (isKeyword) {
        out += COL_KW;
        out += word;
        out += COL_RESET;
      } else {
        out += word;
      }

      i = j;
      continue;
    }

    // default
    out += c;
    ++i;
  }

  // safety reset at end
  out += COL_RESET;
  return out;
}



static void printErrorAt(const TOKEN &tok, const char *msg) {
  int line = tok.lineNo;
  int col  = tok.columnNo;

  const char *fname = InputFileName ? InputFileName : "<input>";

  // header line, very close to clang
  fprintf(stderr,
        "%s%s\033[0m:%d:%d: \033[1;31merror:\033[0m %s\n",
        COL_FILE, fname, line, col, msg);


  // source line with line number and pipe
  if (line >= 1 && line <= (int)SourceLines.size()) {
    const std::string &src = SourceLines[line - 1];
    std::string coloured   = colouriseSourceLine(src);

    // width matches number of digits in line number
    int width = (int)std::to_string(line).size();

    // code line
    fprintf(stderr, " %*d | %s\n", width, line, coloured.c_str());

    // caret line prefix: spaces instead of number, then pipe
    fprintf(stderr, " %*s | ", width, "");

    int caretCol = col;
    if (caretCol < 1) caretCol = 1;

    // align caret under the offending column, count tabs from raw src
    for (int i = 1; i < caretCol; ++i) {
      char c = (i - 1 < (int)src.size()) ? src[i - 1] : ' ';
      if (c == '\t') fputc('\t', stderr);
      else           fputc(' ', stderr);
    }
    fputc('^', stderr);
    fputc('\n', stderr);
  }
}


static void printErrorAtCurrent(const char *msg) {
  if (CurTok.type != INVALID && CurTok.type != EOF_TOK) {
    printErrorAt(CurTok, msg);
  } else {
    const char *fname = InputFileName ? InputFileName : "<input>";
    fprintf(stderr, "%s: error: %s\n", fname, msg);
  }
}


static void printCodegenErrorAtNode(const ASTnode* node, const char *msg) {
  if (!node) {
    const char *fname = InputFileName ? InputFileName : "<input>";
    fprintf(stderr,
            "\033[1m%s:\033[0m \033[1;31merror:\033[0m %s\n",
            fname, msg);
    noteError();
    return;
  }

  int line = node->getLine();
  int col  = node->getCol();

  if (line < 1 || col < 1 || line > (int)SourceLines.size()) {
    const char *fname = InputFileName ? InputFileName : "<input>";
    fprintf(stderr,
            "\033[1m%s:\033[0m \033[1;31merror:\033[0m %s\n",
            fname, msg);
    noteError();
    return;
  }

  printErrorAtLocation(line, col, msg);
  noteError();
}



// Print an error at an explicit (line, col)
static void printErrorAtLocation(int line, int col, const char *msg) {
  TOKEN fake;
  fake.type     = INVALID;
  fake.lexeme   = "";
  fake.lineNo   = line;
  fake.columnNo = col;
  printErrorAt(fake, msg);
}


// Same, but driven from an AST node
static void printErrorAtNode(const ASTnode *node, const char *msg) {
  if (node) {
    int line = node->getLine();
    int col  = node->getCol();
    if (line > 0 && col > 0) {
      printErrorAtLocation(line, col, msg);
      return;
    }
  }
  // Fallback if node has no location
  printErrorAtCurrent(msg);
}


static void reportNodeError(const ASTnode* node, const char* msg) {
  if (node) {
    int line = node->getLine();
    int col  = node->getCol();
    if (line > 0 && col > 0) {
      TOKEN fake;
      fake.type     = INVALID;
      fake.lexeme   = "";
      fake.lineNo   = line;
      fake.columnNo = col;
      printErrorAt(fake, msg);
      noteError();
      return;
    }
  }

  // Fallback if node has no usable location
  fprintf(stderr, "error: %s\n", msg);
  noteError();
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
static std::vector<int> ParseArrayDimsDecl();

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
static std::unique_ptr<ASTnode> ParseArrayAssignStmt(); // new for arrays
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
      if (TraceParser)
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
  // Expect a type token first
  if (CurTok.type != INT_TOK &&
      CurTok.type != FLOAT_TOK &&
      CurTok.type != BOOL_TOK) {
    LogError(CurTok, "expected 'int', 'float' or 'bool' in parameter");
  }

  TOKEN typeTok = CurTok;           // remember which type
  std::string Type = CurTok.lexeme; // "int", "float", "bool"
  getNextToken();                   // eat type

  // Now expect the identifier
  if (CurTok.type != IDENT) {
    LogError(CurTok, "expected identifier in parameter declaration");
  }
  std::string Name = CurTok.getIdentifierStr();
  getNextToken(); // eat IDENT

  std::vector<int> dims;  // new

  // Optional array suffix for int/float parameters:
  //   int a[10]
  //   int a[10][5]
  if ((typeTok.type == INT_TOK || typeTok.type == FLOAT_TOK) &&
      CurTok.type == LBOX) {
    dims = ParseArrayDimsDecl();   // store the dimensions
  }

  return std::make_unique<ParamAST>(Name, Type, std::move(dims));
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

// primary ::= '(' expr ')' 
//          | IDENT 
//          | IDENT '(' args ')' 
//          | IDENT '[' expr ']' ...   (1–3 dims)
//          | INT_LIT | FLOAT_LIT | BOOL_LIT
static std::unique_ptr<ASTnode> ParsePrimary() {
  switch (CurTok.type) {
    case LPAR: {
      getNextToken(); // eat '('
      auto inner = ParseExper();
      if (!inner) return nullptr;
      if (CurTok.type != RPAR)
        return LogError(CurTok, "expected ')'");
      getNextToken(); // eat ')'
      return inner;
    }

    case IDENT: {
      TOKEN identTok = CurTok;
      std::string name = identTok.getIdentifierStr();
      getNextToken(); // eat IDENT

      // function call: name(...)
      if (CurTok.type == LPAR) {
        getNextToken(); // eat '('
        std::vector<std::unique_ptr<ASTnode>> args;
        ParseArgs(args);
        if (CurTok.type != RPAR)
          return LogError(CurTok, "expected ')' after arguments");
        getNextToken(); // eat ')'
        return std::make_unique<CallExprAST>(identTok, name, std::move(args));
      }

      // array element access: name[...][...]...
      if (CurTok.type == LBOX) {
        auto indices = ParseArrayIndices();
        if (indices.empty()) {
          return LogError(CurTok, "array access needs at least one index");
        }
        return std::make_unique<ArrayAccessAST>(identTok, name,
                                                std::move(indices));
      }

      // plain variable
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

  // Special-case only IDENT "[" ... "]" "=" ... as an array assignment
  if (CurTok.type == IDENT) {
    TOKEN identTok = CurTok;
    TOKEN firstAfterIdent = getNextToken();  // peek after IDENT

    if (firstAfterIdent.type == LBOX) {
      // We have IDENT "[" ...  so scan ALL [expr] groups (1D, 2D, 3D)
      std::vector<TOKEN> lookahead;
      lookahead.push_back(firstAfterIdent);  // first '[' already seen

      int dimsSeen = 0;
      TOKEN afterTok;        // first non-'[' token after all index groups
      bool done = false;

      while (!done) {
        // Scan one [ ... ] block (may contain nested brackets in the index expr)
        int depth = 1;
        while (depth > 0) {
          TOKEN t = getNextToken();
          lookahead.push_back(t);

          if (t.type == LBOX)      depth++;
          else if (t.type == RBOX) depth--;

          if (t.type == EOF_TOK) {
            LogError(t, "unterminated '[' in array reference");
          }
        }

        dimsSeen++;
        if (dimsSeen > 3) {
          LogError(firstAfterIdent,
                   "only 1D, 2D and 3D array indexing is supported");
        }

        // Look at what comes immediately after this dimension
        TOKEN t2 = getNextToken();
        if (t2.type == LBOX) {
          // Another "[", so another dimension follows
          lookahead.push_back(t2);
          continue;
        } else {
          // First non-'[' token after all [..] groups
          afterTok = t2;
          lookahead.push_back(afterTok);
          done = true;
        }
      }

      bool isArrayAssign = (afterTok.type == ASSIGN);

      // Restore all lookahead tokens back into the buffer in reverse order
      for (int i = (int)lookahead.size() - 1; i >= 0; --i) {
        putBackToken(lookahead[i]);
      }

      // Restore current token to the IDENT at the start of the statement
      CurTok = identTok;

      if (isArrayAssign) {
        auto arr_assign = ParseArrayAssignStmt();
        if (arr_assign) {
          if (TraceParser)
          fprintf(stderr, "Parsed an array assignment statement\n");
          return arr_assign;
        }
        // If ParseArrayAssignStmt failed, it will already have reported an error
      }
      // If not followed by '=', fall through and treat as a normal expr_stmt

    } else {
      // No '[' after IDENT, put token back and fall through to normal parsing
      putBackToken(firstAfterIdent);
      CurTok = identTok;
    }
  }

  // Normal statement kinds
  if (CurTok.type == NOT || CurTok.type == MINUS || CurTok.type == PLUS ||
      CurTok.type == LPAR || CurTok.type == IDENT || CurTok.type == BOOL_LIT ||
      CurTok.type == INT_LIT || CurTok.type == FLOAT_LIT ||
      CurTok.type == SC) { // FIRST(expr_stmt)
    auto expr_stmt = ParseExperStmt();
    if (TraceParser)
      fprintf(stderr, "Parsed an expression statement\n");
    return expr_stmt;

  } else if (CurTok.type == LBRA) { // block
    auto block_stmt = ParseBlock();
    if (block_stmt) {
      if (TraceParser)
        fprintf(stderr, "Parsed a block\n");
      return block_stmt;
    }

  } else if (CurTok.type == IF) { // if_stmt
    auto if_stmt = ParseIfStmt();
    if (if_stmt) {
      if (TraceParser)
        fprintf(stderr, "Parsed an if statment\n");
      return if_stmt;
    }

  } else if (CurTok.type == WHILE) { // while_stmt
    auto while_stmt = ParseWhileStmt();
    if (while_stmt) {
      if(TraceParser)
        fprintf(stderr, "Parsed a while statment\n");
      return while_stmt;
    }

  } else if (CurTok.type == RETURN) { // return_stmt
    auto return_stmt = ParseReturnStmt();
    if (return_stmt) {
      if (TraceParser)
        fprintf(stderr, "Parsed a return statment\n");
      return return_stmt;
    }

  } else {
    // syntax error
    return LogError(CurTok, "expected BLA BLA\n");
  }

  return nullptr;
}


// stmt_list ::= stmt stmt_list_prime
static std::vector<std::unique_ptr<ASTnode>> ParseStmtList() {
  std::vector<std::unique_ptr<ASTnode>> stmt_list;

  // Allow empty statement list: block can be "{}"
  if (CurTok.type == RBRA) {
    return stmt_list;
  }

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

static std::unique_ptr<ASTnode> ParseArrayAssignStmt() {
  // We are at IDENT, and the next token is "[" (checked in ParseStmt)

  // Use ParsePrimary to build an ArrayAccessAST from IDENT [expr]...
  auto lhsExpr = ParsePrimary();
  if (!lhsExpr) return nullptr;

  // ParsePrimary with IDENT "[" ... gives an ArrayAccessAST,
  // so this static_cast is safe here.
  auto *lhsRaw = static_cast<ArrayAccessAST*>(lhsExpr.release());
  std::unique_ptr<ArrayAccessAST> lhs(lhsRaw);

  if (CurTok.type != ASSIGN) {
    return LogError(CurTok, "expected '=' in array assignment");
  }
  getNextToken(); // eat '='

  auto rhs = ParseExper();
  if (!rhs) return nullptr;

  if (CurTok.type != SC) {
    return LogError(CurTok, "expected ';' to end array assignment");
  }
  getNextToken(); // eat ';'

  return std::make_unique<ArrayAssignAST>(std::move(lhs), std::move(rhs));
}


// local_decl ::= var_type IDENT ";"
//              | var_type IDENT array_suffix ";"
static std::unique_ptr<VarDeclAST> ParseLocalDecl() {
  TOKEN typeTok;
  std::string Type;
  std::string Name;
  std::unique_ptr<VarDeclAST> local_decl;

  if (CurTok.type == INT_TOK || CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {
    typeTok = CurTok;          // remember type token
    getNextToken();            // eat 'int' / 'float' / 'bool'

    if (CurTok.type != IDENT) {
      LogError(CurTok, "expected identifier in local variable declaration");
      return nullptr;
    }

    Type = typeTok.lexeme;     // "int", "float", "bool"
    Name = CurTok.getIdentifierStr();
    TOKEN identTok = CurTok;   // for precise error location
    auto ident = std::make_unique<VariableASTnode>(identTok, Name);
    getNextToken();            // eat IDENT

    // Optional array suffix, only for int/float
    if ((typeTok.type == INT_TOK || typeTok.type == FLOAT_TOK) &&
        CurTok.type == LBOX) {

      auto dims = ParseArrayDimsDecl(); // one to three INT_LIT dims

      if (CurTok.type != SC) {
        LogError(identTok, "expected ';' after array declaration");
        return nullptr;
      }
      getNextToken(); // eat ';'
      if (TraceParser)
        fprintf(stderr, "Parsed a local array declaration\n");
      local_decl = std::make_unique<ArrayDeclAST>(
          std::move(ident),
          Type,
          std::move(dims));
    } else {
      // Scalar local
      if (CurTok.type != SC) {
        LogError(identTok, "Expected ';' to end local variable declaration");
        return nullptr;
      }
      getNextToken(); // eat ';'
      if (TraceParser)
        fprintf(stderr, "Parsed a local variable declaration\n");
      local_decl = std::make_unique<VarDeclAST>(
          std::move(ident),
          Type);
    }
  } else {
    LogError(CurTok, "expected 'int', 'float' or 'bool' in local declaration");
    return nullptr;
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
             CurTok.type == WHILE || CurTok.type == RBRA) { // FOLLOW(local_decls)
    // do nothing
  } else {
    LogError(CurTok, "unexpected token after local variable declaration");
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
  if (TraceParser)
    fprintf(stderr, "Parsed a set of local variable declaration\n");
  stmt_list = ParseStmtList();
  if (TraceParser)
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
//       |  array_decl     // new
static std::unique_ptr<ASTnode> ParseDecl() {
  std::string IdName;
  std::vector<std::unique_ptr<ParamAST>> param_list;

  // Remember the type token for later (int/float/bool/void)
  TOKEN typeTok = CurTok;

  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) {

    // eat the type
    getNextToken();

    // expect an identifier next
    if (CurTok.type != IDENT) {
      return LogError(CurTok, "expected identifier after type specifier");
    }

    TOKEN nameTok = CurTok;              // save location of function name
    IdName = CurTok.getIdentifierStr();
    auto ident = std::make_unique<VariableASTnode>(CurTok, IdName);
    getNextToken(); // eat IDENT


    // ================= ARRAY DECLARATION (global) =================
    // Only int[] and float[] are allowed as arrays
    if ((typeTok.type == INT_TOK || typeTok.type == FLOAT_TOK) &&
        CurTok.type == LBOX) {

      // Parse [N], [N][M] or [N][M][K]
      auto dims = ParseArrayDimsDecl();

      if (CurTok.type != SC) {
        return LogError(CurTok, "expected ';' after array declaration");
      }
      getNextToken(); // eat ';'
      if (TraceParser)
        fprintf(stderr, "Parsed a global array declaration\n");
      return std::make_unique<ArrayDeclAST>(
          std::move(ident),
          typeTok.lexeme,      // "int" or "float"
          std::move(dims));
    }
    // ================= END ARRAY DECLARATION BRANCH =================

    // Plain global variable declaration: int x;
    if (CurTok.type == SC) {
      getNextToken(); // eat ';'
      if (TraceParser)
        fprintf(stderr, "Parsed a variable declaration\n");

      if (typeTok.type == VOID_TOK) {
        return LogError(typeTok, "cannot declare variable of type 'void'");
      }

      return std::make_unique<GlobVarDeclAST>(
          std::move(ident),
          typeTok.lexeme);
    }

    // Function declaration: int f(...) { ... }
    if (CurTok.type == LPAR) {
      getNextToken(); // eat '('

      auto P = ParseParams();
      if (TraceParser)
        fprintf(stderr, "Parsed parameter list for function\n");

      if (CurTok.type != RPAR) {
        return LogError(CurTok, "expected ')' in function declaration");
      }
      getNextToken(); // eat ')'

      if (CurTok.type != LBRA) {
        return LogError(
            CurTok,
            "expected '{' to start function body in declaration");
      }

      auto B = ParseBlock();
      if (!B) {
        return nullptr;
      }
      if (TraceParser){
        fprintf(stderr, "Parsed block of statements in function\n");
        fprintf(stderr, "Parsed a function declaration\n");
      }

      auto Proto = std::make_unique<FunctionPrototypeAST>(
          IdName,
          typeTok.lexeme,
          std::move(P));

      return std::make_unique<FunctionDeclAST>(
          nameTok,                 // new: location
          std::move(Proto),
          std::move(B));

    }

    // None of ';', '[', '(' after identifier
    return LogError(CurTok, "expected ';', '[', or '(' after declarator");
  }

  LogError(CurTok, "expected 'void', 'int', 'float' or 'bool' or EOF token");
  return nullptr;
}


// decl_list_prime ::= decl decl_list_prime
//                  |  ε
static void ParseDeclListPrime() {
  if (CurTok.type == VOID_TOK || CurTok.type == INT_TOK ||
      CurTok.type == FLOAT_TOK || CurTok.type == BOOL_TOK) { // FIRST(decl)

    if (auto decl = ParseDecl()) {
      if (TraceParser)
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
    if (TraceParser)
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
            if (TraceParser)
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
      if (TraceParser)
        fprintf(stderr,"Parsed a top-level external function declaration -- 2\n");
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
    if (TraceParser)
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

static Value* codegenError(const ASTnode* node, const char* msg) {
  printCodegenErrorAtNode(node, msg);
  return nullptr;
}

// ---- Codegen globals and helpers ----

// Current frame locals (name -> alloca)
static std::map<std::string, AllocaInst*> NamedValues;

// Stack of outer frames
static std::vector<std::map<std::string, AllocaInst*>> ScopeStack;

// Global symbols for file scope (name -> GlobalVariable)
static std::map<std::string, GlobalVariable*> GlobalNamedValues;

// Metadata for array parameters (name -> element type and dims)
struct ArrayParamMeta {
  Type* ElemTy;              // pointee type of the parameter pointer
  std::vector<int> Dims;     // full declared dims, e.g. [5], [5,3]
};

static std::map<std::string, ArrayParamMeta> ArrayParamInfo;
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
  noteError();
  return nullptr;
}


// Build a nested LLVM ArrayType for dims like [D1][D2][D3]
static Type* buildArrayType(Type* elemTy, const std::vector<int>& dims) {
  if (!elemTy) {
    // previous error, propagate
    return nullptr;
  }

  Type* arrTy = elemTy;
  for (int i = (int)dims.size() - 1; i >= 0; --i) {
    if (dims[i] <= 0) {
      fprintf(stderr, "Array dimension must be positive, got %d\n", dims[i]);
      noteError();
      return nullptr;
    }
    arrTy = ArrayType::get(arrTy, dims[i]);
  }
  return arrTy;
}



// Build an LLVM FunctionType from a MiniC prototype
static FunctionType* functionTypeFromProto(const FunctionPrototypeAST* P) {
  std::vector<Type*> paramTypes;
  paramTypes.reserve(P->getSize());

  for (const auto& param : P->getParams()) {
    Type* baseTy = typeFromString(param->getType());
    if (!baseTy) {
      // typeFromString already printed an error and called noteError()
      return nullptr;
    }

    const auto& dims = param->getDims();

    if (dims.empty()) {
      // Scalar parameter
      paramTypes.push_back(baseTy);
    } else {
      // Array parameter, represent as a pointer in LLVM IR
      Type* elemTy = baseTy;

      if (dims.size() > 1) {
        std::vector<int> tailDims(dims.begin() + 1, dims.end());
        for (int i = (int)tailDims.size() - 1; i >= 0; --i) {
          elemTy = ArrayType::get(elemTy, tailDims[i]);
        }
      }

      Type* ptrTy = PointerType::get(TheContext, 0);
      paramTypes.push_back(ptrTy);
    }
  }

  Type* retTy = typeFromString(P->getType());
  if (!retTy) {
    // typeFromString already reported the error
    return nullptr;
  }

  return FunctionType::get(retTy, paramTypes, /*isVarArg=*/false);
}



// Declare a function in the module from a prototype, or check a previous one
static Function* declareFunctionFromProto(const FunctionPrototypeAST* P) {
  const std::string& name = P->getName();
  FunctionType* FT = functionTypeFromProto(P);
  if(!FT) {
    // functionTypeFromProto already reported the error
    return nullptr;
  }

  // Look for an existing function with this name
  Function* F = TheModule->getFunction(name);

  if (F) {
    // Check the type matches the prototype
    if (F->getFunctionType() != FT) {
      fprintf(stderr, "Conflicting declarations for function '%s'\n", name.c_str());
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

// Map LLVM types back to MiniC type names for diagnostics
static const char *miniCTypeName(Type *T) {
  if (isIntTy(T))   return "int";
  if (isBoolTy(T))  return "bool";
  if (isFloatTy(T)) return "float";
  return "unknown type";
}


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
    noteError();
    popScope();
    return nullptr;
  }

  Function* F = curBB->getParent();
  if (!F) {
    fprintf(stderr, "Block codegen used outside of a function\n");
    noteError();
    popScope();
    return nullptr;
  }

  // Allocate each local via its own codegen (works for scalars and arrays)
  for (auto& d : LocalDecls) {
    d->codegen();
  }

  // Emit statements in order
  for (auto& s : Stmts) {
    if (s) s->codegen();
  }

  popScope();
  return nullptr;
}



Value* VarDeclAST::codegen() {
  BasicBlock* curBB = Builder.GetInsertBlock();
  if (!curBB) {
    fprintf(stderr, "Local variable '%s' declared outside of a function\n",
            getName().c_str());
    noteError();
    return nullptr;
  }

  Function* F = curBB->getParent();
  if (!F) {
    fprintf(stderr, "Local variable '%s' declared with no parent function\n",
            getName().c_str());
    noteError();
    return nullptr;
  }

  const std::string& name = getName();
  if (lookupLocalCurrent(name)) {
    std::string msg = "Redeclaration of local '" + name + "'";
    reportNodeError(Var.get(), msg.c_str());
    return nullptr;
  }


  Type* ty = typeFromString(getType());
  if (!ty) {
    // typeFromString already reported the error
    return nullptr;
  }

  AllocaInst* slot = CreateEntryAlloca(F, name, ty);
  Builder.CreateStore(zeroOf(ty), slot);
  NamedValues[name] = slot;
  return slot;
}


// ============================================================================
// Array helpers: compute element address for arrays and array parameters
// ============================================================================

static bool codegenIndexAsInt(
    const std::string &name,
    ASTnode *idxNode,
    Value *&idxVal) {

  idxVal = idxNode->codegen();
  if (!idxVal) {
    // Index expression already reported an error
    return false;
  }

  Type *idxTy = idxVal->getType();
  if (!isIntTy(idxTy)) {
    std::string msg = "array index for '" + name + "' must be int";
    printCodegenErrorAtNode(idxNode, msg.c_str());
    return false;
  }

  return true;
}


// Compute address and element type for any array-style access:
//
//   - plain local/global arrays (alloca or global of ArrayType)
//   - pointer parameters which represent arrays (ArrayParamInfo)
//
// On success:
//   elemPtr is a pointer to element
//   elemTy is element type
//
// On failure it reports an error and returns false.
static bool getArrayElementAddress(
    const std::string &name,
    const std::vector<std::unique_ptr<ASTnode>> &indices,
    ASTnode *loc,
    Value *&elemPtr,
    Type *&elemTy) {

  elemPtr = nullptr;
  elemTy  = nullptr;

  if (indices.empty()) {
    std::string msg = "array access on '" + name + "' needs at least one index";
    printCodegenErrorAtNode(loc, msg.c_str());
    return false;
  }

  // Find base symbol
  Value      *basePtr     = nullptr;
  Type       *baseTy      = nullptr;
  AllocaInst *localAlloca = nullptr;

  if (AllocaInst *local = lookupLocal(name)) {
    basePtr     = local;
    baseTy      = local->getAllocatedType();
    localAlloca = local;
  } else if (GlobalVariable *global = lookupGlobal(name)) {
    basePtr = global;
    baseTy  = global->getValueType();
  } else {
    std::string msg = "Unknown array '" + name + "'";
    printErrorAtNode(loc, msg.c_str());
    noteError();
    return false;
  }

  // Case 1: real array variable (local or global)
  if (auto *arrTy = dyn_cast<ArrayType>(baseTy)) {
    std::vector<Value*> gepIdx;
    // For real arrays first GEP index is zero to step from alloca/global
    gepIdx.push_back(ConstantInt::get(getIntTy(), 0));

    Type *curTy = baseTy;

    for (size_t i = 0; i < indices.size(); ++i) {
      if (!isa<ArrayType>(curTy)) {
        std::string msg = "too many indices supplied for array '" + name + "'";
        printCodegenErrorAtNode(indices[i].get(), msg.c_str());
        return false;
      }

      Value *idxVal = nullptr;
      if (!codegenIndexAsInt(name, indices[i].get(), idxVal)) return false;

      gepIdx.push_back(idxVal);
      curTy = cast<ArrayType>(curTy)->getElementType();
    }

    if (isa<ArrayType>(curTy)) {
      std::string msg = "too few indices supplied for array '" + name + "'";
      printCodegenErrorAtNode(indices.back().get(), msg.c_str());
      return false;
    }

    elemTy = curTy;

    Value *baseVal = localAlloca
                     ? static_cast<Value*>(localAlloca)
                     : basePtr;

    elemPtr = Builder.CreateInBoundsGEP(
        baseTy,
        baseVal,
        gepIdx,
        name + std::string("_elem_ptr"));

    return elemPtr != nullptr;
  }

  // Case 2: pointer parameter which represents an array
  if (auto *ptrTy = dyn_cast<PointerType>(baseTy)) {
    // Load actual runtime pointer if this is a local alloca for the param
    Value *ptrVal = nullptr;
    if (localAlloca) {
      ptrVal = Builder.CreateLoad(ptrTy, localAlloca, name + "_ptr");
    } else {
      ptrVal = basePtr;
    }

    auto metaIt = ArrayParamInfo.find(name);
    if (metaIt == ArrayParamInfo.end()) {
      fprintf(stderr,
              "Pointer variable '%s' used in array access is not a known array parameter\n",
              name.c_str());
      noteError();
      return false;
    }

    Type *metaElemTy = metaIt->second.ElemTy;
    Type *curElemTy  = metaElemTy;

    std::vector<Value*> gepIdx;

    // First index steps on pointer itself
    Value *firstIdx = nullptr;
    if (!codegenIndexAsInt(name, indices[0].get(), firstIdx)) return false;
    gepIdx.push_back(firstIdx);

    // Remaining indices walk nested ArrayType structure if present
    for (size_t i = 1; i < indices.size(); ++i) {
      if (!isa<ArrayType>(curElemTy)) {
        std::string msg = "too many indices for pointer parameter '" + name + "'";
        printCodegenErrorAtNode(indices[i].get(), msg.c_str());
        return false;
      }

      Value *idxVal = nullptr;
      if (!codegenIndexAsInt(name, indices[i].get(), idxVal)) return false;

      gepIdx.push_back(idxVal);
      curElemTy = cast<ArrayType>(curElemTy)->getElementType();
    }

    if (isa<ArrayType>(curElemTy)) {
      std::string msg = "too few indices for pointer parameter '" + name + "'";
      printCodegenErrorAtNode(indices.back().get(), msg.c_str());
      return false;
    }

    elemTy = curElemTy;

    elemPtr = Builder.CreateInBoundsGEP(
        metaElemTy,
        ptrVal,
        gepIdx,
        name + std::string("_elem_ptr"));

    return elemPtr != nullptr;
  }

  // Not an array or pointer
  {
    std::string msg = "'" + name + "' is not an array or pointer";
    printErrorAtNode(loc, msg.c_str());
    noteError();
    return false;
  }
}

// Forward declaration for widening helper used by array assignments
static Value *widenOrError(Value *v, Type *targetTy, ASTnode *loc, const char *contextPrefix);


// ----------------------------------------------------------------------------
// Array assignment codegen
// ----------------------------------------------------------------------------

/**
 * ArrayAssignAST::codegen:
 * Resolve LHS element pointer, widen RHS if allowed, then store.
 */
Value *ArrayAssignAST::codegen() {
  const std::string &name    = LHS->getName();
  const auto        &indices = LHS->getIndices();

  Value *elemPtr = nullptr;
  Type  *elemTy  = nullptr;

  // Compute address of the array element
  if (!getArrayElementAddress(name, indices, LHS.get(), elemPtr, elemTy)) {
    // getArrayElementAddress already reported an error
    return nullptr;
  }

  // Generate RHS expression
  Value *rhsV = RHS->codegen();
  if (!rhsV) return nullptr;

  // Disallow assigning array or pointer values to a scalar element
  Type *rhsTy = rhsV->getType();
  if (rhsTy->isArrayTy() || rhsTy->isPointerTy()) {
    std::string msg = "array used as scalar in array assignment";
    printErrorAtNode(LHS.get(), msg.c_str());
    noteError();
    return nullptr;
  }

  // Enforce widening rule for assignment into the element type
  rhsV = widenOrError(rhsV, elemTy, LHS.get(), "assigning to array element");
  if (!rhsV) return nullptr;

  // Store value into the computed element address
  Builder.CreateStore(rhsV, elemPtr);
  return rhsV;
}


// ----------------------------------------------------------------------------
// Array declaration helpers and codegen
// ----------------------------------------------------------------------------

// vtable key
ArrayDeclAST::~ArrayDeclAST() = default;

/**
 * Create a zero-initialised global array and register it.
 */
static Value *emitGlobalArrayDecl(const std::string &name, Type *arrTy) {
  if (lookupGlobal(name) || GlobalNamedValues.count(name)) {
    fprintf(stderr, "Redeclaration of global array '%s'\n", name.c_str());
    noteError();
    return nullptr;
  }

  if (TheModule->getFunction(name)) {
    fprintf(stderr,
            "Global array '%s' conflicts with a function of the same name\n",
            name.c_str());
    noteError();
    return nullptr;
  }

  Constant *init = ConstantAggregateZero::get(arrTy);
  auto *G = new GlobalVariable(
      *TheModule,
      arrTy,
      /*isConstant=*/false,
      GlobalValue::ExternalLinkage,
      init,
      name);

  GlobalNamedValues[name] = G;
  return G;
}

/**
 * Create a zero-initialised local array in the function entry block.
 */
static Value *emitLocalArrayDecl(const std::string &name, Type *arrTy, Function *F) {
  if (lookupLocalCurrent(name)) {
    fprintf(stderr, "Redeclaration of local array '%s'\n", name.c_str());
    noteError();
    return nullptr;
  }

  AllocaInst *slot = CreateEntryAlloca(F, name, arrTy);
  Builder.CreateStore(ConstantAggregateZero::get(arrTy), slot);
  NamedValues[name] = slot;
  return slot;
}

/**
 * ArrayDeclAST::codegen:
 * Build array type, then emit local or global storage, set to zero.
 */
Value *ArrayDeclAST::codegen() {
  const std::string      &name = getName();
  Type                   *elemTy = typeFromString(getType());
  const std::vector<int> &dims   = getDims();

  if (dims.empty()) {
    fprintf(stderr, "Array '%s' has no dimensions\n", name.c_str());
    noteError();
    return nullptr;
  }

  if (!elemTy) return nullptr;

  Type *arrTy = buildArrayType(elemTy, dims);
  if (!arrTy) return nullptr;

  BasicBlock *curBB = Builder.GetInsertBlock();
  Function   *F     = curBB ? curBB->getParent() : nullptr;

  if (!F) return emitGlobalArrayDecl(name, arrTy);
  return emitLocalArrayDecl(name, arrTy, F);
}


/**
 * ArrayAccessAST::codegen:
 * Resolve element pointer and load the element value.
 */
Value *ArrayAccessAST::codegen() {
  const std::string &name    = getName();
  const auto        &indices = getIndices();

  Value *elemPtr = nullptr;
  Type  *elemTy  = nullptr;

  if (!getArrayElementAddress(name, indices, this, elemPtr, elemTy)) {
    return nullptr;
  }

  return Builder.CreateLoad(elemTy, elemPtr, name + "_elem_val");
}


//================== Binary codegen ==================

Value* BinaryExprAST::codegen() {
  Value* lhs = getLHS()->codegen();
  Value* rhs = getRHS()->codegen();
  if (!lhs || !rhs) return nullptr;

  Type* lhs_type = lhs->getType();
  Type* rhs_type = rhs->getType();

  // Remember original bool-ness before promotion
  bool lhsWasBool = isBoolTy(lhs_type);
  bool rhsWasBool = isBoolTy(rhs_type);

  Type* common_type = getCommonType(lhs_type, rhs_type);

  int op = getOpTok();

  // Logical operators: force both sides to bool first
  if (op == AND || op == OR) {
    lhs = castTo(miniCBoolTy(), lhs, "logical lhs");
    rhs = castTo(miniCBoolTy(), rhs, "logical rhs");

    if (op == AND) {
      return Builder.CreateAnd(lhs, rhs, "andtmp");
    } else {
      return Builder.CreateOr(lhs, rhs, "ortmp");
    }
  }

  // For arithmetic and comparisons, promote both sides to the common type
  lhs = castTo(common_type, lhs, "binary lhs");
  rhs = castTo(common_type, rhs, "binary rhs");

  bool useFloat   = isFloat(common_type);
  bool useIntLike = isInt(common_type) || isBool(common_type);

  switch (op) {
    // arithmetic
    case PLUS:
      if (useFloat)   return Builder.CreateFAdd(lhs, rhs, "addtmp");
      if (useIntLike) return Builder.CreateAdd(lhs, rhs, "addtmp");
      printErrorAtNode(getLHS(), "invalid operand types for '+'");
      noteError();
      return nullptr;

    case MINUS:
      if (useFloat)   return Builder.CreateFSub(lhs, rhs, "subtmp");
      if (useIntLike) return Builder.CreateSub(lhs, rhs, "subtmp");
      printErrorAtNode(getLHS(), "invalid operand types for '-'");
      noteError();
      return nullptr;

    case ASTERIX:
      if (useFloat)   return Builder.CreateFMul(lhs, rhs, "multmp");
      if (useIntLike) return Builder.CreateMul(lhs, rhs, "multmp");
      printErrorAtNode(getLHS(), "invalid operand types for '*'");
      noteError();
      return nullptr;

    case DIV:
      if (useFloat)   return Builder.CreateFDiv(lhs, rhs, "divtmp");
      if (useIntLike) return Builder.CreateSDiv(lhs, rhs, "divtmp");
      printErrorAtNode(getLHS(), "invalid operand types for '/'");
      noteError();
      return nullptr;

    case MOD:
      if (useFloat || lhsWasBool || rhsWasBool) {
        std::string msg = "invalid operands to binary '%' expression ('" +
          std::string(miniCTypeName(lhs_type)) + "' and '" +
          std::string(miniCTypeName(rhs_type)) + "')";
        printErrorAtNode(getLHS(), msg.c_str());
        noteError();
        return nullptr;
      }

      if (useIntLike) {
        return Builder.CreateSRem(lhs, rhs, "modtmp");
      }

      printErrorAtNode(getLHS(), "invalid operand types for '%'");
      noteError();
      return nullptr;


    // comparisons (return i1)
    case LT:
      if (useFloat)   return Builder.CreateFCmpOLT(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSLT(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '<'");
      noteError();
      return nullptr;

    case LE:
      if (useFloat)   return Builder.CreateFCmpOLE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSLE(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '<='");
      noteError();
      return nullptr;

    case GT:
      if (useFloat)   return Builder.CreateFCmpOGT(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSGT(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '>'");
      noteError();
      return nullptr;

    case GE:
      if (useFloat)   return Builder.CreateFCmpOGE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpSGE(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '>='");
      noteError();
      return nullptr;

    case EQ:
      if (useFloat)   return Builder.CreateFCmpOEQ(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpEQ(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '=='");
      noteError();
      return nullptr;

    case NE:
      if (useFloat)   return Builder.CreateFCmpONE(lhs, rhs, "cmptmp");
      if (useIntLike) return Builder.CreateICmpNE(lhs, rhs, "cmptmp");
      printErrorAtNode(getLHS(), "invalid operand types for '!='");
      noteError();
      return nullptr;

    default:
      printErrorAtNode(this, "unknown binary operator");
      noteError();
      return nullptr;
  }
}

Value *GlobVarDeclAST::codegen() {
  // MINIC_RULE_GLOBAL_ONCE
  const std::string &name = getName();

  // Look up LLVM type for this MiniC type name
  llvm::Type *varTy = typeFromString(getType());
  if (!varTy) {
    // typeFromString already printed an error and bumped ErrorCount
    return nullptr;
  }

  // Check for redeclaration
  if (lookupGlobal(name) || GlobalNamedValues.count(name)) {
    std::string msg = "Redeclaration of global '" + name + "'";
    reportNodeError(Var.get(), msg.c_str());   // report at variable site
    return nullptr;
  }

  // Disallow a global with the same name as a function
  if (TheModule->getFunction(name)) {
    std::string msg = "Global variable '" + name + "' conflicts with a function of the same name";
    reportNodeError(Var.get(), msg.c_str());
    return nullptr;
  }

  // Default initialiser is zero for this type
  Constant *init = zeroOf(varTy);

  auto *G = new GlobalVariable(
      *TheModule,
      varTy,
      /*isConstant=*/false,
      GlobalValue::ExternalLinkage,
      init,
      name);

  GlobalNamedValues[name] = G;
  return G;
}



//---------------------------------------------------------
// FunctionDeclAST::codegen helpers
//---------------------------------------------------------

//---------------------------------------------------------
// checkFunctionGlobalClash
// Ensures a function name does not collide with a global variable.
// Returns true if safe, false if an error was reported.
//---------------------------------------------------------
static bool checkFunctionGlobalClash(const std::string &name, const ASTnode *loc) {
  // Only error if name exists as a global
  if (!lookupGlobal(name)) return true;

  std::string msg =
    "Function '" + name +
    "' conflicts with a global variable of the same name";
  reportNodeError(loc, msg.c_str());
  return false;
}



//---------------------------------------------------------
// getOrCreateFunction
// Reuse an existing function declaration if types match,
// otherwise create a fresh LLVM Function. Reports errors
// for mismatched types or redefinition.
//---------------------------------------------------------
static Function *getOrCreateFunction(const std::string &name, FunctionType *fnTy, const ASTnode *loc) {
  // Check if LLVM already has function
  Function *F = TheModule->getFunction(name);

  if (F) {
    // Must match prototype exactly
    if (F->getFunctionType() != fnTy) {
      std::string msg =
        "Definition of function '" + name +
        "' does not match a previous declaration";
      reportNodeError(loc, msg.c_str());
      return nullptr;
    }

    // Cannot redefine a function with a body
    if (!F->empty()) {
      std::string msg = "Redefinition of function '" + name + "'";
      reportNodeError(loc, msg.c_str());
      return nullptr;
    }

    return F; // safe to reuse
  }

  // Create new LLVM function object
  return Function::Create(fnTy, Function::ExternalLinkage, name, TheModule.get());
}



//---------------------------------------------------------
// setFunctionArgNames
// Applies the AST parameter names to LLVM function arguments.
//---------------------------------------------------------
static void setFunctionArgNames(Function *F, const FunctionPrototypeAST &proto) {
  unsigned idx = 0;

  // Assign each LLVM arg the corresponding AST name
  for (auto &arg : F->args()) {
    arg.setName(proto.getParams()[idx++]->getName());
  }
}



//---------------------------------------------------------
// resetFunctionScopeState
// Clears all per-function symbol state before codegen.
// Ensures no scope leakage between functions.
//---------------------------------------------------------
static void resetFunctionScopeState() {
  NamedValues.clear();     // local variable table
  ScopeStack.clear();      // lexical block stack
  ArrayParamInfo.clear();  // metadata for pointer-to-array params
}



//---------------------------------------------------------
// recordArrayParamMeta
// Stores metadata for array parameters so later element
// access computations (GEP) know the element type/dimensions.
// Returns false only if typeFromString failed.
//---------------------------------------------------------
static bool recordArrayParamMeta(const ParamAST *param, const std::string &argName) {
  // Skip if not an array parameter
  if (!param->isArrayParam()) return true;

  // Base element type (int/float/bool)
  Type *baseTy = typeFromString(param->getType());
  if (!baseTy) return false; // error already reported elsewhere

  const auto &dims = param->getDims();
  Type *elemTy = baseTy;

  // Reconstruct the element type for multi-dimensional arrays
  if (dims.size() > 1) {
    for (int i = (int)dims.size() - 1; i >= 1; --i) {
      elemTy = ArrayType::get(elemTy, dims[i]);
    }
  }

  // Store metadata for GEP computation later
  ArrayParamInfo[argName] = { elemTy, dims };
  return true;
}



//---------------------------------------------------------
// allocateAndStoreArgs
// Allocates each parameter in the function entry block,
// stores the incoming LLVM argument, and records array metadata.
// Returns false on any array type error.
//---------------------------------------------------------
static bool allocateAndStoreArgs(Function *F, const FunctionPrototypeAST &proto) {
  unsigned idx = 0;
  for (auto &arg : F->args()) {
    std::string argName = std::string(arg.getName());
    Type *argType = arg.getType();

    // Allocate space for parameter on stack
    AllocaInst *slot = CreateEntryAlloca(F, argName, argType);
    Builder.CreateStore(&arg, slot); // store initial value
    NamedValues[argName] = slot;     // record in local variable map

    // Check array metadata if parameter is an array param
    const auto &paramUPtr = proto.getParams()[idx];
    const ParamAST *param = paramUPtr.get();

    if (!recordArrayParamMeta(param, argName))
      return false; // typeFromString error already handled

    ++idx;
  }
  return true;
}



//---------------------------------------------------------
// insertDefaultReturnIfNeeded
// Ensures all functions have a terminating return instruction.
// Inserts:
//   - ret void     for void functions
//   - ret 0/false  for non-void functions
//---------------------------------------------------------
static void insertDefaultReturnIfNeeded(Function *F, FunctionType *fnTy) {
  BasicBlock *entry = &F->getEntryBlock();

  // Do nothing if the block already ends with a terminator
  if (entry->getTerminator()) return;

  Type *retTy = fnTy->getReturnType();

  if (retTy->isVoidTy()) {
    Builder.CreateRetVoid();
  } else {
    // zeroOf handles default literal (0 for int/float, false for bool)
    Builder.CreateRet(zeroOf(retTy));
  }
}



//---------------------------------------------------------
// verifyGeneratedFunction
// Runs LLVM’s verifier and prints MiniC-style error output
// if the IR is structurally invalid.
//---------------------------------------------------------
static bool verifyGeneratedFunction(Function *F, const std::string &name, const ASTnode *loc) {
  // verifyFunction returns false if valid
  if (!verifyFunction(*F)) return true;

  std::string msg = "invalid generated code for function '" + name + "'";
  printErrorAtNode(loc, msg.c_str());
  noteError();
  return false;
}

//---------------------------------------------------------
// FunctionDeclAST::codegen
//---------------------------------------------------------

Value *FunctionDeclAST::codegen() {
  const std::string &function_name = Proto->getName();

  // Disallow clash with global variable
  if (!checkFunctionGlobalClash(function_name, this)) return nullptr;

  // Build LLVM function type from prototype
  FunctionType *function_type = functionTypeFromProto(Proto.get());
  if (!function_type) return nullptr;

  // Reuse or create the LLVM Function
  Function *F = getOrCreateFunction(function_name, function_type, this);
  if (!F) return nullptr;

  // Sync LLVM argument names with AST parameter names
  setFunctionArgNames(F, *Proto);

  // Create entry block and start inserting there
  BasicBlock *entryBB = BasicBlock::Create(TheContext, "entry", F);
  Builder.SetInsertPoint(entryBB);

  // Fresh per-function symbol state
  resetFunctionScopeState();

  // Allocate arguments and record array parameter metadata
  if (!allocateAndStoreArgs(F, *Proto)) return nullptr;

  // Generate body code if present
  if (Block) Block->codegen();

  // Insert default return if body did not return
  insertDefaultReturnIfNeeded(F, function_type);

  // Verify generated function IR
  if (!verifyGeneratedFunction(F, function_name, this)) return nullptr;

  return F;
}



//---------------------------------------------------------
// Helper for callExpr , returnAST and AssignExprAST
//---------------------------------------------------------

/**
 * widenOrError:
 * Attempts to widen value v to targetTy according to MiniC rules.
 * If widening is illegal (narrowing), emits an error at loc.
 * Used by CallExpr, ReturnAST and AssignAST.
 */
static Value *widenOrError(Value *v,
                           Type *targetTy,
                           ASTnode *loc,
                           const char *contextPrefix) {
  Type *srcTy = v->getType();
  if (srcTy == targetTy) return v;

  if (!isWideningType(srcTy, targetTy)) {
    std::string msg = std::string(contextPrefix) + " '" +
      std::string(miniCTypeName(srcTy)) + "' to '" +
      std::string(miniCTypeName(targetTy)) + "' narrows value";
    printErrorAtNode(loc, msg.c_str());
    noteError();
    return nullptr;
  }

  // safe widening
  return castTo(targetTy, v, contextPrefix);
}



/**
 * CallExprAST::codegen:
 * Emits code for a function call.
 * - Looks up callee
 * - Validates argument count
 * - Generates each argument and widens if needed
 * - Emits llvm::CallInst
 */
Value* CallExprAST::codegen() {
  Function* calleeF = TheModule->getFunction(Callee);
  if (!calleeF) {
    std::string msg = "Unknown function '" + Callee + "' in call";
    printErrorAtNode(this, msg.c_str());
    noteError();
    return nullptr;
  }

  // MINIC rule: strict arg count
  if (calleeF->arg_size() != Args.size()) {
    std::string msg =
      "Function '" + Callee + "' expects " +
      std::to_string(static_cast<unsigned>(calleeF->arg_size())) +
      " args, got " + std::to_string(Args.size());
    printErrorAtNode(this, msg.c_str());
    noteError();
    return nullptr;
  }

  std::vector<Value*> argValues;
  argValues.reserve(Args.size());

  // generate and widen arguments
  unsigned idx = 0;
  for (auto &argExpr : Args) {
    Value *argVal = argExpr->codegen();
    if (!argVal) return nullptr;

    Type *paramTy = calleeF->getFunctionType()->getParamType(idx);

    // apply widening rule
    argVal = widenOrError(argVal, paramTy, argExpr.get(), "passing");
    if (!argVal) return nullptr;

    argValues.push_back(argVal);
    ++idx;
  }

  // create call instruction
  CallInst *callInst = Builder.CreateCall(
    calleeF,
    argValues,
    calleeF->getReturnType()->isVoidTy() ? "" : "calltmp"
  );

  return callInst;
}



//---------------------------------------------------------
// Shared helpers
//---------------------------------------------------------

/**
 * getCurrentFunctionOrError:
 * Ensures codegen is occurring inside a function and block.
 * Used by If, While, Return nodes.
 */
static Function *getCurrentFunctionOrError(const char *who, BasicBlock *&curBB) {
  curBB = Builder.GetInsertBlock();
  if (!curBB) {
    fprintf(stderr, "%s used with no insertion block\n", who);
    noteError();
    return nullptr;
  }

  Function *F = curBB->getParent();
  if (!F) {
    fprintf(stderr, "%s used outside of a function\n", who);
    noteError();
    return nullptr;
  }

  return F;
}


/**
 * codegenBoolCondExpr:
 * Generates condition expression and casts it to i1.
 * Used by If and While.
 */
static Value *codegenBoolCondExpr(ASTnode *condNode, const char *tag) {
  Value *condV = condNode->codegen();
  if (!condV) return nullptr;
  return castTo(miniCBoolTy(), condV, tag);
}



/**
 * IfExprAST::codegen:
 * Produces code for an if/else statement.
 * - Creates then, else, and merge blocks
 * - Redirects control flow depending on fallthrough
 */
Value* IfExprAST::codegen() {
  BasicBlock *curBB = nullptr;
  Function *F = getCurrentFunctionOrError("IfExprAST", curBB);
  if (!F) return nullptr;

  // compute boolean condition
  Value *condV = codegenBoolCondExpr(Cond.get(), "ifcond");
  if (!condV) return nullptr;

  BasicBlock *thenBB  = BasicBlock::Create(TheContext, "if.then", F);
  BasicBlock *elseBB  = Else ? BasicBlock::Create(TheContext, "if.else", F) : nullptr;
  BasicBlock *mergeBB = BasicBlock::Create(TheContext, "if.end");

  // conditional branch
  if (Else) Builder.CreateCondBr(condV, thenBB, elseBB);
  else      Builder.CreateCondBr(condV, thenBB, mergeBB);

  // THEN block
  Builder.SetInsertPoint(thenBB);
  if (Then) Then->codegen();
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(mergeBB);

  bool thenFallsThrough =
    thenBB->getTerminator() &&
    thenBB->getTerminator()->getOpcode() == Instruction::Br &&
    thenBB->getTerminator()->getSuccessor(0) == mergeBB;

  // ELSE block (if present)
  bool elseFallsThrough = false;
  if (Else) {
    Builder.SetInsertPoint(elseBB);
    Else->codegen();
    if (!Builder.GetInsertBlock()->getTerminator())
      Builder.CreateBr(mergeBB);

    elseFallsThrough =
      elseBB->getTerminator() &&
      elseBB->getTerminator()->getOpcode() == Instruction::Br &&
      elseBB->getTerminator()->getSuccessor(0) == mergeBB;
  }

  // attach merge block only if reachable
  if (thenFallsThrough || elseFallsThrough || !Else) {
    mergeBB->insertInto(F);
    Builder.SetInsertPoint(mergeBB);
  } else {
    // unreachable merge block
    delete mergeBB;
  }

  return nullptr;
}



/**
 * WhileExprAST::codegen:
 * Implements a classic while-loop:
 * - condition block
 * - body block
 * - end block
 * Each iteration re-evaluates Cond to loop or exit.
 */
Value* WhileExprAST::codegen() {
  BasicBlock *curBB = nullptr;
  Function *F = getCurrentFunctionOrError("WhileExprAST", curBB);
  if (!F) return nullptr;

  BasicBlock *condBB = BasicBlock::Create(TheContext, "while.cond", F);
  BasicBlock *bodyBB = BasicBlock::Create(TheContext, "while.body", F);
  BasicBlock *endBB  = BasicBlock::Create(TheContext, "while.end",  F);

  // jump to first condition check
  Builder.CreateBr(condBB);

  // condition evaluation
  Builder.SetInsertPoint(condBB);
  Value *condV = codegenBoolCondExpr(Cond.get(), "whilecond");
  if (!condV) return nullptr;
  Builder.CreateCondBr(condV, bodyBB, endBB);

  // loop body
  Builder.SetInsertPoint(bodyBB);
  if (Body) Body->codegen();
  // loop back if no terminator already inserted
  if (!Builder.GetInsertBlock()->getTerminator())
    Builder.CreateBr(condBB);

  Builder.SetInsertPoint(endBB);
  return nullptr;
}



//---------------------------------------------------------
// Return helpers
//---------------------------------------------------------

/**
 * checkVoidReturnAllowed:
 * Validates "return;" only appears in void functions.
 */
static bool checkVoidReturnAllowed(Type *funcRetTy, const ASTnode *loc) {
  if (funcRetTy->isVoidTy()) return true;

  printErrorAtNode(loc, "Non-void function missing return value");
  noteError();
  return false;
}



/**
 * ReturnAST::codegen:
 * Handles both `return;` and `return expr;`
 * Applies widening rules and creates LLVM return instruction.
 */
Value* ReturnAST::codegen() {
  BasicBlock *currBB = nullptr;
  Function *F = getCurrentFunctionOrError("ReturnAST", currBB);
  if (!F) return nullptr;

  Type *func_return_type = F->getReturnType();

  // bare return; only valid in void
  if (!Val) {
    if (!checkVoidReturnAllowed(func_return_type, this)) return nullptr;
    return Builder.CreateRetVoid();
  }

  // return with expression
  Value *return_val = Val->codegen();
  if (!return_val) return nullptr;

  // enforce widening rule
  return_val = widenOrError(return_val, func_return_type, Val.get(), "returning");
  if (!return_val) return nullptr;

  return Builder.CreateRet(return_val);
}



// ======== Literal codegen ========

/** Integer literal node */
Value* IntASTnode::codegen() {
  return ConstantInt::get(miniCIntTy(), Val, true);
}

/** Float literal node */
Value* FloatASTnode::codegen() {
  return ConstantFP::get(miniCFloatTy(), Val);
}

/** Bool literal node */
Value* BoolASTnode::codegen() {
  return ConstantInt::get(miniCBoolTy(), Bool ? 1 : 0, false);
}



// ======== Variable reference codegen ========

/**
 * VariableASTnode::codegen:
 * Loads value of a variable from either:
 * - a local alloca
 * - a global variable
 */
Value* VariableASTnode::codegen() {
  const std::string &name = getName();

  if (AllocaInst* local = lookupLocal(name)) {
    Type* ty = local->getAllocatedType();
    return Builder.CreateLoad(ty, local, name + "_val");
  }

  if (GlobalVariable* global = lookupGlobal(name)) {
    Type* ty = global->getValueType();
    return Builder.CreateLoad(ty, global, name + "_val");
  }

  std::string msg = "Unknown variable '" + name + "'";
  printErrorAtNode(this, msg.c_str());
  noteError();
  return nullptr;
}



// ======== Unary operator codegen ========

/**
 * UnaryExprAST::codegen:
 * Supports:
 *   - unary '-'
 *   - unary '!'
 * Performs widening for unary minus on bool → int.
 */
Value* UnaryExprAST::codegen() {
  Value* operandV = Operand->codegen();   // generate operand value
  if (!operandV) return nullptr;

  switch (Op) {
    case '-': {
      Type* T = operandV->getType();

      // bool → int before unary minus
      if (isBoolTy(T)) {
        operandV = castTo(getIntTy(), operandV, "u_minus");
        T = operandV->getType();          // refresh type after cast
      }

      // integer or float negation
      if (isFloatTy(T)) return Builder.CreateFNeg(operandV, "negtmp");
      if (isIntTy(T))   return Builder.CreateNeg(operandV, "negtmp");

      // anything else is invalid
      printErrorAtNode(Operand.get(), "Invalid operand type for unary '-'");
      noteError();
      return nullptr;
    }

    case '!': {
      // logical not always requires bool
      Value* boolV = castTo(getBoolTy(), operandV, "u_not");
      return Builder.CreateNot(boolV, "nottmp");
    }

    default: {
      // unsupported unary operator
      std::string msg = "Unknown unary operator '";
      msg += static_cast<char>(Op);
      msg += "'";
      printErrorAtCurrent(msg.c_str());
      noteError();
      return nullptr;
    }
  }

  return nullptr;
}



// ======== Assignment codegen (scalar) ========

/**
 * AssignAST::codegen:
 * Loads variable address, generates RHS value,
 * applies widening rule, and stores result.
 */
Value* AssignAST::codegen() {
  const std::string &name = LHS->getName();

  // look for variable in locals then globals
  AllocaInst     *local  = lookupLocal(name);
  GlobalVariable *global = lookupGlobal(name);

  if (!local && !global) {
    // variable never declared
    std::string msg = "Assignment to unknown variable '" + name + "'";
    printErrorAtNode(LHS.get(), msg.c_str());
    noteError();
    return nullptr;
  }

  Value *destPtr = nullptr;
  Type  *varTy   = nullptr;

  // resolve storage and declared type
  if (local) {
    destPtr = local;
    varTy   = local->getAllocatedType();
  } else {
    destPtr = global;
    varTy   = global->getValueType();
  }

  // generate RHS expression
  Value *rhsV = RHS->codegen();
  if (!rhsV) return nullptr;

  // enforce widening rule for assignment
  rhsV = widenOrError(rhsV, varTy, RHS.get(), "assigning");
  if (!rhsV) return nullptr;

  // final store
  Builder.CreateStore(rhsV, destPtr);
  return rhsV;
}

//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//
// AST Printer
//===----------------------------------------------------------------------===//
//===----------------------------------------------------------------------===//


// Stream an AST node using its to_string()
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ASTnode& ast) {
  os << ast.to_string();
  return os;
}

// void IntASTnode::display(int tabs) {
//   printf("%s\n",getType().c_str());
// }


//===----------------------------------------------------------------------===//
// AST printing: literals and variables
//===----------------------------------------------------------------------===//

// IntASTnode
std::string IntASTnode::toExprString() const {
  return std::to_string(Val);
}

void IntASTnode::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "IntegerLiteral " + std::to_string(Val));
}

// FloatASTnode
std::string FloatASTnode::toExprString() const {
  return std::to_string(Val);
}

void FloatASTnode::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "FloatingLiteral " + std::to_string(Val));
}

// BoolASTnode
std::string BoolASTnode::toExprString() const {
  return Bool ? "true" : "false";
}

void BoolASTnode::to_string_inner(std::string &buffer, int indent) const {
  const char *text = Bool ? "true" : "false";
  appendln(buffer, indent, std::string("BoolLiteral ") + text);
}

// VariableASTnode
std::string VariableASTnode::toExprString() const {
  return Name;
}

void VariableASTnode::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "DeclRefExpr " + Name);

}



//===----------------------------------------------------------------------===//
// AST printing: expressions
//===----------------------------------------------------------------------===//

// UnaryExprAST
std::string UnaryExprAST::toExprString() const {
  std::string operandText = Operand ? Operand->toExprString() : "?";
  return std::string(1, Op) + operandText;
}

void UnaryExprAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, std::string("UnaryOperator '") + Op + "'");
  if (Operand) {
    Operand->to_string_into(buffer, indent + 1);
  }
}

// BinaryExprAST
std::string BinaryExprAST::toExprString() const {
  std::string lhsText = LHS ? LHS->toExprString() : "?";
  std::string rhsText = RHS ? RHS->toExprString() : "?";
  return "(" + lhsText + " " + std::string(opTokName(OpTok)) + " " + rhsText + ")";
}

void BinaryExprAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, std::string("BinaryOperator '") + opTokName(OpTok) + "'");

  if (LHS) {
    appendln(buffer, indent + 1, "LHS:");
    LHS->to_string_into(buffer, indent + 2);
  }
  if (RHS) {
    appendln(buffer, indent + 1, "RHS:");
    RHS->to_string_into(buffer, indent + 2);
  }
}

// AssignAST
std::string AssignAST::toExprString() const {
  std::string lhsText = LHS ? LHS->toExprString() : "?";
  std::string rhsText = RHS ? RHS->toExprString() : "?";
  return lhsText + " = " + rhsText;
}

void AssignAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "BinaryOperator '='");

  appendln(buffer, indent + 1, "LHS:");
  if (LHS) {
    LHS->to_string_into(buffer, indent + 2);
  }

  appendln(buffer, indent + 1, "RHS:");
  if (RHS) {
    RHS->to_string_into(buffer, indent + 2);
  }
}

// CallExprAST
std::string CallExprAST::toExprString() const {
  std::string text = Callee + "(";
  for (size_t i = 0; i < Args.size(); ++i) {
    if (i > 0) {
      text += ", ";
    }
    text += Args[i] ? Args[i]->toExprString() : "?";
  }
  text += ")";
  return text;
}

void CallExprAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "CallExpr " + Callee);

  int index = 0;
  for (auto &arg : Args) {
    appendln(buffer, indent + 1, "arg[" + std::to_string(index++) + "]:");
    if (arg) {
      arg->to_string_into(buffer, indent + 2);
    }
  }
}



//===----------------------------------------------------------------------===//
// AST printing: functions and declarations
//===----------------------------------------------------------------------===//

// FunctionDeclAST
void FunctionDeclAST::to_string_inner(std::string &buffer, int indent) const {
  std::string header = "FunctionDecl " + Proto->getName() + " '" + Proto->getType() + "'";
  appendln(buffer, indent, header);

  const auto &params = Proto->getParams();
  appendln(buffer, indent + 1, "Params[" + std::to_string(params.size()) + "]:");

  int index = 0;
  for (const auto &paramPtr : params) {
    std::string line = "[" + std::to_string(index++) + "] "
                       + paramPtr->getType() + " " + paramPtr->getName();

    const auto &dims = paramPtr->getDims();
    if (!dims.empty()) {
      line += " [";
      for (size_t j = 0; j < dims.size(); ++j) {
        line += std::to_string(dims[j]);
        if (j + 1 < dims.size()) {
          line += "][";
        }
      }
      line += "]";
    }
    appendln(buffer, indent + 2, line);
  }

  appendln(buffer, indent + 1, "Body:");
  if (Block) {
    Block->to_string_into(buffer, indent + 2);
  }
}

// VarDeclAST
void VarDeclAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "VarDecl " + getName() + " '" + getType() + "'");
}

// GlobVarDeclAST
void GlobVarDeclAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent,"VarDecl " + getName() + " '" + getType() + "' (global)");

}



//===----------------------------------------------------------------------===//
// AST printing: statements
//===----------------------------------------------------------------------===//

// BlockAST
void BlockAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "Block");

  appendln(buffer, indent + 1,
           "Locals[" + std::to_string(LocalDecls.size()) + "]:");
  for (auto &decl : LocalDecls) {
    if (decl) {
      decl->to_string_into(buffer, indent + 2);
    }
  }

  appendln(buffer, indent + 1,
           "Stmts[" + std::to_string(Stmts.size()) + "]:");
  for (size_t i = 0; i < Stmts.size(); ++i) {
    auto &stmt = Stmts[i];
    if (!stmt) {
      continue;
    }

    appendln(buffer, indent + 2, "[" + std::to_string(i) + "]:");
    stmt->to_string_into(buffer, indent + 3);
  }
}

// IfExprAST
void IfExprAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "IfStmt");

  appendln(buffer, indent + 1, "Cond:");
  if (Cond) {
    Cond->to_string_into(buffer, indent + 2);
  }

  appendln(buffer, indent + 1, "Then:");
  if (Then) {
    Then->to_string_into(buffer, indent + 2);
  }

  if (Else) {
    appendln(buffer, indent + 1, "Else:");
    Else->to_string_into(buffer, indent + 2);
  }
}

// WhileExprAST
void WhileExprAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "WhileStmt");

  appendln(buffer, indent + 1, "Cond:");
  if (Cond) {
    Cond->to_string_into(buffer, indent + 2);
  }

  appendln(buffer, indent + 1, "Body:");
  if (Body) {
    Body->to_string_into(buffer, indent + 2);
  }
}

// ReturnAST
std::string ReturnAST::toExprString() const {
  if (!Val) {
    return "return";
  }
  return "return " + Val->toExprString();
}

void ReturnAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "ReturnStmt");
  if (Val) {
    appendln(buffer, indent + 1, "Val:");
    Val->to_string_into(buffer, indent + 2);
  }
}



//===----------------------------------------------------------------------===//
// AST printing: arrays
//===----------------------------------------------------------------------===//

// ArrayAccessAST
std::string ArrayAccessAST::toExprString() const {
  std::string text = name;
  for (const auto &indexExpr : indices) {
    text += "[";
    text += indexExpr ? indexExpr->toExprString() : "?";
    text += "]";
  }
  return text;
}

void ArrayAccessAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "ArraySubscriptExpr " + name);
  for (size_t i = 0; i < indices.size(); ++i) {
    appendln(buffer, indent + 1, "index[" + std::to_string(i) + "]:");
    if (indices[i]) {
      indices[i]->to_string_into(buffer, indent + 2);
    }
  }
}

// ArrayAssignAST
std::string ArrayAssignAST::toExprString() const {
  std::string lhsText = LHS ? LHS->toExprString() : "?";
  std::string rhsText = RHS ? RHS->toExprString() : "?";
  return lhsText + " = " + rhsText;
}

void ArrayAssignAST::to_string_inner(std::string &buffer, int indent) const {
  appendln(buffer, indent, "BinaryOperator '='");

  appendln(buffer, indent + 1, "LHS:");
  if (LHS) {
    LHS->to_string_into(buffer, indent + 2);
  }

  appendln(buffer, indent + 1, "RHS:");
  if (RHS) {
    RHS->to_string_into(buffer, indent + 2);
  }
}

// ArrayDeclAST
void ArrayDeclAST::to_string_inner(std::string &buffer, int indent) const {
  std::string line = "ArrayDecl " + getName() + " : " + getType() + " [";

  const auto &dims = getDims();
  for (size_t i = 0; i < dims.size(); ++i) {
    line += std::to_string(dims[i]);
    if (i + 1 < dims.size()) {
      line += "][";
    }
  }
  line += "]";

  appendln(buffer, indent, line);
}



//===----------------------------------------------------------------------===//
// Extra toExprString implementations for statement-like nodes
//===----------------------------------------------------------------------===//

std::string BlockAST::toExprString() const {
  return "{block}";
}

std::string FunctionDeclAST::toExprString() const {
  return "function " + Proto->getName();
}

std::string IfExprAST::toExprString() const {
  std::string condText = Cond ? Cond->toExprString() : "?";
  return "if (" + condText + ")";
}

std::string WhileExprAST::toExprString() const {
  std::string condText = Cond ? Cond->toExprString() : "?";
  return "while (" + condText + ")";
}

std::string ArrayDeclAST::toExprString() const {
  std::string text = getName();
  const auto &dims = getDims();
  for (size_t i = 0; i < dims.size(); ++i) {
    text += "[";
    text += std::to_string(dims[i]);
    text += "]";
  }
  return text;
}


// Program level AST printer
static void printProgramAST() {
  std::string out;

  // Root node
  appendln(out, 0, "TranslationUnitDecl");

  // Extern function prototypes as top-level children
  for (const auto &ex : gExterns) {
    std::string header = "FunctionDecl " + ex->getName() + " '"
                       + ex->getType() + " (";
    const auto &params = ex->getParams();
    for (size_t j = 0; j < params.size(); ++j) {
      const auto &p = params[j];
      header += p->getType();
      if (j + 1 < params.size()) {
        header += ", ";
      }
    }
    header += ")'";
    appendln(out, 1, header);
  }

  // Top-level declarations (globals, functions, arrays) as children
  for (auto &d : gTopDecls) {
    if (!d) continue;
    d->to_string_into(out, 1);
  }

  fwrite(out.data(), 1, out.size(), stderr);
}



//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

static void loadSourceFile(const char *filename) {
  SourceLines.clear();
  std::ifstream in(filename);
  if (!in) {
    // Fall back to old behaviour if reading fails
    return;
  }
  std::string line;
  while (std::getline(in, line)) {
    SourceLines.push_back(line);
  }
}


int main(int argc, char **argv) {
  if (argc == 2) {
    InputFileName = argv[1];       

    // New: read whole file into memory for error snippets
    loadSourceFile(argv[1]);

    pFile = fopen(argv[1], "r");
    if (pFile == NULL)
      perror("Error opening file");
  } else {
    std::cout << "Usage: ./mccomp InputFile\n";
    return 1;
  }


  // initialise line number and column numbers
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
  if (TraceParser)
    fprintf(stderr, "Parsing Finished\n");

  if (ErrorCount > 0) {
    fprintf(stderr, "%d error(s) generated.\n", ErrorCount);
    fclose(pFile);
    return 1;
  }

  // ---- Dump AST ----
  if (PrintAST) {
    printProgramAST();
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

  if (ErrorCount > 0) {
    fprintf(stderr, "%d error(s) generated.\n", ErrorCount);
    fclose(pFile);
    return 1;
  }

  // ===== IR output =====

  // 1) Print IR to stderr between your markers
  if (PrintIR) {
    errs() << "********************* FINAL IR (begin) ****************************\n";
    TheModule->print(errs(), nullptr);
    errs() << "********************* FINAL IR (end) ******************************\n";
  }

  // 2) Write IR to output.ll
  auto Filename = "output.ll";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

  if (EC) {
    errs() << "Could not open file: " << EC.message() << "\n";
    fclose(pFile);
    return 1;
  }

  TheModule->print(dest, nullptr);

  fclose(pFile);
  return 0;
}