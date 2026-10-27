// Q_PLUGIN_METADATA(IID "x" FILE "x.json"): the metadata of a Qt plugin, and the two entry points a QPluginLoader looks for.
//
//   class DemoPlugin : public QObject, public IDemo {
//     RQT_OBJECT
//     Q_INTERFACES(IDemo)
//    public:
//     int answer() const override { return 42; }
//   };
//
//   static constexpr unsigned char demo_json[] = {      // the FILE "demo.json" of Q_PLUGIN_METADATA
//   #embed "demo.json"
//   };
//   RQT_PLUGIN_METADATA(DemoPlugin, IDemo_iid, demo_json)       // namespace scope, after the class, in one .cpp
//
// #embed is a directive, so it cannot stand inside the macro: the file is embedded in a block next to the class (GCC looks
// for it next to the source file, then in the --embed-dir= directories). The macro
// turns the JSON into the CBOR that moc writes (byte for byte, tests/qt/plugin) in constant evaluation and hands it to
// Qt's own QT_MOC_EXPORT_PLUGIN_V2. Without FILE, leave the last argument out.
//
// Not included by qt.hpp: a plugin includes this header in its one translation unit.
#pragma once

#include <QtCore/QtPlugin>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <span>
#include <string_view>

// The most JSON the encoder takes, in bytes. It is the constexpr operation limit (-fconstexpr-ops-limit, 33554432 by default)
// seen from the side of the file: docs/notes/plugin-metadata.md says how it was measured. A larger file is a static_assert
// that names this macro and the compiler flag; raise both together.
#ifndef RQT_PLUGIN_JSON_MAX_BYTES
#define RQT_PLUGIN_JSON_MAX_BYTES 32768
#endif

