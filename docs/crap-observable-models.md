# CRAP of examples/qt/observable_models

CRAP = CC^2 * (1 - coverage)^3 + CC, limit 8; at full coverage the score is the cyclomatic complexity itself, so a function above the limit
at full coverage has to be split, and a function below it that is not covered is a missing test.

- Date: 2026-10-22
- Tools: g++ 16.2.0, gcov 16.2.0, lizard 1.24.1, gcovr 8.7, crap.py of https://github.com/wrocpp/claude-marketplace at 386447d
- Tests: the whole Qt suite of this repository built with `--coverage -O0` and run (100% tests passed, 0 tests failed out of 138), compile-fail tests excluded
- Reproduce: `docker run --rm -v "$PWD":/src -w /src -e RQT_CRAP_OUT=docs/crap-observable-models.md reflect-moc/gcc16-qt610 bash scripts/crap_observable_models.sh`
- Coverage is line coverage of the function's own lines (gcovr), merged over every test executable; complexity is lizard's.

**266 functions, 2 above 8.**

## Above the limit

| header | function | line | CC | coverage | CRAP | status | proposed split |
|---|---|---|---|---|---|---|---|
| container_model.hpp | `observable::ContainerModelBase::connectSource` | 152 | 13 | 100% | 13.0 | open | split into connectEntries (single add, remove, change), connectMoves, connectRanges, connectBatches and connectResets, each a run of connects (CC 2 to 4) |
| observable_container.hpp | `observable::ObservableContainer::presentKeys` | 315 | 8 | 93% | 8.0 | open | one function per branch (presentKeysSorted with the std::set, presentKeysHashed with the QSet) behind a one-line if constexpr, CC 4 each, and a test of each branch's duplicate and absent keys to cover the 7 percent |

## At the limit (above 7 and not above 8: one more branch puts them over)

- container_model.hpp:353 `observable::ContainerTableModel::data`: CC 8, coverage 100%, CRAP 8.0
- observable_container.hpp:186 `observable::ObservableContainer::insertMany`: CC 8, coverage 100%, CRAP 8.0
- selection_tracker.hpp:321 `observable::applySelectionValue`: CC 8, coverage 100%, CRAP 8.0

## Every function

