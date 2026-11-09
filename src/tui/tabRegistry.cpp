#include "catalist/tui/tabRegistry.hpp"

#include <algorithm>

namespace catalist {

void tabRegistry::add(std::unique_ptr<iTab> tab) {
  std::string tabId = tab->id();
  if (!tabs_.count(tabId))
    registrationOrder_.push_back(tabId);
  tabs_[tabId] = std::move(tab);
}

iTab* tabRegistry::find(const std::string& id) const {
  auto found = tabs_.find(id);
  return found == tabs_.end() ? nullptr : found->second.get();
}

std::vector<iTab*> tabRegistry::ordered(const kdl::document& config) const {
  std::vector<iTab*> shown;
  auto topBar = config.getNode("top-bar");
  for (const auto& item : topBar ? topBar->children : std::vector<kdl::node>{}) {
    if (item.name != "tab")
      continue;
    iTab* tab = find(item.getArgString(0));
    bool alreadyShown = std::find(shown.begin(), shown.end(), tab) != shown.end();
    if (tab && !alreadyShown)
      shown.push_back(tab);
  }
  if (!shown.empty())
    return shown;
  for (const auto& tabId : registrationOrder_)
    shown.push_back(tabs_.at(tabId).get());
  return shown;
}

}  // namespace catalist
