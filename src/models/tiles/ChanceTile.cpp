#include "include/models/tiles/ChanceTile.hpp"
#include "include/core/GameContext.hpp"

ChanceTile::ChanceTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : CardTile(code, id, name, type) {}

void ChanceTile::onLanded(Player &player, GameContext &ctx) { ctx.drawAndResolveChance(player); }
