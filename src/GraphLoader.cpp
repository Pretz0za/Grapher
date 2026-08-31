#include "GraphLoader.hpp"

#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gviz::io {

namespace {

// ---------------------------------------------------------------------
// .edges (Network Repository format)
// ---------------------------------------------------------------------

// Parses one data line into (u, v): 0 if the line is blank/whitespace-only/
// a '%' comment (nothing to parse), 1 on success, -1 if the line has
// content but doesn't start with two whitespace-separated integers. Any
// trailing content after v (including a weight token) is ignored, matching
// the old C parse_edge_line -- see LoadFromEdgesFile's doc comment.
int ParseEdgeLine(std::string_view line, long long &u, long long &v) {
  size_t i = 0;
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    i++;
  if (i >= line.size() || line[i] == '\n' || line[i] == '\r' || line[i] == '%')
    return 0;

  auto r1 = std::from_chars(line.data() + i, line.data() + line.size(), u);
  if (r1.ec != std::errc() || r1.ptr == line.data() + i)
    return -1;

  i = static_cast<size_t>(r1.ptr - line.data());
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    i++;

  auto r2 = std::from_chars(line.data() + i, line.data() + line.size(), v);
  if (r2.ec != std::errc() || r2.ptr == line.data() + i)
    return -1;

  return 1;
}

size_t EnsureVertex(Graph &g, std::unordered_map<long long, size_t> &idMap, long long externalId) {
  auto [it, inserted] = idMap.try_emplace(externalId, size_t{0});
  if (inserted)
    it->second = g.AddVertex();
  return it->second;
}

// ---------------------------------------------------------------------
// .gexf
// ---------------------------------------------------------------------

std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file)
    throw std::runtime_error("gviz::io::LoadFromGexfFile: cannot open " + path.string());
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

constexpr bool IsTagCharValid(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/' || c == '>';
}

// Finds a literal substring within buf[start, end), where a match is only
// valid if it fits entirely inside the bound (found + needle.size() <=
// end). buf.find(needle, start) returns the leftmost occurrence anywhere
// at or after start in the whole string; if that leftmost occurrence
// doesn't fit within end, no occurrence does (any other match starts even
// later), so a single find + bound check suffices -- no loop needed.
size_t FindWithin(const std::string &buf, size_t start, size_t end, std::string_view needle) {
  if (needle.empty() || start > end)
    return std::string::npos;
  size_t found = buf.find(needle, start);
  if (found == std::string::npos || found + needle.size() > end)
    return std::string::npos;
  return found;
}

// Finds the next occurrence of an XML tag opener (e.g. "<node") within
// buf[start, end) where the byte immediately after it is a valid tag
// terminator -- so "<attributes" doesn't spuriously match inside some
// "<attributesFoo". Reading buf[after] for after == end is well-defined
// (std::string::operator[](size()) returns the null terminator; end is
// always <= buf.size() here), mirroring how the original C scan could look
// one byte past a bounded sub-region into the rest of the (still valid,
// NUL-terminated) buffer.
size_t FindTag(const std::string &buf, size_t start, size_t end, std::string_view name) {
  size_t p = start;
  while (true) {
    size_t pos = FindWithin(buf, p, end, name);
    if (pos == std::string::npos)
      return std::string::npos;
    size_t after = pos + name.size();
    if (IsTagCharValid(buf[after]))
      return pos;
    p = after;
  }
}

int CountTags(const std::string &buf, std::string_view name) {
  int count = 0;
  size_t p = 0;
  while ((p = FindTag(buf, p, buf.size(), name)) != std::string::npos) {
    count++;
    p += name.size();
  }
  return count;
}

// Extracts attrName="value" or attrName='value' from within buf[tagStart,
// tagEnd), preferring the double-quote form. Returns std::nullopt if the
// attribute isn't present (a normal, checkable outcome -- most attributes
// here are optional).
std::optional<std::string> ExtractAttr(const std::string &buf, size_t tagStart, size_t tagEnd,
                                        std::string_view attrName) {
  std::string patternD(attrName);
  patternD += "=\"";
  size_t p = FindWithin(buf, tagStart, tagEnd, patternD);
  char quote = '"';
  if (p == std::string::npos) {
    std::string patternS(attrName);
    patternS += "='";
    p = FindWithin(buf, tagStart, tagEnd, patternS);
    quote = '\'';
    if (p == std::string::npos)
      return std::nullopt;
    p += patternS.size();
  } else {
    p += patternD.size();
  }

  size_t valEnd = buf.find(quote, p);
  if (valEnd == std::string::npos || valEnd >= tagEnd)
    return std::nullopt;
  return buf.substr(p, valEnd - p);
}

