// lexer.cpp — Oxide Lexer Implementation

#include "lexer.h"
#include <cctype>
#include <cstdio>
#include <stdexcept>
#include <unordered_map>

namespace oxide {

// ─────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)) {}

// ─────────────────────────────────────────────────────────────
// Low-level helpers
// ─────────────────────────────────────────────────────────────

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

char Lexer::peek() const {
  if (isAtEnd()) return '\0';
  return source_[pos_];
}

char Lexer::peekNext() const {
  if (pos_ + 1 >= source_.size()) return '\0';
  return source_[pos_ + 1];
}

char Lexer::advance() {
  char c = source_[pos_++];
  if (c == '\n') { ++line_; col_ = 1; }
  else           { ++col_; }
  return c;
}

bool Lexer::match(char expected) {
  if (isAtEnd() || source_[pos_] != expected) return false;
  advance();
  return true;
}

void Lexer::addError(const std::string& msg, int line, int col) {
  errors_.push_back(filename_ + ":" + std::to_string(line) + ":" +
                    std::to_string(col) + ": error: " + msg);
}

// ─────────────────────────────────────────────────────────────
// Whitespace and comment skipping
// ─────────────────────────────────────────────────────────────

void Lexer::skipWhitespaceAndComments() {
  while (!isAtEnd()) {
    char c = peek();
    if (std::isspace(static_cast<unsigned char>(c))) {
      advance();
    } else if (c == '/' && peekNext() == '/') {
      // Line comment: skip until newline
      while (!isAtEnd() && peek() != '\n') advance();
    } else if (c == '/' && peekNext() == '*') {
      // Block comment: skip until */
      advance(); advance(); // consume /*
      int startLine = line_, startCol = col_;
      bool closed = false;
      while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
          advance(); advance(); // consume */
          closed = true;
          break;
        }
        advance();
      }
      if (!closed) {
        addError("unterminated block comment", startLine, startCol);
      }
    } else {
      break;
    }
  }
}

// ─────────────────────────────────────────────────────────────
// Number literal scanner  (int and float)
// ─────────────────────────────────────────────────────────────

Token Lexer::scanNumber(int startLine, int startCol) {
  std::string lexeme;
  lexeme.reserve(16); // numbers rarely exceed 16 chars; avoids repeated heap growth
  bool isFloat = false;

  while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
    lexeme += advance();
  }

  // Optional fractional part
  if (!isAtEnd() && peek() == '.' &&
      std::isdigit(static_cast<unsigned char>(peekNext()))) {
    isFloat = true;
    lexeme += advance(); // consume '.'
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
      lexeme += advance();
    }
  }

  // Optional exponent (e/E)
  if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
    isFloat = true;
    lexeme += advance();
    if (!isAtEnd() && (peek() == '+' || peek() == '-')) lexeme += advance();
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
      lexeme += advance();
    }
  }

  TokenKind kind = isFloat ? TokenKind::FLOAT_LITERAL : TokenKind::INT_LITERAL;
  return Token(kind, lexeme, startLine, startCol);
}

// ─────────────────────────────────────────────────────────────
// String literal scanner
// ─────────────────────────────────────────────────────────────

Token Lexer::scanString(int startLine, int startCol) {
  advance(); // consume opening "
  std::string value;
  value.reserve(64); // pre-allocate; covers the majority of string literals

  while (!isAtEnd() && peek() != '"') {
    if (peek() == '\n') {
      addError("unterminated string literal", startLine, startCol);
      break;
    }
    if (peek() == '\\') {
      advance(); // consume backslash
      switch (peek()) {
      case 'n':  value += '\n'; advance(); break;
      case 't':  value += '\t'; advance(); break;
      case 'r':  value += '\r'; advance(); break;
      case '"':  value += '"';  advance(); break;
      case '\\': value += '\\'; advance(); break;
      case '0':  value += '\0'; advance(); break;
      default: {
        char errbuf[40];
        std::snprintf(errbuf, sizeof(errbuf), "unknown escape sequence '\\%c'",
                      static_cast<unsigned char>(peek()));
        addError(errbuf, line_, col_);
        value += advance();
        break;
      }
      }
    } else {
      value += advance();
    }
  }

  if (!isAtEnd()) advance(); // consume closing "
  else addError("unterminated string literal", startLine, startCol);

  return Token(TokenKind::STRING_LITERAL, value, startLine, startCol);
}

