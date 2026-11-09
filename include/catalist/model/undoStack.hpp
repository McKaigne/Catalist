#ifndef CATALIST_MODEL_UNDO_STACK_HPP_
#define CATALIST_MODEL_UNDO_STACK_HPP_

#include <optional>
#include <vector>

#include "catalist/model/entry.hpp"

namespace catalist {

struct historyAction {
  enum type { kMutate, kDelete, kAdd } actionType = kMutate;
  mediaType media = mediaType::kAnime;
  entry before;
  entry after;
};

class undoStack {
public:
  // Recording a new action discards the redo history, as in any editor.
  void record(historyAction action);

  // Returns the action to revert and moves it onto the redo stack.
  [[nodiscard]] std::optional<historyAction> takeUndo();
  // Returns the action to re-apply and moves it back onto the undo stack.
  [[nodiscard]] std::optional<historyAction> takeRedo();

  [[nodiscard]] bool canUndo() const { return !undo_.empty(); }
  [[nodiscard]] bool canRedo() const { return !redo_.empty(); }

private:
  std::vector<historyAction> undo_;
  std::vector<historyAction> redo_;
};

}  // namespace catalist

#endif  // CATALIST_MODEL_UNDO_STACK_HPP_
