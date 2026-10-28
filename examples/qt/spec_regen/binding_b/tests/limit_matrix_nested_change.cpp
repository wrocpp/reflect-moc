// A receiver of a signal of a matrix changes the same matrix while the announcement is still going out. Addendum 1 of the
// spec queues such a change for the containers and leaves the matrix out ("there is no test that shows the failure").
// This file is that test: each case runs the nested change in a child process, with the table model and a
// QAbstractItemModelTester attached before the first mutation, and says how the child ended.
//
//   consistent   the child exited 0: what a view reads from the model's signals alone equals the matrix
//   out of step  the child exited 1: the tester said nothing, but the model or that view differs from the matrix
//   crashed      the child died on a signal: QAbstractItemModelTester (fatal mode) or an assertion refused the sequence
//
// Every case has a control, the same two changes one after the other with no receiver, which must end consistent. The
// expected class of each nested case is pinned in cases() below: while the matrix has no queue, a case that does not end
// consistent is the failure. The pin passes while the cases reproduce and fails the day a queue changes one, so that
// change is made on purpose. The classes are the same in a Debug and a Release build, in both bindings.
//
// What the 32 cases (16 changes, the receiver connected after and before the model) show:
//   - from an "about" signal, receiver after the model: every case crashes, the tester sees a change already in flight;
//   - from a "done" signal, receiver before the model: every case crashes, for the same reason;
//   - from a "done" signal, receiver after the model: every case ends consistent (the model has finished its change);
//   - from an "about" signal, receiver before the model, adding: out of step. The model's size and headers match, but
//     the outer change announces the index it took before the receiver ran, and the nested add has moved it;
//   - from an "about" signal, receiver before the model, removing the first entry: crashes in the matrix itself, whose
//     outer call erases at the index it took before the "about" (an assertion in Debug, a signal in Release).
// POSIX only (it forks); the build registers it where UNIX is true.
#include <obs/matrix.h>
#include <obs/matrix_table_model.h>

#include <reflect_moc/qt/qt.hpp>

#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QTest>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Sheet = obs::Matrix<QString, QString, double>;
using Model = obs::MatrixTableModel<Sheet>;
using Hook = std::function<void(Sheet &, QObject &, std::function<void()>)>;
using Change = std::function<void(Sheet &)>;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr unsigned kChildSeconds = 20;  // a child that loops is killed by SIGALRM and counts as crashed
constexpr int kExitConsistent = 0;
constexpr int kExitOutOfStep = 1;
constexpr int kExitHarness = 2;  // the child could not even set up its output

enum class End { consistent, outOfStep, crashed };

struct Case {
    const char * name;
    Hook connect;  // connects the receiver to the signal under test
    Change outer;  // the change whose announcement the receiver sits in
    Change nested;  // what the receiver does
    End afterModel;  // how the nested case ends when the receiver was connected after the model (pinned)
    End beforeModel;  // and when it was connected before the model
};

// A receiver that fires once, whatever the arity of the signal.
template <class Signal>
Hook on(Signal signal)
{
    return [signal](Sheet & m, QObject & context, std::function<void()> f) { QObject::connect(&m, signal, &context, std::move(f)); };
}