// ─────────────────────────────────────────────────────────────
// String interpolation scanner
// ─────────────────────────────────────────────────────────────
//
// Handles strings of the form  "Hello, ${name}! You have ${n} items."
// Returns a flat list of tokens:
//   INTERP_STR_HEAD("Hello, ")  IDENTIFIER("name")
//   INTERP_STR_MID("! You have ")  IDENTIFIER("n")
//   INTERP_STR_TAIL(" items.")
//
// For plain strings with no ${} the result is a single STRING_LITERAL token,
// so callers can always use scanStringTokens() instead of scanString().
//
// Brace nesting inside ${...} is tracked so expressions like "${fn()}" work.
// ─────────────────────────────────────────────────────────────

std::vector<Token> Lexer::scanStringTokens(int startLine, int startCol) {
  advance(); // consume opening "

  std::string segment;
  segment.reserve(64);
  bool hasInterp = false;

  // Helper: scan one character of the string content into `segment`
  // Returns false if we hit an error or end.
  auto scanChar = [&]() -> bool {
    if (isAtEnd() || peek() == '"') return false;
    if (peek() == '\n') {
      addError("unterminated string literal", startLine, startCol);
      return false;
    }
    if (peek() == '\\') {
      advance(); // consume backslash
      switch (peek()) {
      case 'n':  segment += '\n'; advance(); break;
      case 't':  segment += '\t'; advance(); break;
      case 'r':  segment += '\r'; advance(); break;
      case '"':  segment += '"';  advance(); break;
      case '\\': segment += '\\'; advance(); break;
      case '0':  segment += '\0'; advance(); break;
      default: {
        char errbuf[40];
        std::snprintf(errbuf, sizeof(errbuf), "unknown escape sequence '\\%c'",
                      static_cast<unsigned char>(peek()));
        addError(errbuf, line_, col_);
        segment += advance();
        break;
      }
      }  // end switch
      return true;
    }
    segment += advance();
    return true;
  };

  // First pass: check if this string has any interpolation
  // We scan character by character. When we see ${ we switch mode.
  std::vector<Token> result;

  while (!isAtEnd() && peek() != '"') {
    if (peek() == '$' && peekNext() == '{') {
      // We found an interpolation. Emit what we have so far.
      hasInterp = true;
      if (result.empty()) {
        // First segment: HEAD
        result.emplace_back(TokenKind::INTERP_STR_HEAD, segment, startLine, startCol);
      } else {
        // Middle segment: MID
        result.emplace_back(TokenKind::INTERP_STR_MID, segment, startLine, startCol);
      }
      segment.clear();

      advance(); // consume '$'
      advance(); // consume '{'

      // Extract the expression source text, tracking brace depth
      std::string exprSrc;
      int depth = 1;
      int exprLine = line_;
      int exprCol  = col_;
      while (!isAtEnd() && depth > 0) {
        char c = peek();
        if (c == '{') {
          ++depth;
          exprSrc += advance();
        } else if (c == '}') {
          --depth;
          if (depth > 0) exprSrc += advance();
          else advance(); // consume final '}'
        } else if (c == '\n') {
          addError("unterminated string interpolation expression", exprLine, exprCol);
          break;
        } else {
          exprSrc += advance();
        }
      }

      // Sub-lex the expression source and append its tokens (minus EOF)
      if (!exprSrc.empty()) {
        Lexer subLex(exprSrc, filename_);
        auto subToks = subLex.tokenize();
        // Propagate errors from sub-lexer
        for (auto& e : subLex.errors()) errors_.push_back(e);
        // Append all tokens except the trailing EOF
        for (auto& t : subToks) {
          if (t.kind != TokenKind::END_OF_FILE) {
            result.push_back(t);
          }
        }
      }
    } else {
      if (!scanChar()) break;
    }
  }

  if (!isAtEnd()) advance(); // consume closing "
  else addError("unterminated string literal", startLine, startCol);

  if (!hasInterp) {
    // Plain string — return a single STRING_LITERAL (fast path)
    return { Token(TokenKind::STRING_LITERAL, segment, startLine, startCol) };
  }

  // Emit the final tail segment
  result.emplace_back(TokenKind::INTERP_STR_TAIL, segment, startLine, startCol);
  return result;
}

