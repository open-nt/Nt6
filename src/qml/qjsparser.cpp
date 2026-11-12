// MIT License - see LICENSE
#include "qjsparser_p.h"

#include <QtCore/qhash.h>

#include <cmath>
#include <cstring>
#include <limits>

namespace QtJSPrivate {

namespace {

bool isIdentifierStart(QChar c) { return c.isLetter() || c == u'$' || c == u'_'; }

bool isIdentifierPart(QChar c) {
    return c.isLetterOrNumber() || c == u'$' || c == u'_' || c == QChar(0x200c) || c == QChar(0x200d) ||
           c.category() == QChar::Mark_NonSpacing || c.category() == QChar::Mark_SpacingCombining ||
           c.category() == QChar::Punctuation_Connector;
}

bool isLineTerminator(QChar c) { return c == u'\n' || c == u'\r' || c == QChar(0x2028) || c == QChar(0x2029); }

int hexValue(QChar c) {
    const char16_t u = c.unicode();
    if (u >= u'0' && u <= u'9')
        return u - u'0';
    if (u >= u'a' && u <= u'f')
        return u - u'a' + 10;
    if (u >= u'A' && u <= u'F')
        return u - u'A' + 10;
    return -1;
}

// Longest first.
const char *const kPunctuators[] = {">>>=", "...", "===", "!==", "**=", "<<=", ">>=", ">>>", "&&=", "||=", "?\?=", "=>",
                                    "==",   "!=",  "<=",  ">=",  "&&",  "||",  "??",  "?.",  "++",  "--",  "+=",   "-=",
                                    "*=",   "/=",  "%=",  "&=",  "|=",  "^=",  "<<",  ">>",  "**",  "{",   "}",    "(",
                                    ")",    "[",   "]",   ";",   ",",   "<",   ">",   "+",   "-",   "*",   "/",    "%",
                                    "&",    "|",   "^",   "!",   "~",   "?",   ":",   "=",   ".",   "@"};

const char *const kReserved[] = {
    "break",  "case",       "catch",  "class",   "const",   "continue", "debugger", "default", "delete",
    "do",     "else",       "export", "extends", "finally", "for",      "function", "if",      "import",
    "in",     "instanceof", "new",    "return",  "super",   "switch",   "this",     "throw",   "try",
    "typeof", "var",        "void",   "while",   "with",    "null",     "true",     "false",   "enum"};

} // namespace

// ----------------------------------------------------------------------------------------------------------------
// Lexer

Lexer::Lexer(const QString &source, int begin, int end, int firstLine)
    : m_src(source), m_pos(begin), m_end(end), m_line(firstLine), m_lineStart(begin) {}

QChar Lexer::peek(int ahead) const { return m_pos + ahead < m_end ? m_src.at(m_pos + ahead) : QChar(); }

bool Lexer::skipSpaceAndComments(bool *newline) {
    while (m_pos < m_end) {
        const QChar c = m_src.at(m_pos);
        if (isLineTerminator(c)) {
            if (c == u'\r' && peek(1) == u'\n')
                ++m_pos;
            ++m_pos;
            ++m_line;
            m_lineStart = m_pos;
            *newline = true;
        } else if (c.isSpace() || c == QChar(0xfeff)) {
            ++m_pos;
        } else if (c == u'/' && peek(1) == u'/') {
            while (m_pos < m_end && !isLineTerminator(m_src.at(m_pos)))
                ++m_pos;
        } else if (c == u'/' && peek(1) == u'*') {
            m_pos += 2;
            bool closed = false;
            while (m_pos < m_end) {
                const QChar d = m_src.at(m_pos);
                if (d == u'*' && peek(1) == u'/') {
                    m_pos += 2;
                    closed = true;
                    break;
                }
                if (isLineTerminator(d)) {
                    if (d == u'\r' && peek(1) == u'\n')
                        ++m_pos;
                    ++m_line;
                    m_lineStart = m_pos + 1;
                    *newline = true;
                }
                ++m_pos;
            }
            if (!closed) {
                m_error = QStringLiteral("unterminated comment");
                return false;
            }
        } else {
            break;
        }
    }
    return true;
}

void Lexer::fail(Token *t, const QString &message) {
    t->type = Token::Invalid;
    m_error = message;
}

bool Lexer::readEscape(QString *out, bool inTemplate) {
    // m_pos is after the backslash.
    if (m_pos >= m_end)
        return false;
    const QChar c = m_src.at(m_pos++);
    switch (c.unicode()) {
    case u'n':
        out->append(u'\n');
        return true;
    case u't':
        out->append(u'\t');
        return true;
    case u'r':
        out->append(u'\r');
        return true;
    case u'b':
        out->append(u'\b');
        return true;
    case u'f':
        out->append(u'\f');
        return true;
    case u'v':
        out->append(u'\v');
        return true;
    case u'0':
        if (inTemplate && peek().isDigit())
            return false;
        out->append(QChar(0));
        return true;
    case u'x': {
        const int h = hexValue(peek()), l = hexValue(peek(1));
        if (h < 0 || l < 0)
            return false;
        m_pos += 2;
        out->append(QChar(static_cast<char16_t>(h * 16 + l)));
        return true;
    }
    case u'u': {
        char32_t code = 0;
        if (peek() == u'{') {
            ++m_pos;
            int digits = 0;
            while (m_pos < m_end && m_src.at(m_pos) != u'}') {
                const int v = hexValue(m_src.at(m_pos++));
                if (v < 0 || ++digits > 6)
                    return false;
                code = code * 16 + static_cast<char32_t>(v);
            }
            if (m_pos >= m_end || digits == 0 || code > 0x10ffff)
                return false;
            ++m_pos;
        } else {
            for (int i = 0; i < 4; ++i) {
                const int v = hexValue(peek());
                if (v < 0)
                    return false;
                ++m_pos;
                code = code * 16 + static_cast<char32_t>(v);
            }
        }
        out->append(QString::fromUcs4(&code, 1));
        return true;
    }
    case u'\r':
        if (peek() == u'\n')
            ++m_pos;
        ++m_line;
        m_lineStart = m_pos;
        return true; // a line continuation
    case u'\n':
    case 0x2028:
    case 0x2029:
        ++m_line;
        m_lineStart = m_pos;
        return true;
    default:
        if (c.isDigit() && !inTemplate) {
            // Legacy octal escapes.
            int value = c.digitValue();
            if (value > 7)
                return false;
            for (int i = 0; i < 2 && peek().isDigit() && peek().digitValue() < 8 && value * 8 < 256; ++i)
                value = value * 8 + m_src.at(m_pos++).digitValue();
            out->append(QChar(static_cast<char16_t>(value)));
            return true;
        }
        out->append(c);
        return true;
    }
}

void Lexer::scanNumber(Token *t) {
    t->type = Token::Number;
    const int start = m_pos;
    const QChar first = m_src.at(m_pos);
    if (first == u'0' && (peek(1) == u'x' || peek(1) == u'X' || peek(1) == u'o' || peek(1) == u'O' || peek(1) == u'b' ||
                          peek(1) == u'B')) {
        const QChar kind = peek(1).toLower();
        const int base = kind == u'x' ? 16 : kind == u'o' ? 8 : 2;
        m_pos += 2;
        double value = 0.0;
        int digits = 0;
        while (m_pos < m_end) {
            const int d = hexValue(m_src.at(m_pos));
            if (d < 0 || d >= base)
                break;
            value = value * base + d;
            ++m_pos;
            ++digits;
        }
        if (digits == 0 || (m_pos < m_end && isIdentifierStart(m_src.at(m_pos))))
            return fail(t, QStringLiteral("invalid number"));
        t->number = value;
        return;
    }
    while (m_pos < m_end && m_src.at(m_pos).isDigit())
        ++m_pos;
    if (m_pos < m_end && m_src.at(m_pos) == u'.') {
        ++m_pos;
        while (m_pos < m_end && m_src.at(m_pos).isDigit())
            ++m_pos;
    }
    if (m_pos < m_end && (m_src.at(m_pos) == u'e' || m_src.at(m_pos) == u'E')) {
        int p = m_pos + 1;
        if (p < m_end && (m_src.at(p) == u'+' || m_src.at(p) == u'-'))
            ++p;
        if (p < m_end && m_src.at(p).isDigit()) {
            m_pos = p;
            while (m_pos < m_end && m_src.at(m_pos).isDigit())
                ++m_pos;
        }
    }
    if (m_pos < m_end && isIdentifierStart(m_src.at(m_pos)))
        return fail(t, QStringLiteral("invalid number"));
    bool ok = false;
    t->number = m_src.mid(start, m_pos - start).toDouble(&ok);
    if (!ok)
        t->number = std::numeric_limits<double>::quiet_NaN();
}

void Lexer::scanString(Token *t, QChar quote) {
    t->type = Token::String;
    ++m_pos;
    QString value;
    while (true) {
        if (m_pos >= m_end)
            return fail(t, QStringLiteral("unterminated string"));
        const QChar c = m_src.at(m_pos);
        if (c == quote) {
            ++m_pos;
            break;
        }
        if (c == u'\n' || c == u'\r')
            return fail(t, QStringLiteral("unterminated string"));
        if (c == u'\\') {
            ++m_pos;
            t->escaped = true;
            if (!readEscape(&value, false))
                return fail(t, QStringLiteral("invalid escape in string"));
            continue;
        }
        value.append(c);
        ++m_pos;
    }
    t->text = value;
}

void Lexer::scanTemplate(Token *t) {
    t->type = Token::Template;
    ++m_pos; // the backquote
    QString part;
    while (true) {
        if (m_pos >= m_end)
            return fail(t, QStringLiteral("unterminated template"));
        const QChar c = m_src.at(m_pos);
        if (c == u'`') {
            ++m_pos;
            t->parts.append(part);
            return;
        }
        if (c == u'\\') {
            ++m_pos;
            if (!readEscape(&part, true))
                return fail(t, QStringLiteral("invalid escape in template"));
            continue;
        }
        if (c == u'$' && peek(1) == u'{') {
            t->parts.append(part);
            part.clear();
            m_pos += 2;
            const int start = m_pos;
            // Find the matching brace, skipping strings, templates and comments.
            int depth = 1;
            while (m_pos < m_end && depth > 0) {
                const QChar d = m_src.at(m_pos);
                if (d == u'{') {
                    ++depth;
                } else if (d == u'}') {
                    --depth;
                    if (depth == 0)
                        break;
                } else if (d == u'"' || d == u'\'') {
                    Token skip;
                    scanString(&skip, d);
                    if (skip.type == Token::Invalid)
                        return fail(t, m_error);
                    continue;
                } else if (d == u'`') {
                    Token skip;
                    scanTemplate(&skip);
                    if (skip.type == Token::Invalid)
                        return fail(t, m_error);
                    continue;
                } else if (isLineTerminator(d)) {
                    ++m_line;
                    m_lineStart = m_pos + 1;
                }
                ++m_pos;
            }
            if (m_pos >= m_end)
                return fail(t, QStringLiteral("unterminated template"));
            t->expressions.emplace_back(start, m_pos);
            ++m_pos; // the closing brace
            continue;
        }
        if (isLineTerminator(c)) {
            ++m_line;
            m_lineStart = m_pos + 1;
        }
        part.append(c == u'\r' ? QChar(u'\n') : c);
        if (c == u'\r' && peek(1) == u'\n')
            ++m_pos;
        ++m_pos;
    }
}

Token Lexer::next() {
    Token t;
    bool newline = false;
    if (!skipSpaceAndComments(&newline)) {
        t.type = Token::Invalid;
        return t;
    }
    t.newlineBefore = newline;
    t.line = m_line;
    t.column = m_pos - m_lineStart + 1;
    t.start = m_pos;
    if (m_pos >= m_end) {
        t.type = Token::End;
        t.end = m_pos;
        return t;
    }
    const QChar c = m_src.at(m_pos);
    if (isIdentifierStart(c) || c == u'\\' || c.isHighSurrogate()) {
        t.type = Token::Identifier;
        QString name;
        while (m_pos < m_end) {
            const QChar d = m_src.at(m_pos);
            if (d == u'\\') {
                if (peek(1) != u'u') {
                    fail(&t, QStringLiteral("invalid escape in identifier"));
                    break;
                }
                m_pos += 1;
                QString decoded;
                if (!readEscape(&decoded, false)) {
                    fail(&t, QStringLiteral("invalid escape in identifier"));
                    break;
                }
                name += decoded;
                t.escaped = true;
                continue;
            }
            if (d.isHighSurrogate() && m_pos + 1 < m_end) {
                name += d;
                name += m_src.at(m_pos + 1);
                m_pos += 2;
                continue;
            }
            if (!(name.isEmpty() ? isIdentifierStart(d) : isIdentifierPart(d)))
                break;
            name += d;
            ++m_pos;
        }
        t.text = name;
    } else if (c.isDigit() || (c == u'.' && peek(1).isDigit())) {
        scanNumber(&t);
    } else if (c == u'"' || c == u'\'') {
        scanString(&t, c);
    } else if (c == u'`') {
        scanTemplate(&t);
    } else {
        t.type = Token::Invalid;
        for (const char *p : kPunctuators) {
            const int n = static_cast<int>(std::strlen(p));
            if (m_pos + n > m_end)
                continue;
            bool match = true;
            for (int i = 0; i < n && match; ++i)
                match = m_src.at(m_pos + i) == QLatin1Char(p[i]);
            // "?." followed by a digit is "?" and a number.
            if (match && n == 2 && p[0] == '?' && p[1] == '.' && peek(2).isDigit())
                match = false;
            if (match) {
                t.type = Token::Punct;
                t.text = QString::fromLatin1(p);
                m_pos += n;
                break;
            }
        }
        if (t.type == Token::Invalid)
            fail(&t, QStringLiteral("unexpected character '%1'").arg(c));
    }
    t.end = m_pos;
    return t;
}

Token Lexer::regExp(const Token &slash) {
    Token t = slash;
    t.type = Token::RegExp;
    m_pos = slash.start + 1;
    QString pattern;
    bool inClass = false;
    while (true) {
        if (m_pos >= m_end || isLineTerminator(m_src.at(m_pos))) {
            fail(&t, QStringLiteral("unterminated regular expression"));
            return t;
        }
        const QChar c = m_src.at(m_pos++);
        if (c == u'\\') {
            pattern += c;
            if (m_pos < m_end)
                pattern += m_src.at(m_pos++);
            continue;
        }
        if (c == u'[')
            inClass = true;
        else if (c == u']')
            inClass = false;
        else if (c == u'/' && !inClass)
            break;
        pattern += c;
    }
    QString flags;
    while (m_pos < m_end && isIdentifierPart(m_src.at(m_pos)))
        flags += m_src.at(m_pos++);
    t.text = pattern;
    t.parts = QStringList{flags};
    t.end = m_pos;
    return t;
}

// ----------------------------------------------------------------------------------------------------------------
// Parser

namespace {

struct NestingGuard {
    explicit NestingGuard(int *n) : count(n) { ++*count; }
    ~NestingGuard() { --*count; }
    NestingGuard(const NestingGuard &) = delete;
    NestingGuard &operator=(const NestingGuard &) = delete;
    int *count;
};

struct ParseFailure {
    ParseError error;
};

int precedence(const Token &t, bool noIn) {
    if (t.type == Token::Identifier) {
        if (t.text == QLatin1String("instanceof"))
            return 7;
        if (t.text == QLatin1String("in") && !noIn)
            return 7;
        return 0;
    }
    if (t.type != Token::Punct)
        return 0;
    const QString &p = t.text;
    if (p == QLatin1String("??") || p == QLatin1String("||"))
        return 1;
    if (p == QLatin1String("&&"))
        return 2;
    if (p == QLatin1String("|"))
        return 3;
    if (p == QLatin1String("^"))
        return 4;
    if (p == QLatin1String("&"))
        return 5;
    if (p == QLatin1String("==") || p == QLatin1String("!=") || p == QLatin1String("===") || p == QLatin1String("!=="))
        return 6;
    if (p == QLatin1String("<") || p == QLatin1String(">") || p == QLatin1String("<=") || p == QLatin1String(">="))
        return 7;
    if (p == QLatin1String("<<") || p == QLatin1String(">>") || p == QLatin1String(">>>"))
        return 8;
    if (p == QLatin1String("+") || p == QLatin1String("-"))
        return 9;
    if (p == QLatin1String("*") || p == QLatin1String("/") || p == QLatin1String("%"))
        return 10;
    if (p == QLatin1String("**"))
        return 11;
    return 0;
}

Op binaryOp(const QString &p) {
    static const QHash<QString, Op> ops = {
        {QStringLiteral("+"), Op::Add},        {QStringLiteral("-"), Op::Sub},
        {QStringLiteral("*"), Op::Mul},        {QStringLiteral("/"), Op::Div},
        {QStringLiteral("%"), Op::Mod},        {QStringLiteral("**"), Op::Exp},
        {QStringLiteral("<<"), Op::Shl},       {QStringLiteral(">>"), Op::Shr},
        {QStringLiteral(">>>"), Op::UShr},     {QStringLiteral("&"), Op::BitAnd},
        {QStringLiteral("|"), Op::BitOr},      {QStringLiteral("^"), Op::BitXor},
        {QStringLiteral("<"), Op::Lt},         {QStringLiteral(">"), Op::Gt},
        {QStringLiteral("<="), Op::Le},        {QStringLiteral(">="), Op::Ge},
        {QStringLiteral("=="), Op::Eq},        {QStringLiteral("!="), Op::Ne},
        {QStringLiteral("==="), Op::StrictEq}, {QStringLiteral("!=="), Op::StrictNe},
        {QStringLiteral("in"), Op::In},        {QStringLiteral("instanceof"), Op::InstanceOf},
        {QStringLiteral("&&"), Op::And},       {QStringLiteral("||"), Op::Or},
        {QStringLiteral("??"), Op::Nullish}};
    return ops.value(p, Op::None);
}

} // namespace

Parser::Parser(ProgramData *data, int begin, int end, int firstLine)
    : m_data(data), m_lexer(data->source, begin, end, firstLine) {
    advance();
}

void Parser::fail(const QString &message) { failAt(m_tok, message); }

void Parser::failAt(const Token &t, const QString &message) {
    throw ParseFailure{ParseError{message, t.line, t.column}};
}

void Parser::advance() {
    m_prev = m_tok;
    m_tok = m_lexer.next();
    if (m_tok.type == Token::Invalid)
        fail(m_lexer.error());
}

bool Parser::isPunct(const char *p) const { return m_tok.type == Token::Punct && m_tok.text == QLatin1String(p); }

bool Parser::isName(const char *name) const {
    return m_tok.type == Token::Identifier && !m_tok.escaped && m_tok.text == QLatin1String(name);
}

bool Parser::isKeyword() const {
    if (m_tok.type != Token::Identifier || m_tok.escaped)
        return false;
    for (const char *k : kReserved)
        if (m_tok.text == QLatin1String(k))
            return true;
    return false;
}

bool Parser::eat(const char *p) {
    if (!isPunct(p))
        return false;
    advance();
    return true;
}

void Parser::expect(const char *p) {
    if (!eat(p))
        fail(m_tok.type == Token::End ? QStringLiteral("expected '%1' at the end").arg(QLatin1String(p))
                                      : QStringLiteral("expected '%1'").arg(QLatin1String(p)));
}

QString Parser::expectIdentifier() {
    if (m_tok.type != Token::Identifier || isKeyword())
        fail(QStringLiteral("expected a name"));
    const QString name = m_tok.text;
    advance();
    return name;
}

void Parser::consumeSemicolon() {
    if (eat(";"))
        return;
    if (isPunct("}") || m_tok.type == Token::End || m_tok.newlineBefore)
        return;
    fail(QStringLiteral("expected ';'"));
}

Node *Parser::make(Node::Kind kind, int line) {
    m_data->nodes.push_back(std::make_unique<Node>(kind, line));
    return m_data->nodes.back().get();
}

FunctionNode *Parser::makeFunction() {
    m_data->functions.push_back(std::make_unique<FunctionNode>());
    return m_data->functions.back().get();
}

void Parser::enterFunction(FunctionNode *f) { m_functions.push_back(f); }

void Parser::leaveFunction() { m_functions.pop_back(); }

void Parser::declareVar(const QString &name) {
    FunctionNode *f = m_functions.back();
    if (!f->varNames.contains(name))
        f->varNames.append(name);
}

void Parser::declareLexical(const QString &name) {
    if (m_blocks.empty())
        return declareVar(name);
    QStringList *names = m_blocks.back();
    if (names->contains(name))
        fail(QStringLiteral("'%1' is declared twice").arg(name));
    names->append(name);
}

std::shared_ptr<ProgramData> Parser::parseScript(const QString &source, const QString &fileName, int firstLine,
                                                 ParseError *error) {
    auto data = std::make_shared<ProgramData>();
    data->source = source;
    data->fileName = fileName;
    try {
        Parser p(data.get(), 0, static_cast<int>(source.size()), firstLine);
        FunctionNode *f = p.makeFunction();
        f->line = firstLine;
        f->sourceEnd = static_cast<int>(source.size());
        data->program = f;
        p.enterFunction(f);
        Node *body = p.make(Node::Kind::Block, firstLine);
        p.m_blocks.push_back(&body->names);
        while (p.m_tok.type != Token::End)
            body->list.push_back(p.statement());
        p.m_blocks.pop_back();
        p.leaveFunction();
        f->body = body;
    } catch (const ParseFailure &failure) {
        if (error)
            *error = failure.error;
        return nullptr;
    }
    return data;
}

std::shared_ptr<ProgramData> Parser::parseExpression(const QString &source, const QString &fileName, int firstLine,
                                                     ParseError *error) {
    auto data = std::make_shared<ProgramData>();
    data->source = source;
    data->fileName = fileName;
    try {
        Parser p(data.get(), 0, static_cast<int>(source.size()), firstLine);
        FunctionNode *f = p.makeFunction();
        f->line = firstLine;
        f->sourceEnd = static_cast<int>(source.size());
        f->arrow = true;
        data->program = f;
        p.enterFunction(f);
        f->exprBody = p.expression();
        if (p.m_tok.type != Token::End)
            p.fail(QStringLiteral("unexpected token after the expression"));
        p.leaveFunction();
    } catch (const ParseFailure &failure) {
        if (error)
            *error = failure.error;
        return nullptr;
    }
    return data;
}

// ---- Statements ----

Node *Parser::statement() {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    const int line = m_tok.line;
    if (isPunct("{"))
        return block();
    if (isPunct(";")) {
        advance();
        return make(Node::Kind::Empty, line);
    }
    if (m_tok.type == Token::Identifier && !m_tok.escaped) {
        const QString &w = m_tok.text;
        if (w == QLatin1String("var") || w == QLatin1String("const"))
            return declaration(true);
        if (w == QLatin1String("let")) {
            // "let" is a declaration when a name, "[" or "{" follows.
            const Lexer::State s = m_lexer.state();
            const Token save = m_tok;
            advance();
            const bool decl = m_tok.type == Token::Identifier || isPunct("[") || isPunct("{");
            m_lexer.restore(s);
            m_tok = save;
            if (decl)
                return declaration(true);
        }
        if (w == QLatin1String("function"))
            return functionDeclaration();
        if (w == QLatin1String("if"))
            return ifStatement();
        if (w == QLatin1String("for"))
            return forStatement();
        if (w == QLatin1String("while")) {
            advance();
            Node *n = make(Node::Kind::While, line);
            expect("(");
            n->a = expression();
            expect(")");
            n->d = statement();
            return n;
        }
        if (w == QLatin1String("do")) {
            advance();
            Node *n = make(Node::Kind::DoWhile, line);
            n->d = statement();
            if (!isName("while"))
                fail(QStringLiteral("expected 'while'"));
            advance();
            expect("(");
            n->a = expression();
            expect(")");
            eat(";");
            return n;
        }
        if (w == QLatin1String("return")) {
            if (m_functions.size() < 2 && !m_functions.back()->arrow)
                fail(QStringLiteral("'return' outside of a function"));
            advance();
            Node *n = make(Node::Kind::Return, line);
            if (!isPunct(";") && !isPunct("}") && m_tok.type != Token::End && !m_tok.newlineBefore)
                n->a = expression();
            consumeSemicolon();
            return n;
        }
        if (w == QLatin1String("break") || w == QLatin1String("continue")) {
            const bool isBreak = w == QLatin1String("break");
            advance();
            Node *n = make(isBreak ? Node::Kind::Break : Node::Kind::Continue, line);
            if (m_tok.type == Token::Identifier && !m_tok.newlineBefore && !isKeyword()) {
                n->str = m_tok.text;
                advance();
            }
            consumeSemicolon();
            return n;
        }
        if (w == QLatin1String("throw")) {
            advance();
            if (m_tok.newlineBefore)
                fail(QStringLiteral("no line break is allowed after 'throw'"));
            Node *n = make(Node::Kind::Throw, line);
            n->a = expression();
            consumeSemicolon();
            return n;
        }
        if (w == QLatin1String("try"))
            return tryStatement();
        if (w == QLatin1String("switch"))
            return switchStatement();
        if (w == QLatin1String("debugger")) {
            advance();
            consumeSemicolon();
            return make(Node::Kind::Empty, line);
        }
        if (w == QLatin1String("class"))
            fail(QStringLiteral("classes are not supported"));
        if (w == QLatin1String("with"))
            fail(QStringLiteral("'with' is not supported"));
        if (w == QLatin1String("import") || w == QLatin1String("export"))
            fail(QStringLiteral("modules are not supported"));
        if (w == QLatin1String("async") || w == QLatin1String("await") || w == QLatin1String("yield")) {
            const Lexer::State s = m_lexer.state();
            const Token save = m_tok;
            advance();
            const bool keyword = isName("function") || (m_tok.type == Token::Identifier && !m_tok.newlineBefore);
            m_lexer.restore(s);
            m_tok = save;
            if (keyword && w != QLatin1String("yield"))
                fail(QStringLiteral("async functions are not supported"));
        }
        // A label.
        if (!isKeyword()) {
            const Lexer::State s = m_lexer.state();
            const Token save = m_tok;
            advance();
            if (isPunct(":")) {
                advance();
                Node *n = make(Node::Kind::Labeled, line);
                n->str = save.text;
                n->a = statement();
                return n;
            }
            m_lexer.restore(s);
            m_tok = save;
        }
    }
    Node *n = make(Node::Kind::ExprStmt, line);
    n->a = expression();
    consumeSemicolon();
    return n;
}

Node *Parser::declaration(bool requireSemicolon) {
    const int line = m_tok.line;
    Node *n = make(Node::Kind::VarDecl, line);
    n->op2 = m_tok.text == QLatin1String("var") ? 0 : m_tok.text == QLatin1String("let") ? 1 : 2;
    advance();
    do {
        if (isPunct("[") || isPunct("{"))
            fail(QStringLiteral("destructuring is not supported"));
        Node *d = make(Node::Kind::Declarator, m_tok.line);
        d->str = expectIdentifier();
        if (n->op2 == 0)
            declareVar(d->str);
        else
            declareLexical(d->str);
        if (eat("="))
            d->a = assignment(!requireSemicolon);
        else if (n->op2 == 2 && requireSemicolon)
            fail(QStringLiteral("a const declaration needs a value"));
        n->list.push_back(d);
    } while (eat(","));
    if (requireSemicolon)
        consumeSemicolon();
    return n;
}

Node *Parser::block() {
    Node *n = make(Node::Kind::Block, m_tok.line);
    expect("{");
    m_blocks.push_back(&n->names);
    while (!isPunct("}")) {
        if (m_tok.type == Token::End)
            fail(QStringLiteral("expected '}'"));
        n->list.push_back(statement());
    }
    m_blocks.pop_back();
    advance();
    return n;
}

Node *Parser::ifStatement() {
    Node *n = make(Node::Kind::If, m_tok.line);
    advance();
    expect("(");
    n->a = expression();
    expect(")");
    n->b = statement();
    if (isName("else")) {
        advance();
        n->c = statement();
    }
    return n;
}

Node *Parser::forStatement() {
    const int line = m_tok.line;
    advance();
    if (isName("await"))
        fail(QStringLiteral("'for await' is not supported"));
    expect("(");
    // The loop has a scope of its own for let and const.
    Node *outer = make(Node::Kind::Block, line);
    m_blocks.push_back(&outer->names);
    Node *init = nullptr;
    if (isPunct(";")) {
        // no initializer
    } else if (isName("var") || isName("const") || (isName("let") && [this] {
                   const Lexer::State s = m_lexer.state();
                   const Token save = m_tok;
                   advance();
                   const bool decl = m_tok.type == Token::Identifier;
                   m_lexer.restore(s);
                   m_tok = save;
                   return decl;
               }())) {
        init = declaration(false);
    } else {
        init = expression(true);
    }
    Node *loop = nullptr;
    if (init && (isName("of") || isName("in"))) {
        const bool of = isName("of");
        if (init->kind == Node::Kind::VarDecl && (init->list.size() != 1 || init->list[0]->a))
            fail(QStringLiteral("invalid declaration in a for-in or for-of loop"));
        if (init->kind != Node::Kind::VarDecl && !isAssignable(init))
            fail(QStringLiteral("invalid left side of a for-in or for-of loop"));
        advance();
        loop = make(Node::Kind::ForIn, line);
        loop->flag = of;
        loop->a = init;
        loop->b = of ? assignment() : expression();
        expect(")");
    } else {
        if (init && init->kind == Node::Kind::VarDecl && init->op2 == 2)
            for (const Node *d : init->list)
                if (!d->a)
                    fail(QStringLiteral("a const declaration needs a value"));
        loop = make(Node::Kind::For, line);
        loop->a = init;
        expect(";");
        if (!isPunct(";"))
            loop->b = expression();
        expect(";");
        if (!isPunct(")"))
            loop->c = expression();
        expect(")");
    }
    loop->d = statement();
    m_blocks.pop_back();
    outer->list.push_back(loop);
    return outer;
}

Node *Parser::tryStatement() {
    Node *n = make(Node::Kind::Try, m_tok.line);
    advance();
    n->a = block();
    if (isName("catch")) {
        advance();
        Node *catchBlock = make(Node::Kind::Block, m_tok.line);
        m_blocks.push_back(&catchBlock->names);
        if (eat("(")) {
            if (isPunct("[") || isPunct("{"))
                fail(QStringLiteral("destructuring is not supported"));
            n->str = expectIdentifier();
            catchBlock->names.append(n->str);
            expect(")");
        }
        catchBlock->list.push_back(block());
        m_blocks.pop_back();
        n->b = catchBlock;
    }
    if (isName("finally")) {
        advance();
        n->c = block();
    }
    if (!n->b && !n->c)
        fail(QStringLiteral("expected 'catch' or 'finally'"));
    return n;
}

Node *Parser::switchStatement() {
    Node *n = make(Node::Kind::Switch, m_tok.line);
    advance();
    expect("(");
    n->a = expression();
    expect(")");
    expect("{");
    m_blocks.push_back(&n->names);
    bool hasDefault = false;
    while (!eat("}")) {
        Node *c = make(Node::Kind::Case, m_tok.line);
        if (isName("case")) {
            advance();
            c->a = expression();
        } else if (isName("default")) {
            if (hasDefault)
                fail(QStringLiteral("more than one default clause"));
            hasDefault = true;
            advance();
        } else {
            fail(QStringLiteral("expected 'case' or 'default'"));
        }
        expect(":");
        while (!isName("case") && !isName("default") && !isPunct("}")) {
            if (m_tok.type == Token::End)
                fail(QStringLiteral("expected '}'"));
            c->list.push_back(statement());
        }
        n->list.push_back(c);
    }
    m_blocks.pop_back();
    return n;
}

void Parser::parameters(FunctionNode *f) {
    expect("(");
    while (!isPunct(")")) {
        FunctionNode::Param p;
        if (eat("..."))
            p.rest = true;
        if (isPunct("[") || isPunct("{"))
            fail(QStringLiteral("destructuring is not supported"));
        p.name = expectIdentifier();
        if (!p.rest && eat("="))
            p.defaultValue = assignment();
        f->params.push_back(p);
        if (p.rest)
            break;
        if (!eat(","))
            break;
    }
    expect(")");
}

void Parser::functionBody(FunctionNode *f) {
    enterFunction(f);
    std::vector<QStringList *> saved;
    saved.swap(m_blocks);
    Node *body = make(Node::Kind::Block, m_tok.line);
    expect("{");
    m_blocks.push_back(&body->names);
    while (!isPunct("}")) {
        if (m_tok.type == Token::End)
            fail(QStringLiteral("expected '}'"));
        body->list.push_back(statement());
    }
    f->sourceEnd = m_tok.end;
    advance();
    m_blocks.swap(saved);
    leaveFunction();
    f->body = body;
}

Node *Parser::functionDeclaration() {
    const int line = m_tok.line;
    const int start = m_tok.start;
    advance();
    if (isPunct("*"))
        fail(QStringLiteral("generators are not supported"));
    FunctionNode *f = makeFunction();
    f->line = line;
    f->sourceStart = start;
    f->name = expectIdentifier();
    parameters(f);
    functionBody(f);
    Node *n = make(Node::Kind::FuncDecl, line);
    n->func = f;
    // Declarations at the top of a function are hoisted with its variables; others belong to their block.
    if (m_blocks.size() <= 1) {
        declareVar(f->name);
        m_functions.back()->functionDecls.push_back(n);
    } else {
        QStringList *names = m_blocks.back();
        if (!names->contains(f->name))
            names->append(f->name);
    }
    return n;
}

// ---- Expressions ----

Node *Parser::expression(bool noIn) {
    Node *first = assignment(noIn);
    if (!isPunct(","))
        return first;
    Node *n = make(Node::Kind::Sequence, first->line);
    n->list.push_back(first);
    while (eat(","))
        n->list.push_back(assignment(noIn));
    return n;
}

bool Parser::isAssignable(const Node *n) const {
    return n->kind == Node::Kind::Identifier || (n->kind == Node::Kind::Member && !n->flag);
}

bool Parser::arrowAhead() {
    // At "(": looks for the matching ")" followed by "=>".
    const Lexer::State s = m_lexer.state();
    const Token save = m_tok;
    const Token prev = m_prev;
    int depth = 0;
    bool arrow = false;
    try {
        do {
            if (isPunct("(") || isPunct("[") || isPunct("{"))
                ++depth;
            else if (isPunct(")") || isPunct("]") || isPunct("}"))
                --depth;
            else if (m_tok.type == Token::End)
                break;
            advance();
        } while (depth > 0);
        arrow = depth == 0 && isPunct("=>") && !m_tok.newlineBefore;
    } catch (const ParseFailure &) {
        arrow = false;
    }
    m_lexer.restore(s);
    m_tok = save;
    m_prev = prev;
    return arrow;
}

Node *Parser::arrowFunction() {
    const int line = m_tok.line;
    FunctionNode *f = makeFunction();
    f->arrow = true;
    f->line = line;
    f->sourceStart = m_tok.start;
    if (m_tok.type == Token::Identifier) {
        FunctionNode::Param p;
        p.name = expectIdentifier();
        f->params.push_back(p);
    } else {
        parameters(f);
    }
    if (m_tok.newlineBefore)
        fail(QStringLiteral("no line break is allowed before '=>'"));
    expect("=>");
    if (isPunct("{")) {
        functionBody(f);
    } else {
        enterFunction(f);
        std::vector<QStringList *> saved;
        saved.swap(m_blocks);
        f->exprBody = assignment();
        m_blocks.swap(saved);
        leaveFunction();
        f->sourceEnd = m_prev.end;
    }
    // An arrow function sees the "this" and "arguments" of the function around it.
    if ((f->usesThis || f->usesArguments) && !m_functions.empty()) {
        for (auto it = m_functions.rbegin(); it != m_functions.rend(); ++it) {
            (*it)->usesThis = (*it)->usesThis || f->usesThis;
            (*it)->usesArguments = (*it)->usesArguments || f->usesArguments;
            if (!(*it)->arrow)
                break;
        }
    }
    Node *n = make(Node::Kind::Function, line);
    n->func = f;
    return n;
}

Node *Parser::assignment(bool noIn) {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    if (m_tok.type == Token::Identifier && !isKeyword()) {
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        const bool arrow = isPunct("=>") && !m_tok.newlineBefore;
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
        if (arrow)
            return arrowFunction();
    }
    if (isPunct("(") && arrowAhead())
        return arrowFunction();
    Node *left = conditional(noIn);
    if (m_tok.type != Token::Punct)
        return left;
    const QString p = m_tok.text;
    if (p == QLatin1String("=") ||
        (p.size() >= 2 && p.endsWith(u'=') && p != QLatin1String("==") && p != QLatin1String("===") &&
         p != QLatin1String("!=") && p != QLatin1String("!==") && p != QLatin1String("<=") &&
         p != QLatin1String(">=") && p != QLatin1String("=>"))) {
        if (!isAssignable(left)) {
            if (left->kind == Node::Kind::Array || left->kind == Node::Kind::ObjectLit)
                fail(QStringLiteral("destructuring is not supported"));
            fail(QStringLiteral("invalid assignment target"));
        }
        const int line = m_tok.line;
        advance();
        Node *n = make(Node::Kind::Assign, line);
        n->a = left;
        if (p != QLatin1String("=")) {
            const QString base = p.left(p.size() - 1);
            n->op = binaryOp(base);
        }
        n->b = assignment(noIn);
        return n;
    }
    return left;
}

Node *Parser::conditional(bool noIn) {
    Node *test = binary(1, noIn);
    if (!isPunct("?"))
        return test;
    Node *n = make(Node::Kind::Conditional, m_tok.line);
    advance();
    n->a = test;
    n->b = assignment(false);
    expect(":");
    n->c = assignment(noIn);
    return n;
}

Node *Parser::binary(int minPrecedence, bool noIn) {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    Node *left = unary();
    while (true) {
        const int prec = precedence(m_tok, noIn);
        if (prec == 0 || prec < minPrecedence)
            return left;
        const Token opTok = m_tok;
        const Op op = binaryOp(opTok.text);
        advance();
        // ** is right associative; the others are left associative.
        Node *right = binary(op == Op::Exp ? prec : prec + 1, noIn);
        if (op == Op::Exp && left->kind == Node::Kind::Unary)
            failAt(opTok, QStringLiteral("parenthesize a unary expression before '**'"));
        Node *n = make(op == Op::And || op == Op::Or || op == Op::Nullish ? Node::Kind::Logical : Node::Kind::Binary,
                       opTok.line);
        n->op = op;
        n->a = left;
        n->b = right;
        left = n;
    }
}

Node *Parser::unary() {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    const int line = m_tok.line;
    Op op = Op::None;
    if (m_tok.type == Token::Punct) {
        const QString &p = m_tok.text;
        if (p == QLatin1String("!"))
            op = Op::Not;
        else if (p == QLatin1String("-"))
            op = Op::Minus;
        else if (p == QLatin1String("+"))
            op = Op::Plus;
        else if (p == QLatin1String("~"))
            op = Op::BitNot;
        else if (p == QLatin1String("++") || p == QLatin1String("--")) {
            const Op u = p == QLatin1String("++") ? Op::Inc : Op::Dec;
            advance();
            Node *target = unary();
            if (!isAssignable(target))
                fail(QStringLiteral("invalid target of '%1'").arg(u == Op::Inc ? "++" : "--"));
            Node *n = make(Node::Kind::Update, line);
            n->op = u;
            n->flag = true;
            n->a = target;
            return n;
        }
    } else if (m_tok.type == Token::Identifier && !m_tok.escaped) {
        if (m_tok.text == QLatin1String("typeof"))
            op = Op::TypeOf;
        else if (m_tok.text == QLatin1String("void"))
            op = Op::Void;
        else if (m_tok.text == QLatin1String("delete"))
            op = Op::Delete;
    }
    if (op == Op::None)
        return postfix();
    advance();
    Node *n = make(Node::Kind::Unary, line);
    n->op = op;
    n->a = unary();
    return n;
}

Node *Parser::postfix() {
    Node *e = leftHandSide();
    if ((isPunct("++") || isPunct("--")) && !m_tok.newlineBefore) {
        if (!isAssignable(e))
            fail(QStringLiteral("invalid target of '%1'").arg(m_tok.text));
        Node *n = make(Node::Kind::Update, m_tok.line);
        n->op = isPunct("++") ? Op::Inc : Op::Dec;
        n->a = e;
        advance();
        return n;
    }
    return e;
}

std::vector<Node *> Parser::arguments() {
    std::vector<Node *> args;
    expect("(");
    while (!isPunct(")")) {
        if (isPunct("...")) {
            Node *s = make(Node::Kind::Spread, m_tok.line);
            advance();
            s->a = assignment();
            args.push_back(s);
        } else {
            args.push_back(assignment());
        }
        if (!eat(","))
            break;
    }
    expect(")");
    return args;
}

Node *Parser::leftHandSide() {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    Node *e = nullptr;
    if (isName("new")) {
        const int line = m_tok.line;
        advance();
        if (isPunct(".")) {
            advance();
            if (!isName("target"))
                fail(QStringLiteral("expected 'target'"));
            fail(QStringLiteral("'new.target' is not supported"));
        }
        Node *n = make(Node::Kind::New, line);
        // The callee: a member expression without calls.
        Node *callee = isName("new") ? leftHandSide() : primary();
        while (true) {
            if (isPunct(".")) {
                advance();
                if (m_tok.type != Token::Identifier)
                    fail(QStringLiteral("expected a name after '.'"));
                Node *m = make(Node::Kind::Member, m_tok.line);
                m->a = callee;
                m->str = m_tok.text;
                advance();
                callee = m;
            } else if (isPunct("[")) {
                Node *m = make(Node::Kind::Member, m_tok.line);
                advance();
                m->a = callee;
                m->b = expression();
                expect("]");
                callee = m;
            } else {
                break;
            }
        }
        n->a = callee;
        if (isPunct("("))
            n->list = arguments();
        e = n;
    } else {
        e = primary();
    }
    bool chain = false; // inside an optional chain
    while (true) {
        const int line = m_tok.line;
        if (isPunct(".")) {
            advance();
            if (m_tok.type != Token::Identifier)
                fail(QStringLiteral("expected a name after '.'"));
            Node *m = make(Node::Kind::Member, line);
            m->a = e;
            m->str = m_tok.text;
            m->op2 = chain ? 1 : 0;
            advance();
            e = m;
        } else if (isPunct("?.")) {
            advance();
            chain = true;
            if (isPunct("(")) {
                Node *c = make(Node::Kind::Call, line);
                c->a = e;
                c->list = arguments();
                c->flag = true;
                e = c;
            } else if (isPunct("[")) {
                advance();
                Node *m = make(Node::Kind::Member, line);
                m->a = e;
                m->b = expression();
                m->flag = true;
                expect("]");
                e = m;
            } else {
                if (m_tok.type != Token::Identifier)
                    fail(QStringLiteral("expected a name after '?.'"));
                Node *m = make(Node::Kind::Member, line);
                m->a = e;
                m->str = m_tok.text;
                m->flag = true;
                advance();
                e = m;
            }
        } else if (isPunct("[")) {
            advance();
            Node *m = make(Node::Kind::Member, line);
            m->a = e;
            m->b = expression();
            m->op2 = chain ? 1 : 0;
            expect("]");
            e = m;
        } else if (isPunct("(")) {
            Node *c = make(Node::Kind::Call, line);
            c->a = e;
            c->list = arguments();
            c->op2 = chain ? 1 : 0;
            e = c;
        } else if (m_tok.type == Token::Template) {
            fail(QStringLiteral("tagged templates are not supported"));
        } else {
            return e;
        }
    }
}

Node *Parser::templateLiteral(const Token &t) {
    Node *n = make(Node::Kind::Template, t.line);
    n->strs = t.parts;
    for (const auto &range : t.expressions) {
        // Each expression is parsed by a parser of its own, over its part of the source.
        int line = t.line;
        for (int i = t.start; i < range.first; ++i)
            if (m_data->source.at(i) == u'\n')
                ++line;
        Parser sub(m_data, range.first, range.second, line);
        sub.m_functions = m_functions;
        sub.m_blocks = m_blocks;
        sub.m_nesting = m_nesting;
        n->list.push_back(sub.expression());
        if (sub.m_tok.type != Token::End)
            sub.fail(QStringLiteral("unexpected token in template"));
    }
    return n;
}

Node *Parser::primary() {
    const Token t = m_tok;
    const int line = t.line;
    switch (t.type) {
    case Token::Number: {
        advance();
        Node *n = make(Node::Kind::Number, line);
        n->num = t.number;
        return n;
    }
    case Token::String: {
        advance();
        Node *n = make(Node::Kind::String, line);
        n->str = t.text;
        return n;
    }
    case Token::Template:
        advance();
        return templateLiteral(t);
    case Token::Punct:
        if (t.text == QLatin1String("(")) {
            advance();
            Node *e = expression();
            expect(")");
            return e;
        }
        if (t.text == QLatin1String("["))
            return arrayLiteral();
        if (t.text == QLatin1String("{"))
            return objectLiteral();
        if (t.text == QLatin1String("/") || t.text == QLatin1String("/=")) {
            const Token r = m_lexer.regExp(t);
            if (r.type == Token::Invalid)
                failAt(t, m_lexer.error());
            m_tok = r;
            advance();
            Node *n = make(Node::Kind::RegExp, line);
            n->str = r.text;
            n->str2 = r.parts.value(0);
            return n;
        }
        break;
    case Token::Identifier: {
        if (!t.escaped) {
            if (t.text == QLatin1String("function"))
                return functionExpression();
            if (t.text == QLatin1String("this")) {
                advance();
                for (auto it = m_functions.rbegin(); it != m_functions.rend(); ++it) {
                    (*it)->usesThis = true;
                    if (!(*it)->arrow)
                        break;
                }
                return make(Node::Kind::This, line);
            }
            if (t.text == QLatin1String("null")) {
                advance();
                return make(Node::Kind::Null, line);
            }
            if (t.text == QLatin1String("true")) {
                advance();
                return make(Node::Kind::True, line);
            }
            if (t.text == QLatin1String("false")) {
                advance();
                return make(Node::Kind::False, line);
            }
            if (t.text == QLatin1String("class"))
                fail(QStringLiteral("classes are not supported"));
            if (t.text == QLatin1String("super"))
                fail(QStringLiteral("'super' is not supported"));
        }
        if (isKeyword())
            fail(QStringLiteral("unexpected '%1'").arg(t.text));
        advance();
        if (t.text == QLatin1String("arguments")) {
            for (auto it = m_functions.rbegin(); it != m_functions.rend(); ++it) {
                (*it)->usesArguments = true;
                if (!(*it)->arrow)
                    break;
            }
        }
        Node *n = make(Node::Kind::Identifier, line);
        n->str = t.text;
        return n;
    }
    case Token::End:
        fail(QStringLiteral("unexpected end of input"));
    default:
        break;
    }
    fail(QStringLiteral("unexpected '%1'").arg(t.text));
}

Node *Parser::arrayLiteral() {
    Node *n = make(Node::Kind::Array, m_tok.line);
    expect("[");
    while (!isPunct("]")) {
        if (isPunct(",")) {
            // A hole reads as undefined.
            advance();
            n->list.push_back(nullptr);
            continue;
        }
        if (isPunct("...")) {
            Node *s = make(Node::Kind::Spread, m_tok.line);
            advance();
            s->a = assignment();
            n->list.push_back(s);
        } else {
            n->list.push_back(assignment());
        }
        if (!isPunct("]"))
            expect(",");
    }
    advance();
    return n;
}

Node *Parser::objectLiteral() {
    Node *n = make(Node::Kind::ObjectLit, m_tok.line);
    expect("{");
    while (!isPunct("}")) {
        Node *p = make(Node::Kind::Property, m_tok.line);
        if (isPunct("...")) {
            advance();
            p->op2 = 3;
            p->b = assignment();
            n->list.push_back(p);
            if (!isPunct("}"))
                expect(",");
            continue;
        }
        // get/set accessors: "get name() {}", unless "get" is the key itself.
        if ((isName("get") || isName("set"))) {
            const Lexer::State s = m_lexer.state();
            const Token save = m_tok;
            const Token prev = m_prev;
            advance();
            const bool accessor = !isPunct(":") && !isPunct("(") && !isPunct(",") && !isPunct("}");
            m_lexer.restore(s);
            m_tok = save;
            m_prev = prev;
            if (accessor) {
                p->op2 = isName("get") ? 1 : 2;
                advance();
            }
        }
        if (isPunct("*") || isName("async")) {
            const Lexer::State s = m_lexer.state();
            const Token save = m_tok;
            const Token prev = m_prev;
            advance();
            const bool special = !isPunct(":") && !isPunct("(") && !isPunct(",") && !isPunct("}");
            m_lexer.restore(s);
            m_tok = save;
            m_prev = prev;
            if (special)
                fail(QStringLiteral("generators and async methods are not supported"));
        }
        bool shorthandAllowed = false;
        if (isPunct("[")) {
            advance();
            p->a = assignment();
            expect("]");
        } else if (m_tok.type == Token::Identifier) {
            p->str = m_tok.text;
            shorthandAllowed = !isKeyword();
            advance();
        } else if (m_tok.type == Token::String) {
            p->str = m_tok.text;
            advance();
        } else if (m_tok.type == Token::Number) {
            p->str = QString::number(m_tok.number, 'g', 17);
            // Keys are strings of the canonical number form; integers are the common case.
            if (m_tok.number == std::floor(m_tok.number) && std::fabs(m_tok.number) < 1e15)
                p->str = QString::number(static_cast<qint64>(m_tok.number));
            advance();
        } else {
            fail(QStringLiteral("expected a property name"));
        }
        if (p->op2 == 1 || p->op2 == 2 || isPunct("(")) {
            // A method or an accessor.
            FunctionNode *f = makeFunction();
            f->line = m_tok.line;
            f->sourceStart = m_tok.start;
            f->name = p->str;
            parameters(f);
            functionBody(f);
            Node *fn = make(Node::Kind::Function, f->line);
            fn->func = f;
            p->b = fn;
        } else if (eat(":")) {
            p->b = assignment();
        } else if (shorthandAllowed && (isPunct(",") || isPunct("}"))) {
            Node *id = make(Node::Kind::Identifier, p->line);
            id->str = p->str;
            p->b = id;
        } else {
            fail(QStringLiteral("expected ':'"));
        }
        n->list.push_back(p);
        if (!isPunct("}"))
            expect(",");
    }
    advance();
    return n;
}

Node *Parser::functionExpression() {
    const int line = m_tok.line;
    const int start = m_tok.start;
    advance();
    if (isPunct("*"))
        fail(QStringLiteral("generators are not supported"));
    FunctionNode *f = makeFunction();
    f->line = line;
    f->sourceStart = start;
    if (m_tok.type == Token::Identifier && !isKeyword()) {
        f->name = m_tok.text;
        f->expression = true;
        advance();
    }
    parameters(f);
    functionBody(f);
    Node *n = make(Node::Kind::Function, line);
    n->func = f;
    return n;
}

// ---- QML ----

std::shared_ptr<ProgramData> Parser::parseQml(const QString &source, const QString &fileName, ParseError *error) {
    auto data = std::make_shared<ProgramData>();
    data->source = source;
    data->fileName = fileName;
    try {
        Parser p(data.get(), 0, static_cast<int>(source.size()), 1);
        // Functions and bindings of the document are parsed as parts of one script.
        FunctionNode *f = p.makeFunction();
        f->line = 1;
        data->program = f;
        p.enterFunction(f);
        p.qmlDocument();
        p.leaveFunction();
    } catch (const ParseFailure &failure) {
        if (error)
            *error = failure.error;
        return nullptr;
    }
    return data;
}

QString Parser::qmlQualifiedName() {
    if (m_tok.type != Token::Identifier)
        fail(QStringLiteral("expected a name"));
    QString name = m_tok.text;
    advance();
    while (isPunct(".")) {
        advance();
        if (m_tok.type != Token::Identifier)
            fail(QStringLiteral("expected a name after '.'"));
        name += u'.';
        name += m_tok.text;
        advance();
    }
    return name;
}

QString Parser::qmlType() {
    QString type = qmlQualifiedName();
    if (isPunct("<")) {
        advance();
        type += u'<';
        type += qmlQualifiedName();
        expect(">");
        type += u'>';
    }
    return type;
}

void Parser::qmlDocument() {
    QmlDocument &doc = m_data->qml;
    while (isName("import") || isName("pragma")) {
        const bool import = isName("import");
        const int line = m_tok.line;
        advance();
        if (!import) {
            QString pragma = qmlQualifiedName();
            if (eat(":"))
                pragma += u':' + qmlQualifiedName();
            doc.pragmas.append(pragma);
            eat(";");
            continue;
        }
        QmlImport imp;
        imp.line = line;
        if (m_tok.type == Token::String) {
            imp.path = m_tok.text;
            advance();
        } else {
            imp.uri = qmlQualifiedName();
        }
        if (m_tok.type == Token::Number && !m_tok.newlineBefore) {
            imp.version = m_data->source.mid(m_tok.start, m_tok.end - m_tok.start);
            advance();
        }
        if (isName("as")) {
            advance();
            imp.alias = expectIdentifier();
        }
        eat(";");
        doc.imports.push_back(imp);
    }
    if (m_tok.type != Token::Identifier)
        fail(QStringLiteral("expected an object declaration"));
    const int line = m_tok.line;
    const QString type = qmlQualifiedName();
    doc.root = qmlObject(type, line);
    if (m_tok.type != Token::End)
        fail(QStringLiteral("unexpected token after the root object"));
}

QmlObjectNode *Parser::qmlObject(const QString &typeName, int line) {
    NestingGuard guard(&m_nesting);
    if (m_nesting > kMaxNesting)
        fail(QStringLiteral("too deeply nested"));
    m_data->qmlObjects.push_back(std::make_unique<QmlObjectNode>());
    QmlObjectNode *object = m_data->qmlObjects.back().get();
    object->typeName = typeName;
    object->line = line;
    expect("{");
    while (!isPunct("}")) {
        if (m_tok.type == Token::End)
            fail(QStringLiteral("expected '}'"));
        if (eat(";"))
            continue;
        qmlMember(object);
    }
    advance();
    return object;
}

QmlBinding *Parser::qmlBindingValue(const QString &name, int line) {
    m_data->qmlBindings.push_back(std::make_unique<QmlBinding>());
    QmlBinding *b = m_data->qmlBindings.back().get();
    b->name = name;
    b->line = line;
    b->sourceStart = m_tok.start;
    if (isPunct("[")) {
        // A list of objects, or an array expression.
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        bool objects = false;
        if (m_tok.type == Token::Identifier && m_tok.text.at(0).isUpper()) {
            qmlQualifiedName();
            objects = isPunct("{");
        }
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
        if (objects) {
            advance();
            while (!isPunct("]")) {
                const int l = m_tok.line;
                const QString type = qmlQualifiedName();
                b->objects.push_back(qmlObject(type, l));
                if (!eat(","))
                    break;
            }
            expect("]");
            b->sourceEnd = m_prev.end;
            return b;
        }
    }
    if (m_tok.type == Token::Identifier) {
        // An object: "Type {".
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        bool object = false;
        if (m_tok.text.at(0).isUpper() || m_tok.text.contains(u'.')) {
            qmlQualifiedName();
            object = isPunct("{");
        }
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
        if (object) {
            const int l = m_tok.line;
            const QString type = qmlQualifiedName();
            b->object = qmlObject(type, l);
            b->sourceEnd = m_prev.end;
            return b;
        }
    }
    // The value is the body of a function of its own, so that handlers can return and declare variables.
    FunctionNode *f = makeFunction();
    f->line = line;
    f->name = name;
    f->sourceStart = m_tok.start;
    enterFunction(f);
    std::vector<QStringList *> saved;
    saved.swap(m_blocks);
    if (isPunct("{")) {
        b->block = block();
        f->body = b->block;
    } else {
        b->expression = expression();
        f->exprBody = b->expression;
        consumeSemicolon();
    }
    m_blocks.swap(saved);
    leaveFunction();
    f->sourceEnd = m_prev.end;
    b->function = f;
    b->sourceEnd = m_prev.end;
    return b;
}

void Parser::qmlMember(QmlObjectNode *object) {
    const int line = m_tok.line;
    if (m_tok.type != Token::Identifier)
        fail(QStringLiteral("expected a property, an object or a declaration"));
    // Declarations.
    bool isDefault = false, readonly = false, required = false;
    while (isName("default") || isName("readonly") || isName("required")) {
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        if (isPunct(":") || isPunct(".") || isPunct("{")) {
            m_lexer.restore(s);
            m_tok = save;
            m_prev = prev;
            break;
        }
        if (save.text == QLatin1String("default"))
            isDefault = true;
        else if (save.text == QLatin1String("readonly"))
            readonly = true;
        else
            required = true;
    }
    if (isName("property")) {
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        if (m_tok.type == Token::Identifier) {
            m_data->qmlProperties.push_back(std::make_unique<QmlPropertyDecl>());
            QmlPropertyDecl *decl = m_data->qmlProperties.back().get();
            decl->line = line;
            decl->isDefault = isDefault;
            decl->readonly = readonly;
            decl->required = required;
            decl->type = qmlType();
            decl->alias = decl->type == QLatin1String("alias");
            decl->name = expectIdentifier();
            if (eat(":"))
                decl->value = qmlBindingValue(decl->name, line);
            else
                eat(";");
            object->properties.push_back(decl);
            return;
        }
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
    }
    if (required && !isDefault && !readonly && m_tok.type == Token::Identifier && !isName("property")) {
        // "required name": makes an existing property required.
        m_data->qmlProperties.push_back(std::make_unique<QmlPropertyDecl>());
        QmlPropertyDecl *decl = m_data->qmlProperties.back().get();
        decl->line = line;
        decl->required = true;
        decl->name = expectIdentifier();
        eat(";");
        object->properties.push_back(decl);
        return;
    }
    if (isName("signal")) {
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        if (m_tok.type == Token::Identifier && !m_tok.newlineBefore) {
            m_data->qmlSignals.push_back(std::make_unique<QmlSignalDecl>());
            QmlSignalDecl *sig = m_data->qmlSignals.back().get();
            sig->line = line;
            sig->name = expectIdentifier();
            if (eat("(")) {
                while (!isPunct(")")) {
                    // "type name" or "name: type".
                    const QString first = qmlType();
                    if (eat(":")) {
                        sig->parameterNames.append(first);
                        sig->parameterTypes.append(qmlType());
                    } else {
                        sig->parameterTypes.append(first);
                        sig->parameterNames.append(expectIdentifier());
                    }
                    if (!eat(","))
                        break;
                }
                expect(")");
            }
            eat(";");
            object->signalDecls.push_back(sig);
            return;
        }
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
    }
    if (isName("function")) {
        Node *decl = functionDeclaration();
        object->functions.push_back(decl->func);
        return;
    }
    if (isName("enum")) {
        advance();
        m_data->qmlEnums.push_back(std::make_unique<QmlEnumDecl>());
        QmlEnumDecl *e = m_data->qmlEnums.back().get();
        e->name = expectIdentifier();
        expect("{");
        int value = 0;
        while (!isPunct("}")) {
            e->keys.append(expectIdentifier());
            if (eat("=")) {
                const bool negative = eat("-");
                if (m_tok.type != Token::Number)
                    fail(QStringLiteral("expected a number"));
                value = static_cast<int>(negative ? -m_tok.number : m_tok.number);
                advance();
            }
            e->values.append(value++);
            if (!eat(","))
                break;
        }
        expect("}");
        object->enums.push_back(e);
        return;
    }
    if (isName("component")) {
        const Lexer::State s = m_lexer.state();
        const Token save = m_tok;
        const Token prev = m_prev;
        advance();
        if (m_tok.type == Token::Identifier) {
            const QString name = expectIdentifier();
            expect(":");
            const int l = m_tok.line;
            const QString type = qmlQualifiedName();
            object->components.emplace_back(name, qmlObject(type, l));
            return;
        }
        m_lexer.restore(s);
        m_tok = save;
        m_prev = prev;
    }
    const QString name = qmlQualifiedName();
    if (isName("on")) {
        // "Behavior on width { }": an object that acts on a property.
        advance();
        const QString target = qmlQualifiedName();
        QmlObjectNode *o = qmlObject(name, line);
        m_data->qmlBindings.push_back(std::make_unique<QmlBinding>());
        QmlBinding *b = m_data->qmlBindings.back().get();
        b->name = target;
        b->object = o;
        b->onAssignment = true;
        b->line = line;
        object->bindings.push_back(b);
        return;
    }
    if (isPunct("{")) {
        const QString last = name.mid(name.lastIndexOf(u'.') + 1);
        if (last.at(0).isUpper()) {
            object->children.push_back(qmlObject(name, line));
            return;
        }
        // A group: "anchors { fill: parent }".
        advance();
        while (!isPunct("}")) {
            if (m_tok.type == Token::End)
                fail(QStringLiteral("expected '}'"));
            if (eat(";"))
                continue;
            const int l = m_tok.line;
            const QString sub = qmlQualifiedName();
            expect(":");
            object->bindings.push_back(qmlBindingValue(name + u'.' + sub, l));
        }
        advance();
        return;
    }
    expect(":");
    if (name == QLatin1String("id")) {
        if (m_tok.type != Token::Identifier || isKeyword())
            fail(QStringLiteral("expected an id"));
        if (!m_tok.text.at(0).isLower() && m_tok.text.at(0) != u'_')
            fail(QStringLiteral("ids must start with a lower case letter or an underscore"));
        object->id = m_tok.text;
        advance();
        eat(";");
        return;
    }
    object->bindings.push_back(qmlBindingValue(name, line));
}

} // namespace QtJSPrivate