namespace rqt {
namespace detail::plugin {

// RFC 8949: the major type sits in the top three bits of the initial byte, the argument in the low five or in the 1, 2, 4 or
// 8 bytes after it.
enum major : unsigned { unsigned_int = 0, negative_int = 1, text_string = 3, array = 4, map = 5 };
inline constexpr unsigned major_shift = 5;
inline constexpr std::uint64_t inline_argument_max = 23;  // up to here the argument is in the initial byte
inline constexpr unsigned char argument_1_byte = 24;      // 24..27: the argument follows in 1, 2, 4 and 8 bytes
inline constexpr unsigned char cbor_false = 0xf4;
inline constexpr unsigned char cbor_true = 0xf5;
inline constexpr unsigned char cbor_null = 0xf6;
inline constexpr unsigned char cbor_double = 0xfb;
inline constexpr unsigned char cbor_indefinite_map = 0xbf;
inline constexpr unsigned char cbor_break = 0xff;
inline constexpr unsigned bits_per_byte = 8;
inline constexpr unsigned prefix_bytes = sizeof(std::uint64_t);
inline constexpr unsigned one_byte = 1;  // the sizes of a head's argument
inline constexpr unsigned two_bytes = 2;
inline constexpr unsigned four_bytes = 4;
inline constexpr unsigned eight_bytes = 8;
inline constexpr std::uint64_t max_in_1_byte = 0xFF;
inline constexpr std::uint64_t max_in_2_bytes = 0xFFFF;
inline constexpr std::uint64_t max_in_4_bytes = 0xFFFFFFFF;

// The integer keys of the map at the top of the metadata (QtPluginMetaDataKeys in qpluginloader.cpp).
inline constexpr unsigned key_iid = 2;
inline constexpr unsigned key_class_name = 3;
inline constexpr unsigned key_meta_data = 4;

// A JSON number is read as a double, as QJsonDocument does. A double is written as an integer when it holds one that fits
// 64 bits, and as eight bytes otherwise (never shortened).
inline constexpr double int64_min_as_double = -9223372036854775808.0;
inline constexpr double int64_limit_as_double = 9223372036854775808.0;  // 2^63, the first double past int64
inline constexpr unsigned decimal_base = 10;
inline constexpr unsigned hex_letter_offset = 10;  // 'a' is the digit ten
inline constexpr int exact_power_of_ten_max = 22;  // 10^22 is the largest power of ten a double holds exactly
inline constexpr unsigned double_mantissa_bits = 53;
inline constexpr std::uint64_t exact_double_mantissa = std::uint64_t(1) << double_mantissa_bits;
inline constexpr std::uint64_t unsigned_digit_room = (UINT64_MAX - (decimal_base - 1)) / decimal_base;  // one more digit still fits
inline constexpr int exponent_digits_kept = 100000;  // an exponent past this is clamped: it is far outside any double
inline constexpr unsigned max_nesting = 128;         // each level uses a few frames; -fconstexpr-depth is 512

// The most the CBOR can be longer than its JSON, per byte of JSON: a number such as 0.5 is three bytes of text and nine of
// double. Strings, literals and containers are never longer than their text.
inline constexpr std::size_t expansion_per_json_byte = 3;
inline constexpr std::size_t slack_for_top_level = 64;  // the map's own bytes and the heads of iid and className
inline constexpr std::size_t shortest_member = 4;       // "":0 and the comma: no more members than this fit in a file
inline constexpr std::size_t member_slack = 2;

// UTF-8 of a code point: a lead byte that says how many continuation bytes follow, each carrying six bits.
inline constexpr unsigned ascii_limit = 0x80;
inline constexpr unsigned utf8_2_byte_limit = 0x800;
inline constexpr unsigned utf8_3_byte_limit = 0x10000;
inline constexpr unsigned utf8_continuation_marker = 0x80;
inline constexpr unsigned utf8_continuation_mask = 0x3F;
inline constexpr unsigned utf8_continuation_bits = 6;
inline constexpr unsigned char utf8_lead_marker[] = {0, 0xC0, 0xE0, 0xF0};  // by the number of continuation bytes
inline constexpr unsigned utf8_limits[] = {ascii_limit, utf8_2_byte_limit, utf8_3_byte_limit};  // from here on, one more byte

// \u escapes: four hex digits, and a code point past the first 65536 as a high and a low surrogate.
inline constexpr unsigned high_surrogate_first = 0xD800;
inline constexpr unsigned low_surrogate_first = 0xDC00;
inline constexpr unsigned surrogate_count = 0x400;
inline constexpr unsigned hex_digit_count = 4;
inline constexpr unsigned hex_base = 16;
inline constexpr unsigned not_a_hex_digit = hex_base;

// A bad file stops the build with the reason and the byte it was found at: a throw that nothing catches is a compile error
// that prints what was thrown, as in  error: uncaught exception 'json_error{((const char*)"json: bad number"), 12}'.
struct json_error {
  char const* why;
  std::size_t offset;
};

// The three arrays one encoding works in, and its result. A constant expression has to free what it allocates, so the arrays
// are owned here, and the result is a range that lives as long as this object (the full expression that names it).
class workspace {
 public:
  struct member {
    std::size_t key_from, key_to;  // the bytes of the key's text, after its head
    std::size_t from, to;          // the key and its value
    std::uint64_t prefix;          // the first bytes of the key, first byte highest, zero past its end: most comparisons end here
  };

  constexpr workspace(std::size_t json_bytes, std::size_t top_level_bytes)
      : cap_(expansion_per_json_byte * json_bytes + slack_for_top_level + top_level_bytes),
        member_cap_(json_bytes / shortest_member + member_slack),
        out_(new unsigned char[cap_]),
        scratch_(new unsigned char[cap_]),
        members_(new member[member_cap_]) {}
  constexpr workspace(workspace&& o) noexcept
      : cap_(o.cap_), member_cap_(o.member_cap_), out_(o.out_), scratch_(o.scratch_), members_(o.members_), size_(o.size_) {
    o.out_ = o.scratch_ = nullptr;
    o.members_ = nullptr;
  }
  workspace& operator=(workspace&&) = delete;
  constexpr ~workspace() {
    delete[] out_;
    delete[] scratch_;
    delete[] members_;
  }

  constexpr unsigned char const* begin() const { return out_; }
  constexpr unsigned char const* end() const { return out_ + size_; }
  constexpr std::size_t size() const { return size_; }

