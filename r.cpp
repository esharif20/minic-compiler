// args ::= arg_list | ε
static std::vector<std::unique_ptr<ASTnode>> ParseArgs() {
    std::vector<std::unique_ptr<ASTnode>> args;
    
    // FIRST(arg_list) = FIRST(expr)
    if (CurTok.type == IDENT || CurTok.type == MINUS || CurTok.type == NOT ||
        CurTok.type == LPAR || CurTok.type == INT_LIT || 
        CurTok.type == FLOAT_LIT || CurTok.type == BOOL_LIT) {
        args = ParseArgList();
    }
    // else: ε production (empty args)
    
    return args;
}

// arg_list ::= expr arg_list'
static std::vector<std::unique_ptr<ASTnode>> ParseArgList() {
    std::vector<std::unique_ptr<ASTnode>> args;
    
    auto expr = ParseExper();
    if (expr) {
        args.push_back(std::move(expr));
        auto more_args = ParseArgListPrime();
        for (auto &arg : more_args) {
            args.push_back(std::move(arg));
        }
    }
    
    return args;
}

// arg_list' ::= "," expr arg_list' | ε
static std::vector<std::unique_ptr<ASTnode>> ParseArgListPrime() {
    std::vector<std::unique_ptr<ASTnode>> args;
    
    if (CurTok.type == COMMA) {
        getNextToken();  // eat ,
        auto expr = ParseExper();
        if (expr) {
            args.push_back(std::move(expr));
            auto more_args = ParseArgListPrime();
            for (auto &arg : more_args) {
                args.push_back(std::move(arg));
            }
        }
    }
    // else: ε production
    
    return args;
}

// primary ::= "(" expr ")" 
//          | IDENT "(" args ")" 
//          | IDENT 
//          | INT_LIT | FLOAT_LIT | BOOL_LIT
static std::unique_ptr<ASTnode> ParsePrimary() {
    // Parenthesized expression
    if (CurTok.type == LPAR) {
        getNextToken();  // eat (
        auto expr = ParseExper();
        if (!expr)
            return nullptr;
        if (CurTok.type != RPAR)
            return LogError(CurTok, "expected ')' after expression");
        getNextToken();  // eat )
        return expr;
    }
    
    // Identifier (variable or function call)
    else if (CurTok.type == IDENT) {
        std::string IdName = CurTok.getIdentifierStr();
        TOKEN IdTok = CurTok;
        getNextToken();  // eat IDENT
        
        // Function call: IDENT "(" args ")"
        if (CurTok.type == LPAR) {
            getNextToken();  // eat (
            auto args = ParseArgs();
            if (CurTok.type != RPAR)
                return LogError(CurTok, "expected ')' after function arguments");
            getNextToken();  // eat )
            
            return std::make_unique<CallExprAST>(IdName, std::move(args));
        } 
        // Just a variable
        else {
            return std::make_unique<VariableASTnode>(IdTok, IdName);
        }
    }
    
    // Integer literal
    else if (CurTok.type == INT_LIT) {
        return ParseIntNumberExpr();
    }
    
    // Float literal
    else if (CurTok.type == FLOAT_LIT) {
        return ParseFloatNumberExpr();
    }
    
    // Boolean literal
    else if (CurTok.type == BOOL_LIT) {
        return ParseBoolExpr();
    }
    
    // Error: unexpected token
    else {
        return LogError(CurTok, "expected expression");
    }
}

// unary ::= "-" unary | "!" unary | primary
static std::unique_ptr<ASTnode> ParseUnary() {
    // Unary minus: "-" unary
    if (CurTok.type == MINUS) {
        getNextToken();  // eat -
        auto operand = ParseUnary();  // Recursive for multiple unary ops
        if (!operand)
            return nullptr;
        return std::make_unique<UnaryExprAST>('-', std::move(operand));
    }
    
    // Logical NOT: "!" unary
    else if (CurTok.type == NOT) {
        getNextToken();  // eat !
        auto operand = ParseUnary();  // Recursive
        if (!operand)
            return nullptr;
        return std::make_unique<UnaryExprAST>('!', std::move(operand));
    }
    
    // Base case: primary
    else {
        return ParsePrimary();
    }
}

// mul_expr ::= unary mul_expr'
static std::unique_ptr<ASTnode> ParseMulExpr() {
    auto left = ParseUnary();
    if (!left)
        return nullptr;
    return ParseMulExprPrime(std::move(left));
}

