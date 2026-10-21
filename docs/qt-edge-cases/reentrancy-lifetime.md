# reentrancy-lifetime

A receiver that mutates the container it is announced by, a receiver that deletes, destruction order, and models made
or cut loose in the middle of an announcement. The rule for the first group: a mutator called from a receiver of the
same container (a signal of the container, or of a model that follows it) is refused: it returns false (0, an empty
container; `resetWith` does nothing), changes nothing, emits nothing, and a debug build asserts. The first six rows run
every mutator (insert, remove, replace, editValue, move, insertRange, removeRange, insertMany, removeMany, takeMany,
resetWith, clear, and setData of the model) from the "about" and the "done" signal of every change, on a list and a map.
Before the rule each of these was accepted and either corrupted the model or ended the process (see the commit).
Deleting a container inside its own signal's receiver is undefined behaviour, as for any QObject that emits; only
`deleteLater` is supported, and R-17 shows it.

| id | case | source | test | verdict |
|---|---|---|---|---|
| R-01 | a receiver of entryAboutToBeAdded/Added/Removed/Changed/Moved (connected after the model) calls any mutator | doc:QAbstractItemModel::beginInsertRows | capability_edge_reentrancy::container_single_entry_signals | fixed |
| R-02 | a receiver of the range and batch signals (rangeAdded, entriesRemoved, ...) calls any mutator | doc:QAbstractItemModel::beginRemoveRows | capability_edge_reentrancy::container_batch_signals | fixed |
| R-03 | a receiver of aboutToBeCleared, cleared, aboutToBeReset or resetCompleted calls any mutator | doc:QAbstractItemModel::beginResetModel | capability_edge_reentrancy::container_clear_and_reset_signals | fixed |
| R-04 | the same receivers, connected before the model's own connection, so they run first | own | capability_edge_reentrancy::receiver_before_the_model | fixed |
| R-05 | a receiver of rowsAboutToBeInserted, rowsInserted, rowsAboutToBeRemoved, rowsRemoved, rowsMoved or dataChanged of the model mutates the container, or calls setData | doc:QAbstractItemModel::beginMoveRows | capability_edge_reentrancy::model_row_and_data_signals | fixed |
| R-06 | a receiver of modelAboutToBeReset or modelReset mutates the container | doc:QAbstractItemModel::beginResetModel | capability_edge_reentrancy::model_reset_signals | fixed |
| R-07 | a receiver connected before the model sees the container already changed while the model has the old rows: the model's row count is what it announced and reading every row stays in range | own | capability_edge_reentrancy_order::an_early_receiver_reads_a_model_that_has_not_heard_yet | works |
| R-08 | the container is destroyed before its model (a list model and a table model): each resets to empty once, and rowCount, index, data, setData, flags, headerData, roleNames and a kept index are safe | doc:QObject::destroyed | capability_edge_lifetime::container_destroyed_before_its_model | fixed |
| R-09 | the model is destroyed before its container, which goes on working | own | capability_edge_lifetime::model_destroyed_before_its_container | works |
| R-10 | the container is destroyed first with a selection model, a proxy and a persistent index alive: the selection clears, the index goes invalid, the proxy empties | doc:QAbstractItemModel::modelReset | capability_edge_lifetime::container_destroyed_before_selection_proxy_and_persistent_index | fixed |
| R-11 | the model is destroyed first with the same three alive | doc:QPersistentModelIndex | capability_edge_lifetime::model_destroyed_before_container_with_selection_proxy_and_persistent_index | works |
| R-12 | three models on one container, destroyed in all six orders, the container last; the container changes between the deletions | own | capability_edge_lifetime::models_destroyed_in_every_order_before_the_container | works |
| R-13 | the container destroyed at each of four positions among three models (24 orders): each model left resets to empty once | own | capability_edge_lifetime::container_destroyed_at_every_position_among_three_models | fixed |
| R-14 | a model is made inside a receiver of an "about" signal (container or model), so it never heard the beginning of that change | own | capability_edge_lifetime::model_created_in_an_about_receiver | fixed |
| R-15 | a model is made inside a receiver of a "done" signal of the container, including a batch removal | own | capability_edge_lifetime::model_created_in_a_done_receiver | works |
| R-16 | a model is made inside a "done" receiver of the model, between the runs of a batch removal | own | capability_edge_lifetime::model_created_in_a_model_done_receiver | works |
| R-17 | the container calls deleteLater from its own receiver: the announcement completes, the container goes at the next event loop pass and its model resets to empty (deleting it in the receiver is undefined behaviour) | doc:QObject::deleteLater | capability_edge_lifetime::container_deleted_later_inside_its_own_receiver | fixed |
| R-18 | a model is deleted inside a receiver of the container, before or after it heard the announcement, with a proxy on it and a second model on the container that carries on | doc:QObject::~QObject | capability_edge_lifetime::model_deleted_inside_a_container_receiver | works |
| R-19 | the model's connections are cut before it heard the "about" signal: it keeps its rows, reads the live container for them and nothing is read out of range | own | capability_edge_lifetime::a_connection_cut_before_the_model_heard_the_announcement | works |
| R-20 | the model's connections are cut after it began a change: it has begun a change that nothing ends, and the model cannot know | own | limit_edge_lifetime_cut_connection::a_connection_cut_after_the_model_began_the_change | pinned |
| R-21 | a parent deletes models, a proxy and then the container (children in that order) | doc:QObject::~QObject | capability_edge_lifetime::parent_deletes_models_before_the_container | works |
| R-22 | a parent deletes the container before its models and a proxy (the container is the first child) | doc:QObject::~QObject | capability_edge_lifetime::parent_deletes_the_container_before_the_models | fixed |
| R-23 | a model that is a child of its container: the container's destruction resets it before it is deleted | doc:QObject::~QObject | capability_edge_lifetime::a_model_that_is_a_child_of_its_container | fixed |