 private:
  friend struct encoder;
  std::size_t cap_, member_cap_;
  unsigned char* out_;
  unsigned char* scratch_;
  member* members_;
  std::size_t size_ = 0;
};

// How many bytes follow the initial byte of a head whose argument is `a`.
constexpr unsigned argument_bytes(std::uint64_t a) {
  if (a <= inline_argument_max) return 0;
  if (a <= max_in_1_byte) return one_byte;
  if (a <= max_in_2_bytes) return two_bytes;
  return a <= max_in_4_bytes ? four_bytes : eight_bytes;
}

constexpr bool is_high_surrogate(unsigned cp) { return cp >= high_surrogate_first && cp < low_surrogate_first; }
constexpr bool is_low_surrogate(unsigned cp) { return cp >= low_surrogate_first && cp < low_surrogate_first + surrogate_count; }

// The value of a hex digit, or not_a_hex_digit.
constexpr unsigned hex_value(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + hex_letter_offset;
  if (c >= 'A' && c <= 'F') return c - 'A' + hex_letter_offset;
  return not_a_hex_digit;
}

// What a backslash and one letter stand for, or 0 for a letter that is no escape (\u is handled apart).
struct escape_pair {
  char letter;
  unsigned char value;
};
inline constexpr escape_pair simple_escapes[] = {{'"', '"'},  {'\\', '\\'}, {'/', '/'},  {'b', '\b'},
                                                 {'f', '\f'}, {'n', '\n'},  {'r', '\r'}, {'t', '\t'}};
constexpr unsigned char simple_escape(unsigned char letter) {
  for (escape_pair const& e : simple_escapes)
    if (e.letter == letter) return e.value;
  return 0;
}

// One more digit of an exponent; past exponent_digits_kept it stops growing, so a long run of digits cannot overflow.
constexpr int more_exponent(int exponent, int digit) {
  return exponent < exponent_digits_kept ? exponent * static_cast<int>(decimal_base) + digit : exponent;
}

// A number as the text gives it: digits as an integer, where the point was, and the exponent.
struct decimal {
  std::uint64_t mantissa = 0;
  bool negative = false;
  bool overflow = false;      // more digits than a uint64 holds
  bool inexact_form = false;  // a point or an exponent was written
  int scale = 0;              // minus the digits after the point
  int exponent = 0;
};

// Clinger's fast path: the digits fit 2^53 and the power of ten fits a double, so one multiplication or division is exact.
constexpr bool exact_in_double(decimal const& d, int power) {
  return !d.overflow && d.mantissa <= exact_double_mantissa && power <= exact_power_of_ten_max && power >= -exact_power_of_ten_max;
}
constexpr double scaled(double magnitude, int power) {
  double ten_power = 1;
  for (int n = 0; n < (power < 0 ? -power : power); ++n) ten_power *= decimal_base;
  return power < 0 ? magnitude / ten_power : magnitude * ten_power;
}

// Reads the JSON text once, left to right, and writes the CBOR of each value as it goes. What is not known until a value ends
// (the length of a string after its escapes, the count of an array, the order of an object) is a one-byte placeholder for the
// head, filled in then; only a head that needs more than one byte moves what follows it. No tree of the document is built,
// and the arrays are plain ones: the compiler counts every operation of a constant evaluation, and a std::vector or a
// std::span in the loop costs several times what a pointer does.
struct encoder {
  workspace& w;
  unsigned char const* const begin;  // views the caller's array, which outlives this object
  unsigned char const* cur;
  unsigned char const* const end;
  std::size_t member_top = 0;
  unsigned depth = 0;

