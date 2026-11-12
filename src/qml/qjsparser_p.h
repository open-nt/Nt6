// MIT License - see LICENSE
//
// The lexer and parser of Nt Qml: JavaScript (a large part of ECMAScript 2017, without classes, generators,
// async functions and destructuring) and QML documents. The tree is made of Node and FunctionNode, owned by
// a ProgramData that the functions made from it keep alive.
//
// Source is untrusted: nesting is limited (kMaxNesting), and errors are reported, never asserted.
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>
#include <vector>

namespace QtJSPrivate {

struct FunctionNode;
struct QmlObjectNode;

enum class Op : unsigned char {
    None,
    // Binary.
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Exp,
    Shl,
    Shr,
    UShr,
    BitAnd,
    BitOr,
    BitXor,
    Lt,
    Gt,
    Le,
    Ge,
    Eq,
    Ne,
    StrictEq,
    StrictNe,
    In,
    InstanceOf,
    // Logical.
    And,
    Or,
    Nullish,
    // Unary.
    Plus,
    Minus,
    Not,
    BitNot,
    TypeOf,
    Void,
    Delete,
    // Update.
    Inc,
    Dec
};

// One node type for the whole tree; what each field means depends on the kind.
struct Node {
    enum class Kind : unsigned char {
        // Expressions.
        Number,      // num
        String,      // str
        Template,    // strs: the text parts; list: the expressions between them
        RegExp,      // str: pattern, str2: flags
        Identifier,  // str
        This,        //
        Null,        //
        True,        //
        False,       //
        Array,       // list: elements (Spread for ...x)
        ObjectLit,   // list: Property nodes
        Property,    // str: key, or a: computed key; b: value; op2: 0 value, 1 getter, 2 setter, 3 spread
        Function,    // func
        Unary,       // op, a
        Update,      // op (Inc or Dec), a; flag: prefix
        Binary,      // op, a, b
        Logical,     // op, a, b
        Assign,      // op (None for =), a: target, b: value
        Conditional, // a ? b : c
        Call,        // a: callee, list: arguments; flag: optional call (?.())
        New,         // a: callee, list: arguments
        Member,      // a: object; str: name, or b: computed key; flag: optional (?.)
        Sequence,    // list
        Spread,      // a
        // Statements.
        VarDecl,    // op2: 0 var, 1 let, 2 const; list: Declarator nodes
        Declarator, // str: name, a: initializer
        ExprStmt,   // a
        Block,      // list: statements; names: lexical names declared in it
        If,         // a: test, b: then, c: else
        For,        // a: init (VarDecl or expression), b: test, c: update, d: body
        ForIn,      // a: left (VarDecl or target), b: object, d: body; flag: of (for...of)
        While,      // a: test, d: body
        DoWhile,    // a: test, d: body
        Return,     // a
        Break,      // str: label
        Continue,   // str: label
        Throw,      // a
        Try,        // a: block, str: catch parameter, b: catch block, c: finally block
        Switch,     // a: discriminant, list: Case nodes; names: lexical names
        Case,       // a: test (nullptr for default), list: statements
        FuncDecl,   // func
        Empty,      //
        Labeled     // str: label, a: statement
    };

    explicit Node(Kind k, int l) : kind(k), line(l) {}

    Kind kind;
    int line;
    Op op = Op::None;
    unsigned char op2 = 0;
    bool flag = false;
    double num = 0.0;
    QString str;
    QString str2;
    Node *a = nullptr;
    Node *b = nullptr;
    Node *c = nullptr;
    Node *d = nullptr;
    std::vector<Node *> list;
    QStringList strs;
    QStringList names;
    FunctionNode *func = nullptr;
};

struct FunctionNode {
    struct Param {
        QString name;
        Node *defaultValue = nullptr;
        bool rest = false;
    };

