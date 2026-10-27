// Q_PLUGIN_METADATA(IID ..) with no FILE: moc writes IID and className and no MetaData key; so must the macro with its last
// argument left out. docs/moc-parity.md: E-01.
#include "moc_parity_plugin_cbor_no_file_twin.cpp"
#include "parity_plugin_cbor_no_file_twin.h"

#include <reflect_moc/qt/plugin.hpp>

#include <cstdio>
#include <cstring>

static constexpr auto& ours = RQT_DETAIL_PLUGIN_ARRAY(DemoPlugin, IDemo_iid);
static constexpr auto& moc = qt_pluginMetaDataV2_DemoPlugin;

int main() {
  bool const same = sizeof moc == sizeof ours && std::memcmp(moc, ours, sizeof moc) == 0;
  std::printf("no FILE: moc %zu bytes, reflect-moc %zu bytes: %s\n", sizeof moc, sizeof ours, same ? "identical" : "DIFFERENT");
  return same ? 0 : 1;
}
