#ifndef CATALIST_TUI_TAB_REGISTRY_HPP_
#define CATALIST_TUI_TAB_REGISTRY_HPP_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "catalist/config/kdl.hpp"
#include "catalist/tui/tab.hpp"

namespace catalist {

// Owns every tab, keyed by id(), and decides which are shown and in what
// order from the `top-bar` node of catalist.kdl.
class tabRegistry {
public:
  void add(std::unique_ptr<iTab> tab);
  [[nodiscard]] iTab* find(const std::string& id) const;

  // Tabs named by `tab "<id>"` children of the top bar, in file order;
  // unknown ids are skipped. Without any, every tab in registration order.
  [[nodiscard]] std::vector<iTab*> ordered(const kdl::document& config) const;

private:
  std::vector<std::string> registrationOrder_;
  std::unordered_map<std::string, std::unique_ptr<iTab>> tabs_;
};

}  // namespace catalist

#endif  // CATALIST_TUI_TAB_REGISTRY_HPP_
