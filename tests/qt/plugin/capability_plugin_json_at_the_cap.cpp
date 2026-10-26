// The largest file the macro promises to build, in the two shapes that cost the encoder most per byte, each just under
// RQT_PLUGIN_JSON_MAX_BYTES: it must build under the default -fconstexpr-ops-limit, and the bytes must read back as the value
// that was written. tests/qt/plugin/CMakeLists.txt makes the files and says what the shapes are. docs/moc-parity.md: E-01.
#include "check.hpp"

#include <reflect_moc/qt/plugin.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QCborArray>
#include <QtCore/QCborMap>
#include <QtCore/QCborValue>

static constexpr unsigned char compact_json[] = {
#embed "rqt_plugin_cap_compact.json"
};
static constexpr unsigned char flat_json[] = {
#embed "rqt_plugin_cap_flat.json"
};
static constexpr auto& compact_cbor = RQT_DETAIL_PLUGIN_ARRAY(Compact, "org.example.Compact/1.0", compact_json);
static constexpr auto& flat_cbor = RQT_DETAIL_PLUGIN_ARRAY(Flat, "org.example.Flat/1.0", flat_json);

static constexpr qsizetype features = 215;      // RQT_PLUGIN_CAP_FEATURES in the CMake file
static constexpr qsizetype keys = 1770;         // RQT_PLUGIN_CAP_KEYS
static constexpr std::size_t room_under_the_cap = 1024;  // each file is within this much of the cap
static constexpr int meta_data_key = 4;

template <std::size_t N>
static QCborMap meta_data_of(unsigned char const (&cbor)[N]) {
  QCborParserError error;
  QCborValue const top = QCborValue::fromCbor(QByteArray::fromRawData(reinterpret_cast<char const*>(cbor), N), &error);
  CHECK(error.error == QCborError::NoError);
  return top.toMap().value(meta_data_key).toMap();
}

int main() {
  static_assert(sizeof compact_json <= RQT_PLUGIN_JSON_MAX_BYTES && sizeof flat_json <= RQT_PLUGIN_JSON_MAX_BYTES);
  CHECK(sizeof compact_json + room_under_the_cap >= RQT_PLUGIN_JSON_MAX_BYTES);
  CHECK(sizeof flat_json + room_under_the_cap >= RQT_PLUGIN_JSON_MAX_BYTES);

  QCborArray const list = meta_data_of(compact_cbor).value(QString{"Features"}).toArray();
  CHECK(list.size() == features);
  QCborMap const last = list.at(features - 1).toMap();
  CHECK(last.value(QString{"id"}).toString() == "feature.215");
  CHECK(last.value(QString{"weight"}).toDouble() == 215.5);
  CHECK(last.value(QString{"tags"}).toArray().size() == 3);

  QCborMap const flat = meta_data_of(flat_cbor);
  CHECK(flat.size() == keys);
  CHECK(flat.contains(QString{"key000007"}));  // written as the i whose (i * 7919) mod 1770 is 7
  CHECK(!flat.contains(QString{"key001770"}));
  QString previous;
  bool ascending = true;
  for (auto it = flat.constBegin(); it != flat.constEnd(); ++it) {
    QString const key = it.key().toString();
    ascending = ascending && (previous.isNull() || previous < key);
    previous = key;
  }
  CHECK(ascending);
  return rqt_test::finish("capability_plugin_json_at_the_cap");
}
