// Hands Q_OBJECT and the other macros back to Qt after reflect_moc/compat.hpp.
#pragma once

#pragma pop_macro("Q_OBJECT")
#pragma pop_macro("Q_PROPERTY")
#pragma pop_macro("Q_INVOKABLE")
#pragma pop_macro("Q_ENUM")
#pragma pop_macro("Q_FLAG")
#pragma pop_macro("Q_CLASSINFO")
#pragma pop_macro("emit")
#pragma pop_macro("Q_EMIT")