  [[noreturn]] constexpr void fail(char const* why) const { throw json_error{why, static_cast<std::size_t>(cur - begin)}; }
  constexpr std::size_t size() const { return w.size_; }
  constexpr bool at_end() const { return cur >= end; }
  constexpr bool peek(char c) const { return cur < end && *cur == c; }
  constexpr bool digit_at() const { return cur < end && *cur >= '0' && *cur <= '9'; }
  constexpr void skip_space() {
    while (cur < end && (*cur == ' ' || *cur == '\n' || *cur == '\t' || *cur == '\r')) ++cur;
  }
  constexpr bool eat(char c) {
    skip_space();
    if (!peek(c)) return false;
    ++cur;
    return true;
  }
  constexpr void expect(char c, char const* why) {
    if (!eat(c)) fail(why);
  }
  constexpr bool eat_word(std::string_view word) {
    if (static_cast<std::size_t>(end - cur) < word.size()) return false;
    for (std::size_t k = 0; k < word.size(); ++k)
      if (cur[k] != static_cast<unsigned char>(word[k])) return false;
    cur += word.size();
    return true;
  }

  constexpr void put(unsigned char b) {
    if (w.size_ >= w.cap_) fail("rqt: the CBOR outgrew its buffer");
    w.out_[w.size_++] = b;
  }
  constexpr void put_text(std::string_view s) {
    std::size_t const at = begin_head();
    for (char c : s) put(static_cast<unsigned char>(c));
    end_head(at, text_string, s.size());
  }

  // A head is one byte now and the true head when the argument is known.
  constexpr std::size_t begin_head() {
    put(0);
    return w.size_ - 1;
  }
  constexpr void end_head(std::size_t at, unsigned type, std::uint64_t argument) {
    unsigned char const initial = static_cast<unsigned char>(type << major_shift);
    unsigned const extra = argument_bytes(argument);
    if (extra == 0) {
      w.out_[at] = static_cast<unsigned char>(initial | argument);
      return;
    }
    if (w.size_ + extra > w.cap_) fail("rqt: the CBOR outgrew its buffer");
    for (std::size_t k = w.size_; k > at + 1; --k) w.out_[k - 1 + extra] = w.out_[k - 1];  // make room behind the initial byte
    w.size_ += extra;
    w.out_[at] = static_cast<unsigned char>(initial | (argument_1_byte + std::countr_zero(extra)));
    for (unsigned k = 0; k < extra; ++k) w.out_[at + 1 + k] = static_cast<unsigned char>(argument >> (bits_per_byte * (extra - 1 - k)));
  }
  constexpr void put_unsigned(std::uint64_t n) { end_head(begin_head(), unsigned_int, n); }

  constexpr void put_utf8(unsigned cp) {
    unsigned continuation = 0;  // how many of the limits the code point has passed
    for (unsigned limit : utf8_limits) continuation += cp >= limit;
    if (continuation == 0) return put(static_cast<unsigned char>(cp));
    put(static_cast<unsigned char>(utf8_lead_marker[continuation] | (cp >> (utf8_continuation_bits * continuation))));
    for (unsigned k = continuation; k-- > 0;)
      put(static_cast<unsigned char>(utf8_continuation_marker | ((cp >> (utf8_continuation_bits * k)) & utf8_continuation_mask)));
  }

  constexpr unsigned hex4() {
    unsigned v = 0;
    for (unsigned k = 0; k < hex_digit_count; ++k) {
      if (at_end()) fail("json: truncated \\u escape");
      unsigned const d = hex_value(*cur++);
      if (d == not_a_hex_digit) fail("json: bad hex digit in a \\u escape");
      v = v * hex_base + d;
    }
    return v;
  }

  // After a high surrogate: the \u low surrogate that completes the pair, and the code point of both.
  constexpr unsigned surrogate_pair(unsigned high) {
    if (!eat_word("\\u")) fail("json: a high surrogate must be followed by a \\u low surrogate");
    unsigned const low = hex4();
    if (!is_low_surrogate(low)) fail("json: bad low surrogate");
    return utf8_3_byte_limit + ((high - high_surrogate_first) * surrogate_count) + (low - low_surrogate_first);
  }

  // After \u: a code point, or a surrogate pair that makes one.
  constexpr void unicode_escape() {
    unsigned cp = hex4();
    if (is_low_surrogate(cp)) fail("json: lone low surrogate");
    if (is_high_surrogate(cp)) cp = surrogate_pair(cp);
    put_utf8(cp);
  }

