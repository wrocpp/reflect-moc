// The oracle: namespaces with Q_NAMESPACE (one nested), an enum class with Q_ENUM_NS, a plain enum with a QFlags alias and
// Q_FLAG_NS, and a class with properties of those types (the related meta-object), processed by moc.
#pragma once

#include <QtCore/QMetaEnum>
#include <QtCore/QObject>

namespace viamoc {
namespace Shapes {
Q_NAMESPACE
enum class Mode { off, on = 5 };
Q_ENUM_NS(Mode)
enum Edge { Left = 1, Right = 2, Top = 4 };
Q_DECLARE_FLAGS(Edges, Edge)
Q_FLAG_NS(Edges)

class Holder : public QObject {
  Q_OBJECT
  Q_PROPERTY(viamoc::Shapes::Mode mode MEMBER mode)
  Q_PROPERTY(viamoc::Shapes::Edges edges MEMBER edges)

 public:
  Mode mode = Mode::off;
  Edges edges;
};

namespace Inner {
Q_NAMESPACE
enum class Level { low, high };
Q_ENUM_NS(Level)
}  // namespace Inner
}  // namespace Shapes
}  // namespace viamoc
