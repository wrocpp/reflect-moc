// Deducing-this research: how far can a NON-template base get without CRTP?
// E1 deducing-this API sees the most-derived type (reflection on Self)
// E2 base ctor template taking `this` from the derived mem-initializer
// E3 bind(this) in each ctor body: the most-derived ctor runs last and wins
// E4 zero-line registration: scan a namespace with members_of, RTTI lookup
#include <meta>
#include <cstdio>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace rqt {

struct class_info {
    char const* name;
    int n_members;
};

template <class T>
consteval class_info make_info() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    return {std::define_static_string(std::meta::identifier_of(^^T)),
            static_cast<int>(std::meta::nonstatic_data_members_of(^^T, ctx).size())};
}
template <class T>
inline constexpr class_info info_for = make_info<T>();

// registry for E4
inline std::unordered_map<std::type_index, class_info const*>& registry() {
    static std::unordered_map<std::type_index, class_info const*> r;
    return r;
}

struct Object {  // non-template base, stands in for rqt::QObject : ::QObject
    virtual ~Object() = default;

    // E2: the derived class passes `this`; Self is complete in a mem-initializer
    Object() = default;
    template <class Self>
    explicit Object(Self*) : info_(&info_for<Self>) {}

    // E3: bind from a ctor body; most-derived ctor body runs last
    template <class Self>
    void bind(this Self& self) { self.info_ = &info_for<Self>; }

    // E1: deducing this sees the static most-derived type at the call site
    template <class Self>
    char const* static_name(this Self const&) { return info_for<Self>.name; }

    // what a virtual hook (metaObject()) would do: runtime lookup
    virtual char const* dynamic_name() const {
        if (!info_) {  // E4 fallback: RTTI lookup in the namespace registry
            auto it = registry().find(typeid(*this));
            if (it != registry().end()) info_ = it->second;
        }
        return info_ ? info_->name : "Object";
    }

    mutable class_info const* info_ = nullptr;
};

// E4: register every class in a namespace that derives from Object
template <std::meta::info Ns>
bool register_namespace() {
    template for (constexpr auto m : std::define_static_array(std::meta::members_of(Ns, std::meta::access_context::unchecked()))) {
        if constexpr (std::meta::is_class_type(m) && std::meta::is_complete_type(m)) {
            using T = [:m:];
            if constexpr (std::is_base_of_v<Object, T> && !std::is_same_v<Object, T>)
                registry()[typeid(T)] = &info_for<T>;
        }
    }
    return true;
}

}  // namespace rqt

namespace app {
struct Worker : rqt::Object {
    int progress = 0;
};
struct Viaccess : rqt::Object {  // E2 style
    Viaccess() : rqt::Object(this) {}
    int a = 0, b = 0;
};
struct Bound : rqt::Object {  // E3 style
    Bound() { bind(); }
    int x = 0;
};
struct SubBound : Bound {
    SubBound() { bind(); }
    int y = 0, z = 0;
};
}  // namespace app

// E4: one line per program (or per namespace), zero lines per class
static bool const app_registered = rqt::register_namespace<^^app>();

int main() {
    app::Worker w;
    app::Viaccess v;
    app::SubBound sb;
    rqt::Object& ref = sb;

    std::printf("E1 static_name(w)=%s static_name(ref)=%s\n", w.static_name(), ref.static_name());
    std::printf("E2 Viaccess dynamic=%s members=%d\n", v.dynamic_name(), v.info_->n_members);
    std::printf("E3 SubBound via base ref dynamic=%s members=%d\n", ref.dynamic_name(), sb.info_->n_members);
    rqt::Object& wref = w;
    std::printf("E4 Worker (no code in class) dynamic=%s registered=%d\n", wref.dynamic_name(), app_registered);
}
