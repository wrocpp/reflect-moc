// Can reflection name a member of a d-pointer class that is incomplete where the public header is read?
#include <meta>
class WidgetPrivate;  // as in widget.h
#ifdef ASK_MEMBERS
constexpr auto n = std::meta::members_of(^^WidgetPrivate, std::meta::access_context::unchecked()).size();
#endif
#ifdef ASK_COMPLETE
constexpr bool c = std::meta::is_complete_type(^^WidgetPrivate);
static_assert(!c);
#endif
class WidgetPrivate { public: void onTick(int) {} };  // as in widget_p.h
#ifdef AFTER
consteval bool has_on_tick() {
  for (auto m : std::meta::members_of(^^WidgetPrivate, std::meta::access_context::unchecked()))
    if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == "onTick") return true;
  return false;
}
static_assert(has_on_tick());  // once the class is complete, the member is nameable
#endif
int main() {}
