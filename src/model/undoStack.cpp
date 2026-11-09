#include "catalist/model/undoStack.hpp"

namespace catalist {

void undoStack::record(historyAction action) {
  undo_.push_back(std::move(action));
  redo_.clear();
}

void undoStack::clear() {
  undo_.clear();
  redo_.clear();
}

std::optional<historyAction> undoStack::takeUndo() {
  if (undo_.empty())
    return std::nullopt;
  historyAction action = undo_.back();
  undo_.pop_back();
  redo_.push_back(action);
  return action;
}

std::optional<historyAction> undoStack::takeRedo() {
  if (redo_.empty())
    return std::nullopt;
  historyAction action = redo_.back();
  redo_.pop_back();
  undo_.push_back(action);
  return action;
}

}  // namespace catalist
