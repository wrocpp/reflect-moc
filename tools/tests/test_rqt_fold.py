"""Tests for the fold step of tools/rqt-migrate: a signals-only Q_OBJECT base folded into its class template.

The fixture `fold` is examples/qt/observable_models_qvariant itself (a symlink), so the twin that the benchmark
measures is the input that is ported. tests/fold_e2e.sh builds the result and drives it. `fold_limits` holds the
shapes the step must refuse or only half rewrite. Run from tools/: python3 -m unittest discover -s tests
"""

from __future__ import annotations

import os
import sys
import unittest

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)

from rqt_migrate import moc, syntax  # noqa: E402
from rqt_migrate.report import AUTO, MANUAL, PARTIAL  # noqa: E402
from rqt_migrate.tree import Options, migrate  # noqa: E402

FIXTURES = os.path.join(TOOLS, "tests", "fixtures")
SIGNAL_MEMBERS = (
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryAboutToBeAdded;',
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryAdded;',
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryAboutToBeRemoved;',
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryRemoved;',
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryAboutToBeChanged;',
    '  [[=rqt::names("key")]] rqt::signal<void(const Key&)> entryChanged;',
    '  [[=rqt::names("from, to")]] rqt::signal<void(const Key&, const Key&)> entryAboutToBeMoved;',
    '  [[=rqt::names("from, to")]] rqt::signal<void(const Key&, const Key&)> entryMoved;',
    "  rqt::signal<void()> aboutToBeCleared;",
    "  rqt::signal<void()> cleared;",
)


def run(case: str, style: str = syntax.QTLIKE, macros: str = syntax.MACROS_RQT):
    root = os.path.join(FIXTURES, case)
    return migrate(os.path.join(root, "src"), moc.saved(os.path.join(root, "moc")),
                   Options(reflect_moc_include="../include", title=case, style=style, macros=macros))


def code(text: str) -> str:
    """The lines that are not whole-line comments."""
    return "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("//"))


def where(result, path: str, status: str = MANUAL):
    return [i for i in result.report.items if i.path == path and i.status == status]


