"""Tests for tools/rqt-migrate. Run from tools/: python3 -m unittest discover -s tests

The fixtures' moc JSON is saved under tests/fixtures/<case>/moc (regenerate it
with tests/regen_fixtures.sh in the Qt image), so these tests need no Qt.
"""

from __future__ import annotations

import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)

from rqt_migrate import cli, moc, syntax  # noqa: E402
from rqt_migrate.report import AUTO, MANUAL, PARTIAL, diff_stats  # noqa: E402
from rqt_migrate.text import blank, split_top_level  # noqa: E402
from rqt_migrate.tree import Options, migrate  # noqa: E402

FIXTURES = os.path.join(TOOLS, "tests", "fixtures")


def run_fixture(case: str):
    root = os.path.join(FIXTURES, case)
    return migrate(os.path.join(root, "src"), moc.saved(os.path.join(root, "moc")),
                   Options(reflect_moc_include="../include", title=case))


def read_dir(path: str) -> dict[str, str]:
    out = {}
    for name in os.listdir(path):
        with open(os.path.join(path, name), encoding="utf-8") as f:
            out[name] = f.read()
    return out


def items(result, construct: str, status: str | None = None):
    return [i for i in result.report.items if i.construct == construct and (status is None or i.status == status)]


class Rewrites(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_fixture("rewrites")
        cls.header = cls.result.files["counter.h"]
        cls.source = cls.result.files["counter.cpp"]
        cls.cmake = cls.result.files["CMakeLists.txt"]

    def assertLine(self, text: str, line: str):
        self.assertIn(line, text.splitlines())

    def test_capability_q_object_removed_and_base_wrapped(self):
        self.assertNotIn("Q_OBJECT", self.header)
        self.assertIn(": public rqt::Object<QObject>", self.header)

    def test_capability_q_object_keeps_tr_context_when_the_class_calls_tr(self):
        self.assertLine(self.header, "    Q_DECLARE_TR_FUNCTIONS(Counter)")
        self.assertIn(syntax.TR_INCLUDE, self.header)

    def test_capability_header_is_the_qt_layer_umbrella(self):
        self.assertEqual(syntax.HEADER_INCLUDE, "#include <reflect_moc/qt/qt.hpp>")

    def test_capability_constructor_body_binds_the_class(self):
        self.assertIn("    : rqt::Object<QObject>(parent), m_value(start)\n{\n    bind();\n}", self.source)

    def test_capability_reflect_moc_include_follows_the_last_include(self):
        lines = self.header.splitlines()
        self.assertEqual(lines[lines.index("#include <QString>") + 1], syntax.HEADER_INCLUDE)

    def test_capability_signal_gets_annotation_and_emit_body(self):
        self.assertLine(self.header,
                        "    [[=rqt::signal]] void valueChanged(int value) { rqt::emit{this}(value); }")

    def test_capability_unnamed_signal_parameter_is_named(self):
        self.assertLine(self.header,
                        "    [[=rqt::signal]] void labelChanged(const QString & arg0) { rqt::emit{this}(arg0); }")

    def test_capability_multi_line_signal_keeps_its_line_break(self):
        self.assertIn("    [[=rqt::signal]] void moved(int from,\n               int to) { rqt::emit{this}(from, to); }",
                      self.header)

    def test_capability_signals_section_becomes_public_in_place(self):
        lines = self.header.splitlines()
        at = lines.index("    [[=rqt::signal]] void valueChanged(int value) { rqt::emit{this}(value); }")
        self.assertEqual(lines[at - 1], "public:")
        self.assertNotIn("signals:", self.header)

    def test_capability_slots_keyword_dropped_from_access_specifier(self):
        self.assertNotIn("slots", self.header)
        self.assertNotIn("Q_SLOTS", self.header)
        self.assertLine(self.header, "    [[=rqt::slot]] void tick();")
        self.assertLine(self.header, "    [[=rqt::slot]] void reset();")

    def test_capability_q_slot_macro_becomes_annotation(self):
        self.assertLine(self.header, "    [[=rqt::slot]] void clear();")

    def test_capability_q_invokable_becomes_annotation(self):
        self.assertLine(self.header, "    [[=rqt::invokable]] int add(int n);")
        self.assertLine(self.header, "    [[=rqt::invokable]]")

    def test_capability_property_on_read_accessor_carries_write_notify_reset(self):
        self.assertLine(self.header,
                        '    [[=rqt::property{.write = "setValue", .notify = "valueChanged", .reset = "reset"}]] '
                        "int value() const { return m_value; }")

    def test_limit_property_named_unlike_its_accessor_is_exposed_under_the_accessor_name(self):
        self.assertLine(self.header, "    [[=rqt::property{}]] bool isEnabled() const;")
        detail = " ".join(i.detail for i in items(self.result, "Q_PROPERTY", PARTIAL))
        self.assertIn("NAME (the property is named `isEnabled`)", detail)
        self.assertIn("CONSTANT", detail)

    def test_capability_property_text_is_written_as_string_literals(self):
        self.assertNotIn("define_static_string", self.header)
        self.assertNotIn(".name =", self.header)
        self.assertNotIn(".constant =", self.header)
        self.assertNotIn(".final =", self.header)

    def test_capability_multi_line_q_property_is_removed_whole(self):
        self.assertNotIn("Q_PROPERTY", self.header)
        self.assertNotIn("READ ratio", self.header)
        self.assertLine(self.header, '    [[=rqt::property{.write = "setRatio"}]] double ratio() const;')

    def test_capability_member_property_annotates_the_data_member(self):
        self.assertLine(self.header, '    [[=rqt::property{.notify = "labelChanged"}]] QString m_label;')

    def test_capability_q_enum_annotates_the_enum(self):
        self.assertNotIn("Q_ENUM", self.header)
        self.assertLine(self.header, "    enum class [[=rqt::enum_]] Mode { Up, Down };")

    def test_capability_enum_attribute_goes_before_the_name(self):
        for line in self.header.splitlines():
            if "rqt::enum_" in line or "rqt::flag" in line:
                self.assertRegex(line, r"enum (class )?\[\[=rqt::(enum_|flag)\]\] \w+")

    def test_capability_q_flag_annotates_the_enum_behind_the_alias(self):
        self.assertNotIn("Q_FLAG(", self.header)
        self.assertLine(self.header, "    enum [[=rqt::flag]] Option { None = 0, Wrap = 1, Clamp = 2 };")
        self.assertLine(self.header, "    Q_DECLARE_FLAGS(Options, Option)")

    def test_capability_q_classinfo_becomes_class_annotation_with_escapes_kept(self):
        self.assertNotIn("Q_CLASSINFO", self.header)
        self.assertIn('class [[=rqt::classinfo{"Author", "Fixture \\"quoted\\""}]] Counter', self.header)

    def test_capability_inherited_constructors_name_the_new_direct_base(self):
        self.assertLine(self.header, "    using rqt::Object<QObject>::Object;")

    def test_capability_constructor_initializer_names_the_new_direct_base(self):
        self.assertLine(self.source, "    : rqt::Object<QObject>(parent), m_value(start)")

    def test_capability_emit_and_q_emit_dropped_at_call_sites(self):
        self.assertLine(self.source, "    moved(m_value - n, m_value);")
        self.assertLine(self.source, "        labelChanged(QString());")
        self.assertIn("m_value = v; valueChanged(v);", self.header)

    def test_limit_emit_in_comments_and_strings_is_left_alone(self):
        self.assertLine(self.source, "    // emit nothing here: comments are left alone")
        self.assertIn('{"emit ", tr("value")}', self.source)

    def test_capability_forever_and_foreach_survive_qt_no_keywords(self):
        self.assertLine(self.source, "    for (;;) {")
        self.assertLine(self.source, "    Q_FOREACH (const QString &p, parts)")

    def test_capability_moc_include_deleted(self):
        self.assertNotIn("moc_counter.cpp", self.source)

    def test_capability_cmake_turns_automoc_off_and_adds_reflection(self):
        self.assertNotIn("AUTOMOC ON", self.cmake)
        self.assertLine(self.cmake, "set_target_properties(rewrites PROPERTIES AUTOMOC OFF)")
        self.assertLine(self.cmake, "set(CMAKE_CXX_STANDARD 26)")
        self.assertLine(self.cmake, "set(CMAKE_CXX_EXTENSIONS OFF)")
        self.assertLine(self.cmake, "add_compile_definitions(QT_NO_KEYWORDS)")
        self.assertLine(self.cmake, "add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-freflection>)")
        self.assertLine(self.cmake, "include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)")

    def test_capability_everything_here_migrates_without_manual_work(self):
        self.assertEqual(self.result.report.of_status(MANUAL), [])

    def test_capability_enums_are_reported_partial_for_the_missing_friend(self):
        self.assertEqual(len(items(self.result, "Q_ENUM", PARTIAL)), 1)
        self.assertEqual(len(items(self.result, "Q_FLAG", PARTIAL)), 1)


class Reports(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_fixture("reports")

    def detail(self, construct: str, status: str) -> str:
        found = items(self.result, construct, status)
        self.assertTrue(found, f"no {status} item for {construct}")
        return found[0].detail

    def test_limit_q_private_slot_reported_and_left_in_place(self):
        self.detail("Q_PRIVATE_SLOT", MANUAL)
        self.assertIn("    Q_PRIVATE_SLOT(d_func(), void _q_done())", self.result.files["private_slot.h"])

    def test_limit_q_plugin_metadata_reported(self):
        self.detail("Q_PLUGIN_METADATA", MANUAL)

    def test_limit_unknown_macro_reported(self):
        self.assertIn("unknown macro", self.detail("Q_FIXTURE_UNKNOWN", MANUAL))

    def test_limit_revision_reported(self):
        self.detail("Q_REVISION", MANUAL)
        self.detail("Q_INVOKABLE revision", PARTIAL)

    def test_limit_template_class_reported_from_moc_error(self):
        self.assertIn("Template classes not supported", self.detail("moc", MANUAL))
        self.assertNotIn("templated.h", self.result.files)

    def test_limit_signal_with_default_arguments_migrated_and_reported(self):
        self.assertIn("progress", self.detail("signal with default arguments", PARTIAL))
        self.assertIn("{ rqt::emit{this}(percent, text); }", self.result.files["signals.h"])

    def test_limit_slot_with_default_arguments_reported(self):
        self.detail("slot with default arguments", PARTIAL)

    def test_limit_overloaded_signal_reported_once(self):
        self.assertEqual(len(items(self.result, "overloaded signal", PARTIAL)), 1)

    def test_limit_property_attribute_without_annotation_field_reported(self):
        self.assertIn("designable", self.detail("Q_PROPERTY", PARTIAL))

    def test_limit_property_whose_accessor_is_not_in_the_class_reported(self):
        self.assertIn("baseValue", self.detail("Q_PROPERTY", MANUAL))

    def test_limit_invokable_constructor_reported_once(self):
        self.assertEqual(len(items(self.result, "Q_INVOKABLE constructor", MANUAL)), 1)

    def test_limit_q_gadget_left_in_place_and_reported(self):
        header = self.result.files["gadget.h"]
        self.assertIn("    Q_GADGET", header.splitlines())
        self.assertNotIn("rqt::gadget", header)
        self.assertIn("no gadget annotation", self.detail("Q_GADGET", MANUAL))

    def test_capability_q_interfaces_becomes_a_metacast_override(self):
        header = self.result.files["gadget.h"]
        self.assertIn("    void *qt_metacast(const char *name) override", header.splitlines())
        self.assertIn("qobject_interface_iid<Shape *>()", header)
        self.assertNotIn("rqt::interface", header)
        self.assertNotIn("    Q_INTERFACES(Shape)", header.splitlines())

    def test_limit_qt_wrap_cpp_reported(self):
        self.detail("CMake: qt_wrap_cpp", MANUAL)


class Qml(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_fixture("qml")
        cls.registry = cls.result.files[syntax.QML_HEADER]

    def test_capability_qml_element_registered_under_the_module_uri_and_version(self):
        self.assertIn('qmlRegisterType<Plain>("Demo", 2, 1, "Plain");', self.registry)

    def test_capability_qml_named_element_keeps_its_name(self):
        self.assertIn('qmlRegisterType<Renamed>("Demo", 2, 1, "Fancy");', self.registry)

    def test_capability_qml_anonymous_registered_anonymous(self):
        self.assertIn('qmlRegisterAnonymousType<Hidden>("Demo", 2);', self.registry)

    def test_capability_qml_uncreatable_keeps_its_reason_literal(self):
        self.assertIn('qmlRegisterUncreatableType<Abstract>("Demo", 2, 1, "Abstract", "Abstract \\"base\\"");',
                      self.registry)

    def test_limit_qml_singleton_reported(self):
        self.assertEqual([i.detail for i in items(self.result, "QML registration", MANUAL)],
                         ["`Lonely` uses QML.Singleton; register it by hand"])

    def test_capability_registration_called_before_the_engine(self):
        lines = self.result.files["main.cpp"].splitlines()
        at = lines.index("    QQmlApplicationEngine engine;")
        self.assertEqual(lines[at - 1], "    rqt_register_qml_types();")
        self.assertIn('#include "rqt_qml_types.hpp"', lines)

    def test_capability_qml_class_info_kept_in_the_meta_object(self):
        self.assertIn('class [[=rqt::classinfo{"QML.Element", "auto"}]] Plain', self.result.files["items.h"])
        self.assertIn("    QML_ELEMENT", self.result.files["items.h"].splitlines())

    def test_capability_qml_module_stops_generating_qmltypes(self):
        self.assertIn("    URI Demo NO_GENERATE_QMLTYPES", self.result.files["CMakeLists.txt"].splitlines())


class Binding(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run_fixture("binding")
        cls.header = cls.result.files["binding.h"]
        cls.source = cls.result.files["binding.cpp"]

    def body(self, name: str) -> str:
        """The text of `class <name> ... };` in the migrated header."""
        start = self.header.index(f"class {name} ")
        return self.header[start : self.header.index("\n};", start)]

    def test_capability_class_without_constructor_gets_a_forwarding_one_that_binds(self):
        self.assertIn("template <class... Args> explicit Quiet(Args &&...args) : rqt::Object<QObject>"
                      "(std::forward<Args>(args)...) { bind(); }", self.body("Quiet"))
        self.assertNotIn("using ", self.body("Quiet"))

    def test_capability_derived_class_forwards_to_the_new_direct_base(self):
        self.assertIn("explicit Child(Args &&...args) : rqt::Object<Plain>(std::forward<Args>(args)...) { bind(); }",
                      self.body("Child"))

    def test_capability_inline_constructors_bind_first(self):
        body = self.body("Inline")
        self.assertIn("explicit Inline(QObject *parent = nullptr) : rqt::Object<QObject>(parent) { bind(); }", body)
        self.assertIn("    {\n        bind();\n        m_v += 1;\n    }", body)

    def test_limit_deleted_copy_constructor_is_left_alone(self):
        self.assertIn("    Inline(const Inline &) = delete;", self.body("Inline").splitlines())

    def test_capability_defaulted_constructor_gets_a_body(self):
        self.assertIn("    Defaulted() { bind(); }", self.body("Defaulted").splitlines())

    def test_capability_class_with_no_constructor_gets_a_public_default_one(self):
        lines = self.body("Bare").splitlines()
        self.assertEqual(lines[lines.index("    Bare() { bind(); }") - 1], "public:")
        self.assertEqual(lines[lines.index("    Bare() { bind(); }") + 1], "private:")

    def test_capability_out_of_line_constructors_bind(self):
        self.assertIn("    : rqt::Object<QObject>(parent)\n{\n    bind();\n}", self.source)
        self.assertIn("rqt::Object<QObject>(nullptr) { bind(); (void)first; (void)second; }", self.source)

    def test_capability_generated_members_join_the_following_public_label(self):
        self.assertNotIn("public:\n    static QMetaObject const &staticMetaObject;\npublic:", self.header)

    def test_capability_qobject_cast_target_gets_the_opt_in(self):
        self.assertIn("    static QMetaObject const &staticMetaObject;", self.body("Casted").splitlines())
        self.assertIn("inline RQT_STATIC_META_OBJECT(Casted);", self.header.splitlines())

    def test_capability_pointer_to_member_connect_sender_gets_the_opt_in(self):
        self.assertIn("inline RQT_STATIC_META_OBJECT(Linked);", self.header.splitlines())

    def test_capability_class_scoped_static_metaobject_use_gets_the_opt_in(self):
        self.assertIn("inline RQT_STATIC_META_OBJECT(Meta);", self.header.splitlines())

    def test_capability_class_used_as_a_property_type_gets_the_opt_in(self):
        self.assertIn("inline RQT_STATIC_META_OBJECT(Plain);", self.header.splitlines())

    def test_capability_class_nothing_refers_to_stays_without_the_opt_in(self):
        self.assertNotIn("staticMetaObject", self.body("Quiet"))
        self.assertNotIn("RQT_STATIC_META_OBJECT(Quiet)", self.header)

    def test_capability_opt_in_reasons_are_reported(self):
        reasons = {i.detail for i in items(self.result, "static metaobject opt-in")}
        self.assertIn("qobject_cast", reasons)
        self.assertIn("pointer-to-member signal", reasons)

    def test_capability_interface_metacast_answers_the_interface_after_the_base(self):
        body = self.body("Circle")
        self.assertLess(body.index("rqt::Object<QObject>::qt_metacast(name)"), body.index("qobject_interface_iid"))

    def test_capability_every_constructor_of_every_class_is_bound(self):
        self.assertEqual(self.result.report.of_status(MANUAL), [])


class Syntax(unittest.TestCase):
    def test_capability_target_spelling_changes_in_one_place(self):
        with mock.patch.object(syntax, "SIGNAL", "RQT_SIGNAL"), \
             mock.patch.object(syntax, "BIND", "rqt_bind();"), \
             mock.patch.object(syntax, "object_base", lambda b: f"rqt::QtObject<{b}>"), \
             mock.patch.object(syntax, "STATIC_META_OBJECT_DECLARATION", "static QMetaObject const& meta;"), \
             mock.patch.object(syntax, "STATIC_META_OBJECT_DEFINITION", "RQT_DEFINE({cls});"):
            result = run_fixture("binding")
        header = result.files["binding.h"]
        self.assertIn("    static QMetaObject const& meta;", header.splitlines())
        self.assertIn(": public rqt::QtObject<QObject>", header)
        self.assertIn("{ rqt_bind(); }", header)
        self.assertIn("RQT_DEFINE(Casted);", header.splitlines())
        self.assertIn("RQT_SIGNAL void changed(int value)", header)


class Text(unittest.TestCase):
    def test_capability_blank_masks_comments_and_literal_contents(self):
        src = 'a("x;{", \'}\'); // emit {\n/* { */ b R"(})"'
        out = blank(src)
        self.assertEqual(len(out), len(src))
        self.assertNotIn("{", out)
        self.assertNotIn("}", out)
        self.assertIn("\n", out)

    def test_limit_digit_separator_is_not_a_char_literal(self):
        self.assertEqual(blank("int x = 1'000; f('{');"), "int x = 1'000; f(' ');")

    def test_capability_split_top_level_ignores_nested_commas(self):
        text = "QMap<int, QString> m, std::pair<int,int> p = {1, 2}"
        self.assertEqual([text[a:b].strip() for a, b in split_top_level(text, 0, len(text))],
                         ["QMap<int, QString> m", "std::pair<int,int> p = {1, 2}"])


class Cli(unittest.TestCase):
    def test_capability_writes_tree_diff_and_report_without_touching_the_source(self):
        src = os.path.join(FIXTURES, "rewrites", "src")
        before = read_dir(src)
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "ported")
            stderr = io.StringIO()
            with contextlib.redirect_stderr(stderr):
                rc = cli.main([src, "-o", out, "--json-dir", os.path.join(FIXTURES, "rewrites", "moc"),
                               "--reflect-moc-include", os.path.join(tmp, "include"),
                               "--diff", os.path.join(tmp, "m.diff"), "--report", os.path.join(tmp, "r.md")])
            self.assertEqual(rc, 0)
            self.assertEqual(sorted(os.listdir(out)), sorted(before))
            with open(os.path.join(out, "CMakeLists.txt"), encoding="utf-8") as f:
                self.assertIn("include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)", f.read())
            with open(os.path.join(tmp, "m.diff"), encoding="utf-8") as f:
                files, added, removed = diff_stats(f.read())
            self.assertEqual(files, 3)
            self.assertGreater(added, 0)
            with open(os.path.join(tmp, "r.md"), encoding="utf-8") as f:
                self.assertIn("# rqt-migrate report: src", f.read())
            self.assertIn("auto", stderr.getvalue())
        self.assertEqual(before, read_dir(src))

    def test_limit_no_moc_and_no_json_dir_fails_cleanly(self):
        with mock.patch.object(moc, "find_moc", return_value=None), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(cli.main([FIXTURES, "--reflect-moc-include", "include"]), 2)

    def test_capability_auto_items_dominate_the_examples_report(self):
        result = run_fixture("rewrites")
        counts = result.report.counts()
        self.assertGreater(counts[AUTO], counts[PARTIAL] + counts[MANUAL])


if __name__ == "__main__":
    unittest.main()
