// Pointer-typed properties, signal arguments and slot parameters (Person*,
// const Person*, QObject*): written by name, usable through string connect, a
// queued connection across a QThread and qobject_cast on the pointee.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>

#include <atomic>

struct Person : rqt::Object<> {
  static QMetaObject const& staticMetaObject;
  Person() { bind(); }
  [[= rqt::property{}]] QString name;
};
RQT_STATIC_META_OBJECT(Person);

struct Party : rqt::Object<> {
  Party() { bind(); }

  [[= rqt::property{.write = "setHost", .notify = "hostChanged"}]] Person* host() const { return host_; }
  [[= rqt::property{.write = "setAnyone"}]] QObject* anyone() const { return anyone_; }

  [[= rqt::signal_function]] void hostChanged(Person* host) { rqt::emit{this}(host); }
  [[= rqt::signal_function]] void guestArrived(Person* guest) { rqt::emit{this}(guest); }
  [[= rqt::signal_function]] void guestSeen(Person const* guest) { rqt::emit{this}(guest); }

  [[= rqt::slot]] void setHost(Person* h) {
    host_ = h;
    hostChanged(h);
  }
  [[= rqt::slot]] void setAnyone(QObject* o) { anyone_ = o; }
  [[= rqt::slot]] void welcome(Person const* guest) { welcomed_ = guest; }

  Person* host_ = nullptr;
  QObject* anyone_ = nullptr;
  Person const* welcomed_ = nullptr;
};

struct Doorman : rqt::Object<> {
  Doorman() { bind(); }
  std::atomic<Person*> seen{nullptr};
  std::atomic<QThread*> thread_seen{nullptr};
  QSemaphore done;
  [[= rqt::slot]] void greet(Person* guest) {
    seen = guest;
    thread_seen = QThread::currentThread();
    done.release();
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Person alice;
  Party party;
  QMetaObject const* mo = party.metaObject();

  // the table: pointer types are written by name
  CHECK(mo->indexOfSignal("guestArrived(Person*)") >= 0);
  CHECK(mo->indexOfSlot("welcome(const Person*)") >= 0);
  QMetaProperty const host = mo->property(mo->indexOfProperty("host"));
  CHECK(host.isValid() && QByteArray{host.typeName()} == "Person*");

  // property and slot by name
  CHECK(host.write(&party, QVariant::fromValue(&alice)) && party.host_ == &alice);
  QObject* read_back = host.read(&party).value<QObject*>();
  CHECK(qobject_cast<Person*>(read_back) == &alice);
  CHECK(party.setProperty("anyone", QVariant::fromValue(static_cast<QObject*>(&alice))) && party.anyone_ == &alice);
  CHECK(QMetaObject::invokeMethod(&party, "welcome", Q_ARG(Person const*, &alice)) && party.welcomed_ == &alice);

  // queued across a thread
  Doorman doorman;
  QThread thread;
  doorman.moveToThread(&thread);
  thread.start();
  CHECK(QObject::connect(&party, SIGNAL(guestArrived(Person*)), &doorman, SLOT(greet(Person*)),
                         Qt::QueuedConnection));
  party.guestArrived(&alice);
  doorman.done.acquire();
  CHECK(doorman.seen == &alice && doorman.thread_seen == &thread);
  thread.quit();
  thread.wait();
  return rqt_test::finish("capability_pointer_typed_property_and_signal_argument");
}
