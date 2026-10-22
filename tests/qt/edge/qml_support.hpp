// Helpers of the QML edge tests: a window with a root item made from a QML string, a replay of a container model's
// signals onto QStringListModel (the reference a QML view is compared with), and counters read from the items.
#pragma once

#include "view_support.hpp"

#include <QtCore/QStringListModel>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <algorithm>
#include <memory>
#include <vector>

namespace edge_qml {
using namespace edge_view;

constexpr int kWindowWidth = 240;
constexpr int kWindowHeight = 100;
constexpr int kRowHeight = 20;  // the delegates below are this high; the window shows kWindowHeight / kRowHeight of them

// What the items under `item` show: their count, position, size, opacity and visibility, and for a Flickable its
// content position, so that delegates still being created, a transition and a scroll that is still moving all
// change it.
inline void fingerprint_items(QQuickItem* item, std::uint64_t& hash) {
  mix(hash, static_cast<std::uint64_t>(item->x() * 31 + item->y() * 17 + item->width() * 7 + item->height()));
  mix(hash, static_cast<std::uint64_t>(item->opacity() * 1000) + (item->isVisible() ? 1 : 0));
  mix(hash, static_cast<std::uint64_t>(item->property("contentY").toDouble() * 13 + item->property("contentX").toDouble()));
  for (QQuickItem* child : item->childItems()) fingerprint_items(child, hash);
}

// A root item, made from a QML string, in an offscreen window. The windows of every live scene are polished and
// synchronised (a grab does both, at once) each time settle() looks, so a ListView has created its delegates when
// the wait ends and not whenever the next frame happens to come.
class Scene {
 public:
  Scene() { scenes().push_back(this); }
  ~Scene() { scenes().erase(std::find(scenes().begin(), scenes().end(), this)); }
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;

  static std::vector<Scene*>& scenes() {
    static std::vector<Scene*> live;
    return live;
  }
  static std::uint64_t fingerprint() {
    std::uint64_t hash = 14695981039346656037ULL;
    for (Scene* scene : scenes()) {
      if (!scene->root || !scene->window.isVisible()) continue;
      scene->window.grabWindow();
      fingerprint_items(scene->window.contentItem(), hash);
    }
    return hash;
  }
  // `properties` are the context properties the QML refers to.
  bool load(const QByteArray& qml, const QList<std::pair<QString, QObject*>>& properties) {
    for (const auto& [name, object] : properties) engine.rootContext()->setContextProperty(name, object);
    QQmlComponent component(&engine);
    component.setData(qml, QUrl());
    if (component.isError()) {
      for (const auto& e : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
      return false;
    }
    root.reset(qobject_cast<QQuickItem*>(component.create()));
    if (!root) return false;
    window.resize(kWindowWidth, kWindowHeight);
    root->setParentItem(window.contentItem());
    window.show();
    scene_probe() = &Scene::fingerprint;
    settle();
    return true;
  }
  QObject* find(const char* name) const { return root->findChild<QObject*>(name); }
  QVariant get(const char* property) const { return root->property(property); }
  static QVariant get(QObject* object, const char* property) { return object->property(property); }
  static QVariant call(QObject* object, const char* method, QGenericArgument a0 = QGenericArgument()) {
    QVariant result;
    QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, result), a0);
    return result;
  }

  QQmlEngine engine;
  QQuickWindow window;
  std::unique_ptr<QQuickItem> root;
};

// Applies the structural signals of `source` to a QStringListModel, so that the same change can be shown to a QML
// view over Qt's own list model.
class Replay {
 public:
  Replay(QAbstractItemModel& source, QStringListModel& target) : m_source(source), m_target(target) {
    m_target.setStringList(column(source));
    using M = QAbstractItemModel;
    QObject::connect(&source, &M::rowsInserted, &target, [this](const QModelIndex&, int first, int last) {
      m_target.insertRows(first, last - first + 1);
      for (int row = first; row <= last; ++row) m_target.setData(m_target.index(row), m_source.index(row, 0).data());
    });
    QObject::connect(&source, &M::rowsRemoved, &target, [this](const QModelIndex&, int first, int last) { m_target.removeRows(first, last - first + 1); });
    QObject::connect(&source, &M::rowsMoved, &target, [this](const QModelIndex&, int first, int last, const QModelIndex&, int destination) {
      m_target.moveRows({}, first, last - first + 1, {}, destination);
    });
    QObject::connect(&source, &M::modelReset, &target, [this] { m_target.setStringList(column(m_source)); });
    QObject::connect(&source, &M::dataChanged, &target, [this](const QModelIndex& top, const QModelIndex& bottom) {
      for (int row = top.row(); row <= bottom.row(); ++row) m_target.setData(m_target.index(row), m_source.index(row, 0).data());
    });
  }

 private:
  QAbstractItemModel& m_source;
  QStringListModel& m_target;
};

}  // namespace edge_qml
