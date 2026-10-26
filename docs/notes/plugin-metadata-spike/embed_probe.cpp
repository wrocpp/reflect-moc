// does GCC 16.2 have #embed, in C++26 mode, and does the data reach constant evaluation?
#include <cstddef>
#if defined(__has_embed)
#  if __has_embed("meta.json") == __STDC_EMBED_FOUND__
static_assert(true, "__has_embed finds the file");
#  else
#    error "__has_embed does not find meta.json"
#  endif
#else
#  error "no __has_embed"
#endif
static constexpr unsigned char json[] = {
#embed "meta.json"
};
static_assert(sizeof json > 50);
static_assert(json[0] == '{');
consteval std::size_t count_quotes() { std::size_t n = 0; for (unsigned char c : json) n += c == '"'; return n; }
static constexpr std::size_t quotes = count_quotes();  // read in a constant expression
static_assert(quotes == 22);
// the form with an appended NUL and the limit() parameter
static constexpr char text[] = {
#embed "meta.json" suffix(, 0)
};
static_assert(text[sizeof text - 1] == 0);
static constexpr unsigned char head[] = {
#embed "meta.json" limit(4)
};
static_assert(sizeof head == 4);
#ifdef EMBED_MACRO
// #embed inside a macro argument list is not a directive
#define WRAP(...) static constexpr unsigned char wrapped[] = {__VA_ARGS__};
WRAP(
#embed "meta.json"
)
#endif
int main() {}