// ─────────────────────────────────────────────────────────────
// Keyword lookup table
// ─────────────────────────────────────────────────────────────

TokenKind Lexer::keywordKind(const std::string& word) {
  static const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"let",    TokenKind::KW_LET},
    {"const",  TokenKind::KW_CONST},
    {"fn",       TokenKind::KW_FN},
    {"function", TokenKind::KW_FUNCTION},
    {"return", TokenKind::KW_RETURN},
    {"if",     TokenKind::KW_IF},
    {"else",   TokenKind::KW_ELSE},
    {"while",  TokenKind::KW_WHILE},
    {"for",    TokenKind::KW_FOR},
    {"in",     TokenKind::KW_IN},
    {"import", TokenKind::KW_IMPORT},
    {"from",   TokenKind::KW_FROM},
    {"export", TokenKind::KW_EXPORT},
    {"true",   TokenKind::KW_TRUE},
    {"false",  TokenKind::KW_FALSE},
    {"print",  TokenKind::KW_PRINT},
    {"int",    TokenKind::KW_INT},
    {"float",  TokenKind::KW_FLOAT},
    {"bool",   TokenKind::KW_BOOL},
    {"string", TokenKind::KW_STRING},
    {"void",     TokenKind::KW_VOID},
    {"struct",    TokenKind::KW_STRUCT},
    {"interface", TokenKind::KW_INTERFACE},
    {"break",    TokenKind::KW_BREAK},
    {"continue", TokenKind::KW_CONTINUE},
    {"as",       TokenKind::KW_AS},
    {"type",     TokenKind::KW_TYPE},
    {"enum",     TokenKind::KW_ENUM},
    {"match",    TokenKind::KW_MATCH},
  };
  auto it = kKeywords.find(word);
  return (it != kKeywords.end()) ? it->second : TokenKind::IDENTIFIER;
}

// ─────────────────────────────────────────────────────────────
// Identifier / keyword scanner
// ─────────────────────────────────────────────────────────────