  constexpr void escape() {
    if (at_end()) fail("json: unterminated string");
    unsigned char const letter = *cur++;
    if (letter == 'u') return unicode_escape();
    unsigned char const simple = simple_escape(letter);
    if (simple == 0) fail("json: bad escape");
    put(simple);
  }

  struct text_at {
    std::size_t from;     // the head of the item
    std::size_t payload;  // the first byte of the text after it
  };

  // The text between the quotes, decoded.
  constexpr text_at string() {
    if (!peek('"')) fail("json: a string was expected");
    ++cur;
    std::size_t const at = begin_head();
    std::size_t const text_from = size();
    for (;;) {
      if (at_end()) fail("json: unterminated string");
      unsigned char const c = *cur++;
      if (c == '"') break;
      if (c == '\\') escape();
      else put(c);
    }
    std::size_t const length = size() - text_from;
    end_head(at, text_string, length);
    return {.from = at, .payload = size() - length};
  }

  // One more digit of the integer or fraction part; `fraction` moves the decimal point.
  constexpr void digit(decimal& d, bool fraction) {
    if (d.mantissa > unsigned_digit_room) d.overflow = true;
    else d.mantissa = d.mantissa * decimal_base + static_cast<std::uint64_t>(*cur - '0');
    if (fraction) --d.scale;
    ++cur;
  }
  constexpr void digits(decimal& d, bool fraction) {
    while (digit_at()) digit(d, fraction);
  }
  constexpr void fraction(decimal& d) {
    d.inexact_form = true;
    ++cur;
    if (!digit_at()) fail("json: bad number, no digit after the decimal point");
    digits(d, true);
  }
  constexpr void exponent(decimal& d) {
    d.inexact_form = true;
    ++cur;
    bool const minus = peek('-');
    if (peek('+') || minus) ++cur;
    if (!digit_at()) fail("json: bad number, no digit in the exponent");
    for (; digit_at(); ++cur) d.exponent = more_exponent(d.exponent, *cur - '0');
    if (minus) d.exponent = -d.exponent;
  }

  // A number is a double first, because that is how Qt reads one. An integer literal is converted from its uint64 by the
  // compiler (correctly rounded, so one past 2^53 is rounded as Qt rounds it). A decimal point or an exponent goes through
  // Clinger's fast path: exact when the digits fit 2^53 and the power of ten fits a double, a stop with a reason otherwise.
  constexpr double to_double(decimal const& d) const {
    double const magnitude = static_cast<double>(d.mantissa);
    int const power = d.scale + d.exponent;
    if (d.inexact_form || d.overflow) {
      if (!exact_in_double(d, power))
        fail("json: this number needs a correctly rounded decimal conversion, which is not done: use at most 15 digits");
      return d.negative ? -scaled(magnitude, power) : scaled(magnitude, power);
    }
    return d.negative ? -magnitude : magnitude;
  }

  constexpr bool leading_zero() const { return *cur == '0' && cur + 1 < end && cur[1] >= '0' && cur[1] <= '9'; }

  constexpr double number() {
    decimal d;
    d.negative = peek('-');
    if (d.negative) ++cur;
    if (!digit_at()) fail("json: bad number");
    if (leading_zero()) fail("json: a number does not start with 0 and more digits");
    digits(d, false);
    if (peek('.')) fraction(d);
    if (peek('e') || peek('E')) exponent(d);
    return to_double(d);
  }

  // moc's CBOR has an integer where the number is integral and fits 64 bits (3.0 and 1e3 are written as 3 and 1000).
  constexpr void put_number(double d) {
    if (d > int64_min_as_double && d < int64_limit_as_double && static_cast<double>(static_cast<std::int64_t>(d)) == d) {
      std::int64_t const i = static_cast<std::int64_t>(d);
      if (i >= 0) end_head(begin_head(), unsigned_int, static_cast<std::uint64_t>(i));
      else end_head(begin_head(), negative_int, ~static_cast<std::uint64_t>(i));
      return;
    }
    put(cbor_double);
    auto const bits = std::bit_cast<std::uint64_t>(d);
    for (unsigned k = sizeof bits; k-- > 0;) put(static_cast<unsigned char>(bits >> (bits_per_byte * k)));
  }

