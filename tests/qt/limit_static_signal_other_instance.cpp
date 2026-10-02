// Limit (runtime, and silent): a static signal is one object for the whole class, so
// `emit other->valueChanged(9)` discards `other` and fires on `this`, the object that runs `emit`. Qt's
// own signals fire on `other`. The correct form is `valueChanged(9).from(other)`. This test pins the
// pitfall; tools/rqt-lint-emit.py flags the line.
#include "check.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>

#include <reflect_moc/compat.hpp>

namespace {
constexpr int kValue = 9;
constexpr int kNone = 0;
}  // namespace

class Node : public QObject {
  Q_OBJECT

 public:
  explicit Node(QObject* parent = nullptr) : QObject(parent) {}
  void tellOtherWrongly(Node* other, int v) { emit other->valueChanged(v); }  // fires on this
  void tellOther(Node* other, int v) { valueChanged(v).from(other); }         // fires on other

 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Node me;
  Node other;
  int on_me = kNone;
  int on_other = kNone;
  CHECK(QObject::connect(&me, &Node::valueChanged, &me, [&](int v) { on_me = v; }));
  CHECK(QObject::connect(&other, &Node::valueChanged, &other, [&](int v) { on_other = v; }));

  me.tellOtherWrongly(&other, kValue);
  CHECK(on_me == kValue);     // the pitfall: it fired on `me`
  CHECK(on_other == kNone);   // and `other` heard nothing

  on_me = kNone;
  me.tellOther(&other, kValue);
  CHECK(on_other == kValue);  // the explicit form fires on `other`
  CHECK(on_me == kNone);
  return rqt_test::finish("limit_static_signal_other_instance");
}