// mul_expr' ::= "*" unary mul_expr' 
//            | "/" unary mul_expr' 
//            | "%" unary mul_expr' 
//            | ε
static std::unique_ptr<ASTnode> ParseMulExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == ASTERIX || CurTok.type == DIV || CurTok.type == MOD) {
        char op = CurTok.type;
        getNextToken();  // eat operator
        
        auto right = ParseUnary();
        if (!right)
            return nullptr;
        
        // Create binary operation node
        auto binOp = std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
        
        // Left-associative: continue parsing more operators at this level
        return ParseMulExprPrime(std::move(binOp));
    }
    
    // ε production: no more operators at this level
    return left;
}

// add_expr ::= mul_expr add_expr'
static std::unique_ptr<ASTnode> ParseAddExpr() {
    auto left = ParseMulExpr();
    if (!left)
        return nullptr;
    return ParseAddExprPrime(std::move(left));
}

// add_expr' ::= "+" mul_expr add_expr' 
//            | "-" mul_expr add_expr' 
//            | ε
static std::unique_ptr<ASTnode> ParseAddExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == PLUS || CurTok.type == MINUS) {
        char op = CurTok.type;
        getNextToken();  // eat operator
        
        auto right = ParseMulExpr();
        if (!right)
            return nullptr;
        
        auto binOp = std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
        return ParseAddExprPrime(std::move(binOp));
    }
    
    return left;
}

// rel_expr ::= add_expr rel_expr'
static std::unique_ptr<ASTnode> ParseRelExpr() {
    auto left = ParseAddExpr();
    if (!left)
        return nullptr;
    return ParseRelExprPrime(std::move(left));
}

// rel_expr' ::= "<" add_expr rel_expr' 
//            | "<=" add_expr rel_expr' 
//            | ">" add_expr rel_expr' 
//            | ">=" add_expr rel_expr' 
//            | ε
static std::unique_ptr<ASTnode> ParseRelExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == LT || CurTok.type == LE || 
        CurTok.type == GT || CurTok.type == GE) {
        char op = CurTok.type;
        getNextToken();  // eat operator
        
        auto right = ParseAddExpr();
        if (!right)
            return nullptr;
        
        auto binOp = std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
        return ParseRelExprPrime(std::move(binOp));
    }
    
    return left;
}

// eq_expr ::= rel_expr eq_expr'
static std::unique_ptr<ASTnode> ParseEqExpr() {
    auto left = ParseRelExpr();
    if (!left)
        return nullptr;
    return ParseEqExprPrime(std::move(left));
}

// eq_expr' ::= "==" rel_expr eq_expr' 
//           | "!=" rel_expr eq_expr' 
//           | ε
static std::unique_ptr<ASTnode> ParseEqExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == EQ || CurTok.type == NE) {
        char op = CurTok.type;
        getNextToken();  // eat operator
        
        auto right = ParseRelExpr();
        if (!right)
            return nullptr;
        
        auto binOp = std::make_unique<BinaryExprAST>(op, std::move(left), std::move(right));
        return ParseEqExprPrime(std::move(binOp));
    }
    
    return left;
}

// and_expr ::= eq_expr and_expr'
static std::unique_ptr<ASTnode> ParseAndExpr() {
    auto left = ParseEqExpr();
    if (!left)
        return nullptr;
    return ParseAndExprPrime(std::move(left));
}

// and_expr' ::= "&&" eq_expr and_expr' | ε
static std::unique_ptr<ASTnode> ParseAndExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == AND) {
        getNextToken();  // eat &&
        
        auto right = ParseEqExpr();
        if (!right)
            return nullptr;
        
        auto binOp = std::make_unique<BinaryExprAST>(AND, std::move(left), std::move(right));
        return ParseAndExprPrime(std::move(binOp));
    }
    
    return left;
}

// or_expr ::= and_expr or_expr'
static std::unique_ptr<ASTnode> ParseOrExpr() {
    auto left = ParseAndExpr();
    if (!left)
        return nullptr;
    return ParseOrExprPrime(std::move(left));
}

// or_expr' ::= "||" and_expr or_expr' | ε
static std::unique_ptr<ASTnode> ParseOrExprPrime(std::unique_ptr<ASTnode> left) {
    if (CurTok.type == OR) {
        getNextToken();  // eat ||
        
        auto right = ParseAndExpr();
        if (!right)
            return nullptr;
        
        auto binOp = std::make_unique<BinaryExprAST>(OR, std::move(left), std::move(right));
        return ParseOrExprPrime(std::move(binOp));
    }
    
    return left;
}