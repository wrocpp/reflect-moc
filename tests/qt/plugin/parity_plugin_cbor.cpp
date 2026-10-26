// The CBOR of the plugin metadata, byte for byte against moc's, for the JSON file named by JSON_FILE (one executable per
// file, tests/qt/plugin/CMakeLists.txt): key order, a repeated key, numbers, escapes, containers whose head is longer than
// one byte. moc's bytes are the array moc writes into its output, which this file includes (AUTOMOC);
// the reflect-moc bytes come from the macro RQT_PLUGIN_METADATA stands for, without its entry points (moc's are in this
// translation unit already). docs/moc-parity.md: E-01.
#include "moc_parity_plugin_cbor_twin.cpp"
#include "parity_plugin_cbor_twin.h"

#include <reflect_moc/qt/plugin.hpp>

#include <cstdio>
#include <cstring>

static constexpr unsigned char json[] = {
#embed JSON_FILE
};

static constexpr auto& ours = RQT_DETAIL_PLUGIN_ARRAY(DemoPlugin, IDemo_iid, json);
static constexpr auto& moc = qt_pluginMetaDataV2_DemoPlugin;

int main() {
  std::size_t const n = sizeof moc;
  bool const same = n == sizeof ours && std::memcmp(moc, ours, n) == 0;
  std::printf("%s: moc %zu bytes, reflect-moc %zu bytes: %s\n", JSON_FILE, n, sizeof ours, same ? "identical" : "DIFFERENT");
  if (same) return 0;
  for (std::size_t k = 0; k < n && k < sizeof ours; ++k) {
    if (moc[k] == ours[k]) continue;
    std::printf("first difference at byte %zu: moc %02x, reflect-moc %02x\n", k, moc[k], ours[k]);
    break;
  }
  return 1;
}
