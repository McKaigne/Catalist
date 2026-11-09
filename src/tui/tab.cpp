#include "catalist/tui/tab.hpp"

namespace catalist {

using namespace ftxui;

ftxui::Component makeTabComponent(iTab& tab, appContext& ctx) {
  auto rendered = Renderer(tab.focusRoot(), [&tab, &ctx] { return tab.render(ctx); });
  return CatchEvent(rendered, [&tab, &ctx](Event event) {
    return tab.onEvent(event, ctx);
  });
}

void saveConfig(appContext& ctx) {
  ctx.config.save(ctx.fileSystem, ctx.db.getConfigPath() / "catalist.kdl");
}

}  // namespace catalist
