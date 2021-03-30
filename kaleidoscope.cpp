#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The lexer return tokens [0-255] if it is an unknown character,
// otherwise one of these known things.
// Unknown tokens are processed as-is.
enum token {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
};

// Global variables are used for simplicity

static std::string identifier_str; // Filled in if tok_identifier
static double numeric_value;             // Filled in if tok_number


// gettok - Return the next token from standard input.
// Read a sequence of alphanumerical characters and 
// return corresponding token type
static int get_token() {
    static int last_char = ' ';

    // Skip any whitespace
    while (isspace(last_char))
        last_char = getchar();

    // Handle a sequence of alphabetic characters as a known identifier or a string
    if (isalpha(last_char)) { 
        identifier_str = last_char;
        while (isalnum((last_char = getchar())))
            identifier_str += last_char;

        if (identifier_str == "def") 
            return tok_def;
        if (identifier_str == "extern")
            return tok_extern;
        return tok_identifier;
    }

    // Handle a sequence of characters as a floating-point numeric value
    if (isdigit(last_char) || last_char == '.') {
        std::string numeric_str;
        do {
            numeric_str += last_char;
            last_char = getchar();
        } while (isdigit(last_char) || last_char == '.');

        numeric_value = strtod(numeric_str.c_str(), 0);
        return tok_number;
    }

    // Handle a sequence of characters as a comment till the end of line
    if (last_char == '#') {
        do 
            last_char = getchar();
        while (last_char != EOF && last_char != '\n' && last_char != '\r');
        if (last_char != EOF)
            return get_token();
    }

    // Handle EOF character
    if (last_char == EOF)
        return tok_eof;

    // Handle other kinds of characters as plain ASCII characters
    int this_char = last_char;
    last_char = getchar();
    return this_char;

}

// AST Parser goes here
//
// expr_ast - Base class for all expression nodes.
class expr_ast {
    public:
        // TODO: Read about virtual destructors
        virtual ~expr_ast() {}
};


// number_expr_ast- Expression class for numeric literals like 0, 1.2345
class number_expr_ast: public expr_ast {
    private:
        double value;

    public:
        number_expr_ast(double value): value(value) {}
};

// variable_expr_ast - Expression class for referencing a variable
class variable_expr_ast: public expr_ast {
    private:
        std::string name;

    public:
        variable_expr_ast(const std::string &name): name (name) {}
};

// binary_expr_ast - expression class for a binary operator
class binary_expr_ast: public expr_ast {
    private:
        char op;
        std::unique_ptr<expr_ast> lhs, rhs;

    public:
        binary_expr_ast(char op, std::unique_ptr<expr_ast> lhs,
                std::unique_ptr<expr_ast> rhs) :
            op (op), lhs(std::move(lhs)), rhs(std::move (rhs)) {}
};


// call_expr_ast = expression class for function calls
class call_expr_ast: public expr_ast {
    private:
        std::string callee;
        std::vector<std::unique_ptr<expr_ast>> args;

    public:
        call_expr_ast (const std::string &callee,
                std::vector<std::unique_ptr<expr_ast>> args) :
            callee(callee), args(std::move(args)) {};
};

// prototype_ast - base class for function prototype, 
// which is basically a function name and it's argument names
class prototype_ast {
    private:
        std::string name;
        std::vector<std::string> args;

    public:
        prototype_ast(const std::string name,
                std::vector<std::string> args) :
            name(name), args(std::move(args)) {};

        const std::string& get_name() const {
            return name;
        }
};

// function_ast - class for a function definition itself
class function_ast {
    private:
        std::unique_ptr<prototype_ast> proto;
        std::unique_ptr<expr_ast> body;

    public:
        function_ast(std::unique_ptr<prototype_ast> proto,
                std::unique_ptr<expr_ast> body): 
            proto(std::move(proto)), body(std::move(body)) {};
};

// Simple token buffer. curr_token is a token parser looking at.
// get_next_token reads another token from lexer and updates curr_token.
// It allows to look one token ahead at what the lexer returns.
static int current_token;

static int get_next_token() {
    return current_token = get_token();
}

// Error handling functions
std::unique_ptr<expr_ast> log_error(const std::string& str) {
    std::cerr << "log_error: " << str << "\n";
    return nullptr;
}

std::unique_ptr<prototype_ast> log_error_proto(const std::string& str) {
    log_error(str);
    return nullptr;
}

// numberexpr ::= number
static std::unique_ptr<expr_ast> parse_number_expr() {
    auto result = std::make_unique<number_expr_ast>(numeric_value);
    get_next_token();
    return std::move(result);
}

// parenexpr ::= '(' expression ')'
static std::unique_ptr<expr_ast> parse_paren_expr() {
    get_next_token(); // consume '('
    auto v = parse_expression();
    if (!v)
        return nullptr;

    if (current_token != ')')
        return log_error("expected ')', got " + std::to_string(current_token) + " instead.");
    get_next_token(); // consume ')'
    return v;
}