class FoldTwin(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run("fold")
        cls.template = cls.result.files["observable_container.h"]
        cls.model = cls.result.files["container_model.h"]

    def test_capability_signals_become_typed_members_in_the_template_before_its_private_section(self):
        lines = self.template.splitlines()
        first = lines.index(SIGNAL_MEMBERS[0])
        self.assertEqual(tuple(lines[first : first + len(SIGNAL_MEMBERS)]), SIGNAL_MEMBERS)
        self.assertEqual(lines[first + len(SIGNAL_MEMBERS) + 1], " private:")

    def test_capability_template_derives_from_the_qt_base_with_the_library_macro_first(self):
        lines = self.template.splitlines()
        at = lines.index("class ObservableContainer : public QObject {")
        self.assertEqual(lines[at + 1], "  RQT_OBJECT")

    def test_capability_constructors_name_the_qt_base(self):
        self.assertIn("explicit ObservableContainer(QObject* parent = nullptr) : QObject(parent) {}", self.template)
        self.assertIn(": QObject(parent), m_items(std::move(items)) {}", self.template)

    def test_capability_emit_sites_pass_the_key_unboxed(self):
        self.assertIn("    emit entryAboutToBeAdded(key);", self.template.splitlines())
        self.assertIn("      emit entryMoved(from, to);", self.template.splitlines())
        self.assertNotIn("fromValue", self.template)

    def test_capability_no_code_names_the_base_or_a_qvariant_in_the_template_file(self):
        self.assertNotIn("ContainerSignals", code(self.template))
        self.assertNotIn("QVariant", code(self.template))

    def test_capability_the_base_include_becomes_the_qt_and_library_includes(self):
        lines = self.template.splitlines()
        at = lines.index(syntax.QOBJECT_INCLUDE)
        self.assertEqual(lines[at + 1], syntax.HEADER_INCLUDE)
        self.assertNotIn('"container_signals.h"', self.template)

    def test_capability_lambdas_take_the_typed_key_and_drop_the_unboxing(self):
        self.assertIn("[this](const Key& key) {", self.model)
        self.assertIn("[this](const Key& from, const Key& to) {", self.model)
        self.assertIn("[this](const Key&, const Key&) { endMoveRows(); }", self.model)
        self.assertNotIn(".value<Key>()", self.model)
        self.assertNotIn("const QVariant&", self.model)

    def test_capability_connect_names_the_instantiation_alias_not_the_removed_base(self):
        self.assertIn("connect(m_source, &Source::entryAdded, this,", self.model)
        self.assertNotIn("ContainerSignals", code(self.model))

    def test_capability_a_qvariant_that_is_not_a_signal_argument_is_left_alone(self):
        self.assertIn("return QVariant::fromValue(key);", self.model)
        self.assertIn("std::function<QVariant(const Key&, const Value&)>", self.model)

    def test_capability_the_base_is_not_migrated_as_a_class(self):
        self.assertNotIn("container_signals.h", self.result.files)

    def test_capability_the_unused_base_file_is_reported_for_deletion(self):
        [item] = where(self.result, "container_signals.h", PARTIAL)
        self.assertIn("delete it", item.detail)

    def test_capability_nothing_needs_a_person(self):
        self.assertEqual([], [i for i in self.result.report.items if i.status == MANUAL])

    def test_capability_every_boxed_emit_is_counted(self):
        self.assertEqual(10, len([i for i in self.result.report.items if i.construct == "QVariant boxing"]))

    def test_capability_every_lambda_connect_is_counted(self):
        self.assertEqual(9, len([i for i in self.result.report.items if i.construct == "connect" and i.status == AUTO]))


class FoldStyles(unittest.TestCase):
    def test_capability_annotations_style_folds_the_same_way(self):
        result = run("fold", syntax.ANNOTATIONS)
        self.assertIn("rqt::signal<void(const Key&)> entryAdded;", result.files["observable_container.h"])
        self.assertIn("[this](const Key& key) {", result.files["container_model.h"])
        self.assertEqual([], [i for i in result.report.items if i.status == MANUAL])

    def test_capability_compat_macros_keep_qt_spelling_and_the_compat_header(self):
        result = run("fold", syntax.QTLIKE, syntax.MACROS_Q)
        template = result.files["observable_container.h"]
        self.assertIn("  Q_OBJECT", template.splitlines())
        self.assertIn(syntax.COMPAT_INCLUDE, template.splitlines())
        self.assertNotIn(syntax.HEADER_INCLUDE, template)


class FoldLimits(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run("fold_limits")

    def test_limit_a_template_without_a_key_alias_is_not_folded_and_its_base_stays(self):
        [item] = where(self.result, "no_key.h")
        self.assertIn("no `using Key = ...` alias", item.detail)
        self.assertNotIn("no_key.h", self.result.files)

    def test_limit_two_templates_deriving_from_the_base_are_not_folded(self):
        [item] = where(self.result, "two_templates.h")
        self.assertIn("2 classes derive from it", item.detail)
        self.assertIn("rqt::signal<void(const QVariant&)> changed;", self.result.files["two_templates.h"])

    def test_limit_a_base_that_holds_data_is_not_folded(self):
        [item] = where(self.result, "extra_member.h")
        self.assertEqual((item.line, item.construct), (9, "signals-only base"))
        self.assertNotIn("extra_member.h", self.result.files)

    def test_limit_a_signal_with_a_typed_parameter_is_not_folded(self):
        [item] = where(self.result, "typed_parameter.h")
        self.assertIn("`resized` has a parameter that is not `const QVariant&`", item.detail)
        self.assertNotIn("typed_parameter.h", self.result.files)


class FoldSites(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run("fold_limits")
        cls.text = cls.result.files["sites.h"]
        cls.manual = {i.line: i for i in where(cls.result, "sites.h")}

    def test_capability_a_base_in_the_templates_own_file_is_removed(self):
        self.assertNotIn("class SiteSignals", self.text)
        self.assertIn("class SiteHolder : public QObject {", self.text)
        self.assertEqual([], where(self.result, "sites.h", PARTIAL))

    def test_limit_an_emit_that_does_not_box_with_fromvalue_is_left_and_reported(self):
        self.assertIn("emit removed(QVariant(5));", self.text)
        self.assertIn("passes something other than `QVariant::fromValue(x)`", self.manual[21].detail)
        self.assertIn("emit added(key);", self.text)

    def test_limit_a_connect_to_a_function_is_reported_with_its_line(self):
        self.assertIn("connect(holder, &Holder::added, this, &Watcher::onAdded);", self.text)
        self.assertIn("is not connected to a lambda", self.manual[31].detail)

    def test_limit_a_string_connect_is_reported(self):
        self.assertIn("SIGNAL(removed(QVariant))", self.text)
        self.assertEqual("string connect", self.manual[32].construct)

    def test_limit_a_lambda_that_uses_the_variant_in_another_way_is_left_whole(self):
        self.assertIn("[this](const QVariant& from, const QVariant& to) {", self.text)
        self.assertIn("`from` is used other than as `from.value<T>()`", self.manual[33].detail)

    def test_limit_a_base_pointer_connect_with_no_alias_outside_the_template_is_reported(self):
        self.assertIn("&SiteSignals::removed", self.text)
        self.assertIn("say which `SiteHolder` it connects to", self.manual[40].detail)

    def test_limit_every_reported_site_has_a_line(self):
        self.assertTrue(all(i.line > 0 for i in self.manual.values()))


class FoldLambdas(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.result = run("fold_limits")
        cls.text = cls.result.files["lambdas.h"]

    def test_capability_every_use_as_value_of_t_becomes_the_parameter_itself(self):
        self.assertIn("[this](const Key& k) { use(k); use(k + 1); });", self.text)

    def test_capability_an_unused_named_or_unnamed_parameter_takes_the_key_type(self):
        self.assertIn("[this](const Key& ignored) { use(0); });", self.text)
        self.assertIn("[this](const Key&) mutable { use(1); });", self.text)

    def test_capability_a_conversion_in_the_unboxing_declaration_is_kept(self):
        self.assertIn("[this](const int& k) {\n      const long n = k;\n", self.text)

    def test_limit_a_parameter_with_another_use_keeps_its_lambda(self):
        self.assertIn("[this](const QVariant& k) { use(k.value<int>() + k.toInt()); });", self.text)

    def test_limit_a_class_without_a_key_alias_cannot_type_an_unused_parameter(self):
        self.assertIn("[this](const QVariant&) { use(2); });", self.text)
        [item] = [i for i in where(self.result, "lambdas.h") if i.line == 43]
        self.assertIn("has no `using Key = ...` alias", item.detail)


if __name__ == "__main__":
    unittest.main()
