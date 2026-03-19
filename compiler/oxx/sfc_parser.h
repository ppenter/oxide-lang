// sfc_parser.h — Oxide Single-File Component Parser
//
// Splits a .oxx SFC file into its three named sections.
//
// SFC file format:
//
//   <server [props="param: Type, ..."]>
//       // Oxide server-side code.
//       // import statements here are hoisted to module scope.
//       // function declarations here are hoisted to module scope.
//       // Everything else runs inside render() at request time.
//   </server>
//
//   <template [layout="fn_name"] [title="Page Title"]>
//       // JSX markup — compiled to an HTML string.
//       // Interpolates server variables via {expr}.
//       // May call helper functions defined in <server>.
//   </template>
//
//   <client>
//       // Oxide code compiled to JavaScript (type annotations stripped).
//       // Runs in the browser. Functions defined here are global.
//   </client>
//
// Files without any of these top-level tags are treated as legacy .oxx files
// (backward compatible, returned with isSfc = false).

#pragma once
#include <map>
#include <string>
#include <vector>

namespace oxide {

struct SfcSection {
  std::string                        tag;    // "server" | "template" | "client"
  std::map<std::string, std::string> attrs;  // attr key → unquoted value
  std::string                        content; // raw section body
};

struct SfcFile {
  bool                    isSfc = false;
  std::vector<SfcSection> sections;
};

class SfcParser {
public:
  /// Parse src into an SfcFile.
  /// Returns isSfc=false if no section tags are found (legacy file).
  SfcFile parse(const std::string& src);

private:
  const std::string* src_ = nullptr;
  size_t             pos_ = 0;

  char   cur()                  const;
  char   peek(size_t n = 1)     const;
  void   advance(size_t n = 1);
  bool   atEnd()                const;

  void skipWhitespaceAndComments();

  /// Parse attributes from the current position until '>' or '/>'.
  /// Leaves pos_ pointing at '>' (or '/').
  std::map<std::string, std::string> parseAttrs();

  /// Read forward from pos_ until "</tagName>" and return everything before it.
  /// Advances pos_ past the closing tag.
  std::string extractUntilClose(const std::string& tagName);
};

} // namespace oxide
