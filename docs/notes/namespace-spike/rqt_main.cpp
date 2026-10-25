#include "namespace_meta_experiment.hpp"
#include "probe_common.hpp"

namespace Outer::N {
enum class [[= rqt::enum_]] E { a, b };
enum [[= rqt::enum_]] Plain { x = 1, y = 2 };
using nsx::qt_getEnumMetaObject;
using nsx::qt_getEnumName;
inline QMetaObject const& staticMetaObject = nsx::ns_meta<>;
}  // namespace Outer::N

namespace Outer::N {
// a class in the namespace: hand-rolled QObject plumbing, but the library's tables, plus the extradata link moc writes
class Holder;
}
namespace nsx {
template <class D>
inline constexpr QMetaObject::SuperData extradata_of[] = {QMetaObject::SuperData::link<Outer::N::staticMetaObject>(), QMetaObject::SuperData{}};
template <class D>
inline constexpr QMetaObject holder_meta = {{QMetaObject::SuperData::link<QObject::staticMetaObject>(), rqt::detail::meta_content<D>.staticData.stringdata,
                                              rqt::detail::meta_content<D>.staticData.data, &rqt::detail::static_metacall<D>, extradata_of<D>,
                                              rqt::detail::meta_content<D>.relocatingData.metaTypes, nullptr}};
}
namespace Outer::N {
class Holder : public QObject {
 public:
  [[= rqt::property{}]] E e = E::a;
  QMetaObject const* metaObject() const override { return &nsx::holder_meta<Holder>; }
  int qt_metacall(QMetaObject::Call c, int id, void** a) override {
    id = QObject::qt_metacall(c, id, a);
    if (id < 0) return id;
    if (c == QMetaObject::ReadProperty || c == QMetaObject::WriteProperty || c == QMetaObject::ResetProperty || c == QMetaObject::RegisterPropertyMetaType) {
      nsx::holder_meta<Holder>.d.static_metacall(this, c, id, a);
      id -= 1;
    }
    return id;
  }
};
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Outer::N::Holder h;
  probe<Outer::N::E>(Outer::N::staticMetaObject, nsx::holder_meta<Outer::N::Holder>, h, "");
}