    QString name;
    std::vector<Param> params;
    Node *body = nullptr;     // a Block
    Node *exprBody = nullptr; // the expression of an arrow function without braces
    bool arrow = false;
    bool expression = false; // a named function expression, which sees its own name
    bool usesArguments = false;
    bool usesThis = false;
    // Hoisted to the top of the function: `var` names and function declarations.
    QStringList varNames;
    std::vector<Node *> functionDecls;
    int line = 0;
    int sourceStart = 0;
    int sourceEnd = 0;
};

// A QML document, as written: imports, and a tree of object declarations.
struct QmlImport {
    QString uri;     // a module, "QtQuick"
    QString path;    // or a directory or file, "../components"
    QString version; // "2.15", or empty
    QString alias;   // "as Name"
    int line = 0;
};

struct QmlBinding {
    QString name;                     // "width", "anchors.fill", "onClicked"
    Node *expression = nullptr;       // a JavaScript expression, or
    Node *block = nullptr;            // a block of statements (signal handlers, or bindings written as blocks)
    FunctionNode *function = nullptr; // the expression or block as the body of a function
    QmlObjectNode *object = nullptr;
    std::vector<QmlObjectNode *> objects; // a list of objects, [ A {}, B {} ]
    bool onAssignment = false;            // "Behavior on x {}"
    int line = 0;
    int sourceStart = 0;
    int sourceEnd = 0;
};

struct QmlPropertyDecl {
    QString name;
    QString type; // "int", "var", "list<Item>", "Item"
    bool readonly = false;
    bool isDefault = false;
    bool required = false;
    bool alias = false;
    QmlBinding *value = nullptr; // the initial value, if any
    int line = 0;
};

struct QmlSignalDecl {
    QString name;
    QStringList parameterTypes;
    QStringList parameterNames;
    int line = 0;
};

struct QmlEnumDecl {
    QString name;
    QStringList keys;
    QList<int> values;
};

struct QmlObjectNode {
    QString typeName; // "Rectangle", "QtQuick.Rectangle", "Controls.Button"
    QString id;
    std::vector<QmlBinding *> bindings;
    std::vector<QmlObjectNode *> children; // objects for the default property
    std::vector<QmlPropertyDecl *> properties;
    std::vector<QmlSignalDecl *> signalDecls;
    std::vector<FunctionNode *> functions;
    std::vector<QmlEnumDecl *> enums;
    // Inline components: "component Name: Type {}".
    std::vector<std::pair<QString, QmlObjectNode *>> components;
    int line = 0;
};

struct QmlDocument {
    std::vector<QmlImport> imports;
    QStringList pragmas;
    QmlObjectNode *root = nullptr;
};

// What the parser makes; functions keep it alive while they exist.
struct ProgramData {
    QString source;
    QString fileName;
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    std::vector<std::unique_ptr<QmlObjectNode>> qmlObjects;
    std::vector<std::unique_ptr<QmlBinding>> qmlBindings;
    std::vector<std::unique_ptr<QmlPropertyDecl>> qmlProperties;
    std::vector<std::unique_ptr<QmlSignalDecl>> qmlSignals;
    std::vector<std::unique_ptr<QmlEnumDecl>> qmlEnums;
    FunctionNode *program = nullptr; // a script: the top level, as a function
    QmlDocument qml;                 // a QML document
};

struct ParseError {
    QString message;
    int line = 0;
    int column = 0;
};

struct Token {
    enum Type : unsigned char { End, Identifier, Number, String, Template, Punct, RegExp, Invalid };
    Type type = End;
    QString text; // the name, the punctuator, or the value of a string
    double number = 0.0;
    bool newlineBefore = false;
    bool escaped = false; // an identifier or string written with escapes
    int line = 1;
    int column = 1;
    int start = 0;
    int end = 0;
    // Templates: the cooked text parts, and where the expressions between them are in the source.
    QStringList parts;
    std::vector<std::pair<int, int>> expressions;
};

class Lexer {
public:
    Lexer(const QString &source, int begin, int end, int firstLine);

    Token next();
    // Rescans from `slash`, a "/" or "/=" token, as a regular expression literal.
    Token regExp(const Token &slash);
    struct State {
        int pos;
        int line;
        int lineStart;
    };
    State state() const { return {m_pos, m_line, m_lineStart}; }
    void restore(const State &s) {
        m_pos = s.pos;
        m_line = s.line;
        m_lineStart = s.lineStart;
    }
    QString error() const { return m_error; }

private:
    QChar peek(int ahead = 0) const;
    bool skipSpaceAndComments(bool *newline);
    bool readEscape(QString *out, bool inTemplate);
    void scanNumber(Token *t);
    void scanString(Token *t, QChar quote);
    void scanTemplate(Token *t);
    void fail(Token *t, const QString &message);

    const QString &m_src;
    int m_pos;
    int m_end;
    int m_line;
    int m_lineStart;
    QString m_error;
};

class Parser {
public:
    // Parses `source`; on error returns nullptr and fills `error`.
    static std::shared_ptr<ProgramData> parseScript(const QString &source, const QString &fileName, int firstLine,
                                                    ParseError *error);
    static std::shared_ptr<ProgramData> parseQml(const QString &source, const QString &fileName, ParseError *error);
    // Parses a single expression, as QML bindings and Qt.binding-like helpers need.
    static std::shared_ptr<ProgramData> parseExpression(const QString &source, const QString &fileName, int firstLine,
                                                        ParseError *error);

    static constexpr int kMaxNesting = 400;

private:
    Parser(ProgramData *data, int begin, int end, int firstLine);

    // Tokens.
    void advance();
    bool isPunct(const char *p) const;
    bool isName(const char *name) const;
    bool isKeyword() const;
    bool eat(const char *p);
    void expect(const char *p);
    QString expectIdentifier();
    void consumeSemicolon();
    [[noreturn]] void fail(const QString &message);
    [[noreturn]] void failAt(const Token &t, const QString &message);

    Node *make(Node::Kind kind, int line);
    FunctionNode *makeFunction();

    // Statements.
    Node *statement();
    Node *declaration(bool requireSemicolon);
    Node *block();
    Node *ifStatement();
    Node *forStatement();
    Node *tryStatement();
    Node *switchStatement();
    Node *functionDeclaration();
    void functionBody(FunctionNode *f);
    void parameters(FunctionNode *f);

    // Expressions.
    Node *expression(bool noIn = false);
    Node *assignment(bool noIn = false);
    Node *arrowFunction();
    bool arrowAhead();
    Node *conditional(bool noIn);
    Node *binary(int minPrecedence, bool noIn);
    Node *unary();
    Node *postfix();
    Node *leftHandSide();
    Node *primary();
    Node *arrayLiteral();
    Node *objectLiteral();
    Node *functionExpression();
    Node *templateLiteral(const Token &t);
    std::vector<Node *> arguments();
    bool isAssignable(const Node *n) const;

    // QML.
    void qmlDocument();
    QmlObjectNode *qmlObject(const QString &typeName, int line);
    void qmlMember(QmlObjectNode *object);
    QmlBinding *qmlBindingValue(const QString &name, int line);
    QString qmlQualifiedName();
    QString qmlType();

    void enterFunction(FunctionNode *f);
    void leaveFunction();
    void declareVar(const QString &name);
    void declareLexical(const QString &name);

    ProgramData *m_data;
    Lexer m_lexer;
    Token m_tok;
    Token m_prev;
    int m_nesting = 0;
    std::vector<FunctionNode *> m_functions; // the functions being parsed, innermost last
    std::vector<QStringList *> m_blocks;     // lexical names of the blocks being parsed
};

} // namespace QtJSPrivate
