// reflect-moc behaviour test, not part of the upstream example.
//
// The upstream main.cpp, made finite: it loads the People module's Main.qml
// headless, prints the guest list the same way, then advances the
// HappyBirthdaySong value source by invoking its private slot `advance` by
// name (instead of waiting for its one-second timer) and starts the party.
// QML's console.log of the start time is masked, so the output is stable.
#include "birthdayparty.h"
#include "happybirthdaysong.h"
#include "meta_dump.hpp"
#include "person.h"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>

#include <cstdio>
#include <memory>

namespace {

constexpr int song_lines = 5;

void to_stdout(QtMsgType, QMessageLogContext const&, QString const& message) {
  QString line = message;
  qsizetype at = line.indexOf(u"rockin' at ");
  if (at >= 0) line = line.left(at) + u"rockin' at <time>";
  std::printf("%s\n", line.toUtf8().constData());
  std::fflush(stdout);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  qInstallMessageHandler(to_stdout);
  for (QMetaObject const* mo : {&ShoeDescription::staticMetaObject, &Person::staticMetaObject,
                                &Boy::staticMetaObject, &Girl::staticMetaObject,
                                &BirthdayPartyAttached::staticMetaObject, &BirthdayParty::staticMetaObject,
                                &HappyBirthdaySong::staticMetaObject})
    harness::dump_meta(mo);

  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.loadFromModule("People", "Main");
  std::unique_ptr<BirthdayParty> party{qobject_cast<BirthdayParty*>(component.create())};
  if (!party || !party->host()) {
    qWarning() << component.errors();
    return EXIT_FAILURE;
  }

  qInfo() << party->host()->name() << "is having a birthday!";
  if (qobject_cast<Boy*>(party->host()))
    qInfo() << "He is inviting:";
  else
    qInfo() << "She is inviting:";
  for (qsizetype ii = 0; ii < party->guestCount(); ++ii) {
    Person* guest = party->guest(ii);
    QDate rsvpDate;
    QObject* attached = qmlAttachedPropertiesObject<BirthdayParty>(guest, false);
    if (attached) rsvpDate = attached->property("rsvp").toDate();
    if (rsvpDate.isNull())
      qInfo() << "   " << guest->name() << "RSVP date: Hasn't RSVP'd";
    else
      qInfo() << "   " << guest->name() << "RSVP date:" << rsvpDate.toString(Qt::ISODate);
    qInfo() << "    shoe:" << guest->shoe()->size() << guest->shoe()->color().name() << guest->shoe()->brand()
            << guest->shoe()->price();
  }

  auto* song = party->findChild<HappyBirthdaySong*>();
  qInfo() << "song for" << (song ? song->name() : QStringLiteral("<no song>"));
  for (int i = 0; song && i < song_lines; ++i)
    if (!QMetaObject::invokeMethod(song, "advance")) qWarning() << "invokeMethod advance FAILED";

  party->startParty();
  return 0;
}
