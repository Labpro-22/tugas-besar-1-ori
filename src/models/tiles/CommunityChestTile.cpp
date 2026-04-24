#include "include/models/tiles/CommunityChestTile.hpp"
#include "include/core/GameContext.hpp"

CommunityChestTile::CommunityChestTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : CardTile(code, id, name, type) {}

void CommunityChestTile::onLanded(Player &player, GameContext &ctx) { ctx.drawAndResolveCommunityChest(player); }