enum class GexfAttrKind { String, Number, Bool };

GexfAttrKind AttrKindFromType(std::string_view type) {
  if (type == "integer" || type == "long" || type == "float" || type == "double")
    return GexfAttrKind::Number;
  if (type == "boolean")
    return GexfAttrKind::Bool;
  return GexfAttrKind::String;
}

bool LooksLikeNumber(std::string_view s) {
  size_t i = 0;
  if (i < s.size() && (s[i] == '-' || s[i] == '+'))
    i++;
  size_t digits = 0;
  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
    i++;
    digits++;
  }
  if (i < s.size() && s[i] == '.') {
    i++;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      i++;
      digits++;
    }
  }
  if (digits == 0)
    return false;
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    i++;
    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
      i++;
    if (i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[i])))
      return false;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
      i++;
  }
  return i == s.size();
}

bool LooksLikeBool(std::string_view s) { return s == "true" || s == "false"; }

void AppendJsonEscaped(std::string &out, std::string_view s) {
  out += '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        char esc[8];
        std::snprintf(esc, sizeof(esc), "\\u%04x", c);
        out += esc;
      } else {
        out += static_cast<char>(c);
      }
    }
  }
  out += '"';
}

void AppendJsonValue(std::string &out, GexfAttrKind kind, std::string_view value) {
  if (kind == GexfAttrKind::Number && LooksLikeNumber(value)) {
    out += value;
    return;
  }
  if (kind == GexfAttrKind::Bool && LooksLikeBool(value)) {
    out += value;
    return;
  }
  AppendJsonEscaped(out, value);
}

constexpr std::string_view kGexfJsonIndent = "  ";

void AppendJsonField(std::string &out, bool &hasFields, std::string_view key, GexfAttrKind kind,
                      std::string_view value) {
  out += hasFields ? ",\n" : "\n";
  out += kGexfJsonIndent;
  AppendJsonEscaped(out, key);
  out += ": ";
  AppendJsonValue(out, kind, value);
  hasFields = true;
}

std::string BuildNodeJson(const std::optional<std::string> &label, const std::string &buf,
                           size_t bodyStart, size_t bodyEnd,
                           const std::unordered_map<std::string, GexfAttrKind> &attrTypes) {
  std::string out = "{";
  bool hasFields = false;
  if (label)
    AppendJsonField(out, hasFields, "label", GexfAttrKind::String, *label);

  size_t ap = bodyStart;
  while (true) {
    size_t attrTag = FindTag(buf, ap, bodyEnd, "<attvalue");
    if (attrTag == std::string::npos)
      break;
    size_t attrTagEnd = buf.find('>', attrTag);
    if (attrTagEnd == std::string::npos || attrTagEnd > bodyEnd)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: malformed <attvalue> tag");

    std::optional<std::string> forId = ExtractAttr(buf, attrTag, attrTagEnd, "for");
    if (!forId)
      forId = ExtractAttr(buf, attrTag, attrTagEnd, "id");
    std::optional<std::string> value = ExtractAttr(buf, attrTag, attrTagEnd, "value");
    if (!forId || !value)
      throw std::runtime_error(
          "gviz::io::LoadFromGexfFile: <attvalue> missing for/id or value attribute");

    GexfAttrKind kind = GexfAttrKind::String;
    if (auto it = attrTypes.find(*forId); it != attrTypes.end())
      kind = it->second;

    AppendJsonField(out, hasFields, *forId, kind, *value);
    ap = attrTagEnd + 1;
  }

  out += hasFields ? "\n}" : "}";
  return out;
}

void LoadAttributeTypes(const std::string &buf, std::unordered_map<std::string, GexfAttrKind> &types) {
  size_t p = 0;
  while (true) {
    size_t tagStart = FindTag(buf, p, buf.size(), "<attributes");
    if (tagStart == std::string::npos)
      break;
    size_t tagEnd = buf.find('>', tagStart);
    if (tagEnd == std::string::npos)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: malformed <attributes> tag");

    auto classVal = ExtractAttr(buf, tagStart, tagEnd, "class");
    bool isEdgeClass = classVal && *classVal == "edge";

    size_t bodyStart = tagEnd + 1;
    size_t bodyEnd = buf.find("</attributes>", bodyStart);
    if (bodyEnd == std::string::npos)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: unterminated <attributes> block");

    if (!isEdgeClass) {
      size_t ap = bodyStart;
      while (true) {
        size_t attrTag = FindTag(buf, ap, bodyEnd, "<attribute");
        if (attrTag == std::string::npos)
          break;
        size_t attrTagEnd = buf.find('>', attrTag);
        if (attrTagEnd == std::string::npos || attrTagEnd > bodyEnd)
          throw std::runtime_error("gviz::io::LoadFromGexfFile: malformed <attribute> tag");

        auto id = ExtractAttr(buf, attrTag, attrTagEnd, "id");
        if (!id)
          throw std::runtime_error("gviz::io::LoadFromGexfFile: <attribute> missing id");
        auto type = ExtractAttr(buf, attrTag, attrTagEnd, "type");
        types[*id] = AttrKindFromType(type ? *type : std::string("string"));

        ap = attrTagEnd + 1;
      }
    }

    p = bodyEnd + std::string_view("</attributes>").size();
  }
}