std::vector<Case> cases()
{
    return {
        {"row added, in rowAboutToBeAdded, adds a row", on(&Sheet::rowAboutToBeAdded), [](Sheet & m) { m.addRow("x"); }, [](Sheet & m) { m.addRow("y"); }, End::crashed, End::outOfStep},
        {"row added, in rowAdded, adds a row", on(&Sheet::rowAdded), [](Sheet & m) { m.addRow("x"); }, [](Sheet & m) { m.addRow("y"); }, End::consistent, End::crashed},
        {"rows added, in rowsAboutToBeAdded, adds a row", on(&Sheet::rowsAboutToBeAdded), [](Sheet & m) { m.addRows({"x", "y"}); }, [](Sheet & m) { m.addRow("z"); }, End::crashed, End::outOfStep},
        {"rows added, in rowsAdded, adds a row", on(&Sheet::rowsAdded), [](Sheet & m) { m.addRows({"x", "y"}); }, [](Sheet & m) { m.addRow("z"); }, End::consistent, End::crashed},
        {"row removed, in rowAboutToBeRemoved, removes the first row", on(&Sheet::rowAboutToBeRemoved), [](Sheet & m) { m.removeRow("r2"); }, [](Sheet & m) { m.removeRow("r0"); }, End::crashed, End::crashed},
        {"row removed, in rowRemoved, removes the first row", on(&Sheet::rowRemoved), [](Sheet & m) { m.removeRow("r2"); }, [](Sheet & m) { m.removeRow("r0"); }, End::consistent, End::crashed},
        {"rows removed, in rowsAboutToBeRemoved, removes the first row", on(&Sheet::rowsAboutToBeRemoved), [](Sheet & m) { m.removeRows({"r1", "r2"}); }, [](Sheet & m) { m.removeRow("r0"); }, End::crashed, End::crashed},
        {"rows removed, in rowsRemoved, removes the first row", on(&Sheet::rowsRemoved), [](Sheet & m) { m.removeRows({"r1", "r2"}); }, [](Sheet & m) { m.removeRow("r0"); }, End::consistent, End::crashed},
        {"column added, in columnAboutToBeAdded, adds a column", on(&Sheet::columnAboutToBeAdded), [](Sheet & m) { m.addColumn("x"); }, [](Sheet & m) { m.addColumn("y"); }, End::crashed, End::outOfStep},
        {"column added, in columnAdded, adds a column", on(&Sheet::columnAdded), [](Sheet & m) { m.addColumn("x"); }, [](Sheet & m) { m.addColumn("y"); }, End::consistent, End::crashed},
        {"columns added, in columnsAboutToBeAdded, adds a column", on(&Sheet::columnsAboutToBeAdded), [](Sheet & m) { m.addColumns({"x", "y"}); }, [](Sheet & m) { m.addColumn("z"); }, End::crashed, End::outOfStep},
        {"columns added, in columnsAdded, adds a column", on(&Sheet::columnsAdded), [](Sheet & m) { m.addColumns({"x", "y"}); }, [](Sheet & m) { m.addColumn("z"); }, End::consistent, End::crashed},
        {"column removed, in columnAboutToBeRemoved, removes the first column", on(&Sheet::columnAboutToBeRemoved), [](Sheet & m) { m.removeColumn("c2"); }, [](Sheet & m) { m.removeColumn("c0"); }, End::crashed, End::crashed},
        {"column removed, in columnRemoved, removes the first column", on(&Sheet::columnRemoved), [](Sheet & m) { m.removeColumn("c2"); }, [](Sheet & m) { m.removeColumn("c0"); }, End::consistent, End::crashed},
        {"columns removed, in columnsAboutToBeRemoved, removes the first column", on(&Sheet::columnsAboutToBeRemoved), [](Sheet & m) { m.removeColumns({"c1", "c2"}); }, [](Sheet & m) { m.removeColumn("c0"); }, End::crashed, End::crashed},
        {"columns removed, in columnsRemoved, removes the first column", on(&Sheet::columnsRemoved), [](Sheet & m) { m.removeColumns({"c1", "c2"}); }, [](Sheet & m) { m.removeColumn("c0"); }, End::consistent, End::crashed},
    };
}

// Does the model show the matrix: the same size, and the same key at every header.
bool modelFollows(const Sheet & m, const Model & model)
{
    if (model.rowCount() != m.rowCount() || model.columnCount() != m.columnCount())
        return false;
    for (int i = 0; i < model.rowCount(); ++i)
        if (model.headerData(i, Qt::Vertical).toString() != m.rowKeys()[i])
            return false;
    for (int j = 0; j < model.columnCount(); ++j)
        if (model.headerData(j, Qt::Horizontal).toString() != m.columnKeys()[j])
            return false;
    return true;
}

// What a view holds that follows the model from its signals alone: the keys it read at each inserted position and the
// ranges it dropped. An announcement that names the wrong position leaves it different from the matrix.
class Mirror {
public:
    Mirror(const Sheet & m, Model & model) : rows_(m.rowKeys()), columns_(m.columnKeys())
    {
        QObject::connect(&model, &QAbstractItemModel::rowsInserted, &context_, [this, &model](const QModelIndex &, int first, int last) { insert(rows_, first, last, [&](int i) { return model.headerData(i, Qt::Vertical).toString(); }); });
        QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &context_, [this](const QModelIndex &, int first, int last) { drop(rows_, first, last); });
        QObject::connect(&model, &QAbstractItemModel::columnsInserted, &context_, [this, &model](const QModelIndex &, int first, int last) { insert(columns_, first, last, [&](int i) { return model.headerData(i, Qt::Horizontal).toString(); }); });
        QObject::connect(&model, &QAbstractItemModel::columnsRemoved, &context_, [this](const QModelIndex &, int first, int last) { drop(columns_, first, last); });
    }

    bool equals(const Sheet & m) const { return rows_ == m.rowKeys() && columns_ == m.columnKeys(); }