  constexpr void enter() {
    if (++depth > max_nesting) fail("json: nested too deep");
  }

  constexpr void array_value() {
    enter();
    std::size_t const at = begin_head();
    std::uint64_t count = 0;
    if (!eat(']')) {
      do {
        value();
        ++count;
      } while (eat(','));
      expect(']', "json: ] or , expected in an array");
    }
    end_head(at, array, count);
    --depth;
  }

  // The order of two keys' bytes, which is the order QJsonObject keeps its keys in: negative, zero for a repeated key, positive.
  constexpr int compare_keys(workspace::member const& a, workspace::member const& b) const {
    if (a.prefix != b.prefix) return a.prefix < b.prefix ? -1 : 1;
    std::size_t const la = a.key_to - a.key_from;
    std::size_t const lb = b.key_to - b.key_from;
    int const order = compare_bytes(a.key_from, b.key_from, la < lb ? la : lb);
    return order != 0 || la == lb ? order : la < lb ? -1 : 1;
  }
  constexpr int compare_bytes(std::size_t a, std::size_t b, std::size_t count) const {
    for (std::size_t k = 0; k < count; ++k)
      if (w.out_[a + k] != w.out_[b + k]) return w.out_[a + k] < w.out_[b + k] ? -1 : 1;
    return 0;
  }
  constexpr bool before(workspace::member const& a, workspace::member const& b) const {  // the file order breaks a tie
    int const order = compare_keys(a, b);
    return order != 0 ? order < 0 : a.from < b.from;
  }

  constexpr std::uint64_t key_prefix(text_at key, std::size_t key_to) const {
    std::uint64_t prefix = 0;
    for (unsigned k = 0; k < prefix_bytes; ++k)
      prefix = (prefix << bits_per_byte) | (key.payload + k < key_to ? w.out_[key.payload + k] : 0);
    return prefix;
  }

  // The members of an object, each key and value written where it is read, their places kept on the member stack.
  constexpr void read_members() {
    do {
      skip_space();
      text_at const key = string();
      std::size_t const key_to = size();
      expect(':', "json: : expected after a key");
      value();
      if (member_top >= w.member_cap_) fail("rqt: more members than the buffer holds");
      w.members_[member_top++] = {
          .key_from = key.payload, .key_to = key_to, .from = key.from, .to = size(), .prefix = key_prefix(key, key_to)};
    } while (eat(','));
  }

  constexpr bool keys_ascending(std::size_t base) const {
    for (std::size_t k = base + 1; k < member_top; ++k)
      if (compare_keys(w.members_[k - 1], w.members_[k]) >= 0) return false;
    return true;
  }

  // Puts the members from `base` in key order and drops all but the last of a repeated key; returns how many are left.
  constexpr std::size_t sort_members(std::size_t base) {
    std::sort(w.members_ + base, w.members_ + member_top, [this](auto const& a, auto const& b) { return before(a, b); });
    std::size_t kept = 0;  // a repeated key is a run now, and the last of the run was the last in the file
    for (std::size_t k = base; k < member_top; ++k)
      if (k + 1 == member_top || compare_keys(w.members_[k], w.members_[k + 1]) != 0) w.members_[base + kept++] = w.members_[k];
    return kept;
  }

  // Writes the `kept` members from `base` after the head at `at`, in the order they are listed.
  constexpr void rewrite_members(std::size_t at, std::size_t base, std::size_t kept) {
    for (std::size_t k = at + 1; k < size(); ++k) w.scratch_[k] = w.out_[k];
    w.size_ = at + 1;
    for (std::size_t k = base; k < base + kept; ++k)
      for (std::size_t b = w.members_[k].from; b < w.members_[k].to; ++b) put(w.scratch_[b]);
  }