void LoadNodes(const std::string &buf, Graph &g, std::unordered_map<std::string, size_t> &ids,
               const std::unordered_map<std::string, GexfAttrKind> &attrTypes) {
  size_t p = 0;
  while (true) {
    size_t tagStart = FindTag(buf, p, buf.size(), "<node");
    if (tagStart == std::string::npos)
      break;
    size_t tagEnd = buf.find('>', tagStart);
    if (tagEnd == std::string::npos)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: malformed <node> tag");

    auto id = ExtractAttr(buf, tagStart, tagEnd, "id");
    if (!id)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: <node> missing id attribute");

    auto label = ExtractAttr(buf, tagStart, tagEnd, "label");

    bool selfClosing = tagEnd > tagStart && buf[tagEnd - 1] == '/';
    size_t bodyStart = tagEnd + 1;
    size_t bodyEnd = bodyStart;
    size_t nextP;
    if (selfClosing) {
      nextP = tagEnd + 1;
    } else {
      size_t nodeClose = buf.find("</node>", bodyStart);
      if (nodeClose == std::string::npos)
        throw std::runtime_error("gviz::io::LoadFromGexfFile: unterminated <node> block");
      bodyEnd = nodeClose;
      nextP = nodeClose + std::string_view("</node>").size();
    }

    std::string json = BuildNodeJson(label, buf, bodyStart, bodyEnd, attrTypes);
    size_t idx = g.AddVertex(new std::string(std::move(json)));
    ids.try_emplace(*id, idx);

    p = nextP;
  }
}

void LoadEdges(const std::string &buf, Graph &g, const std::unordered_map<std::string, size_t> &ids) {
  size_t p = 0;
  while (true) {
    size_t tagStart = FindTag(buf, p, buf.size(), "<edge");
    if (tagStart == std::string::npos)
      break;
    size_t tagEnd = buf.find('>', tagStart);
    if (tagEnd == std::string::npos)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: malformed <edge> tag");

    auto source = ExtractAttr(buf, tagStart, tagEnd, "source");
    auto target = ExtractAttr(buf, tagStart, tagEnd, "target");
    if (!source || !target)
      throw std::runtime_error("gviz::io::LoadFromGexfFile: <edge> missing source or target");

    auto fromIt = ids.find(*source);
    auto toIt = ids.find(*target);
    if (fromIt == ids.end() || toIt == ids.end())
      throw std::runtime_error("gviz::io::LoadFromGexfFile: <edge> references an unknown node id");

    double weight = 1.0;
    if (auto w = ExtractAttr(buf, tagStart, tagEnd, "weight"))
      weight = std::strtod(w->c_str(), nullptr);

    g.AddEdge(fromIt->second, toIt->second, weight);

    p = tagEnd + 1;
  }
}

// ---------------------------------------------------------------------
// .obj
// ---------------------------------------------------------------------

bool ParseObjVertexRef(std::string_view tok, size_t vertexCount, size_t &out) {
  long idx = 0;
  auto r = std::from_chars(tok.data(), tok.data() + tok.size(), idx);
  if (r.ec != std::errc() || r.ptr == tok.data() || idx == 0)
    return false;

  if (idx > 0) {
    if (static_cast<size_t>(idx) > vertexCount)
      return false;
    out = static_cast<size_t>(idx - 1);
    return true;
  }

  size_t absIdx = static_cast<size_t>(-idx);
  if (absIdx > vertexCount)
    return false;
  out = vertexCount - absIdx;
  return true;
}

void ObjAddEdgeUndup(Graph &g, size_t a, size_t b) {
  if (a == b)
    return;
  if (a > b)
    std::swap(a, b);
  if (g.EdgeExists(a, b))
    return;
  g.AddEdge(a, b, 1.0);
}

