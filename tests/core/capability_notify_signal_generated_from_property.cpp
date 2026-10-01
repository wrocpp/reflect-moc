// `set<"progress">(v)` on a NOTIFY property emits the generated
// progressChanged signal with the new value, only when the value changes,
// even when the property is a private member. A property without notify
// generates no signal.
#include "check.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int first_value = 25;
constexpr int second_value = 50;

class Worker : public rqt::Object {
  [[= rqt::property{.notify = true}]] int progress = 0;
  [[= rqt::property{}]] std::string title;
};

constexpr auto table = rqt::signal_table<Worker>();
static_assert(table.size() == 1);
static_assert(std::string_view{table[0].name} == "progressChanged");
static_assert(table[0].arity == 1);

}  // namespace

int main() {
  Worker worker;
  std::vector<int> seen;
  worker.connect<"progressChanged">(worker, [&seen](int value) { seen.push_back(value); });

  worker.set<"progress">(first_value);
  worker.set<"progress">(first_value);
  worker.set<"progress">(second_value);
  worker.set<"title">("renamed");

  RQT_CHECK(seen == std::vector{first_value, second_value});
  RQT_CHECK(worker.get<"progress">() == second_value);
  RQT_CHECK(worker.get<"title">() == "renamed");
  return rqt_test::result();
}
