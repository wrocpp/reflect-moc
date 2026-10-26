// Spike: JSON text -> CBOR bytes, all in constant evaluation. Mirrors what moc writes for Q_PLUGIN_METADATA (qt_pluginMetaDataV2_*).
// Not library code. Limits (on purpose, to stay exact): a decimal with more than 15 significant digits or a power of ten past 22
// is rejected with a message instead of being rounded wrongly.
#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace spike {

struct Value {
  enum Kind { Null, False, True, Int, Double, String, Array, Object } kind = Null;
  std::int64_t i = 0;
  double d = 0;
  std::string s;
  std::vector<Value> items;
  std::vector<std::pair<std::string, Value>> members;
};

struct Parser {
  std::string_view t;
  std::size_t p = 0;
  constexpr void fail(char const* why) const { throw why; }  // in a constant expression a throw is a compile error naming the reason
  constexpr void ws() { while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\t' || t[p] == '\r')) ++p; }
  constexpr bool eat(char c) { ws(); if (p < t.size() && t[p] == c) { ++p; return true; } return false; }
  constexpr void expect(char c) { if (!eat(c)) fail("json: unexpected character"); }
  constexpr void utf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += char(cp);
    else if (cp < 0x800) { o += char(0xC0 | cp >> 6); o += char(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) { o += char(0xE0 | cp >> 12); o += char(0x80 | (cp >> 6 & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
    else { o += char(0xF0 | cp >> 18); o += char(0x80 | (cp >> 12 & 0x3F)); o += char(0x80 | (cp >> 6 & 0x3F)); o += char(0x80 | (cp & 0x3F)); }
  }
  constexpr unsigned hex4() {
    unsigned v = 0;
    for (int k = 0; k < 4; ++k) {
      if (p >= t.size()) fail("json: truncated \\u escape");
      char c = t[p++];
      v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : (fail("json: bad hex digit"), 0));
    }
    return v;
  }
  constexpr std::string str() {
    expect('"');
    std::string o;
    while (p < t.size() && t[p] != '"') {
      char c = t[p++];
      if (c != '\\') { o += c; continue; }
      char e = t[p++];
      switch (e) {
        case '"': o += '"'; break; case '\\': o += '\\'; break; case '/': o += '/'; break;
        case 'b': o += '\b'; break; case 'f': o += '\f'; break; case 'n': o += '\n'; break;
        case 'r': o += '\r'; break; case 't': o += '\t'; break;
        case 'u': {
          unsigned cp = hex4();
          if (cp >= 0xD800 && cp < 0xDC00) {
            if (!(p + 1 < t.size() && t[p] == '\\' && t[p + 1] == 'u')) fail("json: lone surrogate");
            p += 2;
            unsigned lo = hex4();
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
          }
          utf8(o, cp);
          break;
        }
        default: fail("json: bad escape");
      }
    }
    if (p >= t.size()) fail("json: unterminated string");
    ++p;
    return o;
  }
  constexpr Value number() {
    std::size_t b = p;
    bool neg = false, frac = false;
    if (t[p] == '-') { neg = true; ++p; }
    std::uint64_t mant = 0;
    int digits = 0, scale = 0;
    bool overflow = false;
    auto digit = [&](char c) {
      if (digits > 0 || c != '0') ++digits;
      if (mant > (UINT64_MAX - 9) / 10) overflow = true; else mant = mant * 10 + std::uint64_t(c - '0');
    };
    while (p < t.size() && t[p] >= '0' && t[p] <= '9') digit(t[p++]);
    if (p < t.size() && t[p] == '.') {
      frac = true; ++p;
      while (p < t.size() && t[p] >= '0' && t[p] <= '9') { digit(t[p++]); --scale; }
    }
    int ex = 0;
    if (p < t.size() && (t[p] == 'e' || t[p] == 'E')) {
      frac = true; ++p;
      bool eneg = false;
      if (t[p] == '+') ++p; else if (t[p] == '-') { eneg = true; ++p; }
      while (p < t.size() && t[p] >= '0' && t[p] <= '9') ex = ex * 10 + (t[p++] - '0');
      if (eneg) ex = -ex;
    }
    if (p == b || (p == b + 1 && neg)) fail("json: bad number");
    Value v;
    if (!frac && !overflow) {  // Qt reads every JSON number as a double, so an integer past 2^53 is rounded to the nearest double
      v.kind = Value::Double;
      v.d = neg ? -double(mant) : double(mant);
      return v;
    }
    // Clinger's fast path: exact when the mantissa fits 2^53 and the power of ten fits a double exactly
    int k = scale + ex;
    if (overflow || mant > (std::uint64_t(1) << 53) || k > 22 || k < -22) fail("json: this number needs a correctly rounded decimal conversion, not done in the spike");
    double d = double(mant);
    double pw = 1;
    for (int n = 0; n < (k < 0 ? -k : k); ++n) pw *= 10;
    d = k < 0 ? d / pw : d * pw;
    v.kind = Value::Double;
    v.d = neg ? -d : d;
    return v;
  }
  constexpr Value value() {
    ws();
    if (p >= t.size()) fail("json: unexpected end");
    Value v;
    char c = t[p];
    if (c == '{') {
      ++p; v.kind = Value::Object;
      if (!eat('}')) {
        do { auto k = str(); expect(':'); v.members.emplace_back(std::move(k), value()); } while (eat(','));
        expect('}');
      }
    } else if (c == '[') {
      ++p; v.kind = Value::Array;
      if (!eat(']')) { do v.items.push_back(value()); while (eat(',')); expect(']'); }
    } else if (c == '"') { v.kind = Value::String; v.s = str(); }
    else if (t.substr(p, 4) == "true") { p += 4; v.kind = Value::True; }
    else if (t.substr(p, 5) == "false") { p += 5; v.kind = Value::False; }
    else if (t.substr(p, 4) == "null") { p += 4; v.kind = Value::Null; }
    else v = number();
    return v;
  }
};

using Bytes = std::vector<unsigned char>;

constexpr void head(Bytes& o, unsigned major, std::uint64_t n) {
  unsigned char m = (unsigned char)(major << 5);
  int extra = n < 24 ? 0 : n <= 0xFF ? 1 : n <= 0xFFFF ? 2 : n <= 0xFFFFFFFF ? 4 : 8;
  o.push_back(extra == 0 ? m | (unsigned char)n : m | (unsigned char)(24 + (extra == 1 ? 0 : extra == 2 ? 1 : extra == 4 ? 2 : 3)));
  for (int k = extra - 1; k >= 0; --k) o.push_back((unsigned char)(n >> (8 * k)));
}
constexpr void text(Bytes& o, std::string const& s) { head(o, 3, s.size()); for (char c : s) o.push_back((unsigned char)c); }

constexpr void encode(Bytes& o, Value const& v) {
  switch (v.kind) {
    case Value::Null: o.push_back(0xf6); break;
    case Value::False: o.push_back(0xf4); break;
    case Value::True: o.push_back(0xf5); break;
    case Value::Int: if (v.i >= 0) head(o, 0, std::uint64_t(v.i)); else head(o, 1, ~std::uint64_t(v.i)); break;
    case Value::Double: {
      // moc's CBOR has an integer where the JSON number is integral and fits 64 bits (3.0 and 1e3 are written as 3 and 1000)
      constexpr double lo = -9223372036854775808.0, hi = 9223372036854775808.0;
      if (v.d > lo && v.d < hi && double(std::int64_t(v.d)) == v.d) { Value iv; iv.kind = Value::Int; iv.i = std::int64_t(v.d); encode(o, iv); break; }
      o.push_back(0xfb); auto b = std::bit_cast<std::uint64_t>(v.d); for (int k = 7; k >= 0; --k) o.push_back((unsigned char)(b >> (8 * k))); break; }
    case Value::String: text(o, v.s); break;
    case Value::Array: head(o, 4, v.items.size()); for (auto const& e : v.items) encode(o, e); break;
    case Value::Object: {
      // QJsonObject keeps its keys sorted, and the CBOR moc writes follows that order (see the note)
      std::vector<std::pair<std::string, Value> const*> order;
      for (auto const& m : v.members) {  // a repeated key keeps its last value, as QJsonObject does
        auto same = std::ranges::find_if(order, [&](auto* o2) { return o2->first == m.first; });
        if (same != order.end()) *same = &m; else order.push_back(&m);
      }
      std::ranges::stable_sort(order, [](auto* a, auto* b) { return a->first < b->first; });
      head(o, 5, order.size());
      for (auto* m : order) { text(o, m->first); encode(o, m->second); }
      break;
    }
  }
}

// the whole top-level map as moc writes it for the V2 section: IID (2), className (3), MetaData (4), in an indefinite-length map
constexpr Bytes plugin_cbor(std::span<unsigned char const> bytes, std::string_view iid, std::string_view cls) {
  std::string const text_in(bytes.begin(), bytes.end());
  std::string_view const json = text_in;
  Parser ps{json};
  Value meta = ps.value();
  ps.ws();
  if (ps.p != json.size()) throw "json: trailing characters";
  if (meta.kind != Value::Object) throw "json: the plugin metadata must be an object";
  Bytes o;
  o.push_back(0xbf);
  head(o, 0, 2); text(o, std::string(iid));
  head(o, 0, 3); text(o, std::string(cls));
  head(o, 0, 4); encode(o, meta);
  o.push_back(0xff);
  return o;
}

// a plain C array inside a struct: QPluginMetaDataV2<MD> needs MD to be a reference to an array of unsigned char, which std::array is not
template <std::size_t N>
struct Blob {
  unsigned char bytes[N];
  static constexpr std::size_t size() { return N; }
  constexpr unsigned char const* data() const { return bytes; }
  constexpr unsigned char operator[](std::size_t k) const { return bytes[k]; }
};

template <std::size_t N>
consteval Blob<N> to_array(Bytes const& in) {
  Blob<N> a{};
  for (std::size_t k = 0; k < N; ++k) a.bytes[k] = in[k];
  return a;
}
}  // namespace spike

// size by one evaluation, contents by a second: the vector cannot leave the constant expression, the array can
#define SPIKE_CBOR(json_array, iid, cls)                                                                           \
  ([]() consteval {                                                                                                \
    constexpr auto n = ::spike::plugin_cbor(std::span<unsigned char const>{json_array}, iid, cls).size(); \
    return ::spike::to_array<n>(::spike::plugin_cbor(std::span<unsigned char const>{json_array}, iid, cls)); \
  }())
