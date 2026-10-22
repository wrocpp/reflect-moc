// Support for the model-protocol edge cases (docs/qt-edge-cases/protocol.md): a fixture that builds the list model or
// the table model of any container kind with a QAbstractItemModelTester in fatal mode attached, a log of every
// signal a model emits, and a probe that reads the model at every row signal.
//
// Entries are named by the number they show: the n-th entry added shows n, in every container kind, so a test says
// "remove the entry that shows 3" and means the same entry in a QList, a QMap, a QHash, a QSet and the multi kinds.
#pragma once

#include "check.hpp"
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QStringList>
#include <QtTest/QAbstractItemModelTester>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace edge {

using observable_test::Make;
using observable_test::Sample;

constexpr int kEdited = 77;            // a value no test entry shows
constexpr int kTwinFactor = 2;         // the second column of a table model shows the first one times this
constexpr int kTableColumns = 2;       // columns of the table model of the fixture
constexpr int kAfterTheEnd = 3;        // how far past the last row a probe looks for a row that must not be there
constexpr int kSmallFill = 5;          // entries of a typical test
constexpr int kSlotCompaction = 100;   // enough slots of a hash index to trigger its compaction (row_index.hpp)
constexpr int kUnknownRole = 9999;     // a role no model of this library knows

inline void report_in(const std::string& kind, bool ok, const char* expr, const char* file, int line) {
  if (ok) return;
  ++rqt_test::failures();
  std::fprintf(stderr, "%s:%d: [%s] CHECK failed: %s\n", file, line, kind.c_str(), expr);
}

// Names the container kind in the report; `kind` is a std::string in scope.
#define KCHECK(cond) ::edge::report_in(kind, static_cast<bool>(cond), #cond, __FILE__, __LINE__)

// Every signal a model emits, in order, as a line of text: "rowsInserted 0 2", "dataChanged 1,0 1,1 roles=", ...
class SignalLog : public QObject {
 public:
  explicit SignalLog(QAbstractItemModel* model) : QObject(model) {
    using M = QAbstractItemModel;
    const auto rows = [this, model](const char* name, auto signal) {
      connect(model, signal, this, [this, name](const QModelIndex&, int first, int last) { note(name, first, last); });
    };
    rows("rowsAboutToBeInserted", &M::rowsAboutToBeInserted);
    rows("rowsInserted", &M::rowsInserted);
    rows("rowsAboutToBeRemoved", &M::rowsAboutToBeRemoved);
    rows("rowsRemoved", &M::rowsRemoved);
    rows("columnsAboutToBeInserted", &M::columnsAboutToBeInserted);
    rows("columnsInserted", &M::columnsInserted);
    rows("columnsAboutToBeRemoved", &M::columnsAboutToBeRemoved);
    rows("columnsRemoved", &M::columnsRemoved);
    const auto moved = [this, model](const char* name, auto signal) {
      connect(model, signal, this, [this, name](const QModelIndex&, int first, int last, const QModelIndex&, int destination) {
        m_lines.append(QStringLiteral("%1 %2 %3 %4").arg(QLatin1String(name)).arg(first).arg(last).arg(destination));
      });
    };
    moved("rowsAboutToBeMoved", &M::rowsAboutToBeMoved);
    moved("rowsMoved", &M::rowsMoved);
    moved("columnsAboutToBeMoved", &M::columnsAboutToBeMoved);
    moved("columnsMoved", &M::columnsMoved);
    const auto plain = [this, model](const char* name, auto signal) {
      connect(model, signal, this, [this, name] { m_lines.append(QLatin1String(name)); });
    };
    plain("modelAboutToBeReset", &M::modelAboutToBeReset);
    plain("modelReset", &M::modelReset);
    connect(model, &M::layoutAboutToBeChanged, this, [this] { m_lines.append(QStringLiteral("layoutAboutToBeChanged")); });
    connect(model, &M::layoutChanged, this, [this] { m_lines.append(QStringLiteral("layoutChanged")); });
    connect(model, &M::headerDataChanged, this, [this](Qt::Orientation, int first, int last) { note("headerDataChanged", first, last); });
    connect(model, &M::dataChanged, this, [this](const QModelIndex& top, const QModelIndex& bottom, const QList<int>& roles) {
      QStringList names;
      for (const int role : roles) names.append(QString::number(role));
      m_lines.append(QStringLiteral("dataChanged %1,%2 %3,%4 roles=%5")
                         .arg(top.row()).arg(top.column()).arg(bottom.row()).arg(bottom.column()).arg(names.join(',')));
      m_changes.append({top, bottom, roles});
    });
  }

  struct Change {
    QModelIndex top;
    QModelIndex bottom;
    QList<int> roles;
  };

  QStringList take() { return std::exchange(m_lines, {}); }
  bool silent() const { return m_lines.isEmpty(); }
  const QStringList& lines() const { return m_lines; }
  const QList<Change>& changes() const { return m_changes; }
  qsizetype count(const QString& prefix) const {
    return std::count_if(m_lines.begin(), m_lines.end(), [&](const QString& line) { return line.startsWith(prefix); });
  }
  bool has(const QString& line) const { return m_lines.contains(line); }

