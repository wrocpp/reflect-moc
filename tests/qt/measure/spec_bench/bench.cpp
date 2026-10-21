// The same source, built once against binding A (moc, QVariant keys) and once against binding B (typed signals)
// of the observable container spec, through the same obs:: API. It times what the spec adds to a plain
// container: per-entry changes, batches, an atomic reset, and a matrix block. Output is csv.
#include "obs/container.h"
#include "obs/container_list_model.h"
#include "obs/matrix.h"
#include "obs/matrix_table_model.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <tuple>

#ifdef BENCH_BINDING_A
#define BINDING "a"
#else
#define BINDING "b"
#endif

namespace {

struct Sample {
    quint32 id = 0;
    double value = 0;
};

using Clock = std::chrono::steady_clock;

double nsSince(Clock::time_point start) { return std::chrono::duration<double, std::nano>(Clock::now() - start).count(); }

template <class C>
class Model : public obs::ContainerListModel<C> {
public:
    using Base = obs::ContainerListModel<C>;
    using Base::Base;

protected:
    QVariant valueData(const typename Base::Key &, const typename Base::Value & v, int role) const override
    {
        return role == Qt::DisplayRole ? QVariant(v.value) : QVariant();
    }
};

void row(const char * container, std::size_t n, const char * op, double ns)
{
    std::printf("spec,%s,%s,%zu,%s,%.1f\n", BINDING, container, n, op, ns);
}

std::vector<quint32> shuffled(std::size_t n)
{
    std::vector<quint32> keys(n);
    std::iota(keys.begin(), keys.end(), 0u);
    std::mt19937 gen(12345);
    std::shuffle(keys.begin(), keys.end(), gen);
    return keys;
}

// Per entry: fill, replace in random order, drain in random order. ns per change.
template <class C>
void perEntry(const char * name, std::size_t n)
{
    constexpr bool isList = obs::SequentialContainer<C>;
    obs::Container<C> c;
    Model<C> model(c);
    const auto order = shuffled(n);

    auto t = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        if constexpr (isList)
            c.append(Sample{static_cast<quint32>(i), 1.0});
        else
            c.insert(static_cast<quint32>(i), Sample{static_cast<quint32>(i), 1.0});
    }
    row(name, n, "fill", nsSince(t) / static_cast<double>(n));

    t = Clock::now();
    for (std::size_t i = 0; i < n; ++i)
        c.replace(order[i] % static_cast<quint32>(n), Sample{order[i], 2.0});
    row(name, n, "replace", nsSince(t) / static_cast<double>(n));

    t = Clock::now();
    for (std::size_t i = 0; i < n; ++i) {
        if constexpr (isList)
            c.remove(static_cast<qsizetype>(order[i] % c.size()));
        else
            c.remove(order[i]);
    }
    row(name, n, "drain", nsSince(t) / static_cast<double>(n));
}

// Batches of k keys on an associative container whose rows are sorted. Existing keys are the even numbers.
template <class C>
void batches(const char * name, std::size_t n)
{
    constexpr std::size_t k = 100;
    constexpr int repeats = 20;
    C initial;
    for (std::size_t i = 0; i < n; ++i)
        initial.insert(static_cast<quint32>(2 * i), Sample{static_cast<quint32>(i), 1.0});
    obs::Container<C> c(initial);
    Model<C> model(c);

    const auto build = [&](auto keyOf) {
        C entries;
        QList<quint32> keys;
        for (std::size_t i = 0; i < k; ++i) {
            const quint32 key = keyOf(i);
            entries.insert(key, Sample{key, 3.0});
            keys.append(key);
        }
        return std::pair{entries, keys};
    };
    const auto stride = static_cast<quint32>(n / k);
    const auto scattered = build([&](std::size_t i) { return static_cast<quint32>(2 * i * stride + 1); });
    const auto contiguous = build([&](std::size_t i) { return static_cast<quint32>(2 * n + 2 * i + 1); });

    const auto run = [&](const char * shape, const auto & batch) {
        double ins = 0, rem = 0;
        for (int r = 0; r < repeats; ++r) {
            auto t = Clock::now();
            c.insertMany(batch.first);
            ins += nsSince(t);
            t = Clock::now();
            c.removeMany(batch.second);
            rem += nsSince(t);
        }
        row(name, n, (std::string("insertMany-") + shape).c_str(), ins / repeats);
        row(name, n, (std::string("removeMany-") + shape).c_str(), rem / repeats);
    };
    run("scattered", scattered);
    run("contiguous", contiguous);
}

template <class C>
void reset(const char * name, std::size_t n)
{
    constexpr int repeats = 20;
    C a, b;
    for (std::size_t i = 0; i < n; ++i) {
        a.insert(static_cast<quint32>(i), Sample{static_cast<quint32>(i), 1.0});
        b.insert(static_cast<quint32>(i + n), Sample{static_cast<quint32>(i), 2.0});
    }
    obs::Container<C> c(a);
    Model<C> model(c);
    const auto t = Clock::now();
    for (int r = 0; r < repeats; ++r)
        c.resetWith(r % 2 == 0 ? b : a);
    row(name, n, "resetWith", nsSince(t) / repeats);
}

void matrix()
{
    constexpr std::size_t side = 100;
    constexpr int repeats = 200;
    using M = obs::Matrix<quint32, quint32, double>;
    M m(std::numeric_limits<double>::quiet_NaN());
    obs::MatrixTableModel<M> model(m);
    QList<quint32> keys;
    for (quint32 i = 0; i < side; ++i)
        keys.append(i);
    m.addRows(keys);
    m.addColumns(keys);

    const auto cells = [&](auto place, double value) {
        QList<std::tuple<quint32, quint32, double>> out;
        for (std::size_t i = 0; i < side; ++i) {
            const auto [r, c] = place(i);
            out.append({r, c, value});
        }
        return out;
    };
    const auto block = [](std::size_t i) { return std::pair<quint32, quint32>(static_cast<quint32>(i / 10), static_cast<quint32>(i % 10)); };
    const auto diagonal = [](std::size_t i) { return std::pair<quint32, quint32>(static_cast<quint32>(i), static_cast<quint32>(i)); };
    const auto time = [&](const char * op, auto place) {
        const auto t = Clock::now();
        for (int r = 0; r < repeats; ++r)
            m.setValues(cells(place, static_cast<double>(r) + 1.0));
        row("Matrix100x100", side * side, op, nsSince(t) / repeats);
    };
    time("setValues-block-100-cells", block);
    time("setValues-diagonal-100-cells", diagonal);
}

} // namespace

int main(int argc, char ** argv)
{
    QCoreApplication app(argc, argv);
    std::printf("size,%s,Container<QHash>,%zu\n", BINDING, sizeof(obs::Container<QHash<quint32, Sample>>));
    std::printf("size,%s,Matrix,%zu\n", BINDING, sizeof(obs::Matrix<quint32, quint32, double>));
    for (const std::size_t n : {1000u, 10000u}) {
        perEntry<QHash<quint32, Sample>>("QHash", n);
        perEntry<QList<Sample>>("QList", n);
        batches<QMap<quint32, Sample>>("QMap", n);
        batches<QHash<quint32, Sample>>("QHash", n);
        reset<QMap<quint32, Sample>>("QMap", n);
    }
    matrix();
    return 0;
}
