#include "include/models/tiles/SpecialTile.hpp"

SpecialTile::SpecialTile(const std::string &code, const std::string &id, const std::string &name, const std::string &type)
    : ActionTile(code, id, name, type) {}

void SpecialTile::onLanded(Player &player, GameContext &ctx) { ActionTile::onLanded(player, ctx); }
