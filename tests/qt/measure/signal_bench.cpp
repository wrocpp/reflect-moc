// An emit micro-benchmark: a signal connected to one functor, emitted a few million times.
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

#include <chrono>
#include <cstdio>

class Ticker : public QObject {
  RQT_OBJECT

 public:
  explicit Ticker(QObject* parent = nullptr) : QObject(parent) {}
  void run(int n) {
    for (int i = 0; i < n; ++i) tick(i);
  }

 signals:
  rqt::signal<void(int)> tick;
  rqt::signal<void(int)> tock;
  rqt::signal<void()> done;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  constexpr int kEmits = 5'000'000;
  Ticker ticker;
  long sum = 0;
  QObject::connect(&ticker, &Ticker::tick, &ticker, [&](int v) { sum += v; });
  auto const start = std::chrono::steady_clock::now();
  ticker.run(kEmits);
  auto const elapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
  std::printf("sizeof(Ticker)=%zu emit=%.1f ns (sum=%ld)\n", sizeof(Ticker), elapsed / kEmits, sum);
}