 private:
  void note(const char* name, int first, int last) { m_lines.append(QStringLiteral("%1 %2 %3").arg(QLatin1String(name)).arg(first).arg(last)); }
  QStringList m_lines;
  QList<Change> m_changes;
};

template <class C, bool Table>
class Fixture {
 public:
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using Source = observable::ObservableContainer<C>;
  using Column = observable::Column<C>;
  using Container = C;

  static constexpr bool table = Table;
  static constexpr bool indexed = Traits::order == observable::Order::indexed;
  static constexpr bool multi = Traits::order == observable::Order::sorted_multi || Traits::order == observable::Order::unordered_multi;
  static constexpr bool keyed = Traits::order == observable::Order::sorted || Traits::order == observable::Order::unordered;
  static constexpr bool sorted = Traits::order == observable::Order::sorted || Traits::order == observable::Order::sorted_multi;
  static constexpr bool movable = Traits::movable;
  static constexpr bool assignable = Traits::assignable;

  // `rows` entries are in the container before the model is made; the model has an editor on its first column
  // when `editable`.
  explicit Fixture(bool editable = false, int rows = 0, bool tested = true) {
    for (int i = 0; i < rows; ++i) add();
    model = make_model(&source, editable);
    if (tested) tester = std::make_unique<QAbstractItemModelTester>(model.get(), QAbstractItemModelTester::FailureReportingMode::Fatal);
  }

  // A second model on the same container, for a case that mixes two models.
  std::unique_ptr<QAbstractItemModel> another_model(bool as_table, bool editable = false) {
    return as_table ? table_model(&source, editable) : list_model(&source, editable);
  }

  static std::unique_ptr<QAbstractItemModel> make_model(Source* source, bool editable) {
    return Table ? table_model(source, editable) : list_model(source, editable);
  }

  static Value from_int(int n) {
    if constexpr (std::is_same_v<Value, Sample>)
      return Sample{n};
    else
      return static_cast<Value>(n);
  }

  // The key of the entry that shows `n`: n itself for a list, and for the others the key add() gave it.
  static Key named(int n) {
    if constexpr (multi)
      return Key{static_cast<decltype(Key::key)>(n), 0};
    else
      return static_cast<Key>(n);
  }

  // The whole content of the entries named, for resetWith or insertMany.
  static C content(std::initializer_list<int> ns) {
    C out;
    for (const int n : ns) {
      const Key key = [&] {
        if constexpr (indexed)
          return static_cast<Key>(Traits::size(out));
        else
          return named(n);
      }();
      Traits::insert(out, key, Make<C>::value(n, key));
    }
    return out;
  }

  // Adds the entry that shows `n` (a list appends it), whatever the number.
  int add_named(int n) {
    const Key key = key_for_new(n);
    source.insert(key, Make<C>::value(n, key));
    return n;
  }

  // Adds the entry that shows the next number and returns the number.
  int add() { return add_named(next++); }
  void fill(int count) {
    for (int i = 0; i < count; ++i) add();
  }

  std::optional<Key> key_of(int shown) const {
    for (const Key& key : Traits::keys(source.items()))
      if (Make<C>::shown(key, Traits::value(source.items(), key)) == shown) return key;
    return std::nullopt;
  }
  bool remove(int shown) {
    const auto key = key_of(shown);
    return key && source.remove(*key);
  }
  bool replace(int shown, int to) {
    const auto key = key_of(shown);
    return key && source.replace(*key, Make<C>::value(to, *key));
  }

  QModelIndex index(int row, int column = 0) const { return model->index(row, column); }
  int rows() const { return model->rowCount(); }
  int shown_at(int row, int column = 0) const { return model->data(model->index(row, column)).toInt(); }
  std::vector<int> shown() const {
    std::vector<int> out;
    for (int row = 0; row < model->rowCount(); ++row) out.push_back(shown_at(row));
    return out;
  }
  // The row order of a hash or set is the order the entries were added; of a map, the key order. Both are
  // ascending when entries are added in ascending order, which add() does.
  bool shows(std::initializer_list<int> expected) const { return shown() == std::vector<int>(expected); }
  bool shows_as_set(std::initializer_list<int> expected) const {
    std::vector<int> have = shown();
    std::vector<int> want(expected);
    std::sort(have.begin(), have.end());
    std::sort(want.begin(), want.end());
    return have == want;
  }

  Source source;
  std::unique_ptr<QAbstractItemModel> model;
  std::unique_ptr<QAbstractItemModelTester> tester;
  int next = 1;

 private:
  Key key_for_new(int n) const {
    if constexpr (indexed) {
      return source.size();
    } else if constexpr (multi) {
      return Key{static_cast<decltype(Key::key)>(n), 0};
    } else {
      return static_cast<Key>(n);
    }
  }

  static QVariant show(const Key& key, const Value& value) { return Make<C>::shown(key, value); }
  static QVariant show_twin(const Key& key, const Value& value) { return kTwinFactor * Make<C>::shown(key, value); }
  static std::optional<Value> parse(const Value&, const QVariant& edited) {
    bool ok = false;
    const int n = edited.toInt(&ok);
    if (!ok) return std::nullopt;
    return from_int(n);
  }