  // QJsonObject keeps its keys sorted and a repeated key keeps its last value, and the CBOR moc writes follows that.
  constexpr void object_value() {
    enter();
    std::size_t const at = begin_head();
    std::size_t const base = member_top;
    if (!eat('}')) {
      read_members();
      expect('}', "json: } or , expected in an object");
    }
    std::size_t kept = member_top - base;
    if (!keys_ascending(base)) {  // keys ascending and none repeated: the members stay where they are
      kept = sort_members(base);
      rewrite_members(at, base, kept);
    }
    end_head(at, map, kept);
    member_top = base;
    --depth;
  }

  constexpr void scalar() {
    if (eat_word("true")) put(cbor_true);
    else if (eat_word("false")) put(cbor_false);
    else if (eat_word("null")) put(cbor_null);
    else put_number(number());
  }

  constexpr void value() {
    skip_space();
    if (at_end()) fail("json: a value was expected");
    switch (*cur) {
      case '{': ++cur; return object_value();
      case '[': ++cur; return array_value();
      case '"': string(); return;
      default: return scalar();
    }
  }
};

// The file of a Q_PLUGIN_METADATA, or none: `file_of()` is no FILE, `file_of(array)` is the embedded array.
struct json_file {
  std::span<unsigned char const> data;
  bool present;
};
constexpr json_file file_of() { return {.data = {}, .present = false}; }
template <std::size_t N>
constexpr json_file file_of(unsigned char const (&array)[N]) {
  static_assert(N <= RQT_PLUGIN_JSON_MAX_BYTES,
                "rqt: the plugin JSON is larger than RQT_PLUGIN_JSON_MAX_BYTES, the most one constant evaluation turns into CBOR "
                "under the default -fconstexpr-ops-limit; raise that flag and define RQT_PLUGIN_JSON_MAX_BYTES to match");
  return {.data = array, .present = true};
}

// What moc writes for the V2 section: an indefinite map with IID (2), className (3) and, when there is a file, MetaData (4).
constexpr workspace plugin_cbor(json_file const& file, std::string_view iid, std::string_view class_name) {
  workspace w{file.data.size(), iid.size() + class_name.size()};
  encoder e{.w = w, .begin = file.data.data(), .cur = file.data.data(), .end = file.data.data() + file.data.size()};
  e.put(cbor_indefinite_map);
  e.put_unsigned(key_iid);
  e.put_text(iid);
  e.put_unsigned(key_class_name);
  e.put_text(class_name);
  if (file.present) {
    e.put_unsigned(key_meta_data);
    e.skip_space();
    if (!e.peek('{')) e.fail("json: the plugin metadata must be an object");
    e.value();
    e.skip_space();
    if (!e.at_end()) e.fail("json: characters after the end of the value");
  }
  e.put(cbor_break);
  return w;
}

}  // namespace detail::plugin

}  // namespace rqt

// The metadata CBOR of a plugin as an array of exactly its size, which is what QT_MOC_EXPORT_PLUGIN_V2 takes by reference.
#define RQT_DETAIL_PLUGIN_ARRAY(Name, iid, ...)                                                                    \
  [: ::std::meta::reflect_constant_array(                                                                           \
         ::rqt::detail::plugin::plugin_cbor(::rqt::detail::plugin::file_of(__VA_ARGS__), iid, #Name)) :]

// Q_PLUGIN_METADATA(IID iid FILE "x.json") for a plugin class `Class` in the global namespace; the last argument is the array
// of the embedded file (see the top of this file), left out for a plugin with no FILE.
#define RQT_PLUGIN_METADATA(Class, iid, ...) RQT_PLUGIN_METADATA_AS(Class, Class, iid __VA_OPT__(, ) __VA_ARGS__)

// The same for a class in a namespace: `Qualified` is the name as it is written there (app::Plugin), `Name` its last
// component, which moc puts in className and in the names of a static plugin.
#define RQT_PLUGIN_METADATA_AS(Qualified, Name, iid, ...)                                                      \
  static constexpr auto& qt_pluginMetaDataV2_##Name = RQT_DETAIL_PLUGIN_ARRAY(Name, iid __VA_OPT__(, ) __VA_ARGS__); \
  QT_MOC_EXPORT_PLUGIN_V2(Qualified, Name, qt_pluginMetaDataV2_##Name)
