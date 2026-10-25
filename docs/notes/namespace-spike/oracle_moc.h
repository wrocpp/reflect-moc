#pragma once
#include <QObject>
#include <QMetaEnum>
namespace Outer { namespace N {
Q_NAMESPACE
enum class E { a, b };
Q_ENUM_NS(E)
enum Plain { x = 1, y = 2 };
Q_FLAG_NS(Plain)
class Holder : public QObject { Q_OBJECT Q_PROPERTY(Outer::N::E e MEMBER e) public: E e = E::a; };
} }