// identifiere_xpr
//   ::= identifier
//   ::= identifier '(' expression* ')'
static std::unique_ptr<expr_ast> parse_identifier_expr() {
    std::string id_name = identifier_str;

    get_next_token(); // consume identifier

    if (current_token != '(') // simple variable ref
        return std::make_unique<variable_expr_ast>(id_name);

    // Call
    get_next_token(); // consume '('
    std::vector<std::unique_ptr<expr_ast>> args;
    if (current_token != ')') {
        while (true) {
            if (auto arg = parse_expression())
                args.push_back(std::move(arg));
            else
                return nullptr;

            if (current_token == ')')
                break;

            if (current_token != ',')
                return log_error("Expected ')' of ',' in argument list, got " + 
                        std::to_string(current_token) + " instead.");
            get_next_token();
        }
    }

    get_next_token(); // consume ')'

    return std::make_unique<call_expr_ast>(id_name, std::move(args));
}

// primary_expr
//   ::= identifier_expr
//   ::= number_expr
//   ::= paren_expr
static std::unique_ptr<expr_ast> parse_primary() {
    switch (current_token) {
        default:
            return log_error("Expected expression, got " + 
                    std::to_string(current_token) + " instead");
        case tok_identifier:
            return parse_identifier_expr();
        case tok_number:
            return parse_number_expr();
        case '(':
            return parse_paren_expr();
    }
}

// biop_precedence - precedence for each binary operator defined
static std::map<char, int> binop_precedence = 
{
    { '<', 10 },
    { '>', 10 },
    { '+', 20 },
    { '-', 20 },
    { '*', 40 },
    { '/', 40 }
};

// get_token_precedence - get the precedence of the pending binary operator
static int get_token_precedence() {
    if (!isascii(current_token)) 
        return -1;

    int token_precedence = binop_precedence[current_token];
    if (token_precedence <= 0)
        return -1;

    return token_precedence;
}


// binop_rhs
//   ::= ('+' primary) *
static std::unique_ptr<expr_ast> parse_binop_rhs(int expr_precedence,
        std::unique_ptr<expr_ast> left_hand_side) {
    // if this is a binop, find its precedence.
    while(true) {
        int token_precedence = get_token_precedence();

        // if this is a binop that binds at lease as tightly as the current binop,
        // consume it, otherwise we are done.
        if (token_precedence < expr_precedence) {
            return left_hand_side;
        }

        // ok, we know it's binop
        int binary_op = current_token;
        get_next_token();  // consume binop

        // parse the primary expression after the binary operator.
        auto right_hand_side = parse_primary();
        if (!right_hand_side) 
            return nullptr;

        // if binop binds less tightly with rhs than the operator after rhs,
        // let the pending operator take rhs and its lhs.
        int next_precedence = get_token_precedence();
        if (token_precedence < next_precedence) {
            right_hand_side = parse_binop_rhs(token_precedence + 1, 
                    std::move(right_hand_side));
            if (!right_hand_side) {
                return nullptr;
            }
        }

        // merge lhs/rhs
        left_hand_side = std::make_unique<binary_expr_ast>(binary_op, 
                std::move(left_hand_side), 
                std::move(right_hand_side));
        
        
    } // loop around to the top of the while loop
}

// expression
//   ::= primary binoprhs
static std::unique_ptr<expr_ast> parse_expression() {
    auto lhs = parse_primary();
    if (!lhs)
        return nullptr;

    return parse_binop_rhs(0, std::move(lhs));
}

// prototype
//   ::= id '(' id* ')'
static std::unique_ptr<prototype_ast> parse_prototype() {
    if (current_token != tok_identifier)
        return log_error_proto("Expected function name in prototype, got " +
                std::to_string(current_token) + " instead");

    std::string fn_name = identifier_str;
    get_next_token();
    if(current_token != '(') 
        return log_error_proto("Expected '(' in prototype, got " + 
                std::to_string(current_token) + " instead.");

    // Read the list of argument names
    std::vector<std::string> arg_names;
    while (get_next_token() == tok_identifier) 
        arg_names.push_back(identifier_str);
    if (current_token != ')')
        return log_error_proto("Expected ')' in prototype, got " +
                std::to_string(current_token) + " instead.");

    // success
    get_next_token(); // consume ')'
    
    return std::make_unique<prototype_ast>(fn_name, std::move(arg_names));

}

// definition 
//   ::= 'def' prototype expression
static std::unique_ptr<function_ast> parse_definition() {
    get_next_token(); // consume 'def'
    auto proto = parse_prototype();
    if (!proto)
        return nullptr;

    if (auto expr = parse_expression())
        return std::make_unique<function_ast>(std::move(proto), std::move(expr));
    return nullptr;
}

// external
//   ::= 'extern' prototype
static std::unique_ptr<prototype_ast> parse_extern() {
    get_next_token(); // consume 'extern'
    return parse_prototype();
}


// toplevel expression
//  ::= expression
static std::unique_ptr<function_ast> parse_top_level_expr() {
    if (auto expr = parse_expression()) {
        // make an anonymous proto
        auto proto = std::make_unique<prototype_ast>("", std::vector<std::string>());
        return std::make_unique<function_ast>(std::move(proto), std::move(expr));
    }
    return nullptr;
}

int main(int argc, char **argv) {
    while (true) {
        fprintf(stderr, "ready> ");
        switch(current_token) {
            case tok_eof:
                return 0;
            case ';': // ignore top_level semicolons
                get_next_token();
                break;
            case tok_def:
                handle_definition();
                break;
            case tok_extern:
                handle_extern();
                break;
            default:
                handle_top_level_expr();
                break;
        }
    }
    return 0;
}
