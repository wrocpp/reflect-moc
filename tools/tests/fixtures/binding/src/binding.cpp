#include "binding.h"

#include <QCoreApplication>

Owner::Owner(QObject *parent)
    : QObject(parent)
{
}

Owner::Owner(int first, int second) : QObject(nullptr) { (void)first; (void)second; }

Plain *Holder::plain() const { return nullptr; }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Plain plain;
    Casted *casted = qobject_cast<Casted *>(&plain);
    Linked linked;
    QObject::connect(&linked, &Linked::changed, &plain, [] {});
    const QMetaObject *mo = &Meta::staticMetaObject;
    (void)casted;
    (void)mo;
    return 0;
}