Token Lexer::scanIdentOrKeyword(int startLine, int startCol) {
  std::string lexeme;
  lexeme.reserve(24); // identifiers/keywords are typically short; prevents small reallocs
  while (!isAtEnd() &&
         (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
    lexeme += advance();
  }
  TokenKind kind = keywordKind(lexeme);
  return Token(kind, lexeme, startLine, startCol);
}

// ─────────────────────────────────────────────────────────────
// Operator / delimiter scanner
// ─────────────────────────────────────────────────────────────

Token Lexer::scanOperatorOrDelim(int startLine, int startCol) {
  char c = advance();
  switch (c) {
  case '(': return Token(TokenKind::LPAREN,    "(", startLine, startCol);
  case ')': return Token(TokenKind::RPAREN,    ")", startLine, startCol);
  case '{': return Token(TokenKind::LBRACE,    "{", startLine, startCol);
  case '}': return Token(TokenKind::RBRACE,    "}", startLine, startCol);
  case '[': return Token(TokenKind::LBRACKET,  "[", startLine, startCol);
  case ']': return Token(TokenKind::RBRACKET,  "]", startLine, startCol);
  case ',': return Token(TokenKind::COMMA,     ",", startLine, startCol);
  case ';': return Token(TokenKind::SEMICOLON, ";", startLine, startCol);
  case '.':
    // Check for '..' range operator (used in for-in loops: 0..10)
    // Only emit DOT_DOT if next char is also '.'; guard against '...'
    if (!isAtEnd() && peek() == '.') {
      advance(); // consume second '.'
      return Token(TokenKind::DOT_DOT, "..", startLine, startCol);
    }
    return Token(TokenKind::DOT, ".", startLine, startCol);
  case '%': return Token(TokenKind::PERCENT,   "%", startLine, startCol);
  case '*': return Token(TokenKind::STAR,      "*", startLine, startCol);
  case '+': return Token(TokenKind::PLUS,      "+", startLine, startCol);
  case '?': return Token(TokenKind::QUESTION,  "?", startLine, startCol);

  case '-':
    if (match('>')) return Token(TokenKind::ARROW,  "->", startLine, startCol);
    return Token(TokenKind::MINUS, "-", startLine, startCol);

  case '/':
    return Token(TokenKind::SLASH, "/", startLine, startCol);

  case '=':
    if (match('=')) return Token(TokenKind::EQ_EQ, "==", startLine, startCol);
    return Token(TokenKind::EQ, "=", startLine, startCol);

  case '!':
    if (match('=')) return Token(TokenKind::BANG_EQ, "!=", startLine, startCol);
    return Token(TokenKind::BANG, "!", startLine, startCol);

  case '<':
    if (match('=')) return Token(TokenKind::LT_EQ, "<=", startLine, startCol);
    return Token(TokenKind::LT, "<", startLine, startCol);

  case '>':
    if (match('=')) return Token(TokenKind::GT_EQ, ">=", startLine, startCol);
    return Token(TokenKind::GT, ">", startLine, startCol);

  case '&':
    if (match('&')) return Token(TokenKind::AMP_AMP, "&&", startLine, startCol);
    addError("unexpected character '&' (did you mean '&&'?)", startLine, startCol);
    return Token(TokenKind::UNKNOWN, "&", startLine, startCol);

  case '|':
    if (match('|')) return Token(TokenKind::PIPE_PIPE, "||", startLine, startCol);
    addError("unexpected character '|' (did you mean '||'?)", startLine, startCol);
    return Token(TokenKind::UNKNOWN, "|", startLine, startCol);

  case ':':
    if (match(':')) return Token(TokenKind::COLON_COLON, "::", startLine, startCol);
    return Token(TokenKind::COLON, ":", startLine, startCol);

  default:
    addError(std::string("unexpected character '") + c + "'", startLine, startCol);
    return Token(TokenKind::UNKNOWN, std::string(1, c), startLine, startCol);
  }
}

// ─────────────────────────────────────────────────────────────
// Main tokenize loop
// ─────────────────────────────────────────────────────────────

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  // Heuristic: on average a token is ~4–5 source characters wide.
  // Pre-allocating avoids O(log n) doubling reallocations for large files.
  tokens.reserve(source_.size() / 4 + 16);

  while (true) {
    skipWhitespaceAndComments();
    if (isAtEnd()) break;

    int startLine = line_;
    int startCol  = col_;
    char c = peek();

    if (std::isdigit(static_cast<unsigned char>(c))) {
      tokens.push_back(scanNumber(startLine, startCol));
    } else if (c == '"') {
      // Use scanStringTokens — handles both plain strings and ${...} interpolation
      auto strToks = scanStringTokens(startLine, startCol);
      for (auto& t : strToks) tokens.push_back(std::move(t));
    } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      tokens.push_back(scanIdentOrKeyword(startLine, startCol));
    } else {
      tokens.push_back(scanOperatorOrDelim(startLine, startCol));
    }
  }

  tokens.emplace_back(TokenKind::END_OF_FILE, "", line_, col_);
  return tokens;
}

} // namespace oxide
