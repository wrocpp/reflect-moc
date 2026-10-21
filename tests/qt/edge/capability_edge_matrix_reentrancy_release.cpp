// capability_edge_matrix_reentrancy without the debug assertions: a mutator that is called from a receiver of its own matrix
// returns false (0) and changes nothing. QT_NO_DEBUG is what a release build of the test defines.
#ifndef QT_NO_DEBUG
#define QT_NO_DEBUG
#endif
#define EDGE_MATRIX_TEST_NAME "capability_edge_matrix_reentrancy_release"
#include "capability_edge_matrix_reentrancy.cpp"
