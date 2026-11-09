#include <doctest/doctest.h>

#include "catalist/model/undoStack.hpp"
#include "support/testSupport.hpp"

using catalist::historyAction;
using catalist::undoStack;
using catalist::test::makeEntry;

namespace {

historyAction mutation(int id) {
  return {historyAction::kMutate, catalist::mediaType::kAnime,
          makeEntry().id(id), makeEntry().id(id).score(5)};
}

}  // namespace

TEST_CASE("undoStack starts empty") {
  undoStack history;
  CHECK_FALSE(history.canUndo());
  CHECK_FALSE(history.canRedo());
  CHECK_FALSE(history.takeUndo().has_value());
  CHECK_FALSE(history.takeRedo().has_value());
}

TEST_CASE("undoStack undo then redo returns actions in order") {
  undoStack history;
  history.record(mutation(1));
  history.record(mutation(2));

  auto undone = history.takeUndo();
  REQUIRE(undone.has_value());
  CHECK(undone->before.id == 2);
  CHECK(history.canRedo());

  auto redone = history.takeRedo();
  REQUIRE(redone.has_value());
  CHECK(redone->before.id == 2);
  CHECK_FALSE(history.canRedo());
  CHECK(history.takeUndo()->before.id == 2);
  CHECK(history.takeUndo()->before.id == 1);
}

TEST_CASE("undoStack recording a new action clears redo") {
  undoStack history;
  history.record(mutation(1));
  (void)history.takeUndo();
  REQUIRE(history.canRedo());
  history.record(mutation(3));
  CHECK_FALSE(history.canRedo());
}

TEST_CASE("undoStack clear forgets both directions") {
  undoStack history;
  history.record(mutation(1));
  history.record(mutation(2));
  (void)history.takeUndo();
  history.clear();
  CHECK_FALSE(history.canUndo());
  CHECK_FALSE(history.canRedo());
}
