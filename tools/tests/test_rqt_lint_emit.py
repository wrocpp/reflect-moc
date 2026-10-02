"""Tests for tools/rqt-lint-emit.py. Run from tools/: python3 -m unittest discover -s tests"""

from __future__ import annotations

import contextlib
import importlib.util
import io
import os
import tempfile
import unittest

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_spec = importlib.util.spec_from_file_location("rqt_lint_emit", os.path.join(TOOLS, "rqt-lint-emit.py"))
lint = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(lint)

DECLARATION = """
class Node : public QObject {
 signals:
  static inline rqt::static_signal<void(int)> valueChanged{};
  [[= rqt::names("a, b")]] static inline rqt::static_signal<void(int, int)> moved{};
  rqt::signal<void(int)> plain;
};
"""


def flagged(body: str, names=("valueChanged", "moved")) -> list:
    return lint.lint_source("x.cpp", body, names)


class StaticSignalNames(unittest.TestCase):
    def test_finds_every_declaration(self):
        self.assertEqual(lint.static_signal_names(DECLARATION), {"valueChanged", "moved"})

    def test_ignores_a_commented_declaration_and_a_non_static_signal(self):
        source = "// static inline rqt::static_signal<void()> gone{};\nrqt::signal<void(int)> plain;\n"
        self.assertEqual(lint.static_signal_names(source), set())


class Flags(unittest.TestCase):
    def test_arrow_and_dot_through_another_object(self):
        found = flagged("void f(Node* o, Node& r) {\n  emit o->valueChanged(1);\n  emit r.moved(1, 2);\n}\n")
        self.assertEqual([(f.line, f.name) for f in found], [(2, "valueChanged"), (3, "moved")])

    def test_q_emit_and_a_call_chain(self):
        found = flagged("void f() { Q_EMIT peer()->valueChanged(1); }\n")
        self.assertEqual([f.name for f in found], ["valueChanged"])

    def test_reports_the_line_text(self):
        found = flagged("a;\n    emit o->valueChanged(1);  // why\n")
        self.assertEqual(found[0].text, "emit o->valueChanged(1);  // why")


class DoesNotFlag(unittest.TestCase):
    def test_own_signal(self):
        self.assertEqual(flagged("void f() { emit valueChanged(1); }\n"), [])

    def test_this_and_the_explicit_form(self):
        body = "void f(Node* o) { emit this->valueChanged(1); valueChanged(1).from(o); }\n"
        self.assertEqual(flagged(body), [])

    def test_a_qualified_name_through_the_class(self):
        self.assertEqual(flagged("void f() { emit Node::valueChanged(1); }\n"), [])

    def test_a_signal_that_is_not_static(self):
        self.assertEqual(flagged("void f(Node* o) { emit o->plain(1); }\n"), [])

    def test_a_comment_or_a_string(self):
        body = 'void f(Node* o) {\n  // emit o->valueChanged(1);\n  log("emit o->valueChanged(1)");\n}\n'
        self.assertEqual(flagged(body), [])

    def test_no_static_signals_known(self):
        self.assertEqual(flagged("void f(Node* o) { emit o->valueChanged(1); }\n", names=()), [])


class Paths(unittest.TestCase):
    def test_a_signal_declared_in_one_file_is_checked_in_another(self):
        with tempfile.TemporaryDirectory() as root:
            with open(os.path.join(root, "node.hpp"), "w") as handle:
                handle.write(DECLARATION)
            with open(os.path.join(root, "use.cpp"), "w") as handle:
                handle.write("void f(Node* o) {\n  emit o->moved(1, 2);\n}\n")
            found = lint.lint_paths([root])
            self.assertEqual([(os.path.basename(f.path), f.line) for f in found], [("use.cpp", 2)])

    def test_exit_status_and_message(self):
        with tempfile.TemporaryDirectory() as root:
            path = os.path.join(root, "use.cpp")
            with open(path, "w") as handle:
                handle.write(DECLARATION + "void f(Node* o) { emit o->valueChanged(1); }\n")
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                status = lint.main([path])
            self.assertEqual(status, 1)
            self.assertIn("write `valueChanged(...).from(object);`", out.getvalue())

    def test_clean_tree_exits_zero(self):
        with tempfile.TemporaryDirectory() as root:
            path = os.path.join(root, "ok.cpp")
            with open(path, "w") as handle:
                handle.write(DECLARATION + "void f(Node* o) { valueChanged(1).from(o); }\n")
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(lint.main([path]), 0)

    def test_the_repository_tests_flag_only_the_pinned_pitfall(self):
        tests = os.path.join(os.path.dirname(TOOLS), "tests", "qt")
        found = lint.lint_paths([tests])
        self.assertEqual({os.path.basename(f.path) for f in found},
                         {"limit_static_signal_other_instance.cpp"})


if __name__ == "__main__":
    unittest.main()