private:
    template <class Read>
    static void insert(QStringList & keys, int first, int last, Read read)
    {
        for (int i = first; i <= last; ++i)
            keys.insert(qMin<qsizetype>(i, keys.size()), read(i));
    }

    static void drop(QStringList & keys, int first, int last)
    {
        if (first < keys.size())
            keys.remove(first, qMin<qsizetype>(last - first + 1, keys.size() - first));
    }

    QObject context_;
    QStringList rows_;
    QStringList columns_;
};

// The scenario, in the child. `inReceiver` false is the control: the nested change is made after the outer one returns.
// `receiverFirst` connects the receiver before the model, so it runs before the model has heard the signal.
int scenario(const Case & c, bool inReceiver, bool receiverFirst)
{
    Sheet m(kNaN);
    m.addRows({"r0", "r1", "r2"});
    m.addColumns({"c0", "c1", "c2"});
    QObject context;
    bool fired = false;
    const auto receiver = [&] {
        if (!std::exchange(fired, true))
            c.nested(m);
    };
    if (inReceiver && receiverFirst)
        c.connect(m, context, receiver);
    Model model(m);
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    Mirror mirror(m, model);
    if (inReceiver && !receiverFirst)
        c.connect(m, context, receiver);
    c.outer(m);
    if (!inReceiver)
        c.nested(m);
    return modelFollows(m, model) && mirror.equals(m) ? kExitConsistent : kExitOutOfStep;
}

template <class Run>
End forked(Run && run)
{
    std::fflush(nullptr);
    const pid_t child = fork();
    if (child == 0) {
        const rlimit noCore{0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        alarm(kChildSeconds);
        // QtTest in the child logs its own failures to stdout and stderr; only the exit status is the result
        if (!std::freopen("/dev/null", "w", stderr) || !std::freopen("/dev/null", "w", stdout))
            _exit(kExitHarness);
        _exit(run());
    }
    int status = 0;
    waitpid(child, &status, 0);
    if (WIFSIGNALED(status) || WEXITSTATUS(status) == kExitHarness)
        return End::crashed;
    return WEXITSTATUS(status) == kExitConsistent ? End::consistent : End::outOfStep;
}

const char * word(End e)
{
    switch (e) {
    case End::consistent: return "consistent";
    case End::outOfStep: return "out of step";
    case End::crashed: return "crashed";
    }
    return "?";
}

}  // namespace

class LimitMatrixNestedChange : public QObject {
    RQT_OBJECT

private slots:
    [[= rqt::slot]] void everyNestedCaseEndsAsPinned_data()
    {
        QTest::addColumn<int>("caseIndex");
        QTest::addColumn<bool>("receiverFirst");
        QTest::addColumn<int>("pinned");
        int index = 0;
        for (const Case & c : cases()) {
            for (const bool first : {false, true})
                QTest::newRow(qPrintable(QStringLiteral("%1 (receiver %2 the model)").arg(QLatin1String(c.name), first ? QStringLiteral("before") : QStringLiteral("after"))))
                    << index << first << static_cast<int>(first ? c.beforeModel : c.afterModel);
            ++index;
        }
    }

    [[= rqt::slot]] void everyNestedCaseEndsAsPinned()
    {
        QFETCH(int, caseIndex);
        QFETCH(bool, receiverFirst);
        QFETCH(int, pinned);
        const std::vector<Case> all = cases();
        const Case & c = all[static_cast<std::size_t>(caseIndex)];
        const End control = forked([&] { return scenario(c, false, receiverFirst); });
        QVERIFY2(control == End::consistent, "the control (no receiver) must end consistent");
        const End nested = forked([&] { return scenario(c, true, receiverFirst); });
        qInfo("%s: %s", c.name, word(nested));
        QCOMPARE(static_cast<int>(nested), pinned);
    }
};

QTEST_GUILESS_MAIN(LimitMatrixNestedChange)
