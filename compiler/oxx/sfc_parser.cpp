// sfc_parser.cpp — Oxide Single-File Component Parser Implementation

#include "sfc_parser.h"
#include <cctype>

namespace oxide {

// ─────────────────────────────────────────────────────────────────────────────
// Low-level helpers
// ─────────────────────────────────────────────────────────────────────────────

char SfcParser::cur() const {
  return pos_ < src_->size() ? (*src_)[pos_] : '\0';
}

char SfcParser::peek(size_t n) const {
  size_t p = pos_ + n;
  return p < src_->size() ? (*src_)[p] : '\0';
}

void SfcParser::advance(size_t n) {
  pos_ = (pos_ + n <= src_->size()) ? pos_ + n : src_->size();
}

bool SfcParser::atEnd() const {
  return pos_ >= src_->size();
}

void SfcParser::skipWhitespaceAndComments() {
  while (!atEnd()) {
    // Plain whitespace
    if (isspace(static_cast<unsigned char>(cur()))) { advance(); continue; }

    // Line comment  // ...
    if (cur() == '/' && peek() == '/') {
      while (!atEnd() && cur() != '\n') advance();
      continue;
    }

    // Block comment  /* ... */
    if (cur() == '/' && peek() == '*') {
      advance(2);
      while (!atEnd() && !(cur() == '*' && peek() == '/')) advance();
      if (!atEnd()) advance(2); // consume */
      continue;
    }

    break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Attribute parser
// ─────────────────────────────────────────────────────────────────────────────

std::map<std::string, std::string> SfcParser::parseAttrs() {
  std::map<std::string, std::string> attrs;

  while (!atEnd() && cur() != '>' && !(cur() == '/' && peek() == '>')) {
    // Skip whitespace
    while (!atEnd() && isspace(static_cast<unsigned char>(cur()))) advance();
    if (cur() == '>' || (cur() == '/' && peek() == '>')) break;

    // Read attribute name
    std::string name;
    while (!atEnd() && cur() != '=' && cur() != '>' &&
           !isspace(static_cast<unsigned char>(cur())))
      name += (*src_)[pos_++];

    if (name.empty()) { advance(); continue; }

    // Whitespace before '='
    while (!atEnd() && isspace(static_cast<unsigned char>(cur()))) advance();

    if (cur() == '=') {
      advance(); // consume '='
      while (!atEnd() && isspace(static_cast<unsigned char>(cur()))) advance();

      if (cur() == '"') {
        advance(); // opening "
        std::string val;
        while (!atEnd() && cur() != '"') val += (*src_)[pos_++];
        if (!atEnd()) advance(); // closing "
        attrs[name] = val;
      } else {
        // Bare value — read until whitespace or '>'
        std::string val;
        while (!atEnd() && !isspace(static_cast<unsigned char>(cur())) &&
               cur() != '>')
          val += (*src_)[pos_++];
        attrs[name] = val;
      }
    } else {
      attrs[name] = "true"; // boolean attribute
    }
  }

  return attrs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Content extractor
// ─────────────────────────────────────────────────────────────────────────────

std::string SfcParser::extractUntilClose(const std::string& tagName) {
  std::string closeTag = "</" + tagName + ">";
  std::string content;

  while (!atEnd()) {
    // Check for the exact closing tag
    if (pos_ + closeTag.size() <= src_->size() &&
        src_->substr(pos_, closeTag.size()) == closeTag) {
      advance(closeTag.size());
      return content;
    }
    content += (*src_)[pos_++];
  }

  return content; // no closing tag — return everything remaining
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

SfcFile SfcParser::parse(const std::string& src) {
  src_ = &src;
  pos_ = 0;
  SfcFile result;

  static const std::vector<std::string> SECTION_TAGS = {
    "server", "template", "client"
  };

  // ── Pass 1: detect whether this is an SFC file ──────────────────────────
  // Look for any "<server", "<template", or "<client" followed by
  // whitespace or '>' anywhere in the source.
  for (const auto& tag : SECTION_TAGS) {
    std::string openTag = "<" + tag;
    size_t p = src.find(openTag);
    while (p != std::string::npos) {
      size_t after = p + openTag.size();
      if (after >= src.size() ||
          isspace(static_cast<unsigned char>(src[after])) ||
          src[after] == '>') {
        result.isSfc = true;
        break;
      }
      p = src.find(openTag, p + 1);
    }
    if (result.isSfc) break;
  }

  if (!result.isSfc) return result;

  // ── Pass 2: extract sections in document order ───────────────────────────
  pos_ = 0;

  while (!atEnd()) {
    skipWhitespaceAndComments();
    if (atEnd()) break;

    if (cur() != '<') {
      // Content between sections (e.g., blank lines) — skip
      advance();
      continue;
    }

    // Try to match a section opening tag
    bool foundSection = false;
    for (const auto& tag : SECTION_TAGS) {
      std::string openTag = "<" + tag;
      if (pos_ + openTag.size() > src_->size()) continue;
      if (src_->substr(pos_, openTag.size()) != openTag) continue;

      // Verify character right after tag name is whitespace or '>'
      size_t after = pos_ + openTag.size();
      if (after < src_->size() &&
          !isspace(static_cast<unsigned char>((*src_)[after])) &&
          (*src_)[after] != '>') continue;

      // Consume "<tagname"
      advance(openTag.size());

      // Parse attributes, then consume '>'
      SfcSection sec;
      sec.tag   = tag;
      sec.attrs = parseAttrs();
      while (!atEnd() && cur() != '>') advance(); // skip to '>'
      if (!atEnd()) advance();                      // consume '>'

      // Extract section content
      sec.content = extractUntilClose(tag);
      result.sections.push_back(std::move(sec));

      foundSection = true;
      break;
    }

    if (!foundSection) advance(); // skip unrecognised '<'
  }

  return result;
}

} // namespace oxide
