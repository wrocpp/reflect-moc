// Limit: the observable container refuses a value type whose move can throw. A throw between the "about" and
// the "done" signal would leave every model mid-change, and a Qt container cannot be prepared before the
// announcement. This file must NOT compile with
//   static assertion failed: ... the value type must be nothrow move constructible and assignable
#include <container_model.hpp>

#include <QtCore/QList>

struct ThrowingMove {
  ThrowingMove() = default;
  ThrowingMove(const ThrowingMove&) = default;
  ThrowingMove(ThrowingMove&&) noexcept(false) {}
  ThrowingMove& operator=(const ThrowingMove&) = default;
  ThrowingMove& operator=(ThrowingMove&&) noexcept(false) { return *this; }
};

observable::ObservableContainer<QList<ThrowingMove>> refused;