void ObjAddFaceEdges(Graph &g, const std::vector<size_t> &verts) {
  if (verts.size() < 2)
    return;
  for (size_t i = 0; i + 1 < verts.size(); i++)
    ObjAddEdgeUndup(g, verts[i], verts[i + 1]);
  ObjAddEdgeUndup(g, verts.back(), verts.front());
}

void ObjParseFaceLine(Graph &g, std::string_view rest, const std::filesystem::path &path) {
  size_t vertexCount = g.Size();
  std::vector<size_t> faceVerts;

  size_t i = 0;
  while (i < rest.size()) {
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t'))
      i++;
    if (i >= rest.size() || rest[i] == '\n' || rest[i] == '\r')
      break;

    size_t tokStart = i;
    while (i < rest.size() && rest[i] != ' ' && rest[i] != '\t' && rest[i] != '\n' &&
           rest[i] != '\r')
      i++;
    std::string_view tok = rest.substr(tokStart, i - tokStart);

    size_t idx = 0;
    if (!ParseObjVertexRef(tok, vertexCount, idx))
      throw std::runtime_error("gviz::io::LoadFromObjFile: bad face vertex reference in " +
                                path.string());
    faceVerts.push_back(idx);
  }

  ObjAddFaceEdges(g, faceVerts);
}

} // namespace

Graph LoadFromEdgesFile(const std::filesystem::path &path, const EdgesFileOptions &opts) {
  std::ifstream file(path);
  if (!file)
    throw std::runtime_error("gviz::io::LoadFromEdgesFile: cannot open " + path.string());

  Graph g(opts.directed);
  std::unordered_map<long long, size_t> idMap;
  bool skipNext = opts.skipHeader;
  bool sawAny = false;

  std::string line;
  while (std::getline(file, line)) {
    long long u = 0;
    long long v = 0;
    int parsed = ParseEdgeLine(line, u, v);
    if (parsed < 0)
      throw std::runtime_error("gviz::io::LoadFromEdgesFile: malformed line in " + path.string());
    if (parsed == 0)
      continue;

    if (skipNext) {
      skipNext = false;
      continue;
    }

    if (u < 0 || v < 0)
      throw std::runtime_error("gviz::io::LoadFromEdgesFile: negative vertex id in " +
                                path.string());

    sawAny = true;
    size_t from = EnsureVertex(g, idMap, u);
    size_t to = EnsureVertex(g, idMap, v);
    g.AddEdge(from, to, 1.0);
  }

  if (!sawAny)
    throw std::runtime_error("gviz::io::LoadFromEdgesFile: no edges found in " + path.string());

  return g;
}

Graph LoadFromGexfFile(const std::filesystem::path &path, bool directed) {
  std::string buf = ReadFile(path);

  int nodeCount = CountTags(buf, "<node");
  if (nodeCount <= 0)
    throw std::runtime_error("gviz::io::LoadFromGexfFile: no <node> elements in " + path.string());

  Graph g(directed, static_cast<size_t>(nodeCount));
  std::unordered_map<std::string, size_t> ids;
  ids.reserve(static_cast<size_t>(nodeCount) * 2);
  std::unordered_map<std::string, GexfAttrKind> attrTypes;

  try {
    LoadAttributeTypes(buf, attrTypes);
    LoadNodes(buf, g, ids, attrTypes);
    LoadEdges(buf, g, ids);
  } catch (...) {
    FreeVertexDataStrings(g);
    throw;
  }

  return g;
}

void FreeVertexDataStrings(Graph &g) {
  size_t n = g.Size();
  for (size_t i = 0; i < n; i++) {
    delete static_cast<std::string *>(g.GetVertexData(i));
    g.SetVertexData(i, nullptr);
  }
}

Graph LoadFromObjFile(const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file)
    throw std::runtime_error("gviz::io::LoadFromObjFile: cannot open " + path.string());

  Graph g(false);
  std::string line;
  while (std::getline(file, line)) {
    std::string_view p(line);
    size_t i = 0;
    while (i < p.size() && (p[i] == ' ' || p[i] == '\t'))
      i++;
    if (i >= p.size() || p[i] == '\n' || p[i] == '\r' || p[i] == '#')
      continue;

    if (p[i] == 'v' && i + 1 < p.size() && (p[i + 1] == ' ' || p[i + 1] == '\t')) {
      g.AddVertex();
      continue;
    }

    if (p[i] == 'f' && i + 1 < p.size() && (p[i + 1] == ' ' || p[i + 1] == '\t'))
      ObjParseFaceLine(g, p.substr(i + 2), path);
  }

  if (g.Size() == 0)
    throw std::runtime_error("gviz::io::LoadFromObjFile: no vertices found in " + path.string());

  return g;
}

} // namespace gviz::io
