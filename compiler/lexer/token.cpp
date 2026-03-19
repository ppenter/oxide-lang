// token.cpp — Token utility implementations

#include "token.h"

namespace oxide {

std::string tokenKindName(TokenKind kind) {
  switch (kind) {
  case TokenKind::INT_LITERAL:    return "INT_LITERAL";
  case TokenKind::FLOAT_LITERAL:  return "FLOAT_LITERAL";
  case TokenKind::BOOL_LITERAL:   return "BOOL_LITERAL";
  case TokenKind::STRING_LITERAL: return "STRING_LITERAL";
  case TokenKind::IDENTIFIER:     return "IDENTIFIER";
  case TokenKind::KW_LET:         return "let";
  case TokenKind::KW_CONST:       return "const";
  case TokenKind::KW_FN:          return "fn";
  case TokenKind::KW_RETURN:      return "return";
  case TokenKind::KW_IF:          return "if";
  case TokenKind::KW_ELSE:        return "else";
  case TokenKind::KW_WHILE:       return "while";
  case TokenKind::KW_FOR:         return "for";
  case TokenKind::KW_IN:          return "in";
  case TokenKind::KW_IMPORT:      return "import";
  case TokenKind::KW_FROM:        return "from";
  case TokenKind::KW_EXPORT:      return "export";
  case TokenKind::KW_TRUE:        return "true";
  case TokenKind::KW_FALSE:       return "false";
  case TokenKind::KW_PRINT:       return "print";
  case TokenKind::KW_INT:         return "int";
  case TokenKind::KW_FLOAT:       return "float";
  case TokenKind::KW_BOOL:        return "bool";
  case TokenKind::KW_STRING:      return "string";
  case TokenKind::KW_VOID:        return "void";
  case TokenKind::KW_STRUCT:      return "struct";
  case TokenKind::KW_BREAK:       return "break";
  case TokenKind::KW_CONTINUE:    return "continue";
  case TokenKind::KW_AS:          return "as";
  case TokenKind::KW_TYPE:        return "type";
  case TokenKind::KW_FUNCTION:    return "function";
  case TokenKind::KW_INTERFACE:   return "interface";
  case TokenKind::PLUS:           return "+";
  case TokenKind::MINUS:          return "-";
  case TokenKind::STAR:           return "*";
  case TokenKind::SLASH:          return "/";
  case TokenKind::PERCENT:        return "%";
  case TokenKind::EQ:             return "=";
  case TokenKind::EQ_EQ:          return "==";
  case TokenKind::BANG_EQ:        return "!=";
  case TokenKind::LT:             return "<";
  case TokenKind::LT_EQ:          return "<=";
  case TokenKind::GT:             return ">";
  case TokenKind::GT_EQ:          return ">=";
  case TokenKind::AMP_AMP:        return "&&";
  case TokenKind::PIPE_PIPE:      return "||"; 
  case TokenKind::BANG:           return "!";
  case TokenKind::ARROW:          return "->";
  case TokenKind::COLON:          return ":";
  case TokenKind::COLON_COLON:    return "::";
  case TokenKind::QUESTION:       return "?";
  case TokenKind::DOT_DOT:        return "..";
  case TokenKind::LPAREN:         return "(";
  case TokenKind::RPAREN:         return ")";
  case TokenKind::LBRACE:         return "{";
  case TokenKind::RBRACE:         return "}";
  case TokenKind::LBRACKET:       return "[";
  case TokenKind::RBRACKET:       return "]";
  case TokenKind::COMMA:          return ",";
  case TokenKind::SEMICOLON:      return ";";
  case TokenKind::DOT:            return ".";
  case TokenKind::INTERP_STR_HEAD: return "INTERP_STR_HEAD";
  case TokenKind::INTERP_STR_MID:  return "INTERP_STR_MID";
  case TokenKind::INTERP_STR_TAIL: return "INTERP_STR_TAIL";
  case TokenKind::END_OF_FILE:    return "<EOF>";
  case TokenKind::UNKNOWN:        return "<UNKNOWN>";
  }
  return "<INVALID>";
}

std::string Token::kindName() const {
  return tokenKindName(kind);
}

} // namespace oxide