  static std::unique_ptr<QAbstractItemModel> list_model(Source* source, bool editable) {
    if (editable) return std::make_unique<observable::ContainerModel<C>>(source, &show, &parse);
    return std::make_unique<observable::ContainerModel<C>>(source, &show);
  }
  static std::unique_ptr<QAbstractItemModel> table_model(Source* source, bool editable) {
    return std::make_unique<observable::ContainerTableModel<C>>(
        source, std::vector<Column>{{.header = "value", .display = &show, .edit = editable ? typename Column::Editor(&parse) : typename Column::Editor{}},
                                    {.header = "twin", .display = &show_twin}});
  }

};

// Collects the messages Qt writes (qWarning and the like) instead of printing them, while it lives.
class MessageCatcher {
 public:
  MessageCatcher() : m_previous(qInstallMessageHandler(&handle)) { current() = this; }
  ~MessageCatcher() {
    qInstallMessageHandler(m_previous);
    current() = nullptr;
  }
  MessageCatcher(const MessageCatcher&) = delete;
  MessageCatcher& operator=(const MessageCatcher&) = delete;

  bool saw(const QString& text) const {
    return std::any_of(m_messages.begin(), m_messages.end(), [&](const QString& m) { return m.contains(text); });
  }
  const QStringList& messages() const { return m_messages; }

 private:
  static MessageCatcher*& current() {
    static MessageCatcher* instance = nullptr;
    return instance;
  }
  static void handle(QtMsgType, const QMessageLogContext&, const QString& text) {
    if (current()) current()->m_messages.append(text);
  }
  QtMessageHandler m_previous;
  QStringList m_messages;
};

// Calls `f(fixture, label)` once for every container kind and for both models.
template <class F>
void for_each_model(F&& f, bool editable = false, int rows = 0, bool tested = true) {
  const auto both = [&]<class C>(std::type_identity<C>, const char* name) {
    {
      Fixture<C, false> fixture(editable, rows, tested);
      f(fixture, std::string(name) + " list");
    }
    {
      Fixture<C, true> fixture(editable, rows, tested);
      f(fixture, std::string(name) + " table");
    }
  };
  using observable_test::Sample;
  both(std::type_identity<QList<Sample>>{}, "QList");
  both(std::type_identity<QMap<int, Sample>>{}, "QMap");
  both(std::type_identity<QHash<unsigned, Sample>>{}, "QHash");
  both(std::type_identity<QSet<unsigned>>{}, "QSet");
  both(std::type_identity<QMultiMap<int, Sample>>{}, "QMultiMap");
  both(std::type_identity<QMultiHash<unsigned, Sample>>{}, "QMultiHash");
}

// Calls `f(fixture, label)` for the list and the table model of a QList, the one container with an order to move in.
template <class F>
void for_each_list_model(F&& f, bool editable = false, int rows = 0) {
  Fixture<QList<Sample>, false> list(editable, rows);
  f(list, std::string("QList list"));
  Fixture<QList<Sample>, true> table(editable, rows);
  f(table, std::string("QList table"));
}

// Reads a model at every row signal: how many rows index() reaches against rowCount(), and which rows of the
// change read as empty at the "about to be removed" signal.
class RowProbe : public QObject {
 public:
  explicit RowProbe(QAbstractItemModel* model) : QObject(model), m_model(model) {
    using M = QAbstractItemModel;
    const auto look = [this, model](auto signal) { connect(model, signal, this, [this] { inspect(); }); };
    look(&M::rowsAboutToBeInserted);
    look(&M::rowsInserted);
    look(&M::rowsAboutToBeRemoved);
    look(&M::rowsRemoved);
    look(&M::rowsAboutToBeMoved);
    look(&M::rowsMoved);
    look(&M::modelAboutToBeReset);
    look(&M::modelReset);
    connect(model, &M::rowsAboutToBeRemoved, this, [this](const QModelIndex&, int first, int last) {
      for (int row = first; row <= last; ++row)
        if (!m_model->data(m_model->index(row, 0)).isValid()) ++m_blankRowsBeingRemoved;
    });
  }

  int looks() const { return m_looks; }
  int disagreements() const { return m_disagreements; }
  int blank_rows_being_removed() const { return m_blankRowsBeingRemoved; }
  int blank_reads() const { return m_blankReads; }

 private:
  void inspect() {
    ++m_looks;
    const int count = m_model->rowCount();
    int reachable = 0;
    for (int row = 0; row < count + kAfterTheEnd; ++row) {
      const QModelIndex index = m_model->index(row, 0);
      if (!index.isValid()) continue;
      ++reachable;
      if (!m_model->data(index).isValid()) ++m_blankReads;
    }
    if (reachable != count || m_model->index(count, 0).isValid()) ++m_disagreements;
  }
  QAbstractItemModel* m_model;
  int m_looks = 0;
  int m_disagreements = 0;
  int m_blankRowsBeingRemoved = 0;
  int m_blankReads = 0;
};

}  // namespace edge
