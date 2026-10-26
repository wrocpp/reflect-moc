// compares the bytes moc writes for Q_PLUGIN_METADATA(IID .. FILE JSON_FILE) with the bytes the constexpr encoder makes from the same file via #embed
#include "plugin_edge.h"
#include MOC_OUTPUT
#include <cstdio>
#include <cstring>
#include "json_to_cbor.hpp"
static constexpr unsigned char json[] = {
#embed JSON_FILE
};
static constexpr auto mine = SPIKE_CBOR(json, "org.example.IDemo/1.0", "EdgePlugin");
int main() {
  std::size_t const n = sizeof qt_pluginMetaDataV2_EdgePlugin;
  std::printf("moc: %zu bytes, constexpr: %zu bytes\n", n, mine.size());
  bool same = n == mine.size() && std::memcmp(qt_pluginMetaDataV2_EdgePlugin, mine.data(), n) == 0;
  std::printf("%s\n", same ? "IDENTICAL" : "DIFFERENT");
  if (!same) {
    for (std::size_t k = 0; k < (n > mine.size() ? n : mine.size()); ++k) {
      int a = k < n ? qt_pluginMetaDataV2_EdgePlugin[k] : -1, b = k < mine.size() ? mine[k] : -1;
      if (a != b) { std::printf("first difference at byte %zu: moc %02x, mine %02x\n", k, a, b); std::size_t from = k > 12 ? k - 12 : 0;
      std::printf("moc : "); for (std::size_t j = from; j < from + 28 && j < n; ++j) std::printf("%02x ", qt_pluginMetaDataV2_EdgePlugin[j]);
      std::printf("\nmine: "); for (std::size_t j = from; j < from + 28 && j < mine.size(); ++j) std::printf("%02x ", mine[j]);
      std::printf("\n"); break; }
    }
  }
}