| header | function | line | CC | coverage | CRAP |
|---|---|---|---|---|---|
| container_model.hpp | `observable::ContainerModelBase::connectSource` | 152 | 13 | 100% | 13.0 |
| observable_container.hpp | `observable::ObservableContainer::presentKeys` | 315 | 8 | 93% | 8.0 |
| container_model.hpp | `observable::ContainerTableModel::data` | 353 | 8 | 100% | 8.0 |
| observable_container.hpp | `observable::ObservableContainer::insertMany` | 186 | 8 | 100% | 8.0 |
| selection_tracker.hpp | `observable::applySelectionValue` | 321 | 8 | 100% | 8.0 |
| container_model.hpp | `observable::ContainerModelBase::setData` | 92 | 7 | 100% | 7.0 |
| container_model.hpp | `observable::ContainerModel::data` | 316 | 7 | 100% | 7.0 |
| container_traits.hpp | `observable::drop_unusable_keys` | 223 | 7 | 100% | 7.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::headerData` | 57 | 7 | 100% | 7.0 |
| observable_container.hpp | `observable::ObservableContainer::insertRange` | 150 | 7 | 100% | 7.0 |
| observable_container.hpp | `observable::ObservableContainer::removeRange` | 168 | 7 | 100% | 7.0 |
| selection_tracker.hpp | `observable::SelectionTracker::rowsRemoved` | 191 | 7 | 100% | 7.0 |
| selection_tracker.hpp | `observable::SelectionTracker::requires` | 258 | 7 | 100% | 7.0 |
| container_model.hpp | `observable::ContainerModelBase::insertRuns` | 256 | 6 | 100% | 6.0 |
| container_model.hpp | `observable::ContainerModelBase::removeRuns` | 273 | 6 | 100% | 6.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::data` | 50 | 6 | 100% | 6.0 |
| observable_container.hpp | `observable::ObservableContainer::takeMany` | 224 | 6 | 100% | 6.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::renameRow` | 265 | 6 | 100% | 6.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::renameColumn` | 281 | 6 | 100% | 6.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::setValues` | 314 | 6 | 100% | 6.0 |
| selection_tracker.hpp | `observable::SelectionTracker::isSelected` | 157 | 6 | 100% | 6.0 |
| selection_tracker.hpp | `observable::SelectionTracker::rowsInserted` | 177 | 6 | 100% | 6.0 |
| selection_tracker.hpp | `observable::selectedValue` | 403 | 6 | 100% | 6.0 |
| container_model.hpp | `observable::ContainerTableModel::headerData` | 362 | 5 | 100% | 5.0 |
| observable_container.hpp | `observable::ObservableContainer::removeMany` | 206 | 5 | 100% | 5.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::setValue` | 298 | 5 | 100% | 5.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::sameValue` | 382 | 5 | 100% | 5.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::freshKeys` | 391 | 5 | 100% | 5.0 |
| row_index.hpp | `observable::LiveSlots::slot_of_row` | 97 | 5 | 100% | 5.0 |
| observable_matrix.hpp | `observable::contiguous_runs` | 60 | 4 | 88% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::sortedIndices` | 404 | 4 | 88% | 4.0 |
| selection_tracker.hpp | `observable::row_runs` | 306 | 4 | 88% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::changedCells` | 443 | 4 | 89% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::runsByRow` | 455 | 4 | 90% | 4.0 |
| container_traits.hpp | `observable::to_row` | 47 | 4 | 100% | 4.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::cellData` | 66 | 4 | 100% | 4.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::connectContent` | 164 | 4 | 100% | 4.0 |
| observable_container.hpp | `observable::ObservableContainer::insert` | 78 | 4 | 100% | 4.0 |
| observable_container.hpp | `observable::ObservableContainer::replace` | 99 | 4 | 100% | 4.0 |
| observable_container.hpp | `observable::ObservableContainer::editValue` | 117 | 4 | 100% | 4.0 |
| observable_container.hpp | `observable::ObservableContainer::move` | 134 | 4 | 100% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::valueAt` | 140 | 4 | 100% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::addRows` | 177 | 4 | 100% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::addColumns` | 191 | 4 | 100% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::clear` | 329 | 4 | 100% | 4.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::rectanglesOf` | 480 | 4 | 100% | 4.0 |
| selection_tracker.hpp | `observable::all_distinct` | 83 | 4 | 100% | 4.0 |
| selection_tracker.hpp | `observable::detail::all_custom` | 92 | 4 | 100% | 4.0 |
| selection_tracker.hpp | `observable::applySelectionValue` | 340 | 4 | 100% | 4.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 186 | 3 | 83% | 3.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 212 | 3 | 83% | 3.0 |
| selection_tracker.hpp | `observable::SelectionTracker::requires` | 242 | 3 | 83% | 3.0 |
| container_model.hpp | `observable::ContainerModelBase::flags` | 86 | 3 | 100% | 3.0 |
| container_model.hpp | `observable::ContainerModelBase::valueAt` | 117 | 3 | 100% | 3.0 |
| container_model.hpp | `observable::ContainerTableModel::editable` | 368 | 3 | 100% | 3.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 86 | 3 | 100% | 3.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::takeContent` | 79 | 3 | 100% | 3.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::changed` | 195 | 3 | 100% | 3.0 |
| observable_container.hpp | `observable::ObservableContainer::remove` | 88 | 3 | 100% | 3.0 |
| observable_container.hpp | `observable::ObservableContainer::resetWith` | 244 | 3 | 100% | 3.0 |
| observable_container.hpp | `observable::ObservableContainer::clear` | 257 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::value` | 134 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::addRow` | 153 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::addColumn` | 164 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::removeRow` | 205 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::removeColumn` | 218 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::removeRows` | 233 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::removeColumns` | 248 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::indexIn` | 372 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::canAdd` | 377 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::appendColumns` | 421 | 3 | 100% | 3.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::eraseColumns` | 435 | 3 | 100% | 3.0 |
| row_index.hpp | `observable::InsertionOrderIndex::compact` | 171 | 3 | 100% | 3.0 |
| row_index.hpp | `observable::MultiInsertionIndex::erase` | 242 | 3 | 100% | 3.0 |
| row_index.hpp | `observable::MultiInsertionIndex::compact` | 263 | 3 | 100% | 3.0 |
| selection_tracker.hpp | `observable::SelectionTracker::count` | 150 | 3 | 100% | 3.0 |
| selection_tracker.hpp | `observable::SelectionTracker::nameTaken` | 269 | 3 | 100% | 3.0 |
| selection_tracker.hpp | `observable::clearSelectionAndNotify` | 364 | 3 | 100% | 3.0 |
| selection_tracker.hpp | `observable::selectedValue` | 423 | 3 | 100% | 3.0 |
| container_model.hpp | `observable::ContainerModelBase::heardTheBeginning` | 143 | 2 | 67% | 2.1 |
| container_model.hpp | `observable::ContainerModelBase::roleNames` | 80 | 2 | 80% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 92 | 2 | 80% | 2.0 |
| container_model.hpp | `observable::ContainerModelBase::rowCount` | 76 | 2 | 100% | 2.0 |
| container_model.hpp | `observable::ContainerModelBase::takeContent` | 130 | 2 | 100% | 2.0 |
| container_model.hpp | `observable::ContainerTableModel::columnCount` | 349 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::fits_rows` | 42 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::usable_key` | 56 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 84 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 85 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 114 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 115 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 135 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 136 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 155 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 156 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 177 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 178 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::contains` | 203 | 2 | 100% | 2.0 |
| container_traits.hpp | `observable::ContainerTraits::can_insert` | 204 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::rowCount` | 47 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::columnCount` | 48 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::heardTheBeginning` | 87 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::finishRowsAdded` | 98 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::finishRowsRemoved` | 103 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::finishColumnsAdded` | 108 | 2 | 100% | 2.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::finishColumnsRemoved` | 113 | 2 | 100% | 2.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::appendRows` | 413 | 2 | 100% | 2.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::eraseRows` | 428 | 2 | 100% | 2.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::joinOrStart` | 469 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::LiveSlots::needs_compaction` | 92 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::LiveSlots::add` | 115 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::LiveSlots::prefix` | 121 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::InsertionOrderIndex::assign` | 135 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::InsertionOrderIndex::erase` | 151 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::MultiSortedIndex::assign` | 191 | 2 | 100% | 2.0 |
| row_index.hpp | `observable::MultiInsertionIndex::assign` | 218 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::SelectionTracker::selectedRole` | 127 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::SelectionTracker::hasSelection` | 143 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::SelectionTracker::clear` | 167 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::SelectionTracker::reset` | 213 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::SelectionTracker::requires` | 231 | 2 | 100% | 2.0 |
| selection_tracker.hpp | `observable::clearSelectionAndNotify` | 352 | 2 | 100% | 2.0 |
| container_model.hpp | `observable::ContainerModelBase::ContainerModelBase` | 100 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModelBase::keyAt` | 112 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModelBase::source` | 113 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModelBase::rowOf` | 120 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModelBase::rowToInsert` | 121 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModelBase::resync` | 124 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModel::ContainerModel` | 309 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModel::ContainerModel` | 313 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModel::editable` | 326 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerModel::editorOf` | 327 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerTableModel::ContainerTableModel` | 346 | 1 | 100% | 1.0 |
| container_model.hpp | `observable::ContainerTableModel::editorOf` | 371 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 83 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 87 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 88 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 89 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 90 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 91 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 113 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 116 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 117 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 118 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 119 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 120 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 121 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 123 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 134 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 137 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 138 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 139 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 140 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 141 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 142 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 143 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 154 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 157 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 158 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 159 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 160 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 161 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 162 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::keys` | 163 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 176 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 179 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 180 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 181 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 182 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 183 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 184 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::size` | 202 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::can_move` | 205 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::insert` | 206 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::erase` | 207 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::assign` | 208 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::move` | 209 | 1 | 100% | 1.0 |
| container_traits.hpp | `observable::ContainerTraits::value` | 210 | 1 | 100% | 1.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::MatrixTableModel` | 40 | 1 | 100% | 1.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::resync` | 73 | 1 | 100% | 1.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::begin` | 96 | 1 | 100% | 1.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::connectStructure` | 119 | 1 | 100% | 1.0 |
| matrix_table_model.hpp | `observable::MatrixTableModel::connectSource` | 188 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::ObservableContainer` | 53 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::ObservableContainer` | 56 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::items` | 61 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::size` | 62 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::contains` | 63 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::value` | 64 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::assertOwnThread` | 68 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::announcing` | 75 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::Announcing::Announcing` | 293 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::Announcing::~Announcing` | 294 | 1 | 100% | 1.0 |
| observable_container.hpp | `observable::ObservableContainer::refusedAsNested` | 303 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::requires` | 55 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::ObservableMatrix` | 88 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::rowCount` | 119 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::columnCount` | 120 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::rowKeys` | 121 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::columnKeys` | 122 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::rowIndexOf` | 125 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::columnIndexOf` | 126 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::hasRow` | 127 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::hasColumn` | 128 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::isUninitialized` | 131 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::assertOwnThread` | 145 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::announcing` | 151 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::Announcing::Announcing` | 353 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::Announcing::~Announcing` | 354 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::refusedAsNested` | 362 | 1 | 100% | 1.0 |
| observable_matrix.hpp | `observable::ObservableMatrix::cellOffset` | 369 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::assign` | 32 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::clear` | 33 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::rowToInsert` | 34 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::rowOf` | 35 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::keyAt` | 36 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::insert` | 37 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::IdentityIndex::erase` | 38 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::assign` | 45 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::clear` | 46 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::rowToInsert` | 48 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::rowOf` | 49 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::keyAt` | 50 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::insert` | 51 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::SortedIndex::erase` | 52 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::clear` | 62 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::count` | 68 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::live` | 69 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::append` | 72 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::kill` | 82 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::alive` | 88 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::row_of` | 94 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::LiveSlots::lowbit` | 113 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::clear` | 140 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::rowToInsert` | 146 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::rowOf` | 147 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::keyAt` | 148 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::insert` | 149 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::contains` | 157 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::size` | 158 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::rename` | 159 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::InsertionOrderIndex::append` | 166 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::clear` | 195 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::rowToInsert` | 196 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::rowOf` | 197 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::keyAt` | 198 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::insert` | 202 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::erase` | 203 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiSortedIndex::first_row_of` | 206 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::clear` | 223 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::rowToInsert` | 229 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::rowOf` | 230 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::keyAt` | 232 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::insert` | 240 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::slot_of` | 252 | 1 | 100% | 1.0 |
| row_index.hpp | `observable::MultiInsertionIndex::append` | 257 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::detail::role_value` | 67 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::is_custom_role` | 78 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::selectedRoleName` | 133 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::appendRoleNames` | 136 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::rowInserted` | 188 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::rowRemoved` | 210 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::requires` | 224 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::SelectionTracker::requires` | 252 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::requires` | 292 | 1 | 100% | 1.0 |
| selection_tracker.hpp | `observable::requires` | 298 | 1 | 100% | 1.0 |
