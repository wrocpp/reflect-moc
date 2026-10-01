// Must NOT compile: "progresChanged" is not a signal of Worker.
// Built only by the capability_connect_by_name_rejects_typo test, which
// expects the diagnostic to name the missing signal and the class.
#include <reflect_moc/core.hpp>

struct Worker : rqt::Object {
  [[= rqt::property{.notify = true}]] int progress = 0;
};

struct Ui : rqt::Object {
  void onProgress(int) {}
};

int main() {
  Worker worker;
  Ui ui;
  rqt::connect<"progresChanged">(worker, ui, &Ui::onProgress);
}
