// loads a plugin library with QPluginLoader and prints what a host sees
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QPluginLoader>
#include <cstdio>
#include "plugin_iface.h"
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QPluginLoader loader(argv[1]);
  QJsonObject md = loader.metaData();
  std::printf("%s\n", QJsonDocument(md).toJson(QJsonDocument::Compact).constData());
  QObject* o = loader.instance();
  std::printf("load: %d (%s)\n", int(o != nullptr), qPrintable(loader.errorString()));
  if (!o) return 1;
  auto* i = qobject_cast<IDemo*>(o);
  std::printf("class=%s qobject_cast<IDemo*>=%d answer=%d\n", o->metaObject()->className(), int(i != nullptr), i ? i->answer() : -1);
  std::printf("inherits IDemo iid: %d\n", int(o->qt_metacast(IDemo_iid) != nullptr));
  return 0;
}
